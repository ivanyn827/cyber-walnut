#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p tools vendor
if [ ! -x tools/arduino-cli ]; then
  echo '请下载 macOS ARM64 Arduino CLI，将 arduino-cli 放入 tools/。'
  echo 'https://arduino.github.io/arduino-cli/latest/installation/'
  exit 1
fi
tools/arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
tools/arduino-cli core install esp32:esp32@3.3.0 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
if [ ! -d vendor/waveshare-216 ]; then
  git clone --filter=blob:none --sparse https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16.git vendor/waveshare-216
  git -C vendor/waveshare-216 checkout --detach 294543798f1a44e2f2c4d2976522323f2beee11d
  git -C vendor/waveshare-216 sparse-checkout set 01_Arduino_Libraries/XPowersLib 02_Example/Arduino-v3.3.3/08_LVGL_V8_Test 02_Example/Arduino-v3.3.3/03_I2C_PCF85063
fi
python3 -m venv .venv
.venv/bin/pip install esptool==4.12.0 pyserial==3.5 Pillow==11.3.0
