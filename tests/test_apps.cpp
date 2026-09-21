#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/app_host.h"
int main() {
  // Optimized row operations must retain clipping and system-area protection.
  for(bool protect:{false,true}) for(int top:{0,32,96,160}) {
    std::vector<uint16_t> fast(480*80),slow(480*80);
    ui::Canvas a{fast.data(),top,80},b{slow.data(),top,80};
    a.protectSystem=b.protectSystem=protect;
    a.clipTop=b.clipTop=12; a.clipBottom=b.clipBottom=190;
    for(int x:{-20,0,270,300,470,500}) for(int y:{-10,20,96,180,470}) {
      a.rect(x,y,80,45,ui::GOLD);
      for(int j=y;j<y+45;j++) for(int i=x;i<x+80;i++) b.pixel(i,j,ui::GOLD);
      assert(fast==slow);
      uint16_t row[80]; for(int i=0;i<80;i++) row[i]=i+1;
      a.scanline(x,y,row,80);
      for(int i=0;i<80;i++) b.pixel(x+i,y,row[i]);
      assert(fast==slow);
    }
  }
  apps::Navigation nav;
  assert(nav.update(apps::HOME,true,80,180)==-1);
  assert(nav.update(apps::HOME,false,80,180)==apps::SALARY);
  nav.cancel();
  assert(nav.update(apps::SALARY,true,380,440)==-1);
  assert(nav.update(apps::SALARY,false,380,440)==-1);
  nav.update(apps::SALARY,true,380,440);
  assert(nav.update(apps::SALARY,false,380,440)==-1);
  nav.update(apps::HOME,true,80,310);
  assert(nav.update(apps::HOME,false,80,310)==apps::CLOCK);
  nav.update(apps::HOME,true,80,180); nav.update(apps::HOME,true,80,220);
  assert(nav.update(apps::HOME,false,80,180)==-1);
  nav.update(apps::HOME,true,20,20);
  assert(nav.update(apps::HOME,false,20,20)==-1);
  std::vector<uint16_t> full(480*480),tile(480*32);
  salary::Date d{2026,9,8,16,0,0}; ui::Battery b; ui::MoneyAnimation animation;
  for(auto id:{apps::HOME,apps::SALARY,apps::CLOCK}) for(bool valid:{false,true}) {
    apps::Context ctx{d,valid,10000000,b,animation,1000};
    ui::Canvas whole{full.data()}; apps::render(whole,id,ctx);
    if(id==apps::SALARY) {
      std::vector<uint16_t> expected(480*480); ui::Canvas direct{expected.data()};
      apps::salaryPage(direct,ctx); ui::battery(direct,b); assert(expected==full);
    }
    for(int y=0;y<480;y+=32) {
      ui::Canvas part{tile.data(),y,32}; apps::render(part,id,ctx);
      for(int i=0;i<480*32;i++) assert(tile[i]==full[y*480+i]);
    }
  }
  // The shell's reserved region must be identical in every app and settings.
  for(int mode=0;mode<5;mode++) {
    b.valid=mode!=0; b.present=mode!=1; b.percent=mode==2?-1:100;
    b.charging=mode==3; b.usb=mode==4;
    apps::Context ctx{d,true,10000000,b,animation,1000};
    ui::Canvas ref{full.data()}; apps::render(ref,apps::HOME,ctx);
    std::vector<uint16_t> other(480*480); ui::Canvas out{other.data()};
    for(int page=0;page<3;page++) {
      if(page<2) apps::render(out,page==0?apps::SALARY:apps::CLOCK,ctx);
      else { interaction::Editor e; e.begin(10000000); apps::renderSettings(out,e,b); }
      if(page==2) for(int y=0;y<480;y+=32) {
        interaction::Editor e; e.begin(10000000);
        ui::Canvas part{tile.data(),y,32}; apps::renderSettings(part,e,b);
        for(int i=0;i<480*32;i++) assert(tile[i]==other[y*480+i]);
      }
      for(int y=0;y<ui::SYSTEM_BOTTOM;y++) for(int x=ui::SYSTEM_X;x<480;x++) assert(full[y*480+x]==other[y*480+x]);
      out.protectSystem=true; out.rect(0,0,480,104,ui::GOLD); out.text(300,34,"999%",font36,ui::INK);
      for(int y=0;y<ui::SYSTEM_BOTTOM;y++) for(int x=ui::SYSTEM_X;x<480;x++) assert(full[y*480+x]==other[y*480+x]);
      out.protectSystem=false;
    }
  }
  assert(interaction::hit(230,40)==13);
  assert(interaction::hit(400,40)==-1);
  puts("PASS: global battery region, protected drawing, settings cancel relocation");
  puts("PASS: launch, removed home hotspot/overlay, release suppression, drag cancellation, tiled apps");
}
