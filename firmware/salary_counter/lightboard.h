#pragma once
#include "connectivity_ui.h"
#include "lightboard_font.h"

namespace lightboard {
constexpr int FRAME_TOP=144,FRAME_ROWS=240,MAX_BYTES=96;
constexpr int MARQUEE_SCALE=7,MARQUEE_Y=168;
static const uint16_t colors[]={ui::rgb(0xFF6262),ui::rgb(0xFFAA45),ui::rgb(0xFFE66D),ui::rgb(0x73EFAC),ui::rgb(0x68DCFF),ui::rgb(0xAE91FF),ui::rgb(0xFF87C8),ui::rgb(0xF5F5F0)};
static const int speeds[]={40,65,100,145,210};
static const char *speedNames[]={"很慢","慢","适中","快","很快"};
static const char *presets[]={"营业中...","下班了！","在干了...","不干了！","放轻松！","干饭中..."};
inline int glyphIndex(uint32_t code) {
  int left=0,right=lightfont::COUNT-1;
  while(left<=right) { int mid=(left+right)/2; if(lightfont::codes[mid]==code) return mid; if(lightfont::codes[mid]<code) left=mid+1; else right=mid-1; }
  return -1;
}
inline bool decode(const char *&s,uint32_t &code) {
  uint8_t first=(uint8_t)*s++; if(first<128) { code=first; return true; }
  int count=first>=0xC2&&first<=0xDF?1:first>=0xE0&&first<=0xEF?2:first>=0xF0&&first<=0xF4?3:-1;
  if(count<0) return false;
  code=first&((1u<<(6-count))-1);
  for(int i=0;i<count;i++) { uint8_t c=(uint8_t)*s; if((c&0xC0)!=0x80) return false; code=(code<<6)|(c&63); s++; }
  return code>=(count==1?0x80u:count==2?0x800u:0x10000u)&&code<=0x10ffff&&!(code>=0xD800&&code<=0xDFFF);
}
inline int advance(uint32_t code) {
  if(code>=128) return glyphIndex(code)>=0?24:0;
  auto g=ui::find(font24,code); return g?g->advance:0;
}
inline int textWidth(const char *text) { int width=0; while(*text) width+=advance(ui::next(text)); return width; }
inline void glyph(ui::Canvas &c,int x,int y,uint32_t code,int scale,uint16_t color,bool dots=false) {
  int size=dots?scale-1:scale;
  if(code<128) {
    auto g=ui::find(font24,code); if(!g) return;
    for(int j=0;j<g->height;j++) {
      int yy=y+(g->dy+j)*scale; if(yy+size<=c.top||yy>=c.top+c.rows) continue;
      for(int i=0;i<g->width;i++) if(font24.pixels[g->offset+j*g->width+i]>=96) c.rect(x+(g->dx+i)*scale,yy,size,size,color);
    }
  } else {
    int index=glyphIndex(code); if(index<0) return;
    const auto *bits=lightfont::pixels+index*72;
    for(int j=0;j<24;j++) {
      int yy=y+j*scale; if(yy+size<=c.top||yy>=c.top+c.rows) continue;
      for(int i=0;i<24;i++) if(bits[j*3+i/8]&(0x80>>(i%8))) c.rect(x+i*scale,yy,size,size,color);
    }
  }
}
inline void smallText(ui::Canvas &c,int x,int y,const char *text,int width,uint16_t color,bool tail=false) {
  if(tail) while(*text&&textWidth(text)>width) ui::next(text);
  int right=x+width;
  while(*text) { uint32_t code=ui::next(text); int w=advance(code); if(x+w>right) break; glyph(c,x,y,code,1,color); x+=w; }
}
inline const char *candidates(const char *syllable) {
  int left=0,right=lightfont::PINYIN_COUNT-1;
  while(left<=right) { int mid=(left+right)/2,cmp=strcmp(syllable,lightfont::dictionary[mid].key);
    if(!cmp) return lightfont::dictionary[mid].characters; if(cmp>0) left=mid+1; else right=mid-1;
  }
  return "";
}
struct Config { char text[MAX_BYTES+1]="营业中..."; uint8_t color=4,speed=2; };
inline bool valid(const Config &c) {
  if(!memchr(c.text,0,sizeof(c.text))||!c.text[0]||c.color>8||c.speed>4) return false;
  bool visible=false;
  for(const char *p=c.text;*p;) { uint32_t code; if(!decode(p,code)||code<32||code==127||(code>=128&&glyphIndex(code)<0)) return false; if(code!=' ') visible=true; }
  return visible;
}
inline uint32_t signature(const Config &c) {
  uint32_t hash=2166136261u;
  for(const char *p=c.text;*p;p++) hash=(hash^(uint8_t)*p)*16777619u;
  return ((hash^c.color)*16777619u^c.speed)*16777619u;
}
enum Action { NONE, REDRAW, SAVE };
struct View {
  Config saved,draft;
  bool editing=false,typing=false,replace=true,error=false;
  bool pinyin=true;
  char composition[7]{};
  int candidatePage=0;
  bool down=false,blocked=false,fired=false;
  int startX=0,startY=0;
  uint32_t started=0,lastTick=0,offset=0;
  int cycleWidth=640;
  connectivity::View keyboard;
  void cancelTouch() { down=false; blocked=true; fired=false; }
  int cycle() const { return cycleWidth; }
  void enter(uint32_t now) {
    int w=textWidth(saved.text)*MARQUEE_SCALE; cycleWidth=(w<480?480:w)+160;
    editing=typing=false; error=false; composition[0]=0; candidatePage=0; offset=0; lastTick=now; cancelTouch();
  }
  void begin() { draft=saved; editing=true; typing=false; replace=true; error=false; composition[0]=0; candidatePage=0; cancelTouch(); }
  void accepted(uint32_t now) { saved=draft; enter(now); }
  void tick(uint32_t now) {
    uint32_t dt=now-lastTick; lastTick=now;
    if(!editing) offset=(offset+(uint64_t)dt*speeds[saved.speed]*256/1000)%(cycle()*256u);
  }
  bool appendText(const char *text) {
    size_t n=replace?0:strlen(draft.text),added=strlen(text);
    if(n+added>MAX_BYTES) { error=true; return false; }
    if(replace) { memset(draft.text,0,sizeof(draft.text)); replace=false; }
    memcpy(draft.text+n,text,added+1); error=false; return true;
  }
  void append(char ch) {
    if(ch<32||ch>126) return;
    if(pinyin&&keyboard.keyboard==0&&((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z'))) {
      if(ch>='A'&&ch<='Z') ch+=32;
      size_t n=strlen(composition); if(n<6) { composition[n]=ch; composition[n+1]=0; candidatePage=0; error=false; } else error=true;
      return;
    }
    if(composition[0]) { error=true; return; }
    char text[]={ch,0}; appendText(text);
  }
  int candidateCount() const { return strlen(candidates(composition))/3; }
  bool selectCandidate(int index) {
    const char *choices=candidates(composition); int count=strlen(choices)/3;
    if(index<0||index>=count) return false;
    char text[4]={}; memcpy(text,choices+index*3,3);
    if(!appendText(text)) return false;
    composition[0]=0; candidatePage=0; return true;
  }
  void erase() {
    size_t composing=strlen(composition); if(composing) { composition[composing-1]=0; candidatePage=0; error=false; return; }
    if(replace) { draft.text[0]=0; replace=false; }
    else { size_t n=strlen(draft.text); if(n) { --n; while(n&&((uint8_t)draft.text[n]&0xC0)==0x80) --n; draft.text[n]=0; } }
    error=false;
  }
  Action tap(int x,int y) {
    if(!editing) return NONE;
    error=false;
    if(x>=184&&x<276&&y>=22&&y<68) {
      if(typing) { typing=false; composition[0]=0; } else editing=false;
      cancelTouch(); return REDRAW;
    }
    if(!typing) {
      if(x>=28&&x<452&&y>=110&&y<200) {
        int col=(x-28)/146,row=(y-110)/48,index=row*3+col;
        if(col<3&&row<2&&(x-28)%146<132&&(y-110)%48<42) { strcpy(draft.text,presets[index]); replace=true; composition[0]=0; }
      }
      else if(x>=28&&x<452&&y>=214&&y<272) { typing=true; pinyin=true; composition[0]=0; keyboard.keyboard=0; keyboard.upper=false; cancelTouch(); }
      else if(y>=306&&y<340&&x>=28&&x<451) { int i=(x-28)/47; if(i<=8&&(x-28)%47<40) draft.color=i; }
      else if(y>=378&&y<414&&x>=28&&x<453) { int i=(x-28)/85; if(i<5&&(x-28)%85<78) draft.speed=i; }
      else if(x>=28&&x<452&&y>=430&&y<470) { if(valid(draft)) return SAVE; error=true; }
      return REDRAW;
    }
    if(x>=28&&x<452&&y>=112&&y<175) { replace=!replace; return REDRAW; }
    if(pinyin&&y>=220&&y<262) {
      if(x>=20&&x<380) selectCandidate(candidatePage*5+(x-20)/72);
      else if(x>=380&&x<460&&candidateCount()) candidatePage=(candidatePage+1)%((candidateCount()+4)/5);
      return REDRAW;
    }
    if(y>=272&&y<404&&x>=20&&x<460) {
      int row=(y-272)/44,col=(x-20)/44,index=row*10+col;
      const char *keys=keyboard.keys();
      if((y-272)%44<40&&(x-20)%44<40&&index<int(strlen(keys))&&keys[index]!=' ') append(keys[index]);
    } else if(y>=420&&y<464) {
      if(x>=20&&x<90) {
        if(composition[0]) error=true;
        else { pinyin=!pinyin; keyboard.keyboard=0; keyboard.upper=false; }
      }
      else if(x>=96&&x<158) {
        if(composition[0]) error=true;
        else if(keyboard.keyboard==0&&!keyboard.upper) keyboard.upper=true;
        else { keyboard.upper=false; keyboard.keyboard=(keyboard.keyboard+1)%3; }
      }
      else if(x>=164&&x<244) { if(composition[0]) selectCandidate(candidatePage*5); else append(' '); }
      else if(x>=250&&x<330) erase();
      else if(x>=336&&x<460) { if(composition[0]) error=true; else { typing=false; cancelTouch(); } }
    }
    return REDRAW;
  }
  Action touch(bool pressed,int x,int y,uint32_t now) {
    if(!pressed) {
      int dx=x-startX,dy=y-startY;
      bool tapped=down&&!blocked&&!fired&&editing&&dx*dx+dy*dy<=12*12;
      down=false; blocked=false; fired=false;
      return tapped?tap(x,y):NONE;
    }
    if(blocked&&!down) return NONE;
    if(!down) { down=true; startX=x; startY=y; started=now; }
    int dx=x-startX,dy=y-startY;
    if(dx*dx+dy*dy>12*12) blocked=true;
    if(!editing&&!blocked&&!fired&&uint32_t(now-started)>=800) { begin(); fired=true; return REDRAW; }
    return NONE;
  }
};
inline void marqueeText(ui::Canvas &c,int x,const Config &config) {
  int index=0;
  for(const char *s=config.text;*s;index++) {
    uint32_t code=ui::next(s); int w=advance(code)*MARQUEE_SCALE;
    if(x+w>=0&&x<480) {
      uint16_t color=colors[config.color==8?index%7:config.color];
      glyph(c,x,MARQUEE_Y,code,MARQUEE_SCALE,color,true);
    }
    x+=w;
  }
}
inline void render(ui::Canvas &c,const View &v) {
  c.rect(0,0,480,480,ui::BG);
  if(!v.editing) {
    int x=32-int(v.offset/256),cycle=v.cycle();
    marqueeText(c,x,v.saved); marqueeText(c,x+cycle,v.saved); return;
  }
  c.text(36,36,v.typing?"输入文字":"灯牌设置",font24,ui::INK);
  c.text(212,36,v.typing?"返回":"取消",font20,ui::MUTED);
  c.text(36,78,v.typing?(v.pinyin?"拼音输入":"英文输入"):"快捷文案",font18,ui::MUTED);
  if(!v.typing) {
    for(int i=0;i<6;i++) connectivity::button(c,28+i%3*146,110+i/3*48,132,42,presets[i],!strcmp(v.draft.text,presets[i]));
    c.round(28,214,424,58,12,ui::PANEL);
    smallText(c,44,230,v.draft.text[0]?v.draft.text:"请输入文字",326,ui::INK);
    c.text(396,234,"编辑",font18,ui::MUTED);
    c.text(36,282,v.error?"文字不能为空或保存失败":"文字颜色",font18,v.error?ui::GOLD:ui::MUTED);
    for(int i=0;i<9;i++) {
      int x=28+i*47;
      c.round(x-2,304,44,38,8,v.draft.color==i?ui::INK:ui::BG);
      c.round(x,306,40,34,7,i==8?ui::PANEL:colors[i]);
      if(i==8) c.text(x+11,314,"彩",font18,ui::INK);
    }
    c.text(36,352,"滚动速度",font18,ui::MUTED);
    for(int i=0;i<5;i++) connectivity::button(c,28+i*85,378,78,36,speedNames[i],v.draft.speed==i);
    connectivity::button(c,28,430,424,40,"保存并播放",true); return;
  }
  c.round(28,112,424,63,12,v.replace?ui::rgb(0x3B3221):ui::PANEL);
  smallText(c,44,130,v.draft.text[0]?v.draft.text:"请输入文字",386,ui::INK,true);
  if(v.pinyin) {
    c.text(36,188,v.composition[0]?v.composition:v.error?"请先选字或检查文字长度":"输入单字拼音，点候选字",font18,v.error?ui::GOLD:ui::MUTED);
    const char *choices=candidates(v.composition); int count=strlen(choices)/3;
    for(int i=0;i<5;i++) {
      int index=v.candidatePage*5+i; if(index>=count) break;
      const char *p=choices+index*3; uint32_t code=ui::next(p);
      c.round(20+i*72,220,64,42,8,ui::PANEL); glyph(c,40+i*72,229,code,1,ui::INK);
    }
    if(count>5) connectivity::button(c,380,220,80,42,"换页");
  } else {
    c.text(36,194,v.replace?"输入替换原文，点击文本可追加":"继续输入，点击文本可全选",font18,ui::MUTED);
    c.text(36,230,v.error?"最多 96 字节，请删除后重试":"英文 / 数字 / 符号",font18,ui::MUTED);
  }
  const char *keys=v.keyboard.keys();
  for(int i=0;keys[i];i++) if(keys[i]!=' ') { char key[]={keys[i],0}; connectivity::button(c,20+i%10*44,272+i/10*44,40,40,key); }
  connectivity::button(c,20,420,70,44,v.pinyin?"中":"EN");
  connectivity::button(c,96,420,62,44,v.keyboard.keyboard==0?(v.keyboard.upper?"123":"ABC"):v.keyboard.keyboard==1?"符号":"abc");
  connectivity::button(c,164,420,80,44,"空格"); connectivity::button(c,250,420,80,44,"删除");
  connectivity::button(c,336,420,124,44,"完成",true);
}
}
