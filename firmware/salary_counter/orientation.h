#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

namespace orientation {
constexpr int SIDE=480,MAX_ROWS=80,SCRATCH_BYTES=SIDE*MAX_ROWS/8;
struct Point { int x,y; };
struct Rect { int x,y,w,h; };
inline Point physical(Point p,int q) {
  switch(q&3) {
    case 1:return {SIDE-1-p.y,p.x};
    case 2:return {SIDE-1-p.x,SIDE-1-p.y};
    case 3:return {p.y,SIDE-1-p.x};
    default:return p;
  }
}
inline Point logical(Point p,int q) { return physical(p,(4-q)&3); }
inline Rect blockRect(int top,int rows,int q) {
  switch(q&3) {
    case 1:return {SIDE-top-rows,0,rows,SIDE};
    case 2:return {0,SIDE-top-rows,SIDE,rows};
    case 3:return {top,0,rows,SIDE};
    default:return {0,top,SIDE,rows};
  }
}
inline int destination(int i,int rows,int q) {
  if(q==1) return (i%SIDE)*rows+rows-1-i/SIDE;
  if(q==2) return SIDE*rows-1-i;
  if(q==3) return (SIDE-1-i%SIDE)*rows+i/SIDE;
  return i;
}
inline void rotateInto(const uint16_t *source,uint16_t *out,int rows,int q) {
  if(q==1) {
    for(int x=0;x<SIDE;x++) for(int y=rows-1;y>=0;y--) *out++=source[y*SIDE+x];
  } else if(q==3) {
    for(int x=SIDE-1;x>=0;x--) for(int y=0;y<rows;y++) *out++=source[y*SIDE+x];
  } else {
    for(int i=0;i<SIDE*rows;i++) out[destination(i,rows,q)]=source[i];
  }
}
// Permute the existing DMA tile in place: only 4.8 KB scratch, no second framebuffer.
inline void rotateBlock(uint16_t *pixels,int rows,int q,uint8_t *visited) {
  int count=SIDE*rows; q&=3;
  if(!q) return;
  if(q==2) { for(int i=0;i<count/2;i++) { auto p=pixels[i]; pixels[i]=pixels[count-1-i]; pixels[count-1-i]=p; } return; }
  memset(visited,0,(count+7)/8);
  for(int i=0;i<count;i++) if(!(visited[i/8]&(1<<(i%8)))) {
    int current=i; uint16_t value=pixels[i];
    do {
      int next=destination(current,rows,q); auto saved=pixels[next]; pixels[next]=value; value=saved;
      visited[current/8]|=1<<(current%8); current=next;
    } while(current!=i);
  }
}
struct Axes {
  int8_t right=0,down=0;
  bool valid() const { return abs(right)>=1&&abs(right)<=2&&abs(down)>=1&&abs(down)<=2&&abs(right)!=abs(down); }
  static int component(int axis,int x,int y) { return (axis<0?-1:1)*(abs(axis)==1?x:y); }
};
inline int dominantAxis(int x,int y,int z) {
  int a=abs(x),b=abs(y),large=a>b?a:b,small=a>b?b:a;
  int64_t norm=int64_t(x)*x+int64_t(y)*y+int64_t(z)*z;
  if(norm<650LL*650||norm>1350LL*1350||large<550||large-small<250) return 0;
  return a>b?(x>0?1:-1):(y>0?2:-2);
}
struct Detector {
  int current=0,candidate=-1;
  uint32_t since=0,last=0;
  void reset() { candidate=-1; }
  bool sample(int x,int y,int z,uint32_t now) {
    if(uint32_t(now-last)>200) reset(); last=now;
    int axis=dominantAxis(x,y,z);
    int next=axis==2?0:axis==-1?1:axis==-2?2:axis==1?3:-1;
    if(next<0||next==current) { reset(); return false; }
    if(candidate!=next) { candidate=next; since=now; return false; }
    if(uint32_t(now-since)<450) return false;
    current=next; reset(); return true;
  }
};
}
