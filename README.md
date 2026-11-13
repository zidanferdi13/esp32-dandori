# ESP32 Dandori — Pencatat Downtime Mesin

Firmware Arduino untuk **Waveshare ESP32-S3-Touch-LCD-7C** (800×480, 32 MB flash, 16 MB PSRAM octal). Operator memilih MO aktif, menekan START/STOP setiap mesin berhenti, mengetik kode problem (1–26), lalu data dikirim ke API.

> Board yang didukung hanya varian **7C**. Varian non-C memakai I2C di GPIO 8/9 dan expander CH422G; lihat `firmware/dandori/board_config.h`.

- Desain: [docs/superpowers/specs/2026-11-09-dandori-downtime-design.md](docs/superpowers/specs/2026-11-09-dandori-downtime-design.md)
- Rencana kerja: [docs/superpowers/plans/2026-11-09-dandori-downtime.md](docs/superpowers/plans/2026-11-09-dandori-downtime.md)
- Mockup tampilan: [docs/mockups/](docs/mockups/)

## Kebutuhan

| Komponen | Versi yang dipakai |
|---|---|
| Arduino IDE + core `esp32` (Espressif) | 3.3.7 |
| LVGL | 9.4.0 |
| ArduinoJson | 7.4.3 |

`tools/build.sh` menyalin `firmware/lv_conf.h` ke `.arduino-user/libraries/lv_conf.h` (folder proyek), jadi sketch LVGL lain di PC tidak terpengaruh.

## Pengaturan board (Arduino IDE → Tools)

| Opsi | Nilai |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) — chip 7C sebenarnya 32MB, tapi `partitions.csv` hanya memakai 16MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) — dikesampingkan otomatis oleh `firmware/dandori/partitions.csv` (3 MB app, ~12.9 MB LittleFS) |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled (log Serial lewat port USB native) |
| Erase All Flash Before Upload | **Disabled** (kalau diaktifkan, antrian downtime dan setting WiFi/PIN ikut terhapus) |

FQBN untuk CLI: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc`

**Jangan pilih Flash Size 4MB.** Bootloader akan menolak partition table (`partition 3 invalid ... exceeds flash chip size 0x400000`) dan board reboot terus.

## Pinout (7C)

| Fungsi | GPIO |
|---|---|
| I2C SDA / SCL | 47 / 48, 400 kHz |
| Touch GT911 INT | 4 (alamat `0x5D`) |
| IO expander | `0x24`, 16-bit, payload `[cmd][lo][hi]`; IO1 = touch reset, IO2 = backlight, IO3 = PA, IO4 = SD CS |
| RGB: DE / VSYNC / HSYNC / PCLK | 5 / 3 / 46 / 7 |
| RGB: R | 1, 2, 42, 41, 40 |
| RGB: G | 39, 0, 45, 9, 8, 21 |
| RGB: B | 14, 38, 18, 17, 10 |

Sumber: `waveshareteam/ESP32-S3-Touch-LCD-7C`, `example/arduino/06_TOUCH`.

## Build dan tes

```bash
bash tools/setup_libs.sh              # sekali: pasang library versi di atas ke .arduino-user/
bash tools/run_host_tests.sh          # unit test logika inti di PC (butuh g++ WinLibs)
bash tools/build.sh                   # compile firmware
bash tools/build.sh --upload COM8     # compile + upload ke board
```

Layar RGB dan sentuh GT911 dijalankan langsung lewat ESP-IDF (`esp_lcd`) dan `Wire`, tanpa library GFX/SensorLib, supaya build tidak perlu memindai ratusan driver yang tidak dipakai.

### Upload lewat Arduino IDE (GUI)

1. **File → Preferences → Sketchbook location** = `D:\Kerjaan\Project\esp32-dandori\.arduino-user`, lalu restart IDE.
   Library di sketchbook OneDrive berbeda versi (LVGL 9.6.0 dan tanpa `lv_conf.h`), jadi build dari sana akan gagal.
2. **File → Open** `firmware/dandori/dandori.ino`.
3. Set menu **Tools** sesuai tabel di atas.
4. **Verify**, lalu **Upload**. Tutup Serial Monitor kalau port dipakai.
5. Log: Serial Monitor 115200 baud. Boot yang sehat menampilkan `display: GT911 at 0x5D` dan `dandori: N events, M pending`.

### Upload lewat CLI

1. Sambungkan board lewat port **USB** (bukan UART). Di Windows muncul sebagai *USB Serial Device (COMx)*, VID `303A`.
2. Jalankan `bash tools/build.sh --upload COM8` (ganti COM8 sesuai port).
3. Kalau upload gagal "port busy" / "no serial data": tutup Serial Monitor yang sedang memakai port itu. Kalau masih gagal, tahan tombol **BOOT**, tekan **RESET**, lepas **BOOT**, lalu upload lagi; setelah selesai tekan **RESET**.
4. Lihat log: Serial Monitor Arduino IDE di COM8, 115200 baud.

Build CLI memakai library di `.arduino-user/` (folder proyek), bukan sketchbook OneDrive, supaya tidak bergantung pada sinkronisasi OneDrive.

### Diagnosa I2C

Upload `tools/i2c_scan/i2c_scan.ino` (tidak butuh library). Board sehat menampilkan `0x24 0x5D` dan `err2` kecil. `(0 found) err2=126` berarti tidak ada device yang menjawab; cek pin (47/48 untuk 7C), kabel FPC touch, dan perangkat tambahan di header I2C.

## API produksi

Di Pengaturan → Server API, isi **`https://services.ama.id`** (tanpa `/kanban` di belakang).

- GET `/kanban/getMOPalletizer[?tgl=YYYY-MM-DD]` → membaca `nomor_mo` dan `product_name`; item `isactive=0` dilewati. Field lama `mo`/`produk` juga diterima untuk kompatibilitas mock.
- POST `/kanban/postDownTime` → payload `id`, `mesin`, `mo`, `kode_problem`, `catatan`, `start`, `stop`, `total_detik`. `catatan` adalah teks bebas dari operator (maks 255 karakter), selalu dikirim dan berisi string kosong bila tidak diisi.
- `tgl` adalah hari produksi (mulai 08:00 WIB). Dihilangkan kalau jam belum sinkron. Backend perlu mengonfirmasi apakah endpoint memfilter parameter ini.
- HTTPS saat ini masih `setInsecure()` (sertifikat belum diverifikasi); sebelum produksi perlu memasang CA yang sesuai.

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
