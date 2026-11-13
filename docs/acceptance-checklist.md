# Checklist Uji Terima di Perangkat

Jalankan dengan board Waveshare ESP32-S3-Touch-LCD-**7C** dan mock lokal (`postman mock run postman/mocks/dandori --port 4500`). Isi kolom **Hasil** dengan ✅ / ❌ dan catatan.

Persiapan: Pengaturan → PIN `1234` → WiFi (pilih jaringan) → Server API `http://<IP-PC>:4500` → Tes koneksi = OK.
Ubah mode mock dari PC: `curl -X POST localhost:4500/__mock/mode -H "Content-Type: application/json" -d '{"mo":"ok","downtime":"ok"}'`.
Lihat data yang masuk: `curl localhost:4500/__mock/received`.

## 0. Bring-up perangkat keras (Task 2)

| # | Langkah | Hasil yang diharapkan | Hasil |
|---|---|---|---|
| 0a | Nyalakan board | Layar Home tampil, warna benar (ikon Downtime merah, Riwayat biru), tidak bergeser/berkedip | |
| 0b | Sentuh keempat sudut layar | Serial menunjukkan tidak ada error GT911; tile yang disentuh terbuka sesuai posisi jari | |
| 0c | Pengaturan → Tampilan → Layar mati 1 mnt, tunggu 1 menit | Lampu layar mati; sentuhan pertama hanya menyalakan layar (tidak menekan tombol) | |
| 0d | WiFi aktif, buka-tutup beberapa layar | Gambar tidak bergeser saat WiFi sibuk (kalau bergeser: naikkan `kBounceBufferPx` di `board_config.h`) | |
| 0e | Cek dokumentasi/board | Apakah lampu layar bisa PWM? Apakah ada buzzer? (catat; dipakai untuk menu Tampilan) | |

## 1–10. Skenario fungsional

| # | Skenario | Hasil yang diharapkan | Hasil |
|---|---|---|---|
| 1 | **Restart saat timer berjalan, jam sinkron**: START, tunggu 30 dtk, tekan RESET | Kembali ke layar Counting merah, timer ≥ 30 dtk, toast "Downtime dilanjutkan sejak …". STOP + KIRIM → `total_detik` mencakup seluruh rentang | |
| 2 | **Restart saat timer berjalan, tanpa jam**: Server NTP diisi host yang salah, putuskan internet (LAN tetap), RESET | Setelah ±20 dtk "Memulihkan downtime…", timer lanjut dari saat boot; baris riwayat bertanda `~` | |
| 3 | **Restart saat Input kode** | Kembali ke layar Input kode dengan durasi yang sama | |
| 4 | **Kirim offline**: mock `downtime=down`, catat 3 kejadian | 3 × ikon antri, badge di status bar = 3. RESET, lalu mock `downtime=ok` → ketiganya terkirim sekali; `/__mock/received` → `duplicates: []` | |
| 5 | **Mock menolak (400)**: mock `downtime=400`, catat 1 kejadian | Layar hasil "Ditolak server (kode 400)", riwayat ✗, tidak dikirim ulang (`/__mock/received` hanya 1 untuk id itu) | |
| 6 | **Tanpa NTP** | Log mock: `GET /kanban/getMOPalletizer tgl=(none)`; POST berisi `"start":null,"stop":null` dan `total_detik` benar | |
| 7 | **Hari produksi** (07:59 → kemarin, 08:00 → hari ini) | Diverifikasi oleh host test (`test_wib_time.cpp`), tidak bisa diatur di perangkat | host-verified |
| 8 | **Start tidak sengaja**: START → STOP → Batal → Buang | Tidak ada data tersimpan/terkirim | |
| 9 | **Ganti MO dikunci saat berjalan; dandori (kode 13)**: saat timer jalan, link Ganti MO tidak ada. Setelah KIRIM, Ganti MO → pilih MO berikutnya → START/STOP → 13 | Data dandori membawa MO baru | |
| 10 | **Tema Auto** setelah 18:00 WIB | Dalam 1 detik berganti ke Dark | |

## Tambahan

| # | Skenario | Hasil yang diharapkan | Hasil |
|---|---|---|---|
| 11 | Kode `27`, `0` di awal, kosong | KIRIM mati untuk 27/kosong; `0` di awal diabaikan | |
| 12 | Mock `mo=empty`, lalu `mo=500` di layar Pilih MO | "Tidak ada MO untuk hari ini" / "Gagal ambil daftar MO (kode 500)"; MO aktif tidak berubah | |
| 13 | PIN salah, lalu ganti PIN ke `4321`, RESET | PIN salah ditolak; setelah reset `1234` gagal, `4321` berhasil | |
| 14 | **Catatan kosong**: STOP → ketik `13` → KIRIM tanpa membuka Catatan | POST berisi `"catatan":""`, layar Hasil tidak menampilkan baris catatan | |
| 15 | **Catatan terisi**: STOP → ketik `13` → ketuk baris Catatan → tulis "bearing aus" → Simpan → KIRIM | Baris Catatan menampilkan teksnya; POST berisi `"catatan":"bearing aus"`; layar Hasil menampilkan `Catatan: bearing aus` | |
| 16 | **Catatan panjang & multi-baris**: tulis ±300 karakter dengan Enter | Textarea berhenti di 255 karakter; baris Catatan tetap satu baris (dipotong `…`); tidak ada crash | |
