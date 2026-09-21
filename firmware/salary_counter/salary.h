#pragma once
#include <stdint.h>

namespace salary {
constexpr int64_t MONTHLY_CENTS = 10000000;
constexpr int64_t MAX_MONTHLY_CENTS = 999999999;
constexpr int START = 9 * 3600 + 30 * 60;
constexpr int END = 18 * 3600 + 30 * 60;
constexpr int DURATION = END - START;
struct Schedule {
  int start=START, end=END, days100=0; // 0: calendar weekdays, otherwise fixed hundredths of a day
  bool valid() const { return start>=0&&end<86400&&end>start&&start%60==0&&end%60==0&&(days100==0||(days100>=100&&days100<=3100)); }
};
struct ConfigV1 {
  int64_t monthly=MONTHLY_CENTS;
  int32_t start=START,end=END;
  bool valid() const { return monthly>=0&&monthly<=MAX_MONTHLY_CENTS&&Schedule{start,end}.valid(); }
};
struct Config : ConfigV1 {
  int32_t days100=0;
  int32_t reserved=0;
  bool valid() const { return ConfigV1::valid()&&Schedule{start,end,days100}.valid(); }
};
struct Date { int year, month, day, hour, minute, second; };
inline bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
inline int days(int y, int m) {
  const int n[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return m >= 1 && m <= 12 ? n[m-1] + (m == 2 && leap(y)) : 0;
}
inline bool valid(const Date &d) {
  return d.year >= 2024 && d.year <= 2099 && d.month >= 1 && d.month <= 12 &&
    d.day >= 1 && d.day <= days(d.year,d.month) && d.hour >= 0 && d.hour < 24 &&
    d.minute >= 0 && d.minute < 60 && d.second >= 0 && d.second < 60;
}
// Gregorian calendar, Sunday = 0. Independent of host timezone and DST.
inline int weekday(int y, int m, int d) {
  const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  y -= m < 3;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}
inline int workdays(int y, int m) {
  int count = 0;
  for (int d=1; d<=days(y,m); ++d) { int w=weekday(y,m,d); count += w>0 && w<6; }
  return count;
}
enum State { INVALID, BEFORE, WORKING, FINISHED, WEEKEND };
struct Result {
  State state = INVALID;
  int work_days = 0, elapsed = 0, remaining = 0, progress = 0;
  int64_t earned_cents = 0, daily_cents = 0, hourly_cents = 0;
};
inline Result calculate(const Date &d,int64_t monthly_cents=MONTHLY_CENTS,Schedule schedule={}) {
  Result r;
  if (!valid(d)||monthly_cents<0||monthly_cents>MAX_MONTHLY_CENTS||!schedule.valid()) return r;
  const int duration=schedule.end-schedule.start;
  r.work_days = workdays(d.year,d.month);
  const int divisor=schedule.days100?schedule.days100:r.work_days*100;
  r.daily_cents = (monthly_cents*100 + divisor/2) / divisor;
  const int64_t denominator=int64_t(divisor)*duration;
  r.hourly_cents = (monthly_cents*360000 + denominator/2) / denominator;
  int w = weekday(d.year,d.month,d.day);
  if (w==0 || w==6) { r.state=WEEKEND; return r; }
  int seconds = d.hour*3600 + d.minute*60 + d.second;
  r.state = seconds < schedule.start ? BEFORE : seconds < schedule.end ? WORKING : FINISHED;
  r.elapsed = seconds <= schedule.start ? 0 : seconds >= schedule.end ? duration : seconds-schedule.start;
  r.remaining = seconds < schedule.end ? schedule.end-seconds : 0;
  r.progress = r.elapsed * 1000 / duration;
  // Round only at display precision; never accumulate timer ticks or rounded hourly pay.
  r.earned_cents = (monthly_cents*r.elapsed*100 + denominator/2) / denominator;
  return r;
}
}
