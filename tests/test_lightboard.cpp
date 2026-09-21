#include <assert.h>
#include <stdio.h>
#include <vector>
#include "../firmware/salary_counter/app_host.h"
int main() {
  lightboard::View v; assert(lightboard::valid(v.saved));
  v.enter(0); v.touch(false,0,0,0);
  assert(v.touch(true,200,240,100)==lightboard::NONE);
  assert(v.touch(true,200,240,899)==lightboard::NONE);
  assert(v.touch(true,200,240,900)==lightboard::REDRAW&&v.editing&&!v.typing);
  assert(v.touch(false,200,240,910)==lightboard::NONE);
  v.touch(true,200,240,1000); v.touch(false,200,240,1100); assert(v.typing&&v.pinyin);
  v.touch(false,0,0,1150);
  v.append('n'); v.append('i');
  const char *ni=lightboard::candidates("ni"),*you=strstr(ni,"你"); assert(you);
  assert(v.selectCandidate((you-ni)/3)&&!strcmp(v.draft.text,"你"));
  v.append('h'); v.append('a'); v.append('o');
  const char *hao=lightboard::candidates("hao"),*good=strstr(hao,"好"); assert(good);
  assert(v.selectCandidate((good-hao)/3)&&!strcmp(v.draft.text,"你好"));
  v.erase(); assert(!strcmp(v.draft.text,"你"));
  strcpy(v.composition,"shi"); assert(v.candidateCount()>5);
  v.tap(420,240); assert(v.candidatePage==1);
  v.tap(40,240); assert(!v.composition[0]&&strlen(v.draft.text)==6);
  v.append('x'); assert(v.tap(400,440)==lightboard::REDRAW&&v.typing&&v.error);
  v.erase(); assert(!v.composition[0]);
  v.pinyin=false; v.replace=true;
  v.append('a'); v.append('!'); assert(!strcmp(v.draft.text,"a!"));
  v.erase(); assert(!strcmp(v.draft.text,"a"));
  for(int i=0;i<100;i++) v.append('x'); assert(strlen(v.draft.text)==96&&v.error);
  v.replace=true; v.erase(); assert(!lightboard::valid(v.draft));
  v.append(' '); assert(!lightboard::valid(v.draft));
  v.replace=true; v.append('A'); assert(lightboard::valid(v.draft));
  v.typing=false;
  for(int i=0;i<6;i++) { v.tap(40+i%3*146,130+i/3*48); assert(!strcmp(v.draft.text,lightboard::presets[i])); assert(lightboard::valid(v.draft)); }
  for(int i=0;i<9;i++) { v.tap(40+i*47,320); assert(v.draft.color==i); }
  for(int i=0;i<5;i++) { v.tap(40+i*85,395); assert(v.draft.speed==i); }
  assert(v.tap(200,435)==lightboard::SAVE);
  v.accepted(1000); assert(!v.editing&&v.saved.speed==4&&v.saved.color==8);
  v.tick(2000); assert(v.offset==210*256);
  v.begin(); auto frozen=v.offset; v.tick(3000); assert(v.offset==frozen);
  v.draft.color=0; v.enter(4000); assert(v.saved.color==8&&!v.editing);
  v.touch(false,0,0,4000); v.touch(true,100,200,4100); v.touch(true,200,200,4200);
  assert(v.touch(true,100,200,5100)==lightboard::NONE&&!v.editing);
  v.cancelTouch(); assert(v.touch(true,100,200,6200)==lightboard::NONE);
  v.enter(0xfffffff0); v.tick(0x3d8); assert(v.offset==210*256); // 1000 ms across wrap.
  lightboard::Config bad;
  strcpy(bad.text,"\xE4"); assert(!lightboard::valid(bad));
  strcpy(bad.text,"\xED\xA0\x80"); assert(!lightboard::valid(bad));
  strcpy(bad.text,"😀"); assert(!lightboard::valid(bad));
  for(const auto &entry:lightfont::dictionary) for(const char *p=entry.characters;*p;) assert(lightboard::glyphIndex(ui::next(p))>=0);
  for(int speed=0;speed<5;speed++) {
    v.saved.speed=speed; v.enter(0);
    for(uint32_t t=0;t<20000;t+=47) { v.tick(t); assert(v.offset<unsigned(v.cycle()*256)); }
  }
  std::vector<uint16_t> full(480*480),tile(480*80),reference(480*480);
  salary::Date d{2026,9,9,12,0,0}; ui::Battery b; b.valid=b.present=b.charging=true; b.percent=88;
  ui::MoneyAnimation a; ui::Canvas ref{reference.data()}; apps::render(ref,apps::HOME,{d,true,6500000,b,a,1000});
  for(int mode=0;mode<5;mode++) for(int color=0;color<9;color++) {
    v.saved.color=color; strcpy(v.saved.text,mode==1?"下班了！ ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 !@#$%":"营业中..."); v.enter(0); v.tick(1731);
    if(mode>=2) { v.begin(); v.typing=mode>=3; v.pinyin=mode==3; if(v.pinyin) strcpy(v.composition,"xia"); v.keyboard.keyboard=mode==4?1:0; }
    apps::Context ctx{d,true,6500000,b,a,1000,0,nullptr,nullptr,nullptr,0,&v};
    ui::Canvas c{full.data()}; apps::render(c,apps::LIGHTBOARD,ctx);
    for(int y=0;y<480;y+=32) {
      ui::Canvas part{tile.data(),y,32}; apps::render(part,apps::LIGHTBOARD,ctx);
      for(int i=0;i<480*32;i++) assert(tile[i]==full[y*480+i]);
    }
    for(int y=0;y<104;y++) for(int x=280;x<480;x++) assert(full[y*480+x]==reference[y*480+x]);
    if(!v.editing) {
      for(int y=104;y<480;y++) if(y<lightboard::FRAME_TOP||y>=lightboard::FRAME_TOP+lightboard::FRAME_ROWS)
        for(int x=0;x<480;x++) assert(full[y*480+x]==ui::BG);
      for(int y=lightboard::FRAME_TOP;y<lightboard::FRAME_TOP+lightboard::FRAME_ROWS;y+=80) {
        ui::Canvas part{tile.data(),y,80}; apps::render(part,apps::LIGHTBOARD,ctx);
        for(int i=0;i<480*80;i++) assert(tile[i]==full[y*480+i]);
      }
    }
  }
  puts("PASS: lightboard hold, keyboard, bounds, save/cancel, colors/speeds, wrap, reserved region, full/partial rendering");
}
