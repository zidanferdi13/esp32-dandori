# Desain: Alat Pencatat Downtime / Dandori Mesin (ESP32-S3 Touch LCD 7")

Tanggal: 2026-11-09
Status: draft, menunggu persetujuan

Penanda di dokumen ini:
- **[Keputusan user]**: disepakati langsung oleh user dalam diskusi.
- **[Usulan Claude]**: keputusan yang saya ambil sendiri. User boleh membatalkan atau mengubahnya.
- **[Terbuka]**: belum diputuskan dan perlu dijawab.

---

## 1. Tujuan dan alasan

Di lantai produksi, mesin bisa berhenti karena trouble, dandori, kekurangan material, utilitas, dan sebagainya. Lama berhentinya perlu dicatat dalam hitungan detik, per MO, beserta jenis masalahnya, supaya data downtime akurat dan bisa dianalisis.

Yang memakai alat ini adalah **operator** yang berdiri di samping mesin. Operator **hanya mencatat**. Perbaikan mesin tetap tanggung jawab MTC. **[Keputusan user]**

Akibatnya, alat harus **cepat dan sederhana**. Saat mesin trouble, operator tidak boleh direpotkan oleh langkah yang panjang atau layar yang membingungkan.

## 2. Sasaran, bukan sasaran, dan hal yang dianggap gagal

**Sasaran**
- Operator memilih MO aktif di awal line berjalan, lalu cukup menekan Start dan Stop setiap kali mesin berhenti.
- Setiap kejadian berhenti tercatat dengan: nomor MO, kode problem, waktu start, waktu stop, dan total detik.
- Data terkirim ke API dan **tidak hilang** walaupun WiFi atau API sedang mati, atau ESP32 restart.
- Tampilan bergaya tablet: layar Home, menu Pengaturan, tema Light/Dark. **[Keputusan user]**

**Bukan sasaran (tidak dikerjakan sekarang)**
- Tidak mencatat siapa yang memperbaiki atau apa tindakan perbaikannya (itu ranah MTC).
- Tidak ada dashboard atau laporan di ESP32. Analisis data dilakukan di sisi backend.
- Tidak membangun backend asli. Tahap ini hanya memakai **mock API Postman**. **[Keputusan user]**
- Tidak ada login per operator.

**Dianggap gagal walaupun secara teknis berjalan**
- Total detik tidak akurat, atau satu kejadian terkirim dua kali dan tercatat ganda.
- Data hilang karena WiFi putus atau listrik mati.
- Operator harus menekan banyak tombol atau mengetik banyak saat mesin trouble.
- Operator tidak bisa membedakan dengan cepat apakah timer sedang berjalan atau tidak.

## 3. Batasan

- **Hardware:** Waveshare **ESP32-S3-Touch-LCD-7**, layar 800×480, sentuh kapasitif. **[Keputusan user]**
  - Builder perlu memastikan board ini bukan varian **7B** (1024×600). Kalau ternyata 7B, tata letak disesuaikan.
- **Satu ESP32 untuk satu mesin.** **[Keputusan user]**
- **Alat pemrograman: Arduino IDE.** **[Keputusan user]**
- Library tampilan memakai **LVGL**, sama seperti demo bawaan Waveshare. **[Usulan Claude]**
- Zona waktu **WIB (UTC+7)**. **[Usulan Claude]**
- **Trial pertama hanya di mesin Palletizer**, jadi cukup satu daftar kode problem. **[Keputusan user]** Daftar kode untuk jenis mesin lain tidak dikerjakan sekarang.
- **Pengujian awal di jaringan lokal (LAN)** memakai mock API yang dijalankan di PC lokal. **[Keputusan user]** Belum tentu WiFi pabrik punya akses internet, jadi NTP publik mungkin tidak terjangkau. Server NTP bisa diganti di Pengaturan → Jam, misalnya ke server NTP internal atau router.

### 3.1 Shift dan "hari produksi" **[Keputusan user: jam shift]**
| Shift | Jam |
|---|---|
| Shift 1 | 08.00 – 17.00 |
| Shift 2 | 17.00 – 23.00 |
| Shift 3 | 23.00 – 08.00 (hari berikutnya) |

- **Hari produksi** dimulai pukul **08.00** dan berakhir pukul 07.59 keesokan harinya. Contohnya, pukul 02.00 tanggal 10 masih termasuk hari produksi **tanggal 9**. **[Usulan Claude, diturunkan dari jam shift]**
- Hari produksi dipakai secara konsisten untuk: parameter `tgl` di Get MO, layar **Riwayat Hari Ini**, ringkasan "3 kejadian · 14 mnt" di layar Counting, dan pembersihan riwayat lama.
- Bar atas menampilkan shift yang sedang berjalan, misalnya "Shift 3". **[Usulan Claude]**
- Jam pergantian shift ditanam di firmware untuk trial ini, tanpa menu pengaturan. **[Usulan Claude]**

## 4. Alur penggunaan

### 4.1 Gambaran besar
1. Saat line mulai jalan, operator membuka **Downtime**, menekan **Get MO**, lalu memilih MO yang sedang dikerjakan. MO itu menjadi **MO aktif**. **[Keputusan user]**
2. Layar menampilkan halaman **Counting** dengan MO aktif. Halaman ini tetap terbuka sepanjang MO berjalan.
3. Saat mesin berhenti, operator menekan **Start** dan timer mulai menghitung.
4. Saat mesin jalan lagi, operator menekan **Stop**.
5. Operator mengetik **kode problem** (1–26) lewat keypad angka. Nama problem tampil sebagai konfirmasi.
6. Operator menekan **Kirim**, lalu data dikirim ke API.
7. Layar kembali ke Counting dengan MO yang sama, siap untuk kejadian berikutnya. Satu MO bisa punya banyak kejadian. **[Keputusan user]**
8. Untuk ganti MO, operator menekan **Ganti MO**, lalu Get MO dan pilih lagi.

### 4.2 MO untuk kejadian khusus **[Keputusan user]**
- **Kode 14, Preparation Awal Shift:** operator **lebih dulu memilih MO yang akan berjalan**, baru menekan Start. Jadi Start **tidak bisa ditekan tanpa MO aktif**. Kalau belum ada MO aktif, tombol Start diganti tombol "Pilih MO dulu". **[Usulan Claude: cara tampilnya]**
- **Kode 13, Dandory Produk:** dicatat ke **MO berikutnya**, yaitu MO setelah dandori. Alurnya **sama dengan kejadian lain**: operator menekan **Ganti MO**, memilih MO berikutnya, lalu Start → Stop → ketik kode 13 → Kirim. Tidak ada layar khusus. **[Keputusan user]**

### 4.3 Aturan perilaku
- Tombol **Ganti MO** dikunci selama timer berjalan. **[Usulan Claude]**
- Tombol **Kirim** tidak aktif kalau kode kosong atau di luar 1–26. **[Usulan Claude]**
- Total detik dihitung dari **timer internal** ESP32, bukan dari selisih jam NTP, supaya tetap akurat walaupun jam belum sinkron atau bergeser saat sinkron ulang. **[Usulan Claude]**
- MO aktif disimpan di memori permanen, sehingga setelah ESP32 restart MO aktif masih sama. **[Usulan Claude]**
- **Restart saat timer berjalan:** waktu start disimpan di memori permanen begitu Start ditekan. Kalau ESP32 restart dan jam sudah sinkron NTP, timer dilanjutkan dari waktu start yang tersimpan, disertai pesan "Downtime dilanjutkan sejak 08:15:30". Kalau jam belum sinkron setelah restart, layar meminta operator menekan Stop dan durasinya dihitung sejak ESP32 menyala, ditandai sebagai *perkiraan*. **[Usulan Claude]** Detail pesan dan tanda perkiraan diserahkan ke builder.
- **Batal Start yang tidak sengaja:** dari layar Input Kode ada tombol **Batal** yang membuang kejadian itu, dengan konfirmasi "Yakin dibuang?". **[Usulan Claude]**

## 5. Layar

Mockup kasar disimpan di `.superpowers/brainstorm/` (file `alur-layar.html` dan `launcher-setting.html`). Ukuran layar 800×480 landscape.

### 5.1 Home (launcher ala tablet) **[Keputusan user: gaya tablet + menu setting]**
- Bar atas: kode mesin, status WiFi, tanggal, dan jam.
- Baris info: MO aktif (atau "Belum ada MO aktif").
- Tiga ikon besar: **Downtime**, **Riwayat Hari Ini**, **Pengaturan**.

### 5.2 Downtime
1. **Pilih MO**: daftar dari `GET /api/mo`. Setiap baris menampilkan nomor MO, dan info produk kalau API menyediakannya. Ada tombol Refresh. Tap satu baris, lalu "Pilih MO ini".
2. **Counting siap**: nomor MO aktif, timer `00:00:00` berwarna abu-abu, tombol **START** hijau yang sangat besar, ringkasan hari ini ("3 kejadian · 14 mnt 22 dtk"), link "Ganti MO", dan tombol kembali ke Home.
3. **Counting berjalan**: seluruh layar bernuansa **merah**, teks "MESIN BERHENTI", timer besar yang terus berjalan, jam mulai, dan tombol **STOP** merah yang sangat besar. Warna merah dipakai di tema Light maupun Dark.
4. **Input kode problem**: durasi dan rentang waktu di bar atas, kotak kode besar, keypad angka 0–9 dengan ⌫ dan Hapus, nama problem tampil langsung (contoh: "Loss Time · Palletizer · **Dandory Produk**"), tombol **KIRIM** dan **Batal**.
5. **Hasil**: "✓ Data terkirim" beserta ringkasan, lalu otomatis kembali ke Counting setelah sekitar 3 detik. Kalau gagal, tampil "Disimpan, akan dikirim ulang otomatis" dan badge jumlah antrian muncul di bar atas.

Kembali ke Home dari Counting diperbolehkan, kecuali saat timer berjalan. **[Usulan Claude]**

### 5.3 Riwayat Hari Ini **[Usulan Claude]**
- Daftar kejadian hari ini yang tersimpan di ESP32, urut dari yang terbaru: jam, MO, kode dan nama problem, durasi, serta status (✓ terkirim / ⏳ antri).
- Hanya untuk dilihat, tidak bisa diedit atau dihapus.
- Riwayat lama dibersihkan otomatis. Batas penyimpanan diserahkan ke builder, minimal data hari ini plus yang masih antri.

### 5.4 Pengaturan **[Keputusan user: ada menu setting]**
Tata letak tablet: daftar menu di kiri, isi di kanan.

| Menu | Isi | Dikunci PIN? |
|---|---|---|
| Tampilan | Tema Light / Dark / Auto, layar mati setelah X menit tidak disentuh, bunyi tombol on/off | Tidak |
| WiFi | Scan jaringan, pilih, isi password dengan keyboard di layar, status koneksi dan IP | Ya |
| Server API | Alamat server (base URL), **Token API**, tombol Tes Koneksi, jumlah antrian, tombol Kirim Ulang | Ya |
| Mesin | Kode mesin (misalnya `PALLETIZER-01`) | Ya |
| Jam | Status sinkron NTP, jam terakhir sinkron, server NTP | Ya |
| Keamanan | Ganti PIN admin | Ya |
| Tentang | Versi firmware, MAC address, sisa memori | Tidak |

- PIN admin terdiri dari 4 digit, dengan default **`1234`** yang bisa diganti dari menu Keamanan. **[Usulan Claude]**
- Semua pengaturan disimpan di memori permanen (NVS). **[Usulan Claude]**
- **Tema Auto:** Dark antara pukul 18.00–06.00, Light di luar jam itu, dan hanya berlaku kalau jam sudah sinkron. Kalau belum sinkron, tema mengikuti pilihan terakhir. **[Usulan Claude]**
- **Kecerahan layar:** belum diketahui apakah lampu layar board 7" bisa diredupkan atau hanya nyala/mati. **[Terbuka, builder cek di dokumentasi Waveshare]** Kalau hanya nyala/mati, slider kecerahan dihapus dan cukup dipakai fitur "layar mati setelah X menit". Sentuhan pertama setelah layar mati **hanya menyalakan layar**, tanpa menekan tombol apa pun.

## 6. Kode problem

Daftar ini ditanam di firmware sebagai tabel tetap. **[Usulan Claude]** Alasannya: validasi dan nama problem harus bisa tampil walaupun WiFi mati. Kalau suatu saat daftar berubah, firmware perlu di-flash ulang. Builder boleh menyusun tabel di satu file terpisah supaya mudah diedit.

| Kode | Jenis | Mesin | Sub Mesin |
|---|---|---|---|
| 1 | Downtime | Palletizer | Conveyor Infeed |
| 2 | Downtime | Palletizer | Turning Belt |
| 3 | Downtime | Palletizer | Cross Inverter |
| 4 | Downtime | Palletizer | Grouping Belt |
| 5 | Downtime | Palletizer | Row Pusher |
| 6 | Downtime | Palletizer | Stripper Plate |
| 7 | Downtime | Palletizer | Pressing Plat BCD |
| 8 | Downtime | Palletizer | Plat B |
| 9 | Downtime | Palletizer | Plat C |
| 10 | Downtime | Palletizer | Plat D |
| 11 | Downtime | Palletizer | Elevator |
| 12 | Downtime | Palletizer | Lain-Lain |
| 13 | Loss Time | Palletizer | Dandory Produk |
| 14 | Loss Time | Palletizer | Preparation Awal Shift |
| 15 | Loss Time | Palletizer | Pasir Kurang / Habis |
| 16 | Loss Time | Palletizer | Kemasan Kurang / Habis |
| 17 | Loss Time | Palletizer | Aditif Kurang / Habis |
| 18 | Loss Time | Palletizer | Semen Kurang / Habis |
| 19 | Loss Time | Palletizer | Menunggu Forklift |
| 20 | Loss Time | Palletizer | Cek Lab |
| 21 | Loss Time | Palletizer | Lain-Lain |
| 22 | Loss Time | Utility | Angin |
| 23 | Loss Time | Utility | Listrik |
| 24 | Loss Time | Utility | Lain-Lain |
| 25 | Performance Loss | Palletizer | Small Stop |
| 26 | Performance Loss | Palletizer | Reduce Speed |

Operator **mengetik** kode lewat keypad angka dan tidak memilih dari daftar. **[Keputusan user]**

## 7. Kontrak API

Backend asli belum ada. Kontrak di bawah adalah usulan, dan tahap pertama memakai **mock API Postman**. **[Keputusan user: mock dulu]** Base URL diambil dari Pengaturan → Server API.

**Token** **[Keputusan user: API asli memakai token]**: setiap request mengirim header `Authorization: Bearer <token>`, dengan token diisi di Pengaturan → Server API. **[Usulan Claude: bentuk header]** Kalau token kosong, header tidak dikirim. Mock lokal tidak memeriksa token.

### 7.1 Ambil daftar MO
```
GET {baseUrl}/api/mo?tgl=2026-11-09
```
- `tgl` berisi tanggal **hari produksi** (lihat 3.1) dari jam NTP dengan format `YYYY-MM-DD`. **[Keputusan user: GET dengan parameter tgl]** **[Usulan Claude: formatnya]**
- **Kalau NTP belum sinkron, `tgl` tidak dikirim** dan backend yang menentukan tanggalnya. **[Keputusan user]**
- Response **[Usulan Claude]**:
```json
{
  "tgl": "2026-11-09",
  "data": [
    { "mo": "MO-2611-0012", "produk": "Bata Ringan 10cm" },
    { "mo": "MO-2611-0013", "produk": "Bata Ringan 7.5cm" }
  ]
}
```
  `produk` bersifat opsional. Kalau tidak ada, layar hanya menampilkan nomor MO.

### 7.2 Kirim data downtime
```
POST {baseUrl}/api/downtime
Content-Type: application/json
```
```json
{
  "id": "PALLETIZER-01-20261109-081530-0007",
  "mesin": "PALLETIZER-01",
  "mo": "MO-2611-0012",
  "kode_problem": 13,
  "start": "2026-11-09T08:15:30+07:00",
  "stop": "2026-11-09T08:21:05+07:00",
  "total_detik": 335
}
```
- `mo`, `kode_problem`, `start`, `stop`, dan `total_detik` adalah field yang diminta user. **[Keputusan user]**
- `mesin` ditambahkan karena satu ESP32 mewakili satu mesin, dan backend perlu tahu data berasal dari mesin mana. **[Usulan Claude]**
- `id` adalah ID unik per kejadian yang dibuat ESP32, supaya kalau data dikirim ulang dari antrian, backend bisa mengenali data yang sama dan tidak mencatatnya dua kali. **[Usulan Claude]** Format persisnya diserahkan ke builder, asalkan unik per perangkat dan tetap unik setelah restart.
- **Kalau jam belum sinkron NTP:** `start` dan `stop` dikirim `null` dan `total_detik` tetap terisi. Backend boleh memakai jam server sebagai perkiraan. **[Usulan Claude]**
- **Sukses:** HTTP `200` atau `201`. Data ditandai terkirim.
- **Gagal (timeout, tidak ada WiFi, 5xx):** data tetap di antrian dan dikirim ulang otomatis.
- **Ditolak (4xx selain 408/429):** data ditandai gagal dan tidak dikirim ulang terus-menerus. Data tetap tampil di Riwayat dengan status ✗. **[Usulan Claude]**

### 7.3 Mock API Postman (lokal)
- Mock dijalankan **di PC lokal** dalam jaringan yang sama dengan ESP32, memakai Postman CLI yang sudah terpasang di PC user. **[Keputusan user: test lokal dulu]** ESP32 memanggilnya lewat `http://<IP-PC>:<port>`.
- Karena mock berjalan lewat HTTP biasa di LAN, firmware harus mendukung `http://`. Dukungan `https://` untuk backend asli juga disiapkan, tetapi cara pengecekan sertifikatnya ditentukan nanti (lihat bagian 9).
- Mock dibuat untuk kedua endpoint di atas, dengan contoh response:
  - `GET /api/mo`: daftar beberapa MO, dan satu contoh daftar kosong.
  - `POST /api/downtime`: sukses `201`, ditambah contoh error `500` untuk menguji antrian.
- URL mock dimasukkan ke Pengaturan → Server API.

## 8. Ketahanan

- **Antrian offline:** setiap kejadian disimpan ke memori permanen **sebelum** dikirim. Pengiriman dicoba di latar belakang secara berkala sampai sukses. Antrian tetap ada setelah restart atau listrik mati. **[Usulan Claude]**
- **NTP:** sinkron saat WiFi tersambung, dicoba ulang berkala di latar belakang, dan disinkron ulang secara rutin. **[Usulan Claude]**
- **WiFi putus:** sambung ulang otomatis. Status WiFi selalu tampil di bar atas.
- **API lambat:** ada timeout yang jelas. Layar tidak boleh membeku saat menunggu jaringan, dan timer tetap berjalan normal.

## 9. Pertanyaan terbuka

1. **Kecerahan layar** (lihat 5.4): perlu dicek di dokumentasi atau contoh kode Waveshare.
2. **Backend asli** (tidak menghambat trial): perlu disepakati dengan tim backend bahwa `tgl` berarti hari produksi (mulai 08.00), apakah API asli memakai HTTPS dan cara mengecek sertifikatnya, serta format token. Mock lokal mengabaikan semua ini.

## 10. Diserahkan ke builder

- Struktur folder sketch Arduino, pembagian file, dan versi library (LVGL, driver layar dan sentuh), mengikuti contoh resmi Waveshare untuk board ini.
- Detail visual: font, ikon, warna persis tema Light dan Dark (mockup hanya acuan).
- Mekanisme penyimpanan (NVS, LittleFS, atau gabungan), interval retry, dan batas jumlah riwayat.
- Format persis `id` kejadian.
- Cara keyboard huruf di layar untuk password WiFi dan URL server.
