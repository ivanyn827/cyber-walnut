#pragma once
#include <math.h>
#include "ui.h"
#include "eye_masks.h"

namespace emotions {
enum Mood { HAPPY, CALM, SLEEPY, SAD, ANGRY, LOVE, SURPRISED, SMUG, COUNT };
constexpr int FREE=COUNT,GRID_TOP=172,GRID_PITCH=68,CARD_HEIGHT=62;
constexpr int FRAME_TOP=148,FRAME_ROWS=192;
constexpr uint32_t HOLD_MS=800;
static const char *names[]={"开心","平静","困困","委屈","生气","心动","惊讶","得意"};
inline float smooth(float x) { return x*x*(3-2*x); }
struct Motion { float x,y; };
inline Motion motion(uint32_t now) {
  // A new glance every 400 ms: 300 ms eased travel, 100 ms gentle pause.
  static const Motion targets[]={{0,0},{12,-2.7f},{0,2.7f},{-12,0},{4.5f,-4},{12,1.3f},{-6,4},{-3,-1.3f}};
  uint32_t phase=now%3200,index=phase/400;
  float t=(phase%400)/300.f; if(t>1) t=1;
  t=smooth(t);
  const auto &a=targets[index],&b=targets[(index+1)%8];
  return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
}
inline float openness(uint32_t now) {
  uint32_t phase=now%2600;
  if(phase<2200||phase>=2520) return 1;
  float t=float(phase-2200)/320;
  return 1-.95f*smooth(t<.5f?t*2:(1-t)*2);
}
struct View {
  Mood mood=HAPPY;
  bool free=false;
  uint32_t randomState=0x6D2B79F5,lastChange=0,interval=2000;
  void seed(uint32_t value) { randomState=value?value:0x6D2B79F5; }
  uint32_t random() {
    randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5;
    return randomState;
  }
  void nextMood(uint32_t now) {
    static const Mood pool[]={HAPPY,CALM,SLEEPY};
    Mood candidates[4]; int count=0;
    for(auto candidate:pool) if(candidate!=mood) candidates[count++]=candidate;
    mood=candidates[random()%count];
    interval=2000+random()%3001; lastChange=now;
  }
  bool tick(uint32_t now) {
    if(!showing||!free||uint32_t(now-lastChange)<interval) return false;
    nextMood(now); return true;
  }
  bool showing=false,down=false,blocked=false,fired=false;
  int startX=0,startY=0,target=-1;
  uint32_t started=0;
  void cancelTouch() { down=false; blocked=true; fired=false; }
  void enter() { showing=false; cancelTouch(); }
  bool choose(int value,uint32_t now=0) {
    if(value<0||value>FREE) return false;
    free=value==FREE;
    if(free) nextMood(now); else mood=static_cast<Mood>(value);
    showing=true; cancelTouch(); return true;
  }
  static int hit(int x,int y) {
    if(x>=28&&x<452&&y>=110&&y<158) return FREE;
    for(int i=0;i<COUNT;i++) {
      int left=28+(i%2)*220,top=GRID_TOP+(i/2)*GRID_PITCH;
      if(x>=left&&x<left+204&&y>=top&&y<top+CARD_HEIGHT) return i;
    }
    return -1;
  }
  bool touch(bool pressed,int x,int y,uint32_t now) {
    if(!pressed) {
      int dx=x-startX,dy=y-startY;
      bool select=down&&!blocked&&!fired&&!showing&&target>=0&&hit(x,y)==target&&dx*dx+dy*dy<=24*24;
      down=false; blocked=false; fired=false;
      if(select) return choose(target,now);
      return false;
    }
    if(blocked&&!down) return false;
    if(!down) { down=true; startX=x; startY=y; started=now; target=hit(x,y); }
    int dx=x-startX,dy=y-startY;
    if(dx*dx+dy*dy>24*24) blocked=true;
    if(showing&&!blocked&&!fired&&uint32_t(now-started)>=HOLD_MS) {
      showing=false; fired=true; return true;
    }
    return false;
  }
};
inline uint16_t blend(uint16_t fg,uint16_t bg,int a) {
  int r=(((fg>>11)&31)*a+((bg>>11)&31)*(255-a)+127)/255;
  int g=(((fg>>5)&63)*a+((bg>>5)&63)*(255-a)+127)/255;
  int b=((fg&31)*a+(bg&31)*(255-a)+127)/255;
  return (r<<11)|(g<<5)|b;
}
// Bilinear fixed-point sampling preserves subpixel motion on the C6.
inline void eye(ui::Canvas &c,int mask,float cx,float cy,float sx,float sy,bool mirror,uint16_t bg) {
  constexpr int W=eye_masks::W,H=eye_masks::H;
  float left=cx-W*sx/2,top=cy-H*sy/2;
  int x0=int(floorf(left)),x1=int(ceilf(left+W*sx));
  int y0=int(floorf(top)),y1=int(ceilf(top+H*sy));
  if(y0<c.top) y0=c.top; if(y1>c.top+c.rows) y1=c.top+c.rows;
  int stepX=int(256/sx);
  int sourceX=int((x0+.5f-left)*256/sx)-128;
  const uint8_t *pixels=eye_masks::pixels[mask];
  // Supported scale is <=1; prepare horizontal sampling once, not per row.
  if(x1-x0>160) return;
  int indices[160],fractions[160];
  uint16_t row[160];
  for(int x=x0,q=sourceX;x<x1;x++,q+=stepX) {
    int ix=q>>8;
    indices[x-x0]=ix<0||ix>=W-1?-1:mirror?W-1-ix:ix;
    fractions[x-x0]=q&255;
  }
  for(int y=y0;y<y1;y++) {
    int sourceY=int((y+.5f-top)*256/sy)-128;
    int iy=sourceY>>8,fy=sourceY&255;
    if(iy<0||iy>=H-1) continue;
    // Screen-anchored dithering keeps the soft RGB565 gradient smooth.
    uint16_t shades[4];
    int red=245-62*iy/(H-1),green=255-13*iy/(H-1),blue=217+iy/(H-1);
    for(int t=0;t<4;t++) {
      int r=(red+t*2)>>3,g=(green+t)>>2,b=(blue+t*2)>>3;
      shades[t]=((r>31?31:r)<<11)|((g>63?63:g)<<5)|(b>31?31:b);
    }
    for(int x=x0;x<x1;x++) {
      int a=indices[x-x0],fx=fractions[x-x0];
      row[x-x0]=bg;
      if(a<0) continue;
      int b=mirror?a-1:a+1;
      int p00=pixels[iy*W+a],p01=pixels[iy*W+b];
      int p10=pixels[(iy+1)*W+a],p11=pixels[(iy+1)*W+b];
      if(!(p00|p01|p10|p11)) continue;
      int alpha=255;
      if((p00&p01&p10&p11)!=255) {
        int upper=p00*(256-fx)+p01*fx,lower=p10*(256-fx)+p11*fx;
        alpha=(upper*(256-fy)+lower*fy+32768)>>16;
      }
      static const int threshold[2][2]={{0,2},{3,1}};
      uint16_t color=shades[threshold[y&1][x&1]];
      if(alpha) row[x-x0]=alpha==255?color:blend(color,bg,alpha);
    }
    c.scanline(x0,y,row,x1-x0);
  }
}
inline void eyes(ui::Canvas &c,Mood mood,uint32_t now,int centerX=240,int centerY=240,float scale=1,uint16_t bg=ui::BG) {
  float open=openness(now);
  auto pose=motion(now);
  float bob=pose.y*scale,gaze=pose.x*scale;
  for(int side=0;side<2;side++) {
    float cx=centerX+(side?94:-94)*scale+gaze,cy=centerY+bob;
    int mask=mood==SMUG&&!side?HAPPY:mood;
    eye(c,mask,cx,cy,scale,scale*open,side!=0,bg);
  }
}
inline void render(ui::Canvas &c,const View &view,uint32_t now) {
  c.rect(0,0,480,480,ui::BG);
  if(view.showing) { eyes(c,view.mood,now); return; }
  c.text(36,36,"灵动表情",font24,ui::INK);
  c.text(36,78,"选择你的心情",font18,ui::MUTED);
  c.round(28,110,424,48,14,view.free?ui::rgb(0x303B37):ui::PANEL);
  c.text(46,126,"自由心情",font20,ui::INK);
  c.text(254,127,"随机 2-5 秒切换",font18,ui::MUTED);
  for(int i=0;i<COUNT;i++) {
    int x=28+(i%2)*220,y=GRID_TOP+(i/2)*GRID_PITCH;
    uint16_t bg=!view.free&&view.mood==i?ui::rgb(0x303B37):ui::PANEL;
    c.round(x,y,204,CARD_HEIGHT,14,bg);
    eyes(c,static_cast<Mood>(i),0,x+102,y+22,.24f,bg);
    c.text(x+(204-ui::width(names[i],font18))/2,y+40,names[i],font18,ui::INK);
  }
  c.center(456,"长按表情返回",font18,ui::MUTED);
}
}
