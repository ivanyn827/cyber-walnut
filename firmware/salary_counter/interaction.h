#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "salary.h"

namespace interaction {
enum Action { NONE, OPEN, EDIT, SAVE, CANCEL };
struct Editor {
  char value[12]{};
  bool replace=true,error=false;
  int field=0;
  char drafts[4][12]{};
  void begin(int64_t cents,salary::Schedule schedule={}) {
    if(cents%100) snprintf(value,sizeof(value),"%lld.%02lld",(long long)(cents/100),(long long)(cents%100));
    else snprintf(value,sizeof(value),"%lld",(long long)(cents/100));
    strcpy(drafts[0],value);
    snprintf(drafts[1],12,"%02d%02d",schedule.start/3600,schedule.start/60%60);
    snprintf(drafts[2],12,"%02d%02d",schedule.end/3600,schedule.end/60%60);
    snprintf(drafts[3],12,"%d.%02d",schedule.days100/100,schedule.days100%100);
    field=0; replace=true; error=false;
  }
  void select(int next) {
    if(next<0||next>3||next==field) return;
    strcpy(drafts[field],value); field=next; strcpy(value,drafts[field]);
    replace=true; error=false;
  }
  bool config(salary::Config &out) const {
    Editor amountEditor=*this;
    strcpy(amountEditor.value,field==0?value:drafts[0]);
    if(!amountEditor.amount(out.monthly)) return false;
    int times[2];
    for(int i=1;i<=2;i++) {
      const char *s=field==i?value:drafts[i];
      if(strlen(s)!=4) return false;
      for(int j=0;j<4;j++) if(s[j]<'0'||s[j]>'9') return false;
      int h=(s[0]-'0')*10+s[1]-'0',m=(s[2]-'0')*10+s[3]-'0';
      if(h>23||m>59) return false;
      times[i-1]=h*3600+m*60;
    }
    strcpy(amountEditor.value,field==3?value:drafts[3]);
    int64_t days;
    if(!amountEditor.amount(days)||days>3100||(days!=0&&days<100))return false;
    out.days100=days;out.reserved=0;
    out.start=times[0];out.end=times[1];return out.valid();
  }
  void display(char *out,size_t size) const {
    if(field==0||field==3) { snprintf(out,size,"%s",value);return; }
    size_t n=strlen(value);
    snprintf(out,size,"%c%c:%c%c",n>0?value[0]:'-',n>1?value[1]:'-',n>2?value[2]:'-',n>3?value[3]:'-');
  }
  bool amount(int64_t &cents) const {
    int64_t whole=0,fraction=0; int decimals=-1,digits=0;
    for(const char *p=value;*p;p++) {
      if(*p=='.') { if(decimals>=0) return false; decimals=0; }
      else if(*p>='0'&&*p<='9') {
        ++digits;
        if(decimals<0) whole=whole*10+*p-'0';
        else { if(++decimals>2) return false; fraction=fraction*10+*p-'0'; }
      } else return false;
    }
    cents=whole*100+fraction*(decimals==1?10:1);
    return digits>0&&cents>=0&&cents<=salary::MAX_MONTHLY_CENTS;
  }
  void key(char k) {
    error=false;
    if(k=='<') { size_t n=strlen(value); if(n) value[n-1]=0; replace=false; return; }
    if((k<'0'||k>'9')&&k!='.') return;
    if(field==1||field==2) {
      if(k=='.') return;
      if(replace) { value[0]=0;replace=false; }
      size_t n=strlen(value);
      if(n<4) {value[n]=k;value[n+1]=0;}
      return;
    }
    if(replace) { value[0]=0; replace=false; }
    size_t n=strlen(value); const char *point=strchr(value,'.');
    if(k=='.') {
      if(point) return;
      if(!n) { strcpy(value,"0"); n=1; }
    } else {
      if(point&&n-size_t(point-value)>2) return;
      if(!point&&n>=7) { error=true; return; }
      if(!point&&n==1&&value[0]=='0') n=0;
    }
    if(n<sizeof(value)-1) { value[n]=k; value[n+1]=0; }
  }
};
inline int hit(int x,int y) {
  if(y>=104&&y<138) for(int i=0;i<4;i++) if(x>=28+i*108&&x<128+i*108) return 14+i;
  if(x>=192&&x<272&&y>=20&&y<66) return 13;
  if(x>=28&&x<452&&y>=421&&y<463) return 12;
  for(int r=0;r<4;r++) for(int c=0;c<3;c++) {
    int left=28+c*146,top=184+r*56;
    if(x>=left&&x<left+132&&y>=top&&y<top+48) return r*3+c;
  }
  return -1;
}
struct Gesture {
  bool down=false,blocked=false,fired=false;
  int startX=0,startY=0,startKey=-1;
  uint32_t started=0;
  void cancel() { down=false; blocked=true; fired=false; }
  Action update(bool pressed,int x,int y,uint32_t now,bool editing,Editor &editor) {
    if(!pressed) {
      Action result=NONE;
      if(down&&!blocked&&!fired&&editing&&hit(x,y)==startKey&&startKey>=0) {
        if(startKey==12) result=SAVE;
        else if(startKey==13) result=CANCEL;
        else if(startKey>=14) { editor.select(startKey-14); result=EDIT; }
        else { const char *keys="123456789.0<"; editor.key(keys[startKey]); result=EDIT; }
      }
      down=false; blocked=false; fired=false; return result;
    }
    if(blocked&&!down) return NONE;
    if(!down) {
      down=true; started=now; startX=x; startY=y; startKey=hit(x,y); fired=false;
      blocked=!editing&&!(x>=24&&x<456&&y>=128&&y<264);
    }
    int dx=x-startX,dy=y-startY;
    if(dx*dx+dy*dy>24*24) blocked=true;
    if(!editing&&!blocked&&!fired&&uint32_t(now-started)>=800) { fired=true; return OPEN; }
    return NONE;
  }
};
}
