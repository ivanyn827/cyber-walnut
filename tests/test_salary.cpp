#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "../firmware/salary_counter/salary.h"
using namespace salary;
int main() {
  auto fixed=calculate({2026,9,8,18,30,0},10000000,{START,END,2175});
  assert(fixed.daily_cents==459770&&fixed.earned_cents==459770);
  assert(calculate({2026,9,8,14,0,0},10000000,{START,END,2000}).earned_cents==250000);
  assert(!Schedule({START,END,99}).valid()&&!Schedule({START,END,3101}).valid());
  assert(calculate({2026,9,12,14,0,0},10000000,{START,END,2000}).state==WEEKEND);
  Schedule shortDay{10*3600,18*3600};
  auto custom=calculate({2026,9,8,14,0,0},10000000,shortDay);
  assert(custom.progress==500&&custom.remaining==14400&&custom.hourly_cents==56818);
  assert(calculate({2026,9,8,9,59,59},10000000,shortDay).state==BEFORE);
  assert(calculate({2026,9,8,18,0,0},10000000,shortDay).state==FINISHED);
  assert(calculate({2026,9,8,10,0,0},10000000,{36000,36000}).state==INVALID);
  assert(!Schedule({23*3600,9*3600}).valid());
  assert(!Schedule({0,86400}).valid());
  assert(calculate({2026,9,8,0,1,0},MAX_MONTHLY_CENTS,{0,60}).progress==1000);
  assert(workdays(2026,9)==22);
  assert(workdays(2026,8)==21);
  assert(workdays(2024,2)==21);
  assert(workdays(2025,2)==20);
  assert(leap(2000)&&!leap(2100));
  assert(weekday(2026,9,8)==2);
  assert(!valid({2026,2,29,0,0,0}));
  assert(calculate({2026,0,1,0,0,0}).state==INVALID);
  auto a=calculate({2026,9,8,9,29,59});
  assert(a.state==BEFORE&&a.earned_cents==0&&a.remaining==32401);
  a=calculate({2026,9,8,9,30,0});
  assert(a.state==WORKING&&a.elapsed==0&&a.remaining==32400);
  a=calculate({2026,9,8,14,0,0});
  assert(a.progress==500&&a.earned_cents==227273&&a.daily_cents==454545&&a.hourly_cents==50505);
  auto lunch1=calculate({2026,9,8,12,0,0}),lunch2=calculate({2026,9,8,13,0,0});
  assert(lunch2.elapsed-lunch1.elapsed==3600&&lunch2.earned_cents>lunch1.earned_cents);
  a=calculate({2026,9,8,18,30,0});
  assert(a.state==FINISHED&&a.earned_cents==454545&&a.remaining==0&&a.progress==1000);
  assert(calculate({2026,9,8,23,59,59}).earned_cents==a.earned_cents);
  assert(calculate({2026,9,9,0,0,0}).earned_cents==0);
  for(int d: {12,13}) { auto r=calculate({2026,9,d,14,0,0}); assert(r.state==WEEKEND&&r.earned_cents==0&&r.progress==0); }
  int64_t prev=-1;
  for(int s=0;s<86400;s++) {
    auto r=calculate({2026,9,8,s/3600,s/60%60,s%60});
    assert(r.earned_cents>=prev&&r.earned_cents<=r.daily_cents);
    assert(r.progress>=0&&r.progress<=1000&&r.remaining>=0);
    prev=r.earned_cents;
  }
  puts("PASS: calendar, invalid dates, start/end, paid lunch, weekend, midnight, full-day monotonicity (86400 seconds)");
}
