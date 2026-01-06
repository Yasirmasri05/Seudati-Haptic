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
