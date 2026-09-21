#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/ui.h"
int main() {
  std::vector<uint16_t> full(ui::W*ui::H),tile(ui::W*32);
  salary::Date dates[]={{2026,9,8,14,27,41},{2026,9,8,20,0,0},{2026,9,12,12,0,0},{2026,9,8,8,0,0}};
  for(int mode=0;mode<5;mode++) {
    auto d=dates[mode%4]; ui::Canvas whole{full.data()};
    ui::render(whole,d,mode!=4);
    for(int y=0;y<ui::H;y+=32) {
      ui::Canvas part{tile.data(),y,32}; ui::render(part,d,mode!=4);
      for(int i=0;i<ui::W*32;i++) assert(tile[i]==full[y*ui::W+i]);
    }
  }
  puts("PASS: all five states, tiled rendering pixel-identical to full frame");
  for(int mode=0;mode<6;mode++) {
    interaction::Editor e; e.begin(999999999); e.error=mode==1;
    if(mode>=3)e.select(mode-2);
    ui::Battery b; b.valid=true; b.present=true; b.usb=true; b.percent=100;
    ui::Canvas whole{full.data()};
    if(mode!=2) ui::settingsPage(whole,e); else ui::render(whole,dates[0],true,nullptr,1000,6500000,&b);
    for(int y=0;y<ui::H;y+=32) {
      ui::Canvas part{tile.data(),y,32};
      if(mode!=2) ui::settingsPage(part,e); else ui::render(part,dates[0],true,nullptr,1000,6500000,&b);
      for(int i=0;i<ui::W*32;i++) assert(tile[i]==full[y*ui::W+i]);
    }
  }
  puts("PASS: keypad, error, battery tiled rendering pixel-identical");
}
