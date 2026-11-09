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
bash tools/setup_libs.sh              # sekali: pasang library versi di atas ke .arduino-user/
bash tools/run_host_tests.sh          # unit test logika inti di PC (butuh g++ WinLibs)
bash tools/build.sh                   # compile firmware
bash tools/build.sh --upload COM5     # compile + upload
```

Build CLI memakai library di `.arduino-user/` (folder proyek), bukan sketchbook OneDrive, supaya tidak bergantung pada sinkronisasi OneDrive. Kalau mau build dari Arduino IDE, pilih salah satu:
- arahkan **File → Preferences → Sketchbook location** ke folder `.arduino-user` di proyek ini, atau
- pastikan OneDrive berjalan (supaya library di sketchbook OneDrive terunduh) dan salin `firmware/lv_conf.h` ke folder `libraries` sketchbook.

## Mock API (lokal)

Kontrak API ada di [api/openapi.yaml](api/openapi.yaml). Ini juga dokumen serah terima untuk tim backend.

```bash
postman mock run postman/mocks/dandori --port 4500
```

- ESP32 memanggil `http://<IP-PC>:4500` (isi di Pengaturan → Server API). Mock mendengarkan di semua interface.
- **Windows Firewall** harus mengizinkan koneksi masuk TCP 4500. Kalau ESP32 tidak bisa terhubung, tambahkan rule inbound untuk port itu.
- Ganti perilaku mock dari PC (ESP32 tidak perlu tahu):
  ```bash
  curl -X POST localhost:4500/__mock/mode -H "Content-Type: application/json" -d '{"mo":"empty","downtime":"500"}'
  ```
  `mo`: `ok` | `empty` | `500`. `downtime`: `ok` | `400` | `500` | `down` (koneksi diputus).
- Lihat semua data yang diterima (dan id yang dobel): `curl localhost:4500/__mock/received`. Kosongkan dengan `curl -X DELETE localhost:4500/__mock/received`.
- POST divalidasi sesuai kontrak (key wajib, `kode_problem` 1–26, format waktu `+07:00` atau `null`). Payload salah dijawab `400`.
