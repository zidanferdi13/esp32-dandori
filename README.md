# ESP32 Dandori — Pencatat Downtime Mesin

Firmware Arduino untuk **Waveshare ESP32-S3-Touch-LCD-7** (800×480). Operator memilih MO aktif, menekan START/STOP setiap mesin berhenti, mengetik kode problem (1–26), lalu data dikirim ke API.

- Desain: [docs/superpowers/specs/2026-11-09-dandori-downtime-design.md](docs/superpowers/specs/2026-11-09-dandori-downtime-design.md)
- Rencana kerja: [docs/superpowers/plans/2026-11-09-dandori-downtime.md](docs/superpowers/plans/2026-11-09-dandori-downtime.md)
- Mockup tampilan: [docs/mockups/](docs/mockups/)

## Kebutuhan

| Komponen | Versi yang dipakai |
|---|---|
| Arduino IDE + core `esp32` (Espressif) | 3.3.7 |
| LVGL | 9.4.0 |
| GFX Library for Arduino | 1.6.2 |
| SensorLib | 0.3.3 |
| ArduinoJson | 7.4.3 |

`tools/build.sh` menyalin `firmware/lv_conf.h` ke folder `libraries` Arduino (`%USERPROFILE%\OneDrive\Documents\Arduino\libraries\lv_conf.h`). Kalau build dari Arduino IDE, salin file itu secara manual sekali. Sketch LVGL lain di PC yang sama akan ikut memakai `lv_conf.h` ini.

## Pengaturan board (Arduino IDE → Tools)

| Opsi | Nilai |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) — dikesampingkan otomatis oleh `firmware/dandori/partitions.csv` (3 MB app, ~12.9 MB LittleFS) |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled (log Serial lewat port USB native) |

FQBN untuk CLI: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc`

## Build dan tes

```bash
bash tools/run_host_tests.sh          # unit test logika inti di PC (butuh g++ WinLibs)
bash tools/build.sh                   # compile firmware
bash tools/build.sh --upload COM5     # compile + upload
```
