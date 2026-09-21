#pragma once
#include <stdint.h>
namespace ui {
struct MoneyAnimation {
  int64_t from=0,to=0;
  uint32_t started=0;
  static constexpr uint32_t DURATION=900;
  void set(int64_t value,uint32_t now,bool animate) {
    if(value==to) return;
    from=animate?to:value; to=value; started=now;
  }
  bool active(uint32_t now) const { return from!=to && uint32_t(now-started)<DURATION; }
  int progress(uint32_t now) const {
    uint32_t elapsed=uint32_t(now-started);
    if(from==to||elapsed>=DURATION) return 1000;
    int64_t t=int64_t(elapsed)*1000/DURATION;
    return t*t*(3000-2*t)/1000000; // smoothstep: zero speed at both endpoints
  }
};
}
