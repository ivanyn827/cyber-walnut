#pragma once
#include "quota_art.h"
namespace quotaSprite {
inline uint16_t mix(uint16_t a,uint16_t b,int t) {
  // Five-bit weights keep blue products below the red lane (bit 11).
  // Eight-bit weights would overflow the packed lane and tint pixels blue/yellow.
  int f=(t+4)>>3;
  uint32_t rb=(((a&0xF81Fu)*(32-f)+(b&0xF81Fu)*f)>>5)&0xF81F;
  uint32_t g=(((a&0x07E0u)*(32-f)+(b&0x07E0u)*f)>>5)&0x07E0;
  return rb|g;
}
inline uint16_t sample(int stage,int x,int y) {
  int xx=x>>8,yy=y>>8;
  if(xx<0||xx>=quotaArt::W-1||yy<0||yy>=quotaArt::H-1)return ui::BG;
  const uint8_t *p=quotaArt::images[stage]+yy*quotaArt::W+xx;
  auto *palette=quotaArt::palettes[stage];
  // Horizontal subpixel filtering; vertical scanline warp avoids four Flash
  // palette lookups per pixel on the no-PSRAM C6 and keeps the original artwork.
  if(p[0]==p[1])return palette[p[0]];
  return mix(palette[p[0]],palette[p[1]],x&255);
}
inline void draw(ui::Canvas &c,int stage,float energy,uint32_t ms) {
  float t=ms*.001f;
  float breath=sinf(t*(1.7f+stage*.35f));
  float sway=sinf(t*.9f)*(4.f+stage*3.f);
  float bob=(stage>=3?fabsf(sinf(t*2.2f))*8:breath*(3.f+stage));
  float sy=1.08f+breath*(stage==0?.035f:.045f),sx=1.08f-breath*.028f;
  // Restore energy with a brief spring, without substituting a different drawing.
  sy*=1+fminf(.08f,fabsf(energy-stage)*.022f)*sinf(t*5);
  sy=fminf(sy,1.13f); // Keep the tallest pose below the reserved header during revival.
  int step=int(256/sx),arm=int(sinf(t*(stage==4?4.8f:2.3f))*(3.f+stage*1.5f)*256);
  uint16_t row[352];
  for(int y=std::max(c.top,112);y<std::min(c.top+c.rows,336);y++) {
    int baseY=int(((y-332+bob)/sy+218)*256);
    int baseX=int(((64-240-sway)/sx+160)*256);
    for(int i=0;i<352;i++) {
      int x=baseX+i*step;
      int edge=abs((x>>8)-160);
      int weight=edge<=72?0:edge>=120?256:(edge-72)*256/48;
      // Softly wave the actual pictured arms, preserving their 3D shading.
      int yy=baseY-arm*weight/256;
      row[i]=sample(stage,x,yy);
    }
    c.scanline(64,y,row,352);
  }
}
}
