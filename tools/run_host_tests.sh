#!/usr/bin/env bash
# Compile and run the host unit tests for firmware/dandori/src/core.
set -euo pipefail
if [ -z "${LOCALAPPDATA:-}" ] || ! command -v cygpath >/dev/null 2>&1; then
  echo "Run this with Git Bash, not WSL bash. From PowerShell use: powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Upload COM8]" >&2
  exit 2
fi
cd "$(dirname "$0")/.."

WINLIBS_BIN="$(cygpath -u "$LOCALAPPDATA")/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
if ! command -v g++ >/dev/null 2>&1 && [ -d "$WINLIBS_BIN" ]; then
  export PATH="$WINLIBS_BIN:$PATH"
fi

ARDUINOJSON_SRC=".arduino-user/libraries/ArduinoJson/src"

mkdir -p build
shopt -s nullglob
g++ -std=c++17 -Wall -Wextra \
  -I firmware/dandori/src -I "$ARDUINOJSON_SRC" \
  test/host/*.cpp firmware/dandori/src/core/*.cpp \
  -o build/host_tests.exe
./build/host_tests.exe
