#pragma once
#include <stdint.h>
namespace input {
struct Scroll {
  int offset=0,maximum=0,top=112,bottom=464;
  bool down=false,blocked=false,drag=false,changed=false;
  int x0=0,y0=0,offset0=0;
  void cancel() { down=false; blocked=true; drag=false; }
  void limit(int value) { maximum=value>0?value:0; if(offset>maximum) offset=maximum; }
  // Returns true only for an unmoved release inside the same viewport.
  bool update(bool pressed,int x,int y) {
    changed=false;
    if(!pressed) {
      if(down&&!blocked) {
        int dx=x-x0,dy=y-y0;
        if(dx*dx+dy*dy>12*12) drag=true;
        if(drag) {
          int next=offset0-dy; if(next<0) next=0; if(next>maximum) next=maximum;
          changed=next!=offset; offset=next;
        }
      }
      bool tap=down&&!blocked&&!drag&&y>=top&&y<bottom;
      down=false; blocked=false; drag=false; return tap;
    }
    if(blocked&&!down) return false;
    if(!down) { down=true; x0=x; y0=y; offset0=offset; blocked=y<top||y>=bottom||x<20||x>=460; }
    int dx=x-x0,dy=y-y0;
    if(dx*dx+dy*dy>12*12) drag=true;
    if(!blocked&&drag) {
      int next=offset0-dy; if(next<0) next=0; if(next>maximum) next=maximum;
      changed=next!=offset; offset=next;
    }
    return false;
  }
};
}
