#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <math.h>
// Fixed 16kHz DAC-only test. Register sequence follows Espressif's ES8311
// driver in Waveshare's official demo; pins from the 2.16 board config.
// No RX channel, ES7210 initialization, microphone capture, or PMU writes.
namespace audioTest {
inline bool reg(uint8_t r,uint8_t v){Wire.beginTransmission(0x18);Wire.write(r);Wire.write(v);return Wire.endTransmission()==0;}
inline bool play(){
  Wire.beginTransmission(0x18);if(Wire.endTransmission()!=0){Serial.println("AUDIO_NO_CODEC");return false;}
  i2s_chan_handle_t tx=nullptr;
  i2s_chan_config_t cfg=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
  cfg.dma_desc_num=4;cfg.dma_frame_num=128;
  esp_err_t error=i2s_new_channel(&cfg,&tx,nullptr);
  if(error!=ESP_OK){Serial.printf("AUDIO_I2S_ERROR %d\n",error);return false;}
  i2s_std_config_t s{};
  s.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000);
  s.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
  s.gpio_cfg.mclk=GPIO_NUM_19;s.gpio_cfg.bclk=GPIO_NUM_20;s.gpio_cfg.ws=GPIO_NUM_22;
  s.gpio_cfg.dout=GPIO_NUM_23;s.gpio_cfg.din=I2S_GPIO_UNUSED;
  bool ok=i2s_channel_init_std_mode(tx,&s)==ESP_OK;
  if(ok)ok=i2s_channel_enable(tx)==ESP_OK;
  const uint8_t init[][2]={
    {0x44,0x08},{0x44,0x08},{0x01,0x30},{0x02,0},{0x03,0x10},
    {0x04,0x20},{0x05,0},{0x0B,0},{0x0C,0},{0x10,0x1F},{0x11,0x7F},
    {0x00,0x80},{0x01,0x3F},{0x06,3},{0x07,0},{0x08,0xFF},
    {0x09,0x0C},{0x0A,0x4C},{0x13,0x10},{0x44,0x08},
    {0x0E,0x02},{0x12,0},{0x14,0x1A},{0x0D,0x01},{0x37,0x08},
    {0x45,0},{0x31,0x60},{0x32,0xB8}
  };
  for(auto &p:init)if(ok)ok=reg(p[0],p[1]);
  int16_t pcm[256]{};size_t written=0;
  if(ok)ok=i2s_channel_write(tx,pcm,sizeof(pcm),&written,100)==ESP_OK;
  delay(30);if(ok)ok=reg(0x31,0);
  size_t bytes=0;
  const float notes[]={659.25f,783.99f}; // Clean, short ascending pair; no ringing tail.
  constexpr int noteSamples=1600,gapSamples=960,slotSamples=noteSamples+gapSamples;
  for(int start=0;ok&&start<2*slotSamples;start+=128){
    for(int i=0;i<128;i++){
      int note=(start+i)/slotSamples,n=(start+i)%slotSamples;int16_t value=0;
      if(note<2&&n<noteSamples){
        float envelope=fmaxf(0,fminf(1.f,fminf(n/80.f,(noteSamples-1-n)/80.f)));
        float phase=6.2831853f*notes[note]*n/16000;
        value=int16_t(3000*envelope*sinf(phase));
      }
      pcm[2*i]=pcm[2*i+1]=value;
    }
    ok=i2s_channel_write(tx,pcm,sizeof(pcm),&written,100)==ESP_OK&&written==sizeof(pcm);bytes+=written;
  }
  memset(pcm,0,sizeof(pcm));if(ok)i2s_channel_write(tx,pcm,sizeof(pcm),&written,100);
  delay(50);reg(0x31,0x60);reg(0x0E,0xFF);reg(0x12,0x02);reg(0x0D,0xFA);
  i2s_channel_disable(tx);i2s_del_channel(tx);
  Serial.printf("AUDIO_TEST_%s bytes=%u\n",ok?"OK":"ERROR",unsigned(bytes));return ok;
}
}
