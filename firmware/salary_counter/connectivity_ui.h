#pragma once
#include "ui.h"
#include "scroll.h"

namespace connectivity {
enum Page { ROOT, WIFI, BLE, PASSWORD };
enum Action { NONE, TOGGLE_WIFI, SCAN, FORGET, TOGGLE_BLE, CONNECT };
struct Network { char ssid[33]{}; int rssi=-100; bool locked=true,supported=true; };
struct Model {
  bool wifi=false,connected=false,connecting=false,scanning=false,ble=false,bleConnected=false;
  int count=0;
  Network networks[12];
  char ssid[33]{},ip[20]{},message[100]{},bleMessage[100]{};
};
struct View {
  Page page=ROOT;
  input::Scroll scroll;
  char selected[33]{},password[64]{};
  bool locked=true,upper=false;
  int keyboard=0;
  bool dirty=false;
  void clearPassword() { memset(password,0,sizeof(password)); }
  void reset() { page=ROOT; clearPassword(); selected[0]=0; scroll.offset=0; scroll.cancel(); dirty=true; }
  void cancelTouch() { scroll.cancel(); }
  const char *keys() const {
    if(keyboard==1) return "1234567890!@#$%^&*()-_=+[]{};:";
    if(keyboard==2) return ",.<>/?\\|`~\"'";
    return upper?"QWERTYUIOPASDFGHJKL ZXCVBNM":"qwertyuiopasdfghjkl zxcvbnm";
  }
  void append(char ch) { size_t n=strlen(password); if(ch>=32&&ch<=126&&n<63) { password[n]=ch; password[n+1]=0; } }
  bool validPassword() const { size_t n=strlen(password); return !locked||(n>=8&&n<=63); }
  Action tap(int x,int y,Model &m) {
    dirty=true;
    if(page!=ROOT&&x>=184&&x<276&&y>=22&&y<68) {
      if(page==PASSWORD) { clearPassword(); page=WIFI; } else page=ROOT;
      scroll.offset=0; scroll.cancel(); return NONE;
    }
    if(page==ROOT) {
      if(y>=130&&y<238) page=WIFI;
      else if(y>=260&&y<368) page=BLE;
      scroll.offset=0; scroll.cancel(); return NONE;
    }
    if(page==BLE) return y>=120&&y<178?TOGGLE_BLE:NONE;
    if(page==WIFI) {
      if(y>=112&&y<164) return TOGGLE_WIFI;
      if(y>=176&&y<218&&m.wifi) return x<244?SCAN:FORGET;
      int index=(y-254+scroll.offset)/60;
      if(y>=254&&y<464&&index>=0&&index<m.count&&m.wifi&&!m.connecting&&!m.scanning) {
        if(!m.networks[index].supported) { strcpy(m.message,"暂不支持此网络认证"); return NONE; }
        strcpy(selected,m.networks[index].ssid); locked=m.networks[index].locked; clearPassword();
        page=PASSWORD; keyboard=0; upper=false; scroll.cancel();
      }
      return NONE;
    }
    if(page==PASSWORD) {
      if(y>=214&&y<248) { keyboard=(keyboard+1)%3; return NONE; }
      if(y>=258&&y<402&&x>=20&&x<460) {
        int row=(y-258)/48,col=(x-20)/44,index=row*10+col;
        const char *s=keys(); if(index<int(strlen(s))&&s[index]!=' ') append(s[index]); return NONE;
      }
      if(y>=418&&y<463) {
        if(x<100) upper=!upper;
        else if(x<206) append(' ');
        else if(x<310) { size_t n=strlen(password); if(n) password[n-1]=0; }
        else if(validPassword()) return CONNECT;
        else strcpy(m.message,"密码需输入至少8位");
      }
    }
    return NONE;
  }
  Action touch(bool pressed,int x,int y,Model &m) {
    scroll.top=0; scroll.bottom=480;
    // Scroll only the network rows; other pages/touch targets still suppress drags.
    scroll.limit(page==WIFI?(m.count*60-210):0);
    bool tapped=scroll.update(pressed,x,y);
    if(page==WIFI&&scroll.y0<254&&scroll.drag) { scroll.offset=scroll.offset0; scroll.changed=false; }
    if(scroll.changed) dirty=true;
    if(tapped) return tap(x,y,m);
    return NONE;
  }
};
inline void fit(ui::Canvas &c,int x,int y,const char *s,int maxWidth,const Font &font,uint16_t color) {
  char out[100]{}; int used=0,n=0;
  while(*s&&n<95) {
    const char *start=s; uint32_t code=ui::next(s); const auto *g=ui::find(font,code);
    int advance=g?g->advance:ui::width("?",font);
    if(used+advance>maxWidth-18) { strcat(out,"..."); break; }
    if(g) { int bytes=s-start; if(n+bytes>=95) break; memcpy(out+n,start,bytes); n+=bytes; }
    else out[n++]='?';
    out[n]=0; used+=advance;
  }
  c.text(x,y,out,font,color);
}
inline void button(ui::Canvas &c,int x,int y,int w,int h,const char *label,bool active=false) {
  c.round(x,y,w,h,10,active?ui::GOLD:ui::PANEL);
  c.text(x+(w-ui::width(label,font18))/2,y+(h-18)/2,label,font18,active?ui::BG:ui::INK);
}
inline void render(ui::Canvas &c,const Model &m,const View &v) {
  c.rect(0,0,480,480,ui::BG);
  const char *title=v.page==ROOT?"设置":v.page==BLE?"蓝牙 BLE":"Wi-Fi";
  c.text(36,36,title,font24,ui::INK);
  if(v.page!=ROOT) c.text(212,36,"返回",font20,ui::MUTED);
  if(v.page==ROOT) {
    button(c,28,130,424,108,"Wi-Fi"); button(c,28,260,424,108,"蓝牙 BLE");
    c.center(421,"网络与连接",font18,ui::MUTED); return;
  }
  if(v.page==BLE) {
    button(c,28,120,424,58,m.ble?"蓝牙：开启":"蓝牙：关闭",m.ble);
    c.text(36,212,"设备名称",font20,ui::MUTED); c.text(36,250,"DeskMate-C6",font24,ui::GOLD);
    c.text(36,307,m.bleConnected?"已连接":m.ble?"可被 BLE 工具发现":"未开启",font20,ui::INK);
    c.text(36,369,"仅支持低功耗蓝牙",font18,ui::MUTED);
    c.text(36,401,"不支持蓝牙耳机音频",font18,ui::MUTED);
    fit(c,36,439,m.bleMessage,408,font18,ui::GOLD); return;
  }
  if(v.page==PASSWORD) {
    fit(c,28,114,v.selected,424,font20,ui::INK);
    c.round(28,151,424,48,10,ui::PANEL);
    char mask[50]; int n=strlen(v.password); snprintf(mask,sizeof(mask),"%s (%d)",n?"********":"",n);
    c.text(44,166,n?mask:v.locked?"输入 Wi-Fi 密码":"开放网络，无需密码",font18,ui::GOLD);
    button(c,28,214,100,34,v.keyboard==0?"abc":v.keyboard==1?"123":"符号");
    fit(c,144,221,m.message,300,font18,ui::GOLD);
    const char *s=v.keys();
    for(int i=0;s[i];i++) if(s[i]!=' ') { char key[]={s[i],0}; button(c,20+i%10*44,258+i/10*48,40,42,key); }
    button(c,20,418,76,44,v.upper?"ABC":"abc"); button(c,102,418,98,44,"空格");
    button(c,206,418,98,44,"删除"); button(c,310,418,150,44,"连接",v.validPassword()); return;
  }
  button(c,28,112,424,52,m.wifi?"Wi-Fi：开启":"Wi-Fi：关闭",m.wifi);
  button(c,28,176,202,42,m.scanning?"扫描中":"扫描网络"); button(c,246,176,206,42,"忘记网络");
  char connected[64]; snprintf(connected,sizeof(connected),"已连接 %s",m.ip);
  const char *state=!m.wifi?"未开启":m.connecting?"连接中":m.connected?connected:m.message;
  fit(c,36,229,state,408,font18,ui::MUTED);
  int oldTop=c.clipTop,oldBottom=c.clipBottom; c.clipTop=254; c.clipBottom=464;
  if(m.wifi) for(int i=0;i<m.count;i++) {
    int y=254+i*60-v.scroll.offset;
    c.round(28,y,424,54,10,ui::PANEL);
    fit(c,44,y+10,m.networks[i].ssid,320,font18,ui::INK);
    char rssi[20]; snprintf(rssi,sizeof(rssi),"%d",m.networks[i].rssi);
    c.text(396,y+10,rssi,font18,ui::MUTED);
    c.text(44,y+33,m.networks[i].supported?(m.networks[i].locked?"需要密码":"开放网络"):"暂不支持认证",font18,ui::MUTED);
  }
  c.clipTop=oldTop; c.clipBottom=oldBottom;
}
}
