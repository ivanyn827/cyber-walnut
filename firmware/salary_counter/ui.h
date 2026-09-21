#pragma once
#include <stdio.h>
#include <string.h>
#include "salary.h"
#include "fonts.h"
#include "animation.h"
#include "interaction.h"

namespace ui {
constexpr int W=480,H=480;
// System-owned top-right area. App content and touch targets must stay outside.
constexpr int SYSTEM_X=280,SYSTEM_BOTTOM=104;
inline bool systemArea(int x,int y) { return x>=SYSTEM_X&&x<W&&y>=0&&y<SYSTEM_BOTTOM; }
constexpr uint16_t rgb(unsigned c) { return ((c>>19)&31)<<11 | ((c>>10)&63)<<5 | ((c>>3)&31); }
constexpr uint16_t BG=rgb(0x161719), PANEL=rgb(0x232426), GOLD=rgb(0xE5BE72), INK=rgb(0xEEEDE8), MUTED=rgb(0x92938E);
inline uint32_t next(const char *&p) {
  uint32_t c=(uint8_t)*p++;
  if(c<128) return c;
  int n=(c&0xE0)==0xC0?1:(c&0xF0)==0xE0?2:3;
  c &= (1<<(6-n))-1;
  while(n-- && *p) c=(c<<6)|((uint8_t)*p++&63);
  return c;
}
inline const Glyph *find(const Font &f,uint32_t code) {
  for(int i=0;i<f.count;i++) if(f.glyphs[i].code==code) return &f.glyphs[i];
  return nullptr;
}
inline int width(const char *s,const Font &f) {
  int w=0; while(*s) { auto g=find(f,next(s)); if(g) w+=g->advance; } return w;
}
struct Canvas {
  uint16_t *pixels;
  int top=0,rows=H;
  int clipTop=0,clipBottom=H;
  bool protectSystem=false;
  void pixel(int x,int y,uint16_t color) { if(x>=0&&x<W&&y>=top&&y<top+rows&&y>=clipTop&&y<clipBottom&&!(protectSystem&&systemArea(x,y))) pixels[(y-top)*W+x]=color; }
  void rect(int x,int y,int w,int h,uint16_t color) {
    int first=y>top?y:top,last=y+h<top+rows?y+h:top+rows;
    if(first<clipTop) first=clipTop; if(last>clipBottom) last=clipBottom;
    int left=x<0?0:x,right=x+w>W?W:x+w;
    for(int j=first;j<last;j++) {
      int end=protectSystem&&j>=0&&j<SYSTEM_BOTTOM&&right>SYSTEM_X?SYSTEM_X:right;
      for(int i=left;i<end;i++) pixels[(j-top)*W+i]=color;
    }
  }
  // Clipped scanline copy; the same system-area protection as pixel/rect.
  void scanline(int x,int y,const uint16_t *colors,int count) {
    if(y<top||y>=top+rows||y<clipTop||y>=clipBottom) return;
    int left=x<0?0:x,right=x+count>W?W:x+count;
    if(protectSystem&&y>=0&&y<SYSTEM_BOTTOM&&right>SYSTEM_X) right=SYSTEM_X;
    if(right>left) memcpy(pixels+(y-top)*W+left,colors+(left-x),(right-left)*sizeof(uint16_t));
  }
  void round(int x,int y,int w,int h,int r,uint16_t color) {
    int first=y<top?top-y:0,last=y+h>top+rows?top+rows-y:h;
    for(int j=first;j<last;j++) for(int i=0;i<w;i++) {
      int dx=i<r?r-1-i:i>=w-r?i-(w-r):0;
      int dy=j<r?r-1-j:j>=h-r?j-(h-r):0;
      if(dx*dx+dy*dy<=r*r) pixel(x+i,y+j,color);
    }
  }
  void text(int x,int y,const char *s,const Font &f,uint16_t color) {
    while(*s) {
      auto g=find(f,next(s)); if(!g) continue;
      int first=y+g->dy<top?top-y-g->dy:0;
      int last=y+g->dy+g->height>top+rows?top+rows-y-g->dy:g->height;
      for(int j=first;j<last;j++) for(int i=0;i<g->width;i++) {
        int xx=x+g->dx+i, yy=y+g->dy+j;
        if(xx<0||xx>=W||yy<top||yy>=top+rows||yy<clipTop||yy>=clipBottom) continue;
        if(protectSystem&&systemArea(xx,yy)) continue;
        int a=f.pixels[g->offset+j*g->width+i];
        if(a==0) continue;
        if(a==255) { pixels[(yy-top)*W+xx]=color; continue; }
        uint16_t b=pixels[(yy-top)*W+xx];
        int rr=(((color>>11)&31)*a+((b>>11)&31)*(255-a)+127)/255;
        int gg=(((color>>5)&63)*a+((b>>5)&63)*(255-a)+127)/255;
        int bb=((color&31)*a+(b&31)*(255-a)+127)/255;
        pixels[(yy-top)*W+xx]=(rr<<11)|(gg<<5)|bb;
      }
      x+=g->advance;
    }
  }
  void center(int y,const char *s,const Font &f,uint16_t color) { text((W-width(s,f))/2,y,s,f,color); }
};
inline void money(char *s,size_t n,int64_t cents,bool symbol=true) {
  snprintf(s,n,"%s%lld.%02lld",symbol?"¥":"",(long long)(cents/100),(long long)(cents%100));
}
inline void rollingMoney(Canvas &c,int64_t from,int64_t to,int progress) {
  char oldValue[32],newValue[32],whole[40];
  money(oldValue,sizeof(oldValue),from,false); money(newValue,sizeof(newValue),to,false);
  money(whole,sizeof(whole),to);
  int x=(W-width(whole,font64))/2;
  c.text(x,189,"¥",font64,GOLD); x+=width("¥",font64);
  int delta=int(strlen(newValue))-int(strlen(oldValue));
  int oldTop=c.clipTop,oldBottom=c.clipBottom;
  c.clipTop=184; c.clipBottom=256;
  for(int i=0;newValue[i];i++) {
    char incoming[2]={newValue[i],0};
    int previous=i-delta;
    char outgoing[2]={previous>=0&&previous<int(strlen(oldValue))?oldValue[previous]:' ',0};
    if(incoming[0]==outgoing[0]||progress>=1000) c.text(x,189,incoming,font64,GOLD);
    else {
      int offset=72*progress/1000;
      c.text(x,189-offset,outgoing,font64,GOLD);
      c.text(x,189+72-offset,incoming,font64,GOLD);
    }
    x+=width(incoming,font64);
  }
  c.clipTop=oldTop; c.clipBottom=oldBottom;
}
struct Battery { bool valid=false,present=false,charging=false,usb=false; int percent=-1; };
inline void battery(Canvas &c,const Battery &b) {
  c.rect(SYSTEM_X,0,W-SYSTEM_X,SYSTEM_BOTTOM,BG);
  char s[50];
  if(!b.valid) strcpy(s,"电量未知");
  else if(!b.present) strcpy(s,"未接电池");
  else if(b.percent<0) strcpy(s,"--%");
  else snprintf(s,sizeof(s),"%d%%",b.percent);
  uint16_t color=b.charging?GOLD:MUTED;
  int x=444-width(s,font18),icon=x-34;
  c.round(icon,36,24,13,3,color); c.rect(icon+24,40,3,5,color);
  c.rect(icon+3,39,18,7,BG);
  if(b.present&&b.percent>=0) c.rect(icon+3,39,18*b.percent/100,7,color);
  c.text(x,34,s,font18,color);
  const char *state=b.valid&&b.present?(b.charging?"充电中":b.usb?(b.percent==100?"已充满":"外接电源"):""):"";
  c.text(444-width(state,font18),66,state,font18,color);
}
inline void settingsPage(Canvas &c,const interaction::Editor &editor) {
  c.rect(0,0,W,H,BG);
  c.text(36,32,"工资设置",font24,INK);
  c.text(212,33,"取消",font20,MUTED);
  c.text(36,77,editor.field==3?"计薪天数 / 1-31":editor.field?"输入时间 / 24h":"每月收入 / 元",font18,MUTED);
  const char *tabs[]={"月薪","上班","下班","天数"};
  for(int i=0;i<4;i++) {
    int x=28+i*108;
    c.round(x,104,100,34,8,i==editor.field?GOLD:PANEL);
    c.text(x+(100-width(tabs[i],font18))/2,113,tabs[i],font18,i==editor.field?BG:MUTED);
  }
  char input[16];editor.display(input,sizeof(input));
  c.round(28,141,424,39,8,editor.replace?rgb(0x3B3221):PANEL);
  if(!editor.field)c.text(44,147,"¥",font36,GOLD);
  c.text(432-width(input,font36),147,input,font36,GOLD);
  if(editor.error) c.center(402,"输入无效，或保存失败",font18,rgb(0xEE987A));
  else if(editor.field==3)c.center(402,"0 = 当月工作日，可输入小数",font18,MUTED);
  const char *keys[]={"1","2","3","4","5","6","7","8","9",".","0","删除"};
  for(int r=0;r<4;r++) for(int col=0;col<3;col++) {
    int x=28+col*146,y=184+r*56,k=r*3+col;
    c.round(x,y,132,48,10,PANEL);
    const Font &f=k==11?font22:font36;
    c.text(x+(132-width(keys[k],f))/2,y+(k==11?15:10),keys[k],f,INK);
  }
  c.round(28,421,424,42,12,GOLD);
  c.center(433,"保存",font22,BG);
}
inline void render(Canvas &c,const salary::Date &date,bool time_ok,const MoneyAnimation *animation=nullptr,int progress=1000,int64_t monthly=salary::MONTHLY_CENTS,const Battery *power=nullptr,salary::Schedule schedule={}) {
  c.rect(0,0,W,H,BG);
  c.round(204,15,72,4,2,rgb(0x544A36));
  if(!time_ok) {
    if(power) battery(c,*power);
    c.center(110,"工资实时计算器",font24,INK);
    c.center(188,"--.--",font64,GOLD);
    c.center(305,"请连接电脑校时",font24,INK);
    c.center(350,"等待时间恢复",font20,MUTED);
    return;
  }
  auto r=salary::calculate(date,monthly,schedule);
  const char *weeks[]={"周日","周一","周二","周三","周四","周五","周六"};
  const char *states[]={"时间无效","未开始","工作中","已下班","休息日"};
  char s[80],a[32],b[32];
  snprintf(s,sizeof(s),"%d年%d月%d日",date.year,date.month,date.day);
  c.text(36,43,s,font22,INK);
  const char *week=weeks[salary::weekday(date.year,date.month,date.day)];
  c.text(36,78,week,font18,MUTED);
  snprintf(s,sizeof(s),"%02d:%02d:%02d",date.hour,date.minute,date.second);
  c.text(86,78,s,font18,MUTED);
  if(power) battery(c,*power);
  c.round(185,82,7,7,3,GOLD);
  c.text(202,78,states[r.state],font18,r.state==salary::WORKING?GOLD:MUTED);
  c.center(140,"今天已赚",font24,MUTED);
  money(s,sizeof(s),r.earned_cents);
  if(animation) rollingMoney(c,animation->from,animation->to,progress);
  else c.center(189,s,font64,GOLD);
  money(a,sizeof(a),r.daily_cents,false); money(b,sizeof(b),r.hourly_cents,false);
  snprintf(s,sizeof(s),"日薪 %s   时薪 %s",a,b);
  c.center(279,s,font18,MUTED);
  c.round(28,326,424,82,18,PANEL);
  const char *caption=r.state==salary::WEEKEND?"周末休息":r.state==salary::FINISHED?"今日已完成":"距离下班";
  c.text(48,357,caption,font20,MUTED);
  if(r.state==salary::WEEKEND || r.state==salary::FINISHED) c.text(306,356,"下班啦！",font24,INK);
  else {
    snprintf(s,sizeof(s),"%02d:%02d:%02d",r.remaining/3600,r.remaining/60%60,r.remaining%60);
    c.text(432-width(s,font36),350,s,font36,INK);
  }
  c.text(36,434,"认真搬砖，悄悄变富",font18,MUTED);
  snprintf(s,sizeof(s),"%d%%",r.progress/10);
  c.text(444-width(s,font18),434,s,font18,GOLD);
  c.round(36,465,408,5,2,rgb(0x373737));
  int fill=408*r.progress/1000;
  if(fill) c.round(36,465,fill,5,fill>=4?2:0,GOLD);
}
}
