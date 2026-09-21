#include <cassert>
#include "../firmware/salary_counter/app_host.h"
void covered(const char *s,const Font &font=font24){while(*s)assert(ui::find(font,ui::next(s)));}
int main(){
  for(const auto &entry:apps::registry)covered(entry.title);
  covered(apps::find(apps::MEOW)->detail,font18);
  for(const char *s:quota::phrases)covered(s);
  covered("时钟请连接电脑校时应用蓝牙 BLE Wi-Fi DeskMate-C6月薪设置工资实时计算器今天已赚下班啦！Codex 精力值形象档位等待电脑同步等我回血任务完成输入文字灯牌设置0123456789年月日");
  for(int c=32;c<127;c++)assert(ui::find(font24,c));
}
