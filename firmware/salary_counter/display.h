#pragma once
#include <Arduino.h>
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include "orientation.h"

// Pin map and initialization sequence from Waveshare's 2.16-inch Arduino example.
// https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16
// Panel protocol matches the vendor's esp_lcd_sh8601 driver (Apache-2.0).
namespace panel {
constexpr int TILE_ROWS=32;
constexpr int AMOUNT_TOP=176,AMOUNT_ROWS=80;
static esp_lcd_panel_io_handle_t io=nullptr;
static SemaphoreHandle_t completed=nullptr;
static XPowersPMU pmu;
static int rotation=0;
static uint8_t rotationScratch[orientation::SCRATCH_BYTES];
static uint16_t *rotationBuffer=nullptr;
static bool IRAM_ATTR done(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*) {
  BaseType_t wake=pdFALSE; xSemaphoreGiveFromISR(completed,&wake); return wake==pdTRUE;
}
inline bool command(uint8_t cmd,const void *data=nullptr,size_t size=0) {
  return esp_lcd_panel_io_tx_param(io,0x02000000|(uint32_t(cmd)<<8),data,size)==ESP_OK;
}
inline bool byte(uint8_t cmd,uint8_t data) { return command(cmd,&data,1); }
inline bool begin() {
  rotationBuffer=(uint16_t*)heap_caps_malloc(480*TILE_ROWS*2,MALLOC_CAP_DMA);
  if(!pmu.begin(Wire,0x34,8,7)) return false;
  // Only the display/reset rail: do not copy demo changes to charging or other rails.
  pmu.setALDO3Voltage(3300);
  pmu.enableALDO3(); delay(100); pmu.disableALDO3(); delay(100); pmu.enableALDO3(); delay(100);
  pinMode(6,OUTPUT); digitalWrite(6,HIGH); // Deselect SD card sharing the QSPI bus.
  spi_bus_config_t bus{};
  bus.sclk_io_num=0; bus.data0_io_num=1; bus.data1_io_num=2; bus.data2_io_num=3; bus.data3_io_num=4;
  bus.max_transfer_sz=480*AMOUNT_ROWS*2;
  if(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK) return false;
  completed=xSemaphoreCreateBinary(); if(!completed) return false;
  esp_lcd_panel_io_spi_config_t cfg{};
  cfg.cs_gpio_num=15; cfg.dc_gpio_num=-1; cfg.spi_mode=0; cfg.pclk_hz=40000000;
  cfg.trans_queue_depth=1; cfg.on_color_trans_done=done; cfg.lcd_cmd_bits=32; cfg.lcd_param_bits=8;
  cfg.flags.quad_mode=true;
  if(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&cfg,&io)!=ESP_OK) return false;
  if(!command(0x11)) return false; delay(600);
  if(!byte(0xFE,0x20)||!byte(0x19,0x10)||!byte(0x1C,0xA0)||!byte(0xFE,0x00)||
     !byte(0xC4,0x80)||!byte(0x3A,0x55)||!byte(0x35,0)||!byte(0x36,0x30)||
     !byte(0x53,0x20)||!byte(0x51,0xA0)||!byte(0x63,0xFF)) return false;
  if(!command(0x29)) return false; delay(100);
  return true;
}
inline bool transfer(orientation::Rect r,uint16_t *pixels) {
  int right=r.x+r.w-1,bottom=r.y+r.h-1;
  uint8_t xRange[]={(uint8_t)(r.x>>8),(uint8_t)r.x,(uint8_t)(right>>8),(uint8_t)right};
  uint8_t yRange[]={(uint8_t)(r.y>>8),(uint8_t)r.y,(uint8_t)(bottom>>8),(uint8_t)bottom};
  if(!command(0x2A,xRange,4)||!command(0x2B,yRange,4)) return false;
  // Panel consumes RGB565 MSB-first; buffer cannot be reused until DMA completes.
  for(int i=0;i<r.w*r.h;i++) pixels[i]=(pixels[i]<<8)|(pixels[i]>>8);
  if(esp_lcd_panel_io_tx_color(io,0x32002C00,pixels,r.w*r.h*2)!=ESP_OK) return false;
  return xSemaphoreTake(completed,pdMS_TO_TICKS(2000))==pdTRUE;
}
inline bool tile(int y,uint16_t *pixels,int rows=TILE_ROWS) {
  if(rows<1||rows>AMOUNT_ROWS||y<0||y+rows>480) return false;
  if((rotation&1)&&rotationBuffer) {
    for(int offset=0;offset<rows;offset+=TILE_ROWS) {
      int count=rows-offset<TILE_ROWS?rows-offset:TILE_ROWS;
      orientation::rotateInto(pixels+offset*480,rotationBuffer,count,rotation);
      if(!transfer(orientation::blockRect(y+offset,count,rotation),rotationBuffer)) return false;
    }
    return true;
  }
  orientation::rotateBlock(pixels,rows,rotation,rotationScratch);
  return transfer(orientation::blockRect(y,rows,rotation),pixels);
}
}
