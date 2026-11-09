#!/usr/bin/env bash
# Compile and run the host unit tests for firmware/dandori/src/core.
set -euo pipefail
cd "$(dirname "$0")/.."

WINLIBS_BIN="$(cygpath -u "$LOCALAPPDATA")/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
if ! command -v g++ >/dev/null 2>&1 && [ -d "$WINLIBS_BIN" ]; then
  export PATH="$WINLIBS_BIN:$PATH"
fi

ARDUINOJSON_SRC="$(cygpath -u "$USERPROFILE")/OneDrive/Documents/Arduino/libraries/ArduinoJson/src"

mkdir -p build
shopt -s nullglob
g++ -std=c++17 -Wall -Wextra \
  -I firmware/dandori/src -I "$ARDUINOJSON_SRC" \
  test/host/*.cpp firmware/dandori/src/core/*.cpp \
  -o build/host_tests.exe
./build/host_tests.exe
