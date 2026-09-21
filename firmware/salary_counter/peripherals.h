#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "ui.h"

namespace peripheral {
// Official schematic: custom KEY=GPIO10; BOOT=GPIO9. PWR is separate.
constexpr int HOME_PIN=10;
static volatile bool homePending=false,homeArmed=true;
static void IRAM_ATTR homeEdge() {
  if(homeArmed) { homeArmed=false; homePending=true; }
}
inline bool homePressed(uint32_t now) {
  static uint32_t releasedAt=0;
  if(homePending) { homePending=false; releasedAt=now; return true; }
  if(digitalRead(HOME_PIN)==LOW) releasedAt=now;
  else if(uint32_t(now-releasedAt)>=80) homeArmed=true;
  return false;
}
inline bool read(uint8_t address,uint8_t reg,uint8_t *data,size_t n) {
  Wire.beginTransmission(address); Wire.write(reg);
  if(Wire.endTransmission(false)) return false;
  if(Wire.requestFrom(address,n)!=n) return false;
  for(size_t i=0;i<n;i++) data[i]=Wire.read();
  return true;
}
inline ui::Battery battery() {
  ui::Battery b; uint8_t state[2],percent;
  if(!read(0x34,0,state,2)) return b;
  b.valid=true; b.present=state[0]&8;
  b.usb=(state[0]&32)&&!(state[1]&8);
  b.charging=b.present&&((state[1]>>5)==1);
  if(b.present&&read(0x34,0xA4,&percent,1)&&percent<=100) b.percent=percent;
  return b;
}
inline void begin() {
  pinMode(9,INPUT_PULLUP); pinMode(5,INPUT_PULLUP);
  pinMode(HOME_PIN,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HOME_PIN),homeEdge,FALLING);
  pinMode(11,OUTPUT); digitalWrite(11,LOW); delay(10); digitalWrite(11,HIGH); delay(50);
}
// CST9220 uses the vendor CST9217-compatible D000 touch report.
inline bool touch(bool &pressed,int &x,int &y) {
  Wire.beginTransmission(0x5A); Wire.write(0xD0); Wire.write(0x00);
  if(Wire.endTransmission()) return false;
  delay(2);
  if(Wire.requestFrom(uint8_t(0x5A),size_t(10))!=10) return false;
  uint8_t d[10]; for(int i=0;i<10;i++) d[i]=Wire.read();
  if(d[6]!=0xAB) return false;
  pressed=(d[5]&0x7F)>0&&(d[0]&15)==6;
  if(pressed) {
    int rawX=(int(d[1])<<4)|(d[3]>>4),rawY=(int(d[2])<<4)|(d[3]&15);
    if(rawX>=480||rawY>=480) return false;
    x=479-rawY; y=rawX;
  }
  return true;
}
}
