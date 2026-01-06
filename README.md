````md
# Smart Wristband Seudati  
**MPU6050 + LCD I2C + Motor Haptic (Mode Coach & Mode Latih)**

Proyek ini adalah **gelang latih berbasis Arduino** untuk membantu latihan ritme/tepukan (misalnya Tari Seudati) menggunakan **MPU6050 (accelerometer)**, **LCD 16x2 I2C**, dan **motor getar**. Sistem memiliki **dua mode utama**: **Coach (pelatih)** dan **Latih (penilaian)**.

---

## 1. Gambaran Umum

### 🎓 Mode Coach (Pelatih)
Alat **mengajarkan ritme** melalui getaran motor sesuai **BPM** dan **pattern** per level. LCD menampilkan status level dan jenis getaran.

### 🏃 Mode Latih (Penilaian)
Alat **menilai gerakan pengguna** berdasarkan:
- **Perubahan getaran (impact/jerk)**,
- **Ketepatan waktu (timing)**,
- **Kekuatan/posisi gerakan tangan**.

Hasil penilaian:
- **SEMPURNA** → 1 getaran
- **SALAH** → 2 getaran

Mode dapat **di-toggle dengan tombol**.

---

## 2. Fitur Utama

- Dua mode: **Coach** dan **Latih**
- 3 level Coach dengan BPM meningkat
- Motor getar non-blocking (state machine)
- LCD informatif dengan “hold message” agar teks tidak cepat hilang
- Debounce tombol non-blocking
- LED indikator mode & naik level

---

## 3. Perangkat Keras (Hardware)

| Komponen | Keterangan |
|---|---|
| Arduino (Uno/Nano) | Board utama |
| MPU6050 | Sensor akselerometer (I2C, default `0x68`) |
| LCD 16x2 I2C | Display (alamat di kode `0x27`) |
| Motor Getar | Output haptic |
| Push Button | Toggle mode (`INPUT_PULLUP`) |
| LED | Indikator |

> **Catatan:** Motor getar idealnya memakai transistor/driver bila arus besar.

---

## 4. Wiring (Koneksi)

### I2C (Shared)
- **SDA** → A4 (Uno)
- **SCL** → A5 (Uno)

### Pin Lain
- Motor getar → `D3`
- Tombol → `D4` (aktif LOW)
- LED → `D13`

---

## 5. Parameter Penting

### Mode Latih (Penilaian)
- `impactThreshold = 8000`  
  Ambang deteksi impact (lonjakan perubahan getaran)
- `armThreshold = 10000`  
  Ambang kekuatan gerak pada salah satu sumbu (ax/ay/az)
- `targetInterval = 3000 ms`  
  Interval target latihan
- `tolerance = 1000 ms`  
  Toleransi timing ±1 detik

### Mode Coach
- Level 1: **90 BPM** (≈ 666 ms)
- Level 2: **105 BPM** (≈ 571 ms)
- Level 3: **125 BPM** (≈ 480 ms)
- `PHRASE_LEN = 8` (panjang pattern)
- `PHRASES_PER_LEVEL = 1`

---

## 6. Cara Kerja Mode Latih (SEMPURNA vs SALAH)

### 6.1 Deteksi Impact (Perubahan Getaran)
1. Baca akselerasi MPU6050:
   ```cpp
   mpu.getAcceleration(&ax, &ay, &az);
````

2. Hitung **magnitude** (kekuatan gerak total):
   [
   Mag = \sqrt{ax^2 + ay^2 + az^2}
   ]
3. Hitung **perubahan getaran (jerk)**:
   [
   Jerk = |Mag_{sekarang} - Mag_{sebelumnya}|
   ]
4. Impact terdeteksi jika:

   * `jerk > impactThreshold`
   * dan **cooldown** 1 detik terpenuhi

### 6.2 Penilaian

Setelah impact:

* **Timing benar** jika:

  ```cpp
  abs(currentTime - nextBeatTime) <= tolerance
  ```
* **Gerak tangan benar** jika:

  ```cpp
  abs(ax) > armThreshold || abs(ay) > armThreshold || abs(az) > armThreshold
  ```

**SEMPURNA** = timing benar **dan** gerak benar
**SALAH** = salah satu tidak terpenuhi

---

## 7. Cara Kerja Mode Coach

* Motor bergetar mengikuti **BPM** dan **pattern** per level.
* Beat diatur oleh `g_beatIntervalMs`.
* Jenis getaran:

  * `HAPTIC_PULSE` (pendek)
  * `HAPTIC_LONG` (panjang)
  * `HAPTIC_DOUBLE` (dua kali)
* LCD:

  * Baris 1: `Mode Coach`
  * Baris 2: status (Level / Haptic), dengan “hold” agar terbaca jelas.
* Setelah Level 3 selesai → **Coach Stopped**.

---

## 8. Cara Pakai (Quick Start)

1. Rangkai hardware sesuai wiring.
2. Upload sketch ke Arduino.
3. Awal menyala di **Mode Latih** (`Ready`).
4. Tekan tombol → masuk **Mode Coach** (motor mulai bergetar).
5. Tekan tombol lagi → kembali ke **Mode Latih**.

---

## 9. Troubleshooting

* **LCD tidak tampil / aneh**: cek alamat (`0x27`/`0x3F`), SDA/SCL, power.
* **MPU6050 tidak terbaca**: cek alamat `0x68`, koneksi I2C.
* **Sering SALAH**:

  * Sesuaikan `impactThreshold` / `armThreshold`
  * Cek `tolerance`
  * Kurangi noise (getaran non-tepukan)

---

## 10. Pengembangan Lanjutan (Opsional)

* Tambah filter (low-pass) akselerasi
* Gunakan orientasi/gyro untuk validasi posisi tangan
* Gunakan **metronome absolut** (jadwal tetap) untuk penilaian timing
* Sinkronkan `targetInterval` dengan BPM musik

---

## 11. Lisensi

Bebas digunakan untuk pembelajaran dan pengembangan proyek sejenis.

```
```
