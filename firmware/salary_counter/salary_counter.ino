#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <time.h>
#include <esp_random.h>
#include "ui.h"
#include "display.h"
#include "peripherals.h"
#include "app_host.h"
#include "connectivity.h"
#include "imu.h"
#include "audio_test.h"

Preferences settings;
Preferences lightSettings;
Preferences systemSettings;
orientation::Axes imuAxes;
orientation::Detector autoRotation;
bool rotationAuto=true;
int calibrationDown=0;
uint32_t lastImu=0;
lightboard::View lightView;
uint32_t lightFrames=0,lightMaxMs=0,lastLightFrame=0;
ui::Canvas canvas{nullptr};
salary::Date current{};
bool clockOK=false, synced=false, displayOK=false;
uint32_t lastDraw=0,lastLog=0;
ui::MoneyAnimation moneyAnimation;
uint32_t lastAnimation=0,animationFrames=0,animationMaxMs=0;
bool wasAnimating=false;
char command[256]; size_t commandSize=0;
bool commandOverflow=false;
int64_t monthlyCents=salary::MONTHLY_CENTS;
salary::Schedule workSchedule;
bool screenOn=true,editing=false;
ui::Battery powerState;
uint32_t powerSamples=0,powerChanges=0,lastPowerRead=0;
interaction::Editor editor;
interaction::Gesture gesture;
apps::Id foreground=apps::HOME;
apps::Navigation navigation;
uint32_t homeKeyCount=0;
emotions::View emotionView;
meow::View meowView;
uint32_t lastMeowFrame=0,meowFrames=0,meowMaxMs=0;
uint32_t frameMs=0,lastEmotionFrame=0,emotionFrames=0,emotionMaxMs=0;
uint32_t emotionRenderUs=0,emotionTransferUs=0;
void renderPage(int progress) {
  if(quotaLink::effect.active){quotaLink::effect.render(canvas,frameMs);return;}
  if(editing) apps::renderSettings(canvas,editor,powerState);
  else apps::render(canvas,foreground,{current,clockOK,monthlyCents,powerState,moneyAnimation,progress,navigation.offset(),&wireless::model,&wireless::view,&emotionView,frameMs,&lightView,&quotaLink::view,meowView.elapsed,meowView.variant,workSchedule});
  quotaLink::effect.render(canvas,frameMs);
}
uint32_t quotaFrames=0,quotaMaxMs=0,lastQuotaFrame=0;
void paintQuota() {
  uint32_t started=millis();frameMs=started;
  for(int y=112;y<336;y+=panel::TILE_ROWS) {
    canvas.top=y;canvas.rows=panel::TILE_ROWS;renderPage(1000);
    if(!panel::tile(y,canvas.pixels)){displayOK=false;break;}
  }
  quotaFrames++;quotaMaxMs=max(quotaMaxMs,millis()-started);
}

bool readRegs(uint8_t reg,uint8_t *data,size_t count) {
  Wire.beginTransmission(0x51); Wire.write(reg);
  if(Wire.endTransmission(false)!=0) return false;
  if(Wire.requestFrom((uint8_t)0x51,count)!=count) return false;
  for(size_t i=0;i<count;i++) data[i]=Wire.read();
  return true;
}
bool writeRegs(uint8_t reg,const uint8_t *data,size_t count) {
  Wire.beginTransmission(0x51); Wire.write(reg); Wire.write(data,count);
  return Wire.endTransmission()==0;
}
int bcd(uint8_t n) { return (n>>4)*10+(n&15); }
uint8_t tobcd(int n) { return (n/10)*16+n%10; }
bool readClock(salary::Date &d) {
  uint8_t data[7], control;
  if(!readRegs(0,&control,1)||!readRegs(4,data,7)) return false;
  if((data[0]&0x80)||(control&0x22)) return false; // oscillator stopped, STOP, or 12-hour mode
  const uint8_t masks[]={0x7f,0x7f,0x3f,0x3f,0x07,0x1f,0xff};
  for(int i=0;i<7;i++) if((data[i]&masks[i]&15)>9) return false;
  d={2000+bcd(data[6]),bcd(data[5]&31),bcd(data[3]&63),bcd(data[2]&63),bcd(data[1]&127),bcd(data[0]&127)};
  return salary::valid(d);
}
bool syncClock(int64_t epoch) {
  if(epoch<1704067200LL||epoch>4102444799LL) return false;
  time_t localEpoch=(time_t)(epoch+8*3600); tm t{}; gmtime_r(&localEpoch,&t);
  salary::Date d={t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec};
  if(!salary::valid(d)) return false;
  uint8_t control;
  if(!readRegs(0,&control,1)) return false;
  uint8_t stopped=(control&~2)|0x20;
  if(!writeRegs(0,&stopped,1)) return false;
  uint8_t values[]={tobcd(t.tm_sec),tobcd(t.tm_min),tobcd(t.tm_hour),tobcd(t.tm_mday),(uint8_t)t.tm_wday,tobcd(t.tm_mon+1),tobcd((t.tm_year+1900)%100)};
  bool ok=writeRegs(4,values,7);
  control &= ~0x22;
  bool restarted=writeRegs(0,&control,1);
  if(!ok||!restarted) return false;
  salary::Date checked{};
  if(!readClock(checked)) return false;
  synced=true; settings.putBool("synced",true);
  return true;
}
int64_t notificationClock() {
  salary::Date d{};if(!readClock(d))return 0;
  int days=0;
  for(int y=1970;y<d.year;y++)days+=salary::leap(y)?366:365;
  for(int m=1;m<d.month;m++)days+=salary::days(d.year,m);
  return int64_t(days+d.day-1)*86400+d.hour*3600+d.minute*60+d.second-28800;
}
void paint(bool amountOnly=false) {
  if(quotaLink::effect.active)amountOnly=false;
  if(displayOK&&screenOn) {
    if(amountOnly&&(editing||foreground!=apps::SALARY)) return;
    uint32_t started=millis();
    frameMs=started;
    int progress=moneyAnimation.progress(started);
    if(amountOnly) {
      canvas.top=panel::AMOUNT_TOP; canvas.rows=panel::AMOUNT_ROWS;
      canvas.rect(0,canvas.top,ui::W,canvas.rows,ui::BG);
      ui::rollingMoney(canvas,moneyAnimation.from,moneyAnimation.to,progress);
      if(!panel::tile(canvas.top,canvas.pixels,canvas.rows)) { displayOK=false; Serial.println("DISPLAY_TRANSFER_ERROR"); }
      canvas.rows=panel::TILE_ROWS;
    } else for(int y=0;y<ui::H;y+=panel::TILE_ROWS) {
      canvas.top=y; renderPage(progress);
      if(!panel::tile(y,canvas.pixels)) { displayOK=false; Serial.println("DISPLAY_TRANSFER_ERROR"); break; }
    }
    static bool measured=false;
    if(!measured) { Serial.printf("RENDER_MS %u\n",millis()-started); measured=true; }
    if(amountOnly) { ++animationFrames; uint32_t duration=millis()-started; if(duration>animationMaxMs) animationMaxMs=duration; }
  }
}
void paintMeow() {
  uint32_t started=millis();frameMs=started;
  for(int y=meow::TOP;y<meow::TOP+meow::ROWS;y+=panel::AMOUNT_ROWS) {
    canvas.top=y;canvas.rows=min(panel::AMOUNT_ROWS,meow::TOP+meow::ROWS-y);
    renderPage(1000);
    if(!panel::tile(y,canvas.pixels,canvas.rows)){displayOK=false;break;}
  }
  canvas.rows=panel::TILE_ROWS;
  meowFrames++;meowMaxMs=max(meowMaxMs,millis()-started);
}
void paintEyes() {
  if(!displayOK||!screenOn||foreground!=apps::EMOTIONS||!emotionView.showing) return;
  uint32_t started=millis(); frameMs=started;
  emotionView.tick(started);
  uint32_t renderUs=0,transferUs=0;
  for(int y=emotions::FRAME_TOP;y<emotions::FRAME_TOP+emotions::FRAME_ROWS;y+=panel::AMOUNT_ROWS) {
    int remaining=emotions::FRAME_TOP+emotions::FRAME_ROWS-y;
    canvas.rows=remaining<panel::AMOUNT_ROWS?remaining:panel::AMOUNT_ROWS;
    canvas.top=y; uint32_t mark=micros(); renderPage(1000); renderUs+=micros()-mark;
    mark=micros();
    if(!panel::tile(y,canvas.pixels,canvas.rows)) { displayOK=false; Serial.println("DISPLAY_TRANSFER_ERROR"); break; }
    transferUs+=micros()-mark;
  }
  canvas.rows=panel::TILE_ROWS;
  emotionRenderUs=renderUs; emotionTransferUs=transferUs;
  ++emotionFrames; uint32_t elapsed=millis()-started; if(elapsed>emotionMaxMs) emotionMaxMs=elapsed;
}
void paintLight() {
  if(!displayOK||!screenOn||foreground!=apps::LIGHTBOARD||lightView.editing) return;
  uint32_t started=millis(); lightView.tick(started);
  for(int y=lightboard::FRAME_TOP;y<lightboard::FRAME_TOP+lightboard::FRAME_ROWS;y+=panel::AMOUNT_ROWS) {
    canvas.top=y; canvas.rows=panel::AMOUNT_ROWS; renderPage(1000);
    if(!panel::tile(y,canvas.pixels,canvas.rows)) { displayOK=false; Serial.println("DISPLAY_TRANSFER_ERROR"); break; }
  }
  canvas.rows=panel::TILE_ROWS;
  lightFrames++; uint32_t duration=millis()-started; if(duration>lightMaxMs) lightMaxMs=duration;
}
void applyRotation(int q) {
  meowView.cancelTouch();
  quotaLink::view.cancelTouch();
  autoRotation.current=q; autoRotation.reset();
  if(panel::rotation==q) return;
  gesture.cancel(); navigation.cancel(); wireless::view.cancelTouch(); emotionView.cancelTouch(); lightView.cancelTouch();
  panel::rotation=q;
  paint(); // Redraw every logical tile: old orientation must not leave stale strips.
  Serial.printf("ORIENTATION_CHANGED %d\n",q*90);
}
void orientationCommand() {
  if(strcmp(command,"ORIENT AUTO")==0) { rotationAuto=true; autoRotation.reset(); Serial.println("ORIENT_AUTO_OK"); return; }
  if(strlen(command)==8&&command[7]>='0'&&command[7]<='3') {
    rotationAuto=false; applyRotation(command[7]-'0'); Serial.println("ORIENT_MANUAL_OK"); return;
  }
  if(strcmp(command,"ORIENT CAL DOWN")==0||strcmp(command,"ORIENT CAL RIGHT")==0) {
    if(!imu::fresh||uint32_t(millis()-imu::sampledAt)>250||!imu::stableAxis||uint32_t(millis()-imu::stableSince)<450) {
      Serial.println("ORIENT_CAL_ERROR hold device upright and still"); return;
    }
    if(strcmp(command,"ORIENT CAL DOWN")==0) { calibrationDown=imu::stableAxis; Serial.println("ORIENT_CAL_DOWN_OK"); return; }
    orientation::Axes next{(int8_t)imu::stableAxis,(int8_t)calibrationDown};
    if(!next.valid()) { Serial.println("ORIENT_CAL_ERROR rotate right edge downward by 90 degrees"); return; }
    if(systemSettings.putBytes("imu-axes",&next,sizeof(next))!=sizeof(next)) { Serial.println("ORIENT_CAL_SAVE_ERROR"); return; }
    imuAxes=next; rotationAuto=true; autoRotation.reset(); calibrationDown=0; Serial.println("ORIENT_CAL_SAVED"); return;
  }
  Serial.println("ORIENT_ERROR");
}
void lightAction(lightboard::Action a) {
  if(a==lightboard::SAVE) {
    if(!lightboard::valid(lightView.draft)||lightSettings.putBytes("config",&lightView.draft,sizeof(lightView.draft))!=sizeof(lightView.draft)) {
      lightView.error=true; Serial.println("LIGHT_SAVE_ERROR");
    } else { lightView.accepted(millis()); Serial.println("LIGHT_SAVED"); }
  }
  if(a!=lightboard::NONE) { if(!lightView.editing) lightView.lastTick=millis(); paint(); }
}
void draw(bool refresh=true) {
  salary::Date old=current;
  bool wasValid=clockOK;
  bool beginAnimation=false;
  clockOK=synced&&readClock(current);
  if(clockOK) {
    auto value=salary::calculate(current,monthlyCents,workSchedule).earned_cents;
    bool sameDay=old.year==current.year&&old.month==current.month&&old.day==current.day;
    beginAnimation=wasValid&&sameDay&&value>moneyAnimation.to;
    moneyAnimation.set(value,millis(),wasValid&&sameDay&&value>moneyAnimation.to);
  }
  if(refresh&&!editing) paint();
  // Start after the once-per-second full refresh, so it cannot consume animation frames.
  if(beginAnimation) moneyAnimation.started=millis();
}
void status() {
  Serial.printf("MEOW elapsed=%u pose=%d frames=%u max_ms=%u decode_error=%d variant=%d\n",meowView.elapsed,meow::pose(meowView.elapsed),meowFrames,meowMaxMs,meow::decodeError,meowView.variant);
  Serial.printf("APPEARANCE mode=%d stage=%d details=%d\n",quotaLink::view.appearance,quotaLink::view.stage(),quotaLink::view.details);
  Serial.printf("NOTIFY enabled=%d total=%llu pending=%llu active=%d\n",quotaLink::view.notify,(unsigned long long)quotaLink::effect.total,(unsigned long long)quotaLink::effect.pending,quotaLink::effect.active);
  Serial.printf("QUOTA remaining=%d minutes=%d source=%s stale=%d sequence=%llu frames=%u max_ms=%u\n",quotaLink::view.sample.remaining,quotaLink::view.sample.minutes,quotaLink::view.source,quotaLink::view.stale(millis()),(unsigned long long)quotaLink::view.sample.sequence,quotaFrames,quotaMaxMs);
  auto r=clockOK?salary::calculate(current,monthlyCents,workSchedule):salary::Result{};
  Serial.printf("SCHEDULE start=%02d:%02d end=%02d:%02d\n",workSchedule.start/3600,workSchedule.start/60%60,workSchedule.end/3600,workSchedule.end/60%60);
  Serial.printf("PAYDAYS fixed100=%d effective100=%d daily_cents=%lld hourly_cents=%lld\n",workSchedule.days100,workSchedule.days100?workSchedule.days100:r.work_days*100,(long long)r.daily_cents,(long long)r.hourly_cents);
  Serial.printf("STATUS clock=%d lcd=%d date=%04d-%02d-%02d time=%02d:%02d:%02d state=%d cents=%lld workdays=%d progress=%d heap=%u\n",
    clockOK,displayOK,current.year,current.month,current.day,current.hour,current.minute,current.second,
    r.state,(long long)r.earned_cents,r.work_days,r.progress,ESP.getFreeHeap());
  Serial.printf("ANIMATION frames=%u max_ms=%u active=%d\n",animationFrames,animationMaxMs,moneyAnimation.active(millis()));
  Serial.printf("UI screen=%d editing=%d monthly_cents=%lld battery_valid=%d battery_present=%d battery_percent=%d charging=%d usb=%d\n",screenOn,editing,(long long)monthlyCents,powerState.valid,powerState.present,powerState.percent,powerState.charging,powerState.usb);
  const auto *app=apps::find(foreground);
  Serial.printf("APP %s\n",app?app->key:"home");
  Serial.printf("HOME_KEY pin=%d level=%d presses=%u\n",peripheral::HOME_PIN,digitalRead(peripheral::HOME_PIN),homeKeyCount);
  Serial.printf("POWER samples=%u changes=%u age_ms=%u\n",powerSamples,powerChanges,millis()-lastPowerRead);
  Serial.printf("ORIENT auto=%d calibrated=%d degrees=%d right_axis=%d down_axis=%d imu=%d address=%u x=%d y=%d z=%d samples=%u errors=%u age_ms=%u\n",rotationAuto,imuAxes.valid(),panel::rotation*90,imuAxes.right,imuAxes.down,imu::ready,imu::address,imu::x,imu::y,imu::z,imu::samples,imu::errors,millis()-imu::sampledAt);
  wireless::status();
  Serial.printf("EMOTION showing=%d mood=%d frames=%u max_ms=%u free=%d interval_ms=%u\n",emotionView.showing,emotionView.mood,emotionFrames,emotionMaxMs,emotionView.free,emotionView.interval);
  Serial.printf("EMOTION_TIMING render_us=%u transfer_us=%u\n",emotionRenderUs,emotionTransferUs);
  Serial.printf("LIGHT editing=%d typing=%d color=%u speed=%u text_bytes=%u offset=%u frames=%u max_ms=%u signature=%08x\n",lightView.editing,lightView.typing,lightView.saved.color,lightView.saved.speed,(unsigned)strlen(lightView.saved.text),lightView.offset,lightFrames,lightMaxMs,lightboard::signature(lightView.saved));
}
void openApp(apps::Id id) {
  meowView.cancelTouch();
  quotaLink::view.details=false;quotaLink::view.cancelTouch();
  if(id!=apps::HOME&&!apps::find(id)) return;
  foreground=id; editing=false; gesture.cancel(); navigation.cancel(); wasAnimating=false;
  wireless::view.reset();
  emotionView.enter();
  lightView.enter(millis());
  moneyAnimation.from=moneyAnimation.to=salary::calculate(current,monthlyCents,workSchedule).earned_cents;
  draw();
  const auto *app=apps::find(id); Serial.printf("APP_OPEN %s\n",app?app->key:"home");
}
void screen(bool on) {
  meowView.cancelTouch();
  quotaLink::view.cancelTouch();
  lightView.cancelTouch(); lightView.lastTick=millis();
  gesture.cancel(); navigation.cancel(); wireless::view.cancelTouch(); emotionView.cancelTouch();
  if(!displayOK||on==screenOn) return;
  if(!on) { if(panel::byte(0x51,0)&&panel::command(0x28)) screenOn=false; }
  else {
    screenOn=true; draw(); if(editing) paint();
    if(!panel::command(0x29)||!panel::byte(0x51,0xA0)) displayOK=false;
  }
  Serial.printf("SCREEN %d\n",screenOn);
}
void homeKeyAction() {
  if(screenOn) { openApp(apps::HOME); Serial.println("HOME_KEY_HANDLED"); }
  else Serial.println("HOME_KEY_IGNORED_SCREEN_OFF");
}
void action(interaction::Action a) {
  if(a==interaction::OPEN) { editor.begin(monthlyCents,workSchedule); editing=true; paint(); Serial.println("SETTINGS_OPEN"); }
  else if(a==interaction::EDIT) paint();
  else if(a==interaction::CANCEL) { editing=false; gesture.cancel(); draw(); Serial.println("SETTINGS_CANCEL"); }
  else if(a==interaction::SAVE) {
    salary::Config next;
    if(!editor.config(next)||settings.putBytes("config-v2",&next,sizeof(next))!=sizeof(next)) { editor.error=true; paint(); Serial.println("SETTINGS_ERROR");return; }
    monthlyCents=next.monthly;workSchedule={next.start,next.end,next.days100};editing=false; gesture.cancel();
    moneyAnimation.set(salary::calculate(current,monthlyCents,workSchedule).earned_cents,millis(),false);
    draw(); Serial.printf("SETTINGS_SAVED %lld\n",(long long)monthlyCents);
  }
}
void handleCommand() {
  command[commandSize]=0;
  if(commandOverflow) Serial.println("ERROR command too long");
  else if(strcmp(command,"HELLO")==0) Serial.println("SALARY_CLOCK_V1");
  else if(strcmp(command,"AUDIO TEST")==0) audioTest::play();
  else if(strcmp(command,"QHELLO")==0) Serial.println(quotaLink::hello);
  else if(strncmp(command,"QPAIR ",6)==0) Serial.println(quotaLink::pair(command+6)?"QPAIR_OK":"QPAIR_REJECT");
  else if(strncmp(command,"Q1 ",3)==0||strncmp(command,"N1 ",3)==0||strncmp(command,"N2 ",3)==0) {
    bool ok=quotaLink::receive(command,true);Serial.println(ok?"QUOTA_OK":"QUOTA_REJECT");
    if(ok&&foreground==apps::QUOTA)paint();
  }
  else if(strcmp(command,"APP QUOTA")==0) openApp(apps::QUOTA);
  else if(strcmp(command,"NOTIFY TOGGLE")==0) {Serial.println(quotaLink::toggleNotify()?"NOTIFY_OK":"NOTIFY_ERROR");paint();}
  else if(strcmp(command,"NOTIFY TEST")==0) {if(quotaLink::effect.enabled)quotaLink::effect.enqueue(millis()+60000);Serial.println("NOTIFY_TEST_OK");}
  else if(strncmp(command,"QUOTA STYLE ",12)==0) {
    char *end;long mode=strtol(command+12,&end,10);
    bool ok=end!=command+12&&!*end&&mode>=-1&&mode<=4&&quotaLink::choose(mode);
    Serial.println(ok?"QUOTA_STYLE_OK":"QUOTA_STYLE_ERROR");if(foreground==apps::QUOTA)paint();
  }
  else if(strcmp(command,"QUOTA SETTINGS")==0&&foreground==apps::QUOTA&&screenOn) {quotaLink::view.details=true;quotaLink::view.cancelTouch();paint();}
  else if(strncmp(command,"ORIENT ",7)==0) orientationCommand();
  else if(strcmp(command,"SCREEN OFF")==0) screen(false);
  else if(strcmp(command,"SCREEN ON")==0) screen(true);
  else if(strcmp(command,"HOME KEY")==0) homeKeyAction(); // Same action, no synthetic GPIO event.
  else if(strcmp(command,"APP HOME")==0) openApp(apps::HOME);
  else if(strcmp(command,"APP SALARY")==0) openApp(apps::SALARY);
  else if(strcmp(command,"APP CLOCK")==0) openApp(apps::CLOCK);
  else if(strcmp(command,"APP MEOW")==0) openApp(apps::MEOW);
  else if((strcmp(command,"MEOW LEFT")==0||strcmp(command,"MEOW RIGHT")==0)&&foreground==apps::MEOW&&screenOn&&!quotaLink::effect.active) {
    bool left=strcmp(command,"MEOW LEFT")==0;
    meowView.cancelTouch();meowView.touch(true,240,280);
    meowView.touch(true,left?140:340,280);
    if(meowView.touch(false,0,0))paint();
    Serial.println("MEOW_SWIPE_OK");
  }
  else if(strcmp(command,"APP SETTINGS")==0) openApp(apps::SETTINGS);
  else if(strcmp(command,"APP EMOTIONS")==0) openApp(apps::EMOTIONS);
  else if(strcmp(command,"APP LIGHTBOARD")==0) openApp(apps::LIGHTBOARD);
  else if(strcmp(command,"LIGHT OPEN")==0&&foreground==apps::LIGHTBOARD&&screenOn) { lightView.begin(); paint(); }
  else if(strcmp(command,"LIGHT INPUT")==0&&foreground==apps::LIGHTBOARD&&lightView.editing) { lightView.typing=true; lightView.pinyin=true; lightView.composition[0]=0; lightView.keyboard.keyboard=0; paint(); }
  else if(strncmp(command,"LIGHT PINYIN ",13)==0&&foreground==apps::LIGHTBOARD&&lightView.editing) {
    const char *py=command+13; bool valid=*py&&strlen(py)<=6;
    for(const char *p=py;*p;p++) if(*p<'a'||*p>'z') valid=false;
    if(valid) { strcpy(lightView.composition,py); lightView.candidatePage=0; lightView.pinyin=true; lightView.typing=true; paint(); Serial.printf("LIGHT_PINYIN_OK candidates=%d\n",lightView.candidateCount()); }
    else Serial.println("LIGHT_PINYIN_ERROR");
  }
  else if(strncmp(command,"LIGHT PICK ",11)==0&&foreground==apps::LIGHTBOARD&&lightView.editing) {
    char *end; long index=strtol(command+11,&end,10);
    if(end!=command+11&&!*end&&index>=0&&index<lightView.candidateCount()&&lightView.selectCandidate(index)) { paint(); Serial.println("LIGHT_PICK_OK"); }
    else Serial.println("LIGHT_PICK_ERROR");
  }
  else if(strcmp(command,"LIGHT CANCEL")==0&&foreground==apps::LIGHTBOARD) { lightView.enter(millis()); paint(); }
  else if(strcmp(command,"LIGHT SAVE")==0&&foreground==apps::LIGHTBOARD&&lightView.editing) lightAction(lightboard::SAVE);
  else if(strncmp(command,"LIGHT TEXT ",11)==0&&foreground==apps::LIGHTBOARD&&lightView.editing) {
    lightboard::Config next=lightView.draft;
    if(strlen(command+11)>lightboard::MAX_BYTES) Serial.println("LIGHT_TEXT_ERROR");
    else {
      strcpy(next.text,command+11);
      if(lightboard::valid(next)) { lightView.draft=next; lightView.replace=false; paint(); Serial.println("LIGHT_TEXT_OK"); }
      else Serial.println("LIGHT_TEXT_ERROR");
    }
  }
  else if((strncmp(command,"LIGHT COLOR ",12)==0||strncmp(command,"LIGHT SPEED ",12)==0)&&foreground==apps::LIGHTBOARD&&lightView.editing) {
    bool color=command[6]=='C'; char *end; long value=strtol(command+12,&end,10);
    if(end!=command+12&&!*end&&value>=0&&value<=(color?8:4)) {
      if(color) lightView.draft.color=value; else lightView.draft.speed=value;
      paint(); Serial.println("LIGHT_OPTION_OK");
    } else Serial.println("LIGHT_OPTION_ERROR");
  }
  else if(strncmp(command,"MOOD ",5)==0&&foreground==apps::EMOTIONS) {
    char *end=nullptr; long mood=strtol(command+5,&end,10);
    if(end!=command+5&&!*end&&mood>=0&&mood<=emotions::FREE&&emotionView.choose(mood,millis())) { paint(); Serial.println("MOOD_OK"); }
    else Serial.println("MOOD_ERROR");
  }
  else if(strcmp(command,"EMOTIONS MENU")==0&&foreground==apps::EMOTIONS) { emotionView.enter(); paint(); }
  else if(strcmp(command,"NET WIFI")==0) { openApp(apps::SETTINGS); wireless::view.page=connectivity::WIFI; paint(); }
  else if(strcmp(command,"NET BLE")==0) { openApp(apps::SETTINGS); wireless::view.page=connectivity::BLE; paint(); }
  else if(strcmp(command,"WIFI ON")==0) wireless::wifiOn(true);
  else if(strcmp(command,"WIFI OFF")==0) wireless::wifiOn(false);
  else if(strcmp(command,"WIFI SCAN")==0) wireless::scan();
  else if(strcmp(command,"BLE ON")==0) wireless::bluetoothOn(true);
  else if(strcmp(command,"BLE OFF")==0) wireless::bluetoothOn(false);
  else if(strcmp(command,"BLE RETRY")==0) Serial.printf("BLE_RETRY result=%d\n",wireless::retryAdvertising());
  else if(strcmp(command,"SETTINGS OPEN")==0&&screenOn&&foreground==apps::SALARY) action(interaction::OPEN);
  else if(strcmp(command,"SETTINGS CANCEL")==0&&editing) action(interaction::CANCEL);
  else if(strcmp(command,"SETTINGS SAVE")==0&&editing) action(interaction::SAVE);
  else if(strncmp(command,"SETTINGS TAB ",13)==0&&editing&&strlen(command)==14&&command[13]>='0'&&command[13]<='3') {editor.select(command[13]-'0');action(interaction::EDIT);}
  else if(strncmp(command,"KEY ",4)==0&&editing&&strlen(command)==5) { editor.key(command[4]); action(interaction::EDIT); }
  else if(strncmp(command,"TIME ",5)==0) {
    char *end=nullptr; int64_t epoch=strtoll(command+5,&end,10);
    bool ok=end!=command+5&&*end==0&&syncClock(epoch);
    Serial.println(ok?"TIME_OK":"TIME_ERROR"); draw(); status();
  } else if(strcmp(command,"PROBE")==0) {
    for(uint8_t addr=1;addr<127;addr++) {
      Wire.beginTransmission(addr);
      if(Wire.endTransmission()==0) Serial.printf("I2C_FOUND 0x%02X\n",addr);
    }
    Serial.println("PROBE_DONE");
  } else if(strcmp(command,"STATUS")==0) { draw(); status(); }
  else if(strcmp(command,"FRAME")==0 && displayOK) {
    frameMs=millis();
    int progress=moneyAnimation.progress(millis());
    Serial.printf("FRAME %d %d %d\n",ui::W,ui::H,ui::W*ui::H*2);
    for(int y=0;y<ui::H;y+=panel::TILE_ROWS) {
      canvas.top=y; renderPage(progress);
      Serial.write((uint8_t*)canvas.pixels,ui::W*panel::TILE_ROWS*2);
    }
    Serial.print("\nFRAME_END\n");
  } else if(strcmp(command,"REBOOT")==0) { Serial.println("REBOOTING"); Serial.flush(); delay(100); ESP.restart(); }
  else if(commandSize) Serial.println("ERROR unknown command");
  commandSize=0; commandOverflow=false;
}
void setup() {
  quotaLink::wallClock=notificationClock;
  Serial.begin(115200);
  quotaLink::begin();
  emotionView.seed(esp_random());
  Wire.begin(8,7); Wire.setClock(100000); Wire.setTimeOut(100);
  settings.begin("salary-mvp",false); synced=settings.getBool("synced",false);
  systemSettings.begin("desk-system",false);
  orientation::Axes axes;
  if(systemSettings.getBytesLength("imu-axes")==sizeof(axes)&&systemSettings.getBytes("imu-axes",&axes,sizeof(axes))==sizeof(axes)&&axes.valid()) imuAxes=axes;
  monthlyCents=settings.getLong64("monthly",salary::MONTHLY_CENTS);
  if(monthlyCents<0||monthlyCents>salary::MAX_MONTHLY_CENTS) monthlyCents=salary::MONTHLY_CENTS;
  salary::ConfigV1 saved;
  if(settings.getBytesLength("config-v1")==sizeof(saved)&&settings.getBytes("config-v1",&saved,sizeof(saved))==sizeof(saved)&&saved.valid()) {
    monthlyCents=saved.monthly;workSchedule={saved.start,saved.end};
  }
  salary::Config savedV2;
  if(settings.getBytesLength("config-v2")==sizeof(savedV2)&&settings.getBytes("config-v2",&savedV2,sizeof(savedV2))==sizeof(savedV2)&&savedV2.valid()) {
    monthlyCents=savedV2.monthly;workSchedule={savedV2.start,savedV2.end,savedV2.days100};
  }
  lightSettings.begin("desk-light",false);
  lightboard::Config loaded;
  if(lightSettings.getBytesLength("config")==sizeof(loaded)&&lightSettings.getBytes("config",&loaded,sizeof(loaded))==sizeof(loaded)&&lightboard::valid(loaded)) lightView.saved=loaded;
  lightView.enter(millis());
  canvas.rows=panel::TILE_ROWS;
  canvas.pixels=(uint16_t*)heap_caps_malloc(ui::W*panel::AMOUNT_ROWS*2,MALLOC_CAP_DMA);
  displayOK=canvas.pixels&&panel::begin();
  peripheral::begin(); powerState=peripheral::battery();
  imu::begin();
  powerSamples=1; lastPowerRead=millis();
  draw();
  wireless::begin();
  Serial.printf("SALARY_MVP 2.1 schedule ESP32-C6 AMOLED-2.16 480x480 monthly=%lld weekday-only lunch-paid timezone=UTC+8\n",(long long)(monthlyCents/100));
  // Probe only: do not change PMU voltages or charging settings.
  for(uint8_t addr: {uint8_t(0x34),uint8_t(0x51),uint8_t(0x5A)}) {
    Wire.beginTransmission(addr); Serial.printf("I2C 0x%02X ack=%d\n",addr,Wire.endTransmission()==0);
  }
  status();
}
void loop() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n') handleCommand();
    else if(c!='\r') {
      if(commandSize<sizeof(command)-1) command[commandSize++]=c;
      else commandOverflow=true;
    }
  }
  uint32_t now=millis();
  if(uint32_t(now-lastImu)>=40) {
    lastImu=now;
    if(imu::poll(now)&&rotationAuto&&imuAxes.valid()) {
      int x=orientation::Axes::component(imuAxes.right,imu::x,imu::y);
      int y=orientation::Axes::component(imuAxes.down,imu::x,imu::y);
      if(autoRotation.sample(x,y,imu::z,now)) applyRotation(autoRotation.current);
    }
  }
  if(peripheral::homePressed(now)) {
    ++homeKeyCount;
    Serial.printf("HOME_KEY_PRESSED %u\n",homeKeyCount);
    homeKeyAction();
  }
  static bool buttonRaw=true,buttonStable=true;
  static uint32_t buttonChanged=0,lastTouch=0,lastPower=0;
  bool button=digitalRead(9);
  if(button!=buttonRaw) { buttonRaw=button; buttonChanged=now; }
  if(button!=buttonStable&&uint32_t(now-buttonChanged)>=35) {
    buttonStable=button; if(!button) screen(!screenOn);
  }
  if(now-lastTouch>=25) {
    lastTouch=now; static int x=0,y=0; static bool previous=false;
    bool pressed=false;
    if(peripheral::touch(pressed,x,y)) {
      if(pressed) { auto p=orientation::logical({x,y},panel::rotation); x=p.x; y=p.y; }
      if(pressed!=previous) {
        if(!(foreground==apps::SETTINGS&&wireless::view.page==connectivity::PASSWORD)) Serial.printf("TOUCH %d x=%d y=%d\n",pressed,x,y);
        previous=pressed;
      }
      if(screenOn) {
        if(quotaLink::effect.active||(pressed&&ui::systemArea(x,y))) { meowView.cancelTouch(); quotaLink::view.cancelTouch(); gesture.cancel(); navigation.cancel(); wireless::view.cancelTouch(); emotionView.cancelTouch(); lightView.cancelTouch(); }
        else {
        if(foreground==apps::QUOTA) {
          if(quotaLink::view.touch(pressed,x,y,millis())) {
            if(quotaLink::view.requested>=-1)quotaLink::choose(quotaLink::view.requested);
            if(quotaLink::view.notifyRequested)quotaLink::toggleNotify();
            if(quotaLink::view.previewRequested){quotaLink::view.previewRequested=false;quotaLink::effect.preview(millis());}
            paint();
          }
        } else if(foreground==apps::MEOW) {
          if(meowView.touch(pressed,x,y))paint();
        } else if(foreground==apps::LIGHTBOARD) {
          lightAction(lightView.touch(pressed,x,y,millis()));
        } else if(foreground==apps::EMOTIONS) {
          if(emotionView.touch(pressed,x,y,millis())) paint();
        } else if(foreground==apps::SETTINGS) {
          wireless::act(wireless::view.touch(pressed,x,y,wireless::model));
          if(wireless::view.dirty) { wireless::view.dirty=false; paint(); }
        } else {
        int target=editing?-1:navigation.update(foreground,pressed,x,y);
        if(target>=0) openApp(static_cast<apps::Id>(target));
        else if(foreground==apps::SALARY) action(gesture.update(pressed,x,y,now,editing,editor));
        else if(navigation.changed) paint();
        }
        }
      } else { meowView.cancelTouch(); quotaLink::view.cancelTouch(); gesture.cancel(); navigation.cancel(); wireless::view.cancelTouch(); emotionView.cancelTouch(); lightView.cancelTouch(); }
    } else { meowView.cancelTouch(); quotaLink::view.cancelTouch(); gesture.cancel(); navigation.cancel(); wireless::view.cancelTouch(); emotionView.cancelTouch(); lightView.cancelTouch(); }
  }
  if(wireless::poll()&&foreground==apps::SETTINGS) paint();
  if(quotaLink::poll()&&foreground==apps::QUOTA)paint();
  if(now-lastPower>=1000) {
    lastPower=now; auto next=peripheral::battery();
    bool changed=next.valid!=powerState.valid||next.present!=powerState.present||next.charging!=powerState.charging||next.usb!=powerState.usb||next.percent!=powerState.percent;
    powerState=next;
    ++powerSamples; lastPowerRead=millis();
    if(changed) { ++powerChanges; paint(); }
  }
  if(now-lastDraw>=100) { lastDraw=now; static int second=-1;
    salary::Date d{}; bool valid=synced&&readClock(d);
    if(valid!=clockOK||(valid&&d.second!=second)) { draw(foreground==apps::SALARY||foreground==apps::CLOCK); second=valid?current.second:-1; }
  }
  now=millis();
  meowView.tick(now,foreground==apps::MEOW&&screenOn&&displayOK&&!quotaLink::effect.active);
  static uint32_t lastNotice=0;
  bool noticeChanged=quotaLink::effect.tick(now,screenOn);
  if(noticeChanged||(screenOn&&quotaLink::effect.active&&now-lastNotice>=80)){lastNotice=now;paint();}
  if(screenOn&&quotaLink::effect.active&&quotaLink::effect.takeSound())audioTest::play();
  if(quotaLink::effect.active){delay(5);return;}
  if(foreground==apps::QUOTA&&screenOn) {
    static bool stale=true;bool next=quotaLink::view.stale(now);
    if(stale!=next){stale=next;paint();}
    if(!quotaLink::view.details&&displayOK&&now-lastQuotaFrame>=40){lastQuotaFrame=now;paintQuota();}
  }
  if(foreground==apps::LIGHTBOARD&&!lightView.editing&&screenOn&&now-lastLightFrame>=40) { lastLightFrame=now; paintLight(); }
  if(foreground==apps::MEOW&&screenOn&&displayOK&&now-lastMeowFrame>=meow::FRAME_MS) {lastMeowFrame=now;paintMeow();}
  if(foreground==apps::EMOTIONS&&emotionView.showing&&screenOn&&now-lastEmotionFrame>=40) {
    lastEmotionFrame=now; paintEyes();
  }
  bool animating=clockOK&&screenOn&&!editing&&foreground==apps::SALARY&&moneyAnimation.active(now);
  if((animating||wasAnimating)&&now-lastAnimation>=20) {
    lastAnimation=now; paint(true); wasAnimating=animating;
  }
  if(now-lastLog>=10000) { lastLog=now; if(Serial) status(); }
  delay(5);
}
