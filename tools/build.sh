#!/usr/bin/env bash
# Compile (and optionally upload) firmware/dandori with the arduino-cli bundled in Arduino IDE.
# Usage: tools/build.sh [--upload COMx] [--define NAME]...
set -euo pipefail
if [ -z "${LOCALAPPDATA:-}" ] || ! command -v cygpath >/dev/null 2>&1; then
  echo "Run this with Git Bash, not WSL bash. From PowerShell use: powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Upload COM8]" >&2
  exit 2
fi
cd "$(dirname "$0")/.."

CLI="$(cygpath -u "$LOCALAPPDATA")/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
export ARDUINO_DIRECTORIES_USER="$(cygpath -w "$PWD/.arduino-user")"
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
  cp firmware/lv_conf.h .arduino-user/libraries/lv_conf.h
fi

ARGS=(compile --fqbn "$FQBN" --build-path build/fw --warnings default)
if [ -n "$EXTRA_FLAGS" ]; then
  ARGS+=(--build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS")
fi
if [ -n "$UPLOAD_PORT" ]; then
  ARGS+=(--upload -p "$UPLOAD_PORT")
fi
"$CLI" "${ARGS[@]}" firmware/dandori
