#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/salary_counter/interaction.h"
int main() {
  using namespace interaction;
  Editor e; int64_t n;
  e.begin(10000000); assert(e.amount(n)&&n==10000000);
  for(char c: "65000") if(c) e.key(c);
  assert(e.amount(n)&&n==6500000);
  e.begin(0); for(char c: "12.345") if(c) e.key(c);
  assert(e.amount(n)&&n==1234);
  e.begin(0); for(char c: "9999999.99") if(c) e.key(c);
  assert(e.amount(n)&&n==salary::MAX_MONTHLY_CENTS);
  e.begin(1); e.key('<'); e.key('<'); e.key('<'); e.key('<'); assert(!e.amount(n));
  e.key('.'); e.key('5'); assert(e.amount(n)&&n==50);
  Gesture g; e.begin(10000000);
  assert(g.update(true,240,210,0,false,e)==NONE);
  assert(g.update(true,240,210,799,false,e)==NONE);
  assert(g.update(true,240,210,800,false,e)==OPEN);
  assert(g.update(false,240,210,900,true,e)==NONE);
  assert(g.update(true,90,210,1000,true,e)==NONE);
  assert(g.update(false,90,210,1100,true,e)==EDIT);
  assert(!strcmp(e.value,"1"));
  g.update(true,90,210,1200,true,e); g.update(true,180,300,1250,true,e);
  assert(g.update(false,90,210,1300,true,e)==NONE);
  g.update(true,240,210,1400,false,e); g.cancel();
  assert(g.update(false,240,210,1500,true,e)==NONE);
  g.update(true,240,440,1600,true,e); assert(g.update(false,240,440,1700,true,e)==SAVE);
  salary::Date d{2026,9,8,18,30,0};
  assert(salary::calculate(d,6500000).daily_cents==295455);
  assert(salary::calculate(d,0).earned_cents==0);
  puts("PASS: salary input, bounds, decimals, long press, drag cancellation, save");
  salary::Config cfg;
  e.begin(1234567);e.select(1);
  for(char c:"1000")if(c)e.key(c);
  e.select(2);for(char c:"1800")if(c)e.key(c);
  assert(e.config(cfg)&&cfg.monthly==1234567&&cfg.start==36000&&cfg.end==64800);
  e.select(1);for(char c:"2400")if(c)e.key(c);assert(!e.config(cfg));
  e.select(0);e.select(1);for(char c:"1260")if(c)e.key(c);assert(!e.config(cfg));
  e.select(0);e.select(1);for(char c:"1800")if(c)e.key(c);assert(!e.config(cfg));
  e.select(0);e.select(1);for(char c:"0930")if(c)e.key(c);
  char display[16];e.display(display,sizeof(display));assert(!strcmp(display,"09:30"));
  assert(e.config(cfg));e.key('<');assert(!e.config(cfg));
  e.begin(1234567);g=Gesture{};
  g.update(true,220,120,0,true,e);assert(g.update(false,220,120,100,true,e)==EDIT&&e.field==1);
  puts("PASS: time tabs, drafts, leading zero, range/order and incomplete input validation");
  e.begin(1234567);e.select(3);for(char c:"21.75")if(c)e.key(c);
  assert(e.config(cfg)&&cfg.days100==2175&&cfg.monthly==1234567);
  e.select(1);e.select(3);assert(!strcmp(e.value,"21.75"));
  for(char c:"32")if(c)e.key(c);assert(!e.config(cfg));
  e.select(0);e.select(3);e.key('0');assert(e.config(cfg)&&cfg.days100==0);
  e.select(0);e.select(3);for(char c:"0.5")if(c)e.key(c);assert(!e.config(cfg));
  g=Gesture{};g.update(true,400,120,0,true,e);assert(g.update(false,400,120,100,true,e)==EDIT&&e.field==3);
}
