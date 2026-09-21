#pragma once
#include "ui.h"
#include <math.h>
namespace completion {
struct Effect {
  bool enabled=true,active=false,previewOnly=false,soundRequested=false;
  uint64_t total=0,pending=0;
  uint32_t started=0;
  uint32_t deadlines[16]{},activeDeadline=0;
  void enqueue(uint32_t deadline) {
    if(pending==16){for(unsigned i=1;i<16;i++)deadlines[i-1]=deadlines[i];--pending;}
    deadlines[pending++]=deadline;
  }
  void accept(uint64_t value,uint32_t now=0,uint32_t ttl=60000,bool single=false) {
    if(value<=total)return;
    if(enabled&&ttl)for(uint64_t i=0,n=single?1:std::min<uint64_t>(16,value-total);i<n;i++)enqueue(now+std::min<uint32_t>(ttl,60000));
    total=value;
  }
  void enable(bool value) {enabled=value;if(!value){pending=0;active=false;soundRequested=false;}}
  void preview(uint32_t now){if(!active)previewOnly=true;active=true;started=now;soundRequested=true;}
  bool takeSound(){bool value=soundRequested;soundRequested=false;return value;}
  bool tick(uint32_t now,bool screen) {
    bool changed=false;
    unsigned kept=0;
    for(unsigned i=0;i<pending;i++)if(int32_t(deadlines[i]-now)>0)deadlines[kept++]=deadlines[i];
    pending=kept;
    if(active&&!previewOnly&&int32_t(activeDeadline-now)<=0){active=false;soundRequested=false;changed=true;}
    if(!screen) {soundRequested=false;if(active){active=false;if(!previewOnly)enqueue(activeDeadline);previewOnly=false;}return changed;}
    if(active&&uint32_t(now-started)>=4000){active=false;previewOnly=false;changed=true;}
    if(!active&&enabled&&pending){activeDeadline=deadlines[0];--pending;for(unsigned i=0;i<pending;i++)deadlines[i]=deadlines[i+1];active=true;previewOnly=false;started=now;changed=true;soundRequested=true;}
    return changed;
  }
  void render(ui::Canvas &c,uint32_t now) const {
    if(!active)return;
    // User-authorized system-level takeover, including the battery region.
    bool protection=c.protectSystem;c.protectSystem=false;
    float t=float(uint32_t(now-started))/1000;
    // Four slow, large-area pulses; start bright so even a quick glance catches it.
    float glow=.5f+.5f*cosf(t*6.283185f);
    uint16_t color=ui::rgb((uint32_t(30+225*glow)<<16)|(uint32_t(32+203*glow)<<8)|uint32_t(36+134*glow));
    c.rect(0,0,480,480,color);
    c.round(40,170,400,140,24,ui::BG);
    const char *label="任务完成";int x=(480-ui::width(label,font24)*3)/2;
    while(*label){
      auto g=ui::find(font24,ui::next(label));if(!g)continue;
      for(int j=0;j<g->height;j++)for(int i=0;i<g->width;i++)
        if(font24.pixels[g->offset+j*g->width+i]>=96)c.rect(x+3*(g->dx+i),213+3*(g->dy+j),3,3,ui::GOLD);
      x+=3*g->advance;
    }
    c.protectSystem=protection;
  }
};
}
