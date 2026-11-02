#!/bin/sh
# Build in the ESP-IDF docker image and export binaries to dist/.
# Usage: ./tools/build.sh   (run from repo root; needs docker)
set -e
cd "$(dirname "$0")/.."
IMG=espressif/idf:v5.4.2
RUN="docker run --rm -v $PWD:/p -w /p --user $(id -u):$(id -g) -e HOME=/tmp $IMG"
$RUN idf.py build
mkdir -p dist
$RUN esptool.py --chip esp32s3 merge_bin -o dist/doorbell-esp32s3-merged.bin \
    --flash_mode dio --flash_size 8MB --flash_freq 80m \
    0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x20000 build/doorbell.bin
cp build/doorbell.bin dist/doorbell-app.bin
(cd dist && sha256sum *.bin > SHA256SUMS && cat SHA256SUMS)
