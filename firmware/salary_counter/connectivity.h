#pragma once
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include "connectivity_ui.h"
#include "quota_link.h"
// Pinned ESP-IDF 5.5 NimBLE diagnostic: exercise the actual cached retry path.
extern "C" int ble_gap_slave_adv_reattempt(void);

namespace wireless {
static connectivity::Model model;
static connectivity::View view;
static Preferences prefs;
static BLEServer *server=nullptr;
static bool bleStarted=false,wasBleConnected=false,connectingNew=false;
static uint32_t connectStarted=0,scanStarted=0,lastPoll=0;
static uint32_t lastAdvertiseAttempt=0;
static BLEAdvertisementData advertisingPayload,scanPayload;
// ESP-IDF NimBLE's connection-reattempt cache shallow-copies these pointers.
// Arduino 3.3.0's default start() frees its UUID array after set_fields().
// Keep the underlying UUID alive for the entire firmware lifetime instead.
static ble_uuid128_t advertisingService;
static ble_hs_adv_fields advertisingFields{};
static String pendingSsid,pendingPass;
struct Credentials { char ssid[33]; char pass[64]; };
inline void message(const char *s) { snprintf(model.message,sizeof(model.message),"%s",s); view.dirty=true; }
inline void bleMessage(const char *s) { snprintf(model.bleMessage,sizeof(model.bleMessage),"%s",s); view.dirty=true; }
inline void erasePending() { pendingPass=""; pendingSsid=""; connectingNew=false; }
inline void wifiOn(bool on) {
  model.wifi=on; prefs.putBool("wifi",on);
  if(on) { WiFi.mode(WIFI_STA); message("点击扫描网络"); }
  else {
    WiFi.scanDelete(); WiFi.disconnect(true,false); WiFi.mode(WIFI_OFF);
    model.scanning=model.connecting=model.connected=false; model.count=0; erasePending();
    message("未开启");
  }
}
inline void scan() {
  if(!model.wifi||model.scanning||model.connecting) return;
  WiFi.scanDelete(); model.count=0; view.scroll.offset=0;
  int result=WiFi.scanNetworks(true,false,false,180);
  model.scanning=result==WIFI_SCAN_RUNNING; scanStarted=millis();
  message(model.scanning?"扫描中":"扫描失败，请重试");
}
inline void connect(const char *ssid,const char *password,bool save) {
  if(!model.wifi||!ssid[0]) return;
  WiFi.scanDelete(); model.scanning=false; WiFi.disconnect(false,false);
  pendingSsid=ssid; pendingPass=password; connectingNew=save;
  model.connecting=true; model.connected=false; connectStarted=millis();
  WiFi.begin(pendingSsid.c_str(),pendingPass.c_str()); message("连接中");
}
inline bool startAdvertising() {
  auto *adv=BLEDevice::getAdvertising();
  if(adv->isAdvertising())return true;
  lastAdvertiseAttempt=millis();
  // Encode UUID bytes once in owned payloads. Reinstall on every restart,
  // bypassing the library's dynamically built/cached service-field path.
  return adv->setAdvertisementData(advertisingPayload)&&
    ble_gap_adv_set_fields(&advertisingFields)==0&&
    adv->setScanResponseData(scanPayload)&&adv->start();
}
inline int retryAdvertising() {
  if(!bleStarted||!model.ble||server->getConnectedCount()||!BLEDevice::getAdvertising()->isAdvertising())return -1;
  return ble_gap_slave_adv_reattempt();
}
inline void bluetoothOn(bool on) {
  if(on&&!bleStarted) {
    // Initialize once. Logical OFF stops advertising and disconnects peers; stack retained.
    BLEDevice::init("DeskMate-C6");
    if(!BLEDevice::getInitialized()) { bleMessage("蓝牙启动失败"); return; }
    server=BLEDevice::createServer();
    auto *service=server->createService("180A");
    service->createCharacteristic("2A29",BLECharacteristic::PROPERTY_READ)->setValue("DeskMate");
    service->createCharacteristic("2A24",BLECharacteristic::PROPERTY_READ)->setValue("ESP32-C6");
    service->start();
    quotaLink::service(server);
    advertisingPayload.setFlags(0x06);
    advertisingPayload.setCompleteServices(BLEUUID(quotaLink::SERVICE));
    advertisingService=BLEUUID(quotaLink::SERVICE).getNative()->u128;
    advertisingFields.flags=0x06;
    advertisingFields.uuids128=&advertisingService;
    advertisingFields.num_uuids128=1;
    advertisingFields.uuids128_is_complete=1;
    scanPayload.setName("DeskMate-C6");
    BLEDevice::getAdvertising()->setScanResponse(true); bleStarted=true;
  }
  if(on) {
    if(!server->getConnectedCount()&&!startAdvertising()) { bleMessage("蓝牙广播失败"); return; }
  } else if(bleStarted) {
    BLEDevice::getAdvertising()->stop();
    if(server->getConnectedCount()) server->disconnect(server->getConnId());
  }
  model.ble=on; prefs.putBool("ble",on); bleMessage(on?"蓝牙已开启":"蓝牙已关闭");
}
inline void act(connectivity::Action action) {
  using namespace connectivity;
  if(action==TOGGLE_WIFI) wifiOn(!model.wifi);
  else if(action==SCAN) scan();
  else if(action==TOGGLE_BLE) bluetoothOn(!model.ble);
  else if(action==FORGET) {
    WiFi.disconnect(false,true); prefs.remove("network");
    model.connecting=model.connected=false; erasePending(); message("已忘记网络");
  } else if(action==CONNECT&&view.validPassword()) {
    connect(view.selected,view.locked?view.password:"",true); view.clearPassword(); view.page=WIFI; view.scroll.offset=0; view.cancelTouch();
  }
}
inline void begin() {
  prefs.begin("desk-net",false); WiFi.persistent(false); WiFi.setAutoReconnect(false);
  bool wifi=prefs.getBool("wifi",false),ble=prefs.getBool("ble",false);
  if(wifi) {
    wifiOn(true); Credentials saved{};
    if(prefs.getBytesLength("network")==sizeof(saved)&&prefs.getBytes("network",&saved,sizeof(saved))==sizeof(saved)) {
      saved.ssid[32]=0; saved.pass[63]=0;
      if(saved.ssid[0]) connect(saved.ssid,saved.pass,false);
    }
    memset(&saved,0,sizeof(saved));
  }
  if(ble) bluetoothOn(true);
}
inline bool poll() {
  uint32_t now=millis(); if(uint32_t(now-lastPoll)<250) return false; lastPoll=now;
  if(model.scanning) {
    int count=WiFi.scanComplete();
    if(count>=0) {
      model.count=0;
      for(int i=0;i<count&&model.count<12;i++) {
        String ssid=WiFi.SSID(i); if(!ssid.length()) continue;
        bool duplicate=false; for(int j=0;j<model.count;j++) if(ssid==model.networks[j].ssid) duplicate=true;
        if(duplicate) continue;
        auto &n=model.networks[model.count++]; snprintf(n.ssid,sizeof(n.ssid),"%s",ssid.c_str());
        n.rssi=WiFi.RSSI(i); auto auth=WiFi.encryptionType(i);
        n.locked=auth!=WIFI_AUTH_OPEN;
        n.supported=auth==WIFI_AUTH_OPEN||auth==WIFI_AUTH_WPA_PSK||auth==WIFI_AUTH_WPA2_PSK||auth==WIFI_AUTH_WPA_WPA2_PSK||auth==WIFI_AUTH_WPA3_PSK||auth==WIFI_AUTH_WPA2_WPA3_PSK;
      }
      WiFi.scanDelete(); model.scanning=false; message(model.count?"选择网络连接":"未发现网络，请重试");
    } else if(count==WIFI_SCAN_FAILED||uint32_t(now-scanStarted)>15000) {
      WiFi.scanDelete(); model.scanning=false; message("扫描失败，请重试");
    }
  }
  if(model.wifi) {
    bool connected=WiFi.status()==WL_CONNECTED;
    if(connected!=model.connected) {
      model.connected=connected;
      if(connected) {
        snprintf(model.ssid,sizeof(model.ssid),"%s",WiFi.SSID().c_str());
        snprintf(model.ip,sizeof(model.ip),"%s",WiFi.localIP().toString().c_str());
        if(connectingNew) {
          Credentials credentials{};
          pendingSsid.toCharArray(credentials.ssid,sizeof(credentials.ssid)); pendingPass.toCharArray(credentials.pass,sizeof(credentials.pass));
          bool saved=prefs.putBytes("network",&credentials,sizeof(credentials))==sizeof(credentials);
          memset(&credentials,0,sizeof(credentials));
          message(saved?"已连接并保存":"已连接，保存失败");
        } else message("已连接");
        model.connecting=false; erasePending();
      } else { model.ip[0]=0; message("连接已断开"); }
    }
    if(model.connecting&&uint32_t(now-connectStarted)>20000) {
      WiFi.disconnect(false,false); model.connecting=false; erasePending(); message("连接失败，请检查密码");
    }
  }
  if(bleStarted) {
    bool connected=server->getConnectedCount()>0;
    if(!model.ble&&connected) server->disconnect(server->getConnId());
    if(model.ble&&!connected&&!BLEDevice::getAdvertising()->isAdvertising()&&
       (wasBleConnected||uint32_t(now-lastAdvertiseAttempt)>=2000))startAdvertising();
    if(connected!=model.bleConnected) { model.bleConnected=connected; view.dirty=true; }
    wasBleConnected=connected;
  }
  bool changed=view.dirty; view.dirty=false; return changed;
}
inline void status() {
  Serial.printf("NET wifi=%d connected=%d connecting=%d scanning=%d networks=%d ble=%d ble_connected=%d advertising=%d\n",model.wifi,model.connected,model.connecting,model.scanning,model.count,model.ble,model.bleConnected,bleStarted&&BLEDevice::getAdvertising()->isAdvertising());
}
}
