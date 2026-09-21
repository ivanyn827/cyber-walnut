#pragma once
#include "quota.h"
#include "completion.h"
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <mbedtls/md.h>
#include <esp_random.h>
namespace quotaLink {
static quota::View view;
static Preferences prefs;
static completion::Effect effect;
static int64_t (*wallClock)()=nullptr;
static uint64_t notifySequence=0,ackSequence=0;
static uint8_t key[32]; static bool paired=false;
static char hello[80],nonce[17],device[17];
static const char *SERVICE="d593f000-7b1e-4c58-9da0-0a12e4c6d216";
static const char *INFO="d593f001-7b1e-4c58-9da0-0a12e4c6d216";
static const char *DATA="d593f002-7b1e-4c58-9da0-0a12e4c6d216";
static const char *ACK="d593f003-7b1e-4c58-9da0-0a12e4c6d216";
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static char pending[256]; static bool queued=false;
static BLECharacteristic *ack=nullptr;
inline int nibble(char c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1; }
inline bool unhex(const char *s,uint8_t *out,int n) {
  if(strlen(s)!=size_t(n*2))return false;
  for(int i=0;i<n;i++) { int a=nibble(s[i*2]),b=nibble(s[i*2+1]);if(a<0||b<0)return false;out[i]=(a<<4)|b; }return true;
}
inline void begin() {
  prefs.begin("desk-quota",false); paired=prefs.getBytesLength("key")==32&&prefs.getBytes("key",key,32)==32;
  int mode=prefs.getChar("style",-1);view.select(mode>=-1&&mode<=4?mode:-1,millis());view.previous=view.stage();
  view.notify=prefs.getBool("notify",true);effect.enabled=view.notify;effect.total=prefs.getULong64("done",0);
  snprintf(device,sizeof(device),"%012llx",(unsigned long long)ESP.getEfuseMac());
  snprintf(nonce,sizeof(nonce),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
  snprintf(hello,sizeof(hello),"QHELLO %s %s",device,nonce);
}
inline bool choose(int mode) {
  view.requested=-2;
  if(mode< -1||mode>4)return false;
  if(mode!=view.appearance&&prefs.putChar("style",(int8_t)mode)!=1){view.saveError=true;return false;}
  return view.select(mode,millis());
}
inline bool toggleNotify() {
  view.notifyRequested=false;
  if(prefs.putBool("notify",!view.notify)!=1){view.saveError=true;return false;}
  view.notify=!view.notify;effect.enable(view.notify);view.saveError=false;return true;
}
inline bool pair(const char *hex) {
  uint8_t candidate[32];if(!unhex(hex,candidate,32))return false;
  if(paired) return memcmp(candidate,key,32)==0;
  if(prefs.putBytes("key",candidate,32)!=32)return false;
  memcpy(key,candidate,32);paired=true;return true;
}
inline bool receive(char *packet,bool usb) {
  if(!paired)return false;
  char *signature=strrchr(packet,' ');if(!signature)return false;
  uint8_t expected[32],actual[32];if(!unhex(signature+1,expected,32))return false;
  *signature=0;
  if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),key,32,(const uint8_t*)packet,strlen(packet),actual)!=0)return false;
  unsigned diff=0;for(int i=0;i<32;i++)diff|=actual[i]^expected[i];if(diff)return false;
  char challenge[17],tail; unsigned long long seq;long long observed,reset;int remaining,minutes;
  if(strncmp(packet,"N2 ",3)==0||strncmp(packet,"N1 ",3)==0) {
    unsigned long long count;
    long long expires=0,issued=0;
    bool modern=strncmp(packet,"N2 ",3)==0;
    int fields=modern?sscanf(packet,"N2 %16s %llu %llu %lld %lld %c",challenge,&seq,&count,&expires,&issued,&tail):sscanf(packet,"N1 %16s %llu %llu %c",challenge,&seq,&count,&tail);
    if(fields!=(modern?5:3)||strcmp(challenge,nonce)||seq<=notifySequence||(!modern&&count<effect.total))return false;
    if(modern&&(issued<1704067200||expires<0||expires>issued+60))return false;
    // Signed absolute expiry prevents reconnect/retry from renewing a notice.
    long long clockNow=wallClock?wallClock():0;
    long long remaining=expires-std::max(issued,clockNow);
    uint32_t ttl=modern&&remaining>0?uint32_t(std::min<long long>(60,remaining))*1000:0;
    // A restarted companion may retry an already-consumed event. ACK it without
    // lowering the durable watermark or replaying its effect.
    if(count>effect.total&&prefs.putULong64("done",count)!=8)return false;
    effect.accept(count,millis(),ttl,true);notifySequence=seq;ackSequence=seq;return true;
  }
  if(sscanf(packet,"Q1 %16s %llu %lld %d %d %lld %c",challenge,&seq,&observed,&remaining,&minutes,&reset,&tail)!=6||strcmp(challenge,nonce))return false;
  bool ok=view.accept({seq,observed,reset,remaining,minutes},usb,millis());if(ok)ackSequence=seq;return ok;
}
class Receiver:public BLECharacteristicCallbacks {
  char buffer[256]{};size_t used=0;bool overflow=false;
  void onWrite(BLECharacteristic *c) override {
    auto value=c->getValue();
    for(size_t i=0;i<value.length();i++) {
      char ch=value[i];
      if(ch=='\n') {
        buffer[used]=0;
        if(!overflow&&used) { portENTER_CRITICAL(&mux);if(!queued) { memcpy(pending,buffer,used+1);queued=true; }portEXIT_CRITICAL(&mux); }
        used=0;overflow=false;
      } else if(used<sizeof(buffer)-1)buffer[used++]=ch;else overflow=true;
    }
  }
};
inline void service(BLEServer *server) {
  auto *s=server->createService(SERVICE);
  s->createCharacteristic(INFO,BLECharacteristic::PROPERTY_READ)->setValue(hello);
  auto *data=s->createCharacteristic(DATA,BLECharacteristic::PROPERTY_WRITE);data->setCallbacks(new Receiver());
  ack=s->createCharacteristic(ACK,BLECharacteristic::PROPERTY_READ);ack->setValue("WAIT");s->start();
}
inline bool poll() {
  char packet[256];bool ready;
  portENTER_CRITICAL(&mux);ready=queued;if(ready){strcpy(packet,pending);queued=false;}portEXIT_CRITICAL(&mux);
  if(!ready)return false;
  bool ok=receive(packet,false);char reply[60];snprintf(reply,sizeof(reply),"%s %llu",ok?"OK":"REJECT",(unsigned long long)ackSequence);
  if(ack)ack->setValue(reply);return ok;
}
}
