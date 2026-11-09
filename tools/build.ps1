# Compile (and optionally upload) firmware/dandori with the arduino-cli bundled in Arduino IDE.
# Usage (PowerShell):
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Upload COM8
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Define DANDORI_SELFTEST
param(
  [string]$Upload = "",
  [string[]]$Define = @()
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
if (-not (Test-Path $cli)) { throw "arduino-cli not found at $cli (is Arduino IDE installed?)" }
$env:ARDUINO_DIRECTORIES_USER = Join-Path $root '.arduino-user'
$fqbn = 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc'

if (-not (Test-Path '.arduino-user\libraries\lvgl')) {
  throw "Libraries missing: run tools\setup_libs.ps1 first"
}
if (Test-Path 'firmware\lv_conf.h') {
  Copy-Item 'firmware\lv_conf.h' '.arduino-user\libraries\lv_conf.h' -Force
}

$cliArgs = @('compile', '--fqbn', $fqbn, '--build-path', 'build\fw', '--warnings', 'default')
if ($Define.Count -gt 0) {
  $flags = ($Define | ForEach-Object { "-D$_" }) -join ' '
  $cliArgs += @('--build-property', "compiler.cpp.extra_flags=$flags")
}
if ($Upload) { $cliArgs += @('--upload', '-p', $Upload) }
$cliArgs += 'firmware\dandori'

& $cli @cliArgs
exit $LASTEXITCODE
