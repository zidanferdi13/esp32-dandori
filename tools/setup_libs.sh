#!/usr/bin/env bash
# Install the pinned Arduino libraries into a project-local sketchbook (.arduino-user/),
# so builds do not depend on the OneDrive-synced global sketchbook.
set -euo pipefail
if [ -z "${LOCALAPPDATA:-}" ] || ! command -v cygpath >/dev/null 2>&1; then
  echo "Run this with Git Bash, not WSL bash. From PowerShell use: powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Upload COM8]" >&2
  exit 2
fi
cd "$(dirname "$0")/.."

CLI="$(cygpath -u "$LOCALAPPDATA")/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
export ARDUINO_DIRECTORIES_USER="$(cygpath -w "$PWD/.arduino-user")"
mkdir -p .arduino-user/libraries

"$CLI" lib update-index
"$CLI" lib install "lvgl@9.4.0" "ArduinoJson@7.4.3"
"$CLI" lib list
