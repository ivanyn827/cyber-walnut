#pragma once
#include "ui.h"
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <algorithm>
#include "quota_sprite.h"

namespace quota {
static const char *const phrases[]={"彻底蔫了","有点虚了","还能继续","稳得很","我能打十个"};
struct Sample { uint64_t sequence=0; int64_t observed=0,reset=0; int remaining=-1,minutes=0; };
struct View {
  Sample sample;
  uint32_t received=0,usbAt=0,changedAt=0;
  bool hasUSB=false,details=false,down=false,blocked=false;
  uint32_t pressedAt=0; int x0=0,y0=0; float previous=2;
  const char *source="--";
  int appearance=-1,requested=-2; bool saveError=false,notify=true,notifyRequested=false;
  bool tapWaiting=false,previewRequested=false;uint32_t tapAt=0;int tapX=0,tapY=0;
  static bool onMascot(int x,int y){return x>=80&&x<400&&y>=112&&y<336;}
  int stage() const { return appearance>=0?appearance:sample.remaining<0?2:sample.remaining>=80?4:sample.remaining/20; }
  bool select(int mode,uint32_t now) {
    if(mode< -1||mode>4)return false;
    previous=level(now);changedAt=now;appearance=mode;saveError=false;return true;
  }
  static int hit(int x,int y) {
    if(x>=28&&x<452&&y>=328&&y<368)return 7;
    if(x>=28&&x<452&&y>=438&&y<476)return 6;
    for(int i=0;i<6;i++) {
      int left=28+(i%2)*220,top=150+(i/2)*50;
      if(x>=left&&x<left+204&&y>=top&&y<top+46)return i;
    }return -1;
  }
  bool accept(const Sample &s,bool usb,uint32_t now) {
    if(s.remaining < -1||s.remaining>100||s.minutes<0||s.minutes>525600||s.observed<1704067200||s.reset<0||s.sequence<=sample.sequence||s.observed<sample.observed) return false;
    if(!usb&&hasUSB&&uint32_t(now-usbAt)<6000) return false;
    previous=level(now); changedAt=now; sample=s; received=now; source=usb?"USB":"BLE";
    if(usb) { hasUSB=true; usbAt=now; } return true;
  }
  float level(uint32_t now) const { float t=fminf(1,float(uint32_t(now-changedAt))/1000); t=t*t*(3-2*t); return previous+(stage()-previous)*t; }
  bool stale(uint32_t now) const { return !sample.sequence||uint32_t(now-received)>90000; }
  void cancelTouch() { down=false; blocked=true;requested=-2;tapWaiting=false;previewRequested=false; }
  bool touch(bool pressed,int x,int y,uint32_t now) {
    if(!pressed) {
      bool shortTap=down&&!blocked&&!details&&uint32_t(now-pressedAt)<=250&&abs(x-x0)<=18&&abs(y-y0)<=18&&onMascot(x0,y0)&&onMascot(x,y);
      bool tapped=down&&!blocked&&details&&abs(x-x0)<=18&&abs(y-y0)<=18;
      bool released=down;
      down=false;blocked=false;
      if(shortTap){
        if(tapWaiting&&uint32_t(now-tapAt)<=400&&abs(x-tapX)<=48&&abs(y-tapY)<=48){tapWaiting=false;previewRequested=true;return true;}
        tapWaiting=true;tapAt=now;tapX=x;tapY=y;
      }else if(released)tapWaiting=false;
      int index=hit(x,y);
      if(tapped&&index>=0&&index==hit(x0,y0)) {
        if(index==6){details=false;cancelTouch();return true;}
        if(index==7){notifyRequested=true;return true;}
        requested=index==0?-1:5-index;return true;
      }
      return false;
    }
    if(blocked) return false;
    if(!down) { down=true; pressedAt=now; x0=x;y0=y; }
    if(abs(x-x0)>18||abs(y-y0)>18) { cancelTouch(); return false; }
    if(!details&&uint32_t(now-pressedAt)>=650) { details=true; cancelTouch(); return true; } return false;
  }
};
inline void mascot(ui::Canvas &c,const View &v,uint32_t ms) {
  quotaSprite::draw(c,v.stage(),v.level(ms),ms);
}
inline void dateText(char *out,size_t size,int64_t epoch) {
  if(!epoch) { snprintf(out,size,"--");return; } time_t local=epoch+28800; tm d{}; gmtime_r(&local,&d);
  snprintf(out,size,"%02d-%02d %02d:%02d",d.tm_mon+1,d.tm_mday,d.tm_hour,d.tm_min);
}
inline void render(ui::Canvas &c,const View &v,uint32_t ms) {
  c.rect(0,0,480,480,ui::BG); c.text(32,32,"Codex 精力值",font24,ui::INK);
  c.text(32,74,v.sample.minutes==10080?"本周剩余额度":"周期剩余额度",font18,ui::MUTED);
  char s[80];
  if(v.details) {
    c.text(36,123,"形象档位",font24,ui::GOLD);
    for(int i=0;i<6;i++) {
      int mode=i==0?-1:5-i,x=28+(i%2)*220,y=150+(i/2)*50;
      bool selected=v.appearance==mode;
      c.round(x,y,204,46,10,selected?ui::GOLD:ui::PANEL);
      const char *label=mode<0?"自动":phrases[mode];
      c.text(x+(204-ui::width(label,font20))/2,y+14,label,font20,selected?ui::BG:ui::INK);
    }
    c.center(305,v.saveError?"保存失败，请重试":"仅改变形象，额度保持真实",font18,v.saveError?ui::GOLD:ui::MUTED);
    snprintf(s,sizeof(s),"额度 %d%%  /  %s",v.sample.remaining,v.source);
    if(v.sample.remaining<0)snprintf(s,sizeof(s),"额度 --%%  /  %s",v.source);
    c.round(28,328,424,40,10,ui::PANEL);c.text(40,339,"任务完成提醒",font18,ui::INK);
    c.round(358,334,80,28,12,v.notify?ui::GOLD:ui::MUTED);c.text(378,340,v.notify?"开启":"关闭",font18,ui::BG);
    c.text(36,375,s,font18,ui::MUTED);
    char d[40];dateText(d,sizeof(d),v.sample.observed);snprintf(s,sizeof(s),"最后同步  %s",d);c.text(36,395,s,font18,ui::MUTED);
    dateText(d,sizeof(d),v.sample.reset);snprintf(s,sizeof(s),"重置时间  %s",d);c.text(36,415,s,font18,ui::MUTED);
    c.round(28,438,424,38,10,ui::PANEL);c.center(448,"返回",font20,ui::INK);return;
  }
  mascot(c,v,ms);
  const uint16_t colors[]={ui::rgb(0xB9AFD9),ui::rgb(0xF3B283),ui::rgb(0xE5CE84),ui::rgb(0x99D9DD),ui::rgb(0xA9E5C4)};
  auto color=colors[v.stage()];
  if(v.sample.remaining<0) strcpy(s,"--%");else snprintf(s,sizeof(s),"%d%%",v.sample.remaining);
  c.center(339,s,font64,color);
  c.center(412,v.appearance<0&&v.sample.remaining<0?"等待电脑同步":v.appearance<0&&v.sample.remaining==0?"等我回血":phrases[v.stage()],font24,ui::INK);
  c.round(56,448,368,12,6,ui::PANEL);if(v.sample.remaining>0)c.round(56,448,368*v.sample.remaining/100,12,6,color);
  snprintf(s,sizeof(s),"%s%s",v.stale(ms)?"未同步":v.source,v.appearance>=0?" / 手动":"");
  c.text(32,111,s,font18,ui::MUTED);
}
}
