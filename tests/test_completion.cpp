#include <cassert>
#include <vector>
#include "../firmware/salary_counter/completion.h"
int main(){
  completion::Effect e;e.accept(2);assert(e.pending==2);
  e.accept(2);e.accept(1);assert(e.pending==2);
  assert(!e.tick(1,false)&&e.pending==2);
  assert(e.tick(10,true)&&e.active&&e.pending==1);
  assert(e.takeSound());assert(!e.takeSound());
  assert(!e.tick(4009,true));assert(e.tick(4010,true)&&e.active&&e.pending==0);
  assert(e.takeSound());assert(!e.takeSound());
  e.tick(4100,false);assert(!e.active&&e.pending==1);
  e.tick(5000,true);assert(e.active);
  e.enable(false);assert(!e.active&&e.pending==0);e.accept(4);assert(e.pending==0&&e.total==4);
  e.enable(true);e.accept(5);e.tick(6000,true);
  std::vector<uint16_t> full(480*480,123),tile(480*32,123);
  ui::Canvas c{full.data()};e.render(c,6500);
  for(int y=0;y<104;y++)for(int x=280;x<480;x++)assert(full[y*480+x]!=123);
  for(int y=0;y<480;y+=32){std::fill(tile.begin(),tile.end(),123);ui::Canvas part{tile.data(),y,32};e.render(part,6500);for(int i=0;i<480*32;i++)assert(tile[i]==full[y*480+i]);}
  std::vector<uint16_t> bright(480*480,123);ui::Canvas brightCanvas{bright.data()};e.render(brightCanvas,6000);
  int changed=0;for(int i=0;i<480*480;i++)if(bright[i]!=full[i])++changed;
  assert(changed>80000); // Large-area pulse, not just a thin animated border.
  assert(e.tick(10000,true)&&!e.active);
  e.preview(11000);assert(e.takeSound());e.preview(12000);e.tick(12001,false);assert(!e.takeSound());
  e.preview(13000);e.enable(false);assert(!e.takeSound());
  completion::Effect ttl;
  ttl.accept(1,1000,60000,true);ttl.tick(60999,false);assert(ttl.pending==1);
  ttl.tick(61000,false);assert(ttl.pending==0);assert(!ttl.tick(61001,true));
  ttl.accept(10,62000,0,true);assert(ttl.total==10&&ttl.pending==0);
  ttl.accept(11,62000,2000,true);ttl.accept(11,63000,60000,true);
  ttl.tick(62000,true);assert(ttl.active);ttl.tick(64000,true);assert(!ttl.active&&!ttl.takeSound());
  ttl.accept(12,65000,1000,true);ttl.accept(13,65000,60000,true);
  ttl.tick(67000,true);assert(ttl.active&&ttl.pending==0);
  ttl.tick(68000,false);assert(ttl.pending==1);ttl.tick(125000,false);assert(ttl.pending==0);
  completion::Effect wrap;wrap.accept(1,0xfffffff0u,100,true);
  wrap.tick(0x40u,false);assert(wrap.pending==1);wrap.tick(0x54u,true);assert(wrap.pending==0&&!wrap.active);
}
