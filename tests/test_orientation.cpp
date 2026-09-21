#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/orientation.h"
#include "../firmware/salary_counter/app_host.h"
int main() {
  using namespace orientation;
  std::vector<uint8_t> scratch(SCRATCH_BYTES);
  for(int q=0;q<4;q++) for(int y=0;y<SIDE;y++) for(int x=0;x<SIDE;x++) {
    auto p=logical(physical({x,y},q),q); assert(p.x==x&&p.y==y);
  }
  for(int rows:{1,2,16,32,80}) for(int top:{0,104,176,480-rows}) for(int q=0;q<4;q++) {
    if(top+rows>480) continue;
    std::vector<uint16_t> data(SIDE*rows);
    for(int i=0;i<SIDE*rows;i++) data[i]=i;
    std::vector<uint16_t> copied(data.size());rotateInto(data.data(),copied.data(),rows,q);
    rotateBlock(data.data(),rows,q,scratch.data()); auto r=blockRect(top,rows,q);
    assert(data==copied);
    for(int y=0;y<rows;y++) for(int x=0;x<SIDE;x++) {
      auto p=physical({x,top+y},q); assert(p.x>=r.x&&p.x<r.x+r.w&&p.y>=r.y&&p.y<r.y+r.h);
      assert(data[(p.y-r.y)*r.w+p.x-r.x]==y*SIDE+x);
    }
  }
  Axes axes{1,-2}; assert(axes.valid()); assert(Axes::component(-2,20,100)==-100);
  assert((!Axes{0,2}.valid())); assert((!Axes{1,-1}.valid()));
  Detector detector; uint32_t now=0;
  auto settle=[&](int x,int y,int z) { for(int i=0;i<16;i++) { now+=40; detector.sample(x,y,z,now); } };
  settle(-1000,0,0); assert(detector.current==1);
  settle(0,-1000,0); assert(detector.current==2);
  settle(1000,0,0); assert(detector.current==3);
  settle(0,1000,0); assert(detector.current==0);
  settle(0,-100,995); assert(detector.current==0); // Flat, keep last orientation.
  settle(700,-700,0); assert(detector.current==0); // Diagonal deadband.
  settle(1700,0,0); assert(detector.current==0); // Shaking.
  for(int i=0;i<20;i++) { now+=40; detector.sample(i%2?1000:-1000,0,0,now); }
  assert(detector.current==0); // No unstable flip.
  detector.reset(); now=0xffffff00; settle(-1000,0,0); assert(detector.current==1);
  detector.current=0; detector.reset(); detector.sample(-1000,0,0,1000);
  assert(!detector.sample(-1000,0,0,2000)&&detector.current==0); // Stale sample is not continuous stability.
  std::vector<uint16_t> full(SIDE*SIDE),physicalFrame(SIDE*SIDE),tile(SIDE*MAX_ROWS);
  salary::Date date{2026,9,9,14,0,0}; ui::Battery b; b.valid=b.present=b.charging=true;b.percent=88;
  ui::MoneyAnimation money; emotions::View eyes; eyes.choose(0); lightboard::View lamp; lamp.enter(0);
  connectivity::Model network; connectivity::View networkView;
  for(int mode=0;mode<10;mode++) {
    apps::Id app=mode<6?static_cast<apps::Id>(mode):mode==6?apps::LIGHTBOARD:mode==7?apps::SETTINGS:apps::SALARY;
    lamp.editing=mode==6; lamp.typing=mode==6; strcpy(lamp.composition,"xia");
    networkView.page=mode==7?connectivity::PASSWORD:connectivity::ROOT;
    interaction::Editor editor; editor.begin(1234567);
    apps::Context ctx{date,true,1234567,b,money,1000,mode==0?308:0,&network,&networkView,&eyes,280,&lamp};
    auto render=[&](ui::Canvas &c) { if(mode>=8) { if(mode==9) editor.error=true; apps::renderSettings(c,editor,b); } else apps::render(c,app,ctx); };
    ui::Canvas c{full.data()}; render(c);
    for(int q=0;q<4;q++) for(int rows:{32,80}) {
      for(int top=0;top<SIDE;top+=rows) {
        ui::Canvas part{tile.data(),top,rows};render(part);
        if(q&1) {
          std::vector<uint16_t> buffer(SIDE*32);
          for(int offset=0;offset<rows;offset+=32) {
            int count=rows-offset<32?rows-offset:32;
            rotateInto(tile.data()+offset*SIDE,buffer.data(),count,q);auto r=blockRect(top+offset,count,q);
            for(int y=0;y<r.h;y++) for(int x=0;x<r.w;x++) physicalFrame[(r.y+y)*SIDE+r.x+x]=buffer[y*r.w+x];
          }
        } else {
          rotateBlock(tile.data(),rows,q,scratch.data());auto r=blockRect(top,rows,q);
          for(int y=0;y<r.h;y++) for(int x=0;x<r.w;x++) physicalFrame[(r.y+y)*SIDE+r.x+x]=tile[y*r.w+x];
        }
      }
      for(int y=0;y<SIDE;y++) for(int x=0;x<SIDE;x++) {
        auto p=physical({x,y},q);assert(physicalFrame[p.y*SIDE+p.x]==full[y*SIDE+x]);
      }
    }
  }
  // Hit the same logical app after converting a physical tap in every orientation.
  apps::Navigation nav; nav.scroll.offset=apps::COUNT*130+10-352;
  for(int q=0;q<4;q++) {
    auto p=logical(physical({90,390},q),q); assert(nav.hit(apps::HOME,p.x,p.y)==apps::SETTINGS);
  }
  puts("PASS: 4-way tile permutation, touch inverses, all apps/keyboards, partial frames, hysteresis, flat/shake/stale rejection and timer wrap");
}
