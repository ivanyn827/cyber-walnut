#include <cassert>
#include <vector>
#include <cstdio>
#include "../firmware/salary_counter/app_host.h"
int main() {
  for(unsigned a=0;a<65536;a+=997) for(unsigned b=0;b<65536;b+=991) for(int t=0;t<256;t+=7) {
    int f=(t+4)>>3;
    unsigned r=(((a>>11)*(32-f)+(b>>11)*f)>>5)<<11;
    unsigned g=((((a>>5)&63)*(32-f)+((b>>5)&63)*f)/32)*32;
    unsigned blue=(((a&31)*(32-f)+(b&31)*f)>>5);
    assert(quotaSprite::mix(a,b,t)==(r|g|blue));
  }
  quota::View v;
  assert(v.sample.remaining==-1&&v.stale(0));
  assert(v.accept({1,1788970000,1789400000,57,10080},true,100));
  assert(!v.accept({2,1788970001,0,30,10080},false,200));
  assert(!v.accept({1,1788970001,0,30,10080},true,300));
  assert(!v.accept({2,1788969999,0,30,10080},true,300));
  assert(v.accept({2,1788970001,0,30,10080},false,6200));
  assert(v.stage()==1&&!v.stale(6200)&&v.stale(100000));
  assert(!v.touch(true,200,200,100));assert(v.touch(true,200,200,751));assert(v.details);
  v.touch(false,200,200,760);v.touch(true,200,200,800);v.touch(true,240,200,900);assert(!v.touch(true,240,200,1600));
  salary::Date d{2026,9,9,15,0,0};ui::Battery b;ui::MoneyAnimation a;
  std::vector<uint16_t> full(480*480),tile(480*32),ref(480*480);
  uint64_t seq=3;
  for(int value:{-1,0,19,20,39,40,59,60,79,80,100}) for(bool detail:{false,true}) {
    assert(v.accept({seq++,1788970002,1789435927,value,10080},true,8000));v.details=detail;
    apps::Context ctx{d,true,10000000,b,a,1000};ctx.quotaView=&v;ctx.frameMs=10000;
    ui::Canvas c{full.data()},r{ref.data()};apps::render(c,apps::QUOTA,ctx);apps::render(r,apps::HOME,ctx);
    for(int y=0;y<104;y++)for(int x=280;x<480;x++)assert(full[y*480+x]==ref[y*480+x]);
    for(int y=0;y<480;y+=32){ui::Canvas part{tile.data(),y,32};apps::render(part,apps::QUOTA,ctx);for(int i=0;i<480*32;i++)assert(tile[i]==full[y*480+i]);}
    if(value==80&&!detail){FILE *f=fopen("build/quota-preview.ppm","wb");fprintf(f,"P6\n480 480\n255\n");for(auto p:full){unsigned char rgb[]={static_cast<unsigned char>(((p>>11)&31)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
  }
  assert(apps::menuEntry(apps::COUNT-1).id==apps::SETTINGS);
  for(int mode=-1;mode<=4;mode++) for(int value:{-1,0,19,20,39,40,59,60,79,80,100}) {
    quota::View q;q.sample={12,1788970000,1789435927,value,10080};
    assert(q.select(mode,100));assert(q.sample.remaining==value&&q.sample.sequence==12);
    assert(q.stage()==(mode>=0?mode:value<0?2:value>=80?4:value/20));
    assert(q.accept({13,1788970001,1789435927,8,10080},true,200));
    assert(q.appearance==mode&&q.sample.remaining==8);
    q.details=true;
    apps::Context ctx{d,true,10000000,b,a,1000};ctx.quotaView=&q;ctx.frameMs=1000;
    ui::Canvas c{full.data()},r{ref.data()};apps::render(c,apps::QUOTA,ctx);apps::render(r,apps::HOME,ctx);
    for(int y=0;y<104;y++)for(int x=280;x<480;x++)assert(full[y*480+x]==ref[y*480+x]);
    for(int y=0;y<480;y+=32){ui::Canvas part{tile.data(),y,32};apps::render(part,apps::QUOTA,ctx);for(int i=0;i<480*32;i++)assert(tile[i]==full[y*480+i]);}
  }
  for(int i=0;i<6;i++) {
    quota::View q;q.details=true;int x=100+(i%2)*220,y=181+(i/2)*54;
    assert(!q.touch(true,x,y,0));assert(q.touch(false,x,y,100));assert(q.requested==(i==0?-1:5-i));
    assert(q.appearance==-1); // Persistence layer must accept before changing presentation.
    q.requested=-2;q.touch(true,x,y,200);q.touch(true,x+25,y,300);assert(!q.touch(false,x,y,400)&&q.requested==-2);
    q.touch(true,x,y,500);assert(!q.touch(true,x,y,1200));assert(q.details);q.cancelTouch();q.touch(false,x,y,1300);assert(q.requested==-2);
  }
  {
    quota::View q;q.details=true;q.select(4,0);q.sample.remaining=17;
    q.touch(true,200,450,0);assert(!q.touch(true,200,450,700)&&q.details);
    assert(q.touch(false,200,450,750)&&!q.details&&q.appearance==4&&q.sample.remaining==17&&q.requested==-2);
    assert(!q.touch(true,200,450,800));q.touch(false,200,450,810);
    q.details=true;q.touch(true,200,450,900);q.touch(true,230,450,1000);assert(!q.touch(false,200,450,1100)&&q.details);
    q.touch(true,200,420,1200);assert(!q.touch(false,200,440,1300)&&q.details);
  }
  assert(!v.select(-2,0)&&!v.select(5,0));
  {quota::View q;q.details=true;q.touch(true,390,348,0);assert(q.touch(false,390,348,100)&&q.notifyRequested&&q.notify);}
  puts("PASS manual appearance: all modes, real data preservation, sync independence, selection/drag/hold and tiled settings");
  puts("PASS quota: boundaries, unknown, stale, replay, USB priority, long press, reserved area, tiled rendering");
}
