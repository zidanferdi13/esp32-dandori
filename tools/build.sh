#!/usr/bin/env bash
# Compile (and optionally upload) firmware/dandori with the arduino-cli bundled in Arduino IDE.
# Usage: tools/build.sh [--upload COMx] [--define NAME]...
set -euo pipefail
cd "$(dirname "$0")/.."

CLI="$(cygpath -u "$LOCALAPPDATA")/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
export ARDUINO_DIRECTORIES_USER="$USERPROFILE/OneDrive/Documents/Arduino"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc"

UPLOAD_PORT=""
EXTRA_FLAGS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --upload) UPLOAD_PORT="$2"; shift 2 ;;
    --define) EXTRA_FLAGS="$EXTRA_FLAGS -D$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [ -f firmware/lv_conf.h ]; then
  cp firmware/lv_conf.h "$(cygpath -u "$ARDUINO_DIRECTORIES_USER")/libraries/lv_conf.h"
fi

ARGS=(compile --fqbn "$FQBN" --build-path build/firmware --warnings default)
if [ -n "$EXTRA_FLAGS" ]; then
  ARGS+=(--build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS")
fi
if [ -n "$UPLOAD_PORT" ]; then
  ARGS+=(--upload -p "$UPLOAD_PORT")
fi
"$CLI" "${ARGS[@]}" firmware/dandori
