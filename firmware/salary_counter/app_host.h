#pragma once
#include "ui.h"
#include "connectivity_ui.h"
#include "emotions.h"
#include "lightboard.h"
#include "quota.h"
#include "meow.h"

// Apps share device services and one drawing buffer; only the foreground app renders.
namespace apps {
enum Id { HOME, SALARY, CLOCK, SETTINGS, EMOTIONS, LIGHTBOARD, QUOTA, MEOW };
struct Context {
  const salary::Date &date;
  bool clockOK;
  int64_t monthly;
  const ui::Battery &battery;
  const ui::MoneyAnimation &animation;
  int progress;
  int homeOffset=0;
  const connectivity::Model *network=nullptr;
  const connectivity::View *networkView=nullptr;
  const emotions::View *emotion=nullptr;
  uint32_t frameMs=0;
  const lightboard::View *light=nullptr;
  const quota::View *quotaView=nullptr;
  uint32_t meowMs=0;
  int meowVariant=1;
  salary::Schedule schedule{};
};
inline void salaryPage(ui::Canvas &c,const Context &ctx) {
  ui::render(c,ctx.date,ctx.clockOK,&ctx.animation,ctx.progress,ctx.monthly,nullptr,ctx.schedule);
}
inline void clockPage(ui::Canvas &c,const Context &ctx) {
  c.rect(0,0,480,480,ui::BG);
  c.text(36,36,"时钟",font24,ui::INK);
  if(!ctx.clockOK) { c.center(208,"请连接电脑校时",font24,ui::GOLD); return; }
  char s[64];
  snprintf(s,sizeof(s),"%d年%d月%d日",ctx.date.year,ctx.date.month,ctx.date.day);
  c.center(150,s,font24,ui::MUTED);
  snprintf(s,sizeof(s),"%02d:%02d",ctx.date.hour,ctx.date.minute);
  c.center(204,s,font64,ui::GOLD);
  snprintf(s,sizeof(s),"%02d",ctx.date.second);
  c.center(296,s,font36,ui::MUTED);
}
struct Descriptor { Id id; const char *key,*title,*detail; void (*render)(ui::Canvas&,const Context&); };
inline void settingsPage(ui::Canvas &c,const Context &ctx) {
  static const connectivity::Model empty;
  static const connectivity::View view;
  connectivity::render(c,ctx.network?*ctx.network:empty,ctx.networkView?*ctx.networkView:view);
}
static const Descriptor registry[]={
  {SALARY,"salary","工资计数器","今天已赚与下班倒计时",salaryPage},
  {CLOCK,"clock","时钟","日期与时间",clockPage},
  {SETTINGS,"settings","设置","Wi-Fi 与蓝牙",settingsPage},
  {EMOTIONS,"emotions","灵动表情","选择心情，让眼睛说话",[](ui::Canvas &c,const Context &ctx) {
    static const emotions::View empty; emotions::render(c,ctx.emotion?*ctx.emotion:empty,ctx.frameMs);
  }},
  {LIGHTBOARD,"lightboard","灯牌","彩色文字跑马灯",[](ui::Canvas &c,const Context &ctx) {
    static const lightboard::View empty; lightboard::render(c,ctx.light?*ctx.light:empty);
  }},
  {QUOTA,"quota","Codex 精力值","额度与小精灵",[](ui::Canvas &c,const Context &ctx) {
    static const quota::View empty; quota::render(c,ctx.quotaView?*ctx.quotaView:empty,ctx.frameMs);
  }},
  {MEOW,"meow","喵喵","可爱的桌面喵喵",[](ui::Canvas &c,const Context &ctx) { meow::render(c,ctx.meowMs,ctx.meowVariant); }}
};
constexpr int COUNT=sizeof(registry)/sizeof(registry[0]);
inline const Descriptor *find(Id id) {
  for(const auto &app:registry) if(app.id==id) return &app;
  return nullptr;
}
// Keep Settings last even when new applications are appended to the registry.
inline const Descriptor &menuEntry(int index) {
  if(index==COUNT-1) return *find(SETTINGS);
  for(const auto &app:registry) if(app.id!=SETTINGS&&index--==0) return app;
  return *find(SETTINGS);
}
inline void renderContent(ui::Canvas &c,Id id,const Context &ctx) {
  if(const auto *app=find(id)) {
    app->render(c,ctx);
    return;
  }
  c.rect(0,0,480,480,ui::BG);
  c.text(36,36,"应用",font24,ui::INK);
  c.text(36,78,"桌面助手",font18,ui::MUTED);
  int oldTop=c.clipTop,oldBottom=c.clipBottom; c.clipTop=112; c.clipBottom=464;
  for(int i=0;i<COUNT;i++) {
    int y=122+i*130-ctx.homeOffset;
    c.round(28,y,424,112,18,ui::PANEL);
    c.text(52,y+23,menuEntry(i).title,font24,ui::GOLD);
    c.text(52,y+67,menuEntry(i).detail,font18,ui::MUTED);
  }
  c.clipTop=oldTop; c.clipBottom=oldBottom;
  c.round(465,116,3,344,1,ui::PANEL);
  int thumb=344*352/(COUNT*130+10);
  int range=COUNT*130+10-352;
  c.round(465,116+(range>0?(344-thumb)*ctx.homeOffset/range:0),3,thumb,1,ui::MUTED);
}
inline void render(ui::Canvas &c,Id id,const Context &ctx) {
  bool old=c.protectSystem; c.protectSystem=true;
  renderContent(c,id,ctx);
  c.protectSystem=false; ui::battery(c,ctx.battery); c.protectSystem=old;
}
inline void renderSettings(ui::Canvas &c,const interaction::Editor &editor,const ui::Battery &battery) {
  bool old=c.protectSystem; c.protectSystem=true;
  ui::settingsPage(c,editor);
  c.protectSystem=false; ui::battery(c,battery); c.protectSystem=old;
}
// Tap activates on release; movement or an interrupted report cancels it.
struct Navigation {
  input::Scroll scroll;
  bool changed=false;
  void cancel() { scroll.cancel(); }
  int offset() const { return scroll.offset; }
  int hit(Id id,int x,int y) const {
    if(id!=HOME) return -1;
    if(y<112||y>=464) return -1;
    for(int i=0;i<COUNT;i++) if(x>=28&&x<452&&y>=122+i*130-scroll.offset&&y<234+i*130-scroll.offset) return menuEntry(i).id;
    return -1;
  }
  int update(Id id,bool pressed,int x,int y) {
    changed=false;
    if(id!=HOME) { if(!pressed) { scroll.down=false; scroll.blocked=false; scroll.drag=false; } return -1; }
    scroll.limit(COUNT*130+10-352);
    bool tapped=scroll.update(pressed,x,y); changed=scroll.changed;
    return tapped&&hit(id,x,y)==hit(id,scroll.x0,scroll.y0)?hit(id,x,y):-1;
  }
};
}
