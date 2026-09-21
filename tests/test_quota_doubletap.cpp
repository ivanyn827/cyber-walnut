#include <cassert>
#include "../firmware/salary_counter/quota.h"
#include "../firmware/salary_counter/completion.h"
bool tap(quota::View &v,int x,int y,uint32_t t){v.touch(true,x,y,t);return v.touch(false,x,y,t+60);}
int main(){
  quota::View v;
  assert(!tap(v,240,220,0));v.touch(false,240,220,100);
  assert(tap(v,245,220,200)&&v.previewRequested&&!v.details);
  v.previewRequested=false;assert(!tap(v,240,220,400));
  v.cancelTouch();v.touch(false,240,220,500);assert(!tap(v,240,220,600));
  assert(!tap(v,240,220,1200));assert(!tap(v,320,220,1300));
  v.cancelTouch();v.touch(false,0,0,1400);
  assert(!tap(v,240,370,1500));assert(!tap(v,240,370,1700));
  v.touch(true,240,220,1800);v.touch(true,270,220,1850);v.touch(false,240,220,1900);assert(!tap(v,240,220,2000));
  v.touch(true,240,220,2200);assert(v.touch(true,240,220,2851)&&v.details&&!v.previewRequested);
  v.touch(false,240,220,2900);tap(v,240,220,3000);tap(v,240,220,3200);assert(!v.previewRequested);
  completion::Effect e;e.enable(false);e.preview(0);assert(e.active&&!e.enabled&&e.total==0&&e.pending==0);
  e.tick(500,false);assert(!e.active&&e.pending==0);
  e.enable(true);e.accept(2);e.tick(1000,true);e.preview(1100);assert(e.pending==1&&e.total==2);
  e.tick(5100,true);assert(e.active&&e.pending==0);
}
