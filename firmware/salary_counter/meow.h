#pragma once
#include <math.h>
#include "ui.h"
#include "meow_art.h"
#ifdef ARDUINO
#include "esp32c6/rom/tjpgd.h"
#endif
namespace meow {
constexpr int TOP=128,ROWS=320,FRAME_MS=140;
struct View {
  uint32_t elapsed=0,last=0;bool initialized=false,wasRunning=false;
  int variant=1,startX=0,startY=0,endX=0,endY=0;
  bool touching=false;
  void cancelTouch(){touching=false;}
  bool touch(bool pressed,int x,int y) {
    if(pressed) {
      if(!touching){touching=true;startX=x;startY=y;}
      endX=x;endY=y;return false;
    }
    if(!touching)return false;
    touching=false;
    int dx=endX-startX,dy=endY-startY;
    if(abs(dx)<60||abs(dx)<2*abs(dy))return false;
    variant=(variant+(dx<0?1:meowArt::VARIANTS-1))%meowArt::VARIANTS;
    elapsed=0;wasRunning=false;return true;
  }
  void tick(uint32_t now,bool running) {
    if(initialized&&running&&wasRunning)elapsed+=uint32_t(now-last);
    last=now;initialized=true;wasRunning=running;
  }
};
inline int pose(uint32_t ms) {
  return (ms/FRAME_MS)%meowArt::COUNT;
}
static uint16_t cache[meowArt::W*meowArt::H];
static int cached=-1,decodeError=0;
#ifdef ARDUINO
struct Stream {unsigned position,end;};
inline UINT input(JDEC *decoder,BYTE *dest,UINT count) {
  auto &s=*static_cast<Stream*>(decoder->device);
  if(count>s.end-s.position)count=s.end-s.position;
  if(dest)memcpy(dest,meowArt::data+s.position,count);
  s.position+=count;return count;
}
inline UINT output(JDEC *,void *bitmap,JRECT *rect) {
  if(rect->right>=meowArt::W||rect->bottom>=meowArt::H)return 0;
  const uint8_t *p=static_cast<uint8_t*>(bitmap);
  for(int y=rect->top;y<=rect->bottom;y++)for(int x=rect->left;x<=rect->right;x++) {
    unsigned r=*p++,g=*p++,b=*p++;
    cache[y*meowArt::W+x]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
  }
  return 1;
}
#endif
inline bool prepare(int frame) {
  if(cached==frame)return true;
#ifdef ARDUINO
  alignas(4) static uint8_t workspace[4096];JDEC decoder{};
  Stream stream{meowArt::offsets[frame],meowArt::offsets[frame+1]};
  decodeError=jd_prepare(&decoder,input,workspace,sizeof(workspace),&stream);
  if(!decodeError&&(decoder.width!=meowArt::W||decoder.height!=meowArt::H))decodeError=JDR_FMT1;
  if(!decodeError)decodeError=jd_decomp(&decoder,output,0);
  if(decodeError){cached=-1;return false;}
#else
  memcpy(cache,meowArt::hostPixels+frame*meowArt::W*meowArt::H,sizeof(cache));
#endif
  // Match the generated dark matte to the shell background.
  for(auto &p:cache)if((p>>11)<6&&((p>>5)&63)<12&&(p&31)<6)p=ui::BG;
  cached=frame;return true;
}
inline uint16_t mix(uint16_t a,uint16_t b,int t) {
  unsigned r=(((a>>11)*(32-t)+(b>>11)*t)+16)>>5;
  unsigned g=(( ((a>>5)&63)*(32-t)+((b>>5)&63)*t)+16)>>5;
  unsigned blue=(((a&31)*(32-t)+(b&31)*t)+16)>>5;
  return (r<<11)|(g<<5)|blue;
}
inline void render(ui::Canvas &c,uint32_t ms,int variant=1) {
  c.rect(0,0,480,480,ui::BG);
  const int frame=meowArt::frames[pose(ms)+(variant>=0&&variant<meowArt::VARIANTS?variant:1)*meowArt::COUNT];
  if(!prepare(frame))return;
  int first=c.top>TOP?c.top:TOP,last=c.top+c.rows<TOP+ROWS?c.top+c.rows:TOP+ROWS;
  uint16_t row[320],upper[320],lower[320];int previous=-1;
  auto expand=[](int y,uint16_t *out) {
    const auto *in=cache+y*meowArt::W;
    for(int x=0;x<meowArt::W;x++) {
      out[x*2]=in[x];
      out[x*2+1]=mix(in[x],in[x+1<meowArt::W?x+1:x],16);
    }
  };
  for(int y=first;y<last;y++) {
    int sy=(y-TOP)/2;
    if(sy!=previous) {
      if(sy==previous+1&&previous>=0)memcpy(upper,lower,sizeof(upper));
      else expand(sy,upper);
      expand(sy+1<meowArt::H?sy+1:sy,lower);previous=sy;
    }
    if((y-TOP)&1) {for(int x=0;x<320;x++)row[x]=mix(upper[x],lower[x],16);c.scanline(80,y,row,320);}
    else c.scanline(80,y,upper,320);
  }
}
}
