#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/app_host.h"
int main() {
  assert(apps::menuEntry(apps::COUNT-1).id==apps::SETTINGS);
  int rowIndex=0;
  for(const auto &app:apps::registry) if(app.id!=apps::SETTINGS) assert(apps::menuEntry(rowIndex++).id==app.id);
  apps::Navigation n;
  n.update(apps::HOME,true,90,440); n.update(apps::HOME,true,90,120);
  assert(n.offset()==320&&n.changed);
  assert(n.update(apps::HOME,false,90,120)==-1);
  for(auto id:{apps::SETTINGS,apps::EMOTIONS,apps::LIGHTBOARD,apps::QUOTA}) {
    int row=0; while(apps::menuEntry(row).id!=id) row++;
    n.scroll.offset=std::min(n.scroll.maximum,row*130);
    int y=122+row*130-n.offset()+56;
    n.update(apps::HOME,true,90,y); assert(n.update(apps::HOME,false,90,y)==id);
  }
  n.scroll.offset=300;
  n.update(apps::HOME,true,90,120); n.update(apps::HOME,true,90,460);
  assert(n.offset()==0); assert(n.update(apps::HOME,false,90,460)==-1);
  connectivity::View v; connectivity::Model m;
  for(int page=0;page<3;page++) { v.keyboard=page; assert(strlen(v.keys())<=30); }
  v.locked=true; assert(!v.validPassword());
  for(char c:"Abc!1234") if(c) v.append(c);
  assert(v.validPassword());
  for(int i=0;i<100;i++) v.append('x'); assert(strlen(v.password)==63);
  v.reset(); assert(!v.password[0]);
  m.wifi=true; m.count=8;
  for(int i=0;i<m.count;i++) snprintf(m.networks[i].ssid,33,"Test Network %d",i);
  v.page=connectivity::WIFI;
  v.touch(false,0,0,m);
  v.touch(true,100,440,m); v.touch(true,100,280,m);
  v.touch(false,100,280,m); assert(v.page==connectivity::WIFI&&v.scroll.offset==160);
  v.tap(100,300,m); assert(v.page==connectivity::PASSWORD);
  v.append('z'); v.tap(230,36,m); assert(v.page==connectivity::WIFI&&!v.password[0]);
  salary::Date d{2026,9,8,16,0,0}; ui::Battery b; b.valid=b.present=b.charging=true; b.percent=88;
  ui::MoneyAnimation a;
  std::vector<uint16_t> full(480*480),tile(480*32),reference(480*480);
  ui::Canvas ref{reference.data()}; ui::battery(ref,b);
  for(auto page:{connectivity::ROOT,connectivity::WIFI,connectivity::BLE,connectivity::PASSWORD}) {
    v.page=page; strcpy(v.selected,"Example-WiFi");
    apps::Context ctx{d,true,6500000,b,a,1000,0,&m,&v};
    ui::Canvas c{full.data()}; apps::render(c,apps::SETTINGS,ctx);
    for(int y=0;y<480;y+=32) {
      ui::Canvas part{tile.data(),y,32}; apps::render(part,apps::SETTINGS,ctx);
      for(int i=0;i<480*32;i++) assert(tile[i]==full[y*480+i]);
    }
    for(int y=0;y<104;y++) for(int x=280;x<480;x++) assert(full[y*480+x]==reference[y*480+x]);
  }
  puts("PASS: app swipe bounds and tap suppression, Wi-Fi list swipe, password validation/clear, all settings tiles/system region");
}
