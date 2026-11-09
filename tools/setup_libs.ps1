# Install the pinned Arduino libraries into a project-local sketchbook (.arduino-user\).
# Usage: powershell -ExecutionPolicy Bypass -File tools\setup_libs.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$env:ARDUINO_DIRECTORIES_USER = Join-Path $root '.arduino-user'
New-Item -ItemType Directory -Force '.arduino-user\libraries' | Out-Null

& $cli lib update-index
& $cli lib install 'lvgl@9.4.0' 'ArduinoJson@7.4.3'
& $cli lib list
