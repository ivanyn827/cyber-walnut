#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tools/arduino-cli compile \
  --fqbn 'esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=huge_app' \
  --library "$PWD/vendor/waveshare-216/01_Arduino_Libraries/XPowersLib" \
  --build-path "$PWD/build" \
  "$PWD/firmware/salary_counter"

cp "${HOME}/Library/Arduino15/packages/esp32/hardware/esp32/3.3.0/tools/partitions/boot_app0.bin" build/boot_app0.bin
