#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/ui.h"
int main() {
  ui::MoneyAnimation a;
  a.set(9999,100,false); assert(!a.active(100));
  a.set(10000,100,true); assert(a.progress(100)==0&&a.active(100));
  int last=0;
  for(int t=0;t<=900;t++) { int p=a.progress(100+t); assert(p>=last&&p<=1000); last=p; }
  assert(!a.active(1000)&&a.progress(1000)==1000);
  a.set(0,600,false); assert(a.from==0&&a.to==0&&!a.active(600));
  a.set(1,UINT32_MAX-100,true); assert(a.active(50)&&a.progress(900)==1000);
  std::vector<uint16_t> full(ui::W*ui::H),expected(full.size()),tile(ui::W*32);
  int64_t pairs[][2]={{28047,28048},{9999,10000},{99999,100000},{9,10},{109,110}};
  for(auto &pair:pairs) {
    for(int p: {0,100,500,900,1000}) {
      ui::Canvas whole{full.data()}; whole.rect(0,0,ui::W,ui::H,ui::BG);
      ui::rollingMoney(whole,pair[0],pair[1],p);
      for(int y=0;y<ui::H;y+=32) {
        ui::Canvas part{tile.data(),y,32}; part.rect(0,0,ui::W,ui::H,ui::BG);
        ui::rollingMoney(part,pair[0],pair[1],p);
        for(int i=0;i<ui::W*32;i++) assert(tile[i]==full[y*ui::W+i]);
      }
      for(int y=0;y<ui::H;y++) if(y<184||y>=256)
        for(int x=0;x<ui::W;x++) assert(full[y*ui::W+x]==ui::BG);
      if(p==1000) {
        ui::Canvas target{expected.data()}; target.rect(0,0,ui::W,ui::H,ui::BG);
        char s[40]; ui::money(s,sizeof(s),pair[1]); target.center(189,s,font64,ui::GOLD);
        assert(expected==full);
      }
    }
  }
  puts("PASS: easing, carry, reset, timer wrap, clipping, tiled frames and exact settled amount");
}
