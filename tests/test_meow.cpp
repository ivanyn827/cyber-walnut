#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../firmware/salary_counter/app_host.h"
int main(int argc,char **argv) {
  meow::View v;v.tick(100,true);v.tick(200,true);assert(v.elapsed==100);
  v.tick(300,false);v.tick(9000,false);v.tick(10000,true);assert(v.elapsed==100);
  v.tick(10100,true);assert(v.elapsed==200);
  v.last=0xfffffff0;v.tick(16,true);assert(v.elapsed==232);
  for(unsigned i=0;i<16;i++)assert(meow::pose(i*meow::FRAME_MS)==int(i));
  assert(meow::pose(16*meow::FRAME_MS)==0);
  assert(apps::menuEntry(apps::COUNT-1).id==apps::SETTINGS);
  assert(apps::menuEntry(apps::COUNT-2).id==apps::MEOW);
  assert(v.variant==1);
  v.touch(true,300,250);v.touch(true,190,260);assert(v.touch(false,0,0)&&v.variant==0&&v.elapsed==0);
  assert(!v.touch(false,0,0));
  v.touch(true,190,250);v.touch(true,300,250);assert(v.touch(false,0,0)&&v.variant==1);
  v.touch(true,200,250);v.touch(true,220,250);assert(!v.touch(false,0,0)&&v.variant==1);
  v.touch(true,200,250);v.touch(true,270,350);assert(!v.touch(false,0,0));
  v.touch(true,300,250);v.touch(true,190,250);v.cancelTouch();assert(!v.touch(false,0,0));
  salary::Date d{2026,9,10,12,0,0};ui::Battery b;b.valid=b.present=true;b.percent=96;
  ui::MoneyAnimation a;apps::Context ctx{d,true,6500000,b,a,1000};
  std::vector<uint16_t> full(480*480),ref(480*480),tiled(480*480);
  ui::Canvas reference{ref.data()};apps::render(reference,apps::HOME,ctx);
  for(int variant=0;variant<2;variant++)for(uint32_t t=0;t<16*meow::FRAME_MS;t+=meow::FRAME_MS) {
    ctx.meowVariant=variant;
    ctx.meowMs=t;ui::Canvas c{full.data()};apps::render(c,apps::MEOW,ctx);
    for(int rows:{32,80}) {
      for(int y=0;y<480;y+=rows) {
        ui::Canvas part{tiled.data()+y*480,y,rows};apps::render(part,apps::MEOW,ctx);
      }
      assert(full==tiled);
    }
    for(int y=0;y<104;y++)for(int x=280;x<480;x++)assert(full[y*480+x]==ref[y*480+x]);
  }
  if(argc>1) {
    ctx.meowVariant=argc>2?atoi(argv[2]):1;
    ctx.meowMs=strtoul(argv[1],nullptr,10);ui::Canvas c{full.data()};apps::render(c,apps::MEOW,ctx);
    printf("P6\n480 480\n255\n");
    for(auto p:full){putchar(((p>>11)&31)*255/31);putchar(((p>>5)&63)*255/63);putchar((p&31)*255/31);}
  }else puts("PASS: meow timing, pause, wrap, navigation, tiles and system area");
}
