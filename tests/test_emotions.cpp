#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/app_host.h"
int main() {
  for(uint32_t now=0;now<6400;now++) {
    auto p=emotions::motion(now),next=emotions::motion(now+1);
    assert(fabsf(p.x)<=12&&fabsf(p.y)<=4);
    assert(fabsf(next.x-p.x)<.1f&&fabsf(next.y-p.y)<.04f);
  }
  assert(emotions::openness(2360)<.06f);
  assert(emotions::openness(2200)==1&&emotions::openness(2520)==1);
  emotions::View v;
  for(int i=0;i<emotions::COUNT;i++) {
    v.enter(); v.touch(false,0,0,0);
    int x=80+(i%2)*220,y=emotions::GRID_TOP+30+(i/2)*emotions::GRID_PITCH;
    assert(!v.touch(true,x,y,100));
    assert(v.touch(false,x,y,110)&&v.showing&&v.mood==i);
    v.touch(false,x,y,120);
    assert(!v.touch(true,150,250,200));
    assert(!v.touch(true,150,250,999));
    assert(v.touch(true,150,250,1000)&&!v.showing);
    assert(!v.touch(false,150,250,1100)&&!v.showing);
  }
  v.choose(emotions::CALM); v.touch(false,0,0,0);
  v.touch(true,150,250,200); v.touch(true,200,250,300);
  assert(!v.touch(true,150,250,1200)&&v.showing);
  v.cancelTouch(); assert(!v.touch(true,150,250,2200));
  v.touch(false,150,250,2300);
  v.touch(true,150,250,0xffffff00);
  assert(v.touch(true,150,250,0x300)&&!v.showing);
  assert(!v.choose(-1)&&!v.choose(emotions::FREE+1));
  v.enter(); v.touch(false,0,0,0);
  v.touch(true,80,134,100);
  assert(v.touch(false,80,134,200)&&v.free&&v.showing);
  unsigned seen=0; uint32_t now=0xffffff00;
  v.seed(123); v.choose(emotions::FREE,now);
  for(int i=0;i<1000;i++) {
    assert(v.mood==emotions::HAPPY||v.mood==emotions::CALM||v.mood==emotions::SLEEPY);
    seen|=1u<<v.mood;
    assert(v.interval>=2000&&v.interval<=5000);
    auto old=v.mood; uint32_t delay=v.interval;
    assert(!v.tick(now+delay-1)&&v.mood==old);
    now+=delay; assert(v.tick(now)&&v.mood!=old);
  }
  assert(seen==((1u<<emotions::HAPPY)|(1u<<emotions::CALM)|(1u<<emotions::SLEEPY)));
  v.enter(); assert(!v.tick(now+6000));
  v.choose(emotions::SAD,now); assert(!v.free&&!v.tick(now+6000)&&v.mood==emotions::SAD);
  assert(emotions::View::hit(400,80)==-1);
  std::vector<uint16_t> full(480*480),tile(480*80),reference(480*480);
  salary::Date d{2026,9,9,12,0,0}; ui::Battery b; b.valid=b.present=true; b.percent=88;
  ui::MoneyAnimation a; ui::Canvas ref{reference.data()};
  apps::render(ref,apps::HOME,{d,true,6500000,b,a,1000});
  for(int mood=-1;mood<=emotions::FREE;mood++) for(uint32_t now:{0u,300u,400u,1200u,1234u,1500u,1900u,2200u,2240u,2300u,2360u,2450u,2510u,2800u,3199u,5600u}) {
    v.enter(); if(mood>=0) v.choose(mood);
    apps::Context ctx{d,true,6500000,b,a,1000,0,nullptr,nullptr,&v,now};
    ui::Canvas c{full.data()}; apps::render(c,apps::EMOTIONS,ctx);
    for(int y=0;y<480;y+=32) {
      ui::Canvas part{tile.data(),y,32}; apps::render(part,apps::EMOTIONS,ctx);
      for(int i=0;i<480*32;i++) assert(tile[i]==full[y*480+i]);
    }
    for(int y=0;y<104;y++) for(int x=280;x<480;x++) assert(full[y*480+x]==reference[y*480+x]);
    for(int y=emotions::FRAME_TOP;y<emotions::FRAME_TOP+emotions::FRAME_ROWS;y+=80) {
      int remaining=emotions::FRAME_TOP+emotions::FRAME_ROWS-y;
      int rows=remaining<80?remaining:80;
      ui::Canvas part{tile.data(),y,rows}; apps::render(part,apps::EMOTIONS,ctx);
      for(int i=0;i<480*rows;i++) assert(tile[i]==full[y*480+i]);
    }
    if(mood>=0) for(int y=104;y<480;y++) if(y<emotions::FRAME_TOP||y>=emotions::FRAME_TOP+emotions::FRAME_ROWS)
      for(int x=0;x<480;x++) assert(full[y*480+x]==ui::BG);
  }
  puts("PASS: 8 mood taps, hold/release suppression, drag/cancel, timer wrap, all moods/blink tiles and reserved battery region");
}
