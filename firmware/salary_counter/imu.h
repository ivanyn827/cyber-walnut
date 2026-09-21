#pragma once
#include "peripherals.h"
#include "orientation.h"

namespace imu {
// Registers/ranges from Waveshare's 02_I2C_QMI8658 example. Accel only, +/-2g, 62.5 Hz.
static uint8_t address=0;
static int x=0,y=0,z=0;
static uint32_t sampledAt=0,samples=0,errors=0,lastTry=0;
static bool ready=false,fresh=false;
static int stableAxis=0;
static uint32_t stableSince=0;
inline bool write(uint8_t reg,uint8_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value); return Wire.endTransmission()==0;
}
inline bool begin() {
  ready=false; fresh=false;
  for(uint8_t addr:{uint8_t(0x6A),uint8_t(0x6B)}) {
    uint8_t who=0;
    if(!peripheral::read(addr,0,&who,1)||who!=0x05) continue;
    address=addr;
    ready=write(0x08,0)&&write(0x02,0x60)&&write(0x03,0x07)&&write(0x08,0x01);
    return ready;
  }
  return false;
}
inline bool poll(uint32_t now) {
  if(!ready) { if(uint32_t(now-lastTry)>=5000) { lastTry=now; begin(); } return false; }
  uint8_t status=0,data[6];
  if(!peripheral::read(address,0x2E,&status,1)) { ++errors; fresh=false; ready=false; return false; }
  if(!(status&1)) return false;
  if(!peripheral::read(address,0x35,data,6)) { ++errors; fresh=false; ready=false; return false; }
  x=int16_t((uint16_t(data[1])<<8)|data[0])*1000/16384;
  y=int16_t((uint16_t(data[3])<<8)|data[2])*1000/16384;
  z=int16_t((uint16_t(data[5])<<8)|data[4])*1000/16384;
  int axis=orientation::dominantAxis(x,y,z);
  if(axis!=stableAxis||uint32_t(now-sampledAt)>200) { stableAxis=axis; stableSince=now; }
  sampledAt=now; ++samples; fresh=true; return true;
}
}
