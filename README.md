# ESP32-S3 DIY Electronic Drum Module

แปลงกลองอะคูสติกเป็นกลองไฟฟ้า 10 ช่อง (9 piezo + 1 hi-hat pedal) ด้วย ESP32-S3 Super Mini
อ่าน WAV จาก microSD, เล่นผ่าน I2S (PCM5102A), แสดงผลบน OLED 128×64, ปรับ velocity ตามแรงตีจริง
รองรับ polyphony และเปลี่ยนชุดเสียงได้ 4 ชุด (POP / ROCK / HIPHOP / JAZZ)

---

## 1. สิ่งที่ส่งมอบ (ตรงตามข้อ 9 ของโจทย์)

| # | สิ่งที่ขอ | ไฟล์ |
|---|-----------|------|
| 1 | BOM พร้อมจำนวนและราคา | [`docs/01-bom.md`](docs/01-bom.md) |
| 2 | Wiring Diagram ทุกอุปกรณ์ | [`docs/02-wiring.md`](docs/02-wiring.md) |
| 3 | ตาราง GPIO ไม่ชนกัน | [`docs/03-gpio-map.md`](docs/03-gpio-map.md) |
| 4 | วงจรป้องกัน Piezo | [`docs/04-piezo-input.md`](docs/04-piezo-input.md) |
| 5 | โครงสร้างโฟลเดอร์โปรเจกต์ | หัวข้อ 3 ด้านล่าง |
| 6 | PlatformIO Configuration | [`platformio.ini`](platformio.ini) |
| 7 | Source code ทุกไฟล์ | [`src/`](src) + [`include/`](include) (39 ไฟล์, 3,579 บรรทัด) |
| 8 | วิธีติดตั้ง Library | หัวข้อ 4 |
| 9 | วิธีอัปโหลด Firmware | หัวข้อ 5 |
| 10 | วิธีทดสอบทีละส่วน | [`docs/06-testing.md`](docs/06-testing.md) |

เพิ่มเติมที่ให้มาด้วย: **ชุดเสียงทดสอบ 48 ไฟล์** (`sdcard/`), **ชุดทดสอบอัตโนมัติ 2,348 checks** (`tests/host/`),
**ตัวสร้างไฟล์เสียง** (`tools/make_test_wavs.py`)

---

## 2. การตัดสินใจเชิงวิศวกรรมที่สำคัญ (อ่านก่อนซื้อของ)

โจทย์มีจุดที่ขัดกันเองอยู่ 3 จุด ต่อไปนี้คือวิธีที่เลือกและเหตุผล

### 2.1 MCP3208 ต้องใช้ **2 ตัว** ไม่ใช่ 1

โจทย์บอก "MCP3208 จำนวน 1 ตัว" แต่ต้องการ 10 ช่อง (9 แพด + pedal) — MCP3208 1 ตัวมี 8 ช่อง
จึงขาด 2 ช่อง → ออกแบบให้ใช้ 2 ตัวบน SPI เดียวกัน (CS แยก, 16 ช่อง, เพิ่มเงิน 130 บาท)
ถ้ามีตัวเดียวจริง ๆ ดูวิธีแก้ใน [`docs/02-wiring.md`](docs/02-wiring.md) ข้อ 6

### 2.2 โหลด WAV เข้า RAM ทั้งชุด ไม่ได้ stream จาก SD ตอนเล่น

ถ้า stream จาก SD ตอนเล่น 10 voices ต้องใช้ 441 KB/s และ SD มี latency ที่ไม่คงที่ → เกิดเสียงแตก
ซึ่งโจทย์ห้ามไว้ จึงเลือก **โหลดเข้า RAM ตอนเลือกชุดเสียง** แล้วเล่นจาก RAM ล้วน ๆ
(ไม่มีทางสะดุดเพราะ SD เลย) โดยเก็บเป็น **u-law 8-bit** เพื่อลด RAM ครึ่งหนึ่ง

### 2.3 ไม่ใช้ PSRAM

บอร์ด Super Mini ส่วนใหญ่มี PSRAM 2 MB แต่โจทย์บอกไม่ให้สมมติว่ามี → ค่าเริ่มต้นจึงใช้ SRAM ภายใน
ทั้งหมด (pool 150 KB) และมี build `esp32s3-psram` ไว้ให้เลือกใช้ถ้าบอร์ดมี

---

## 3. โครงสร้างโปรเจกต์

```
.
├── platformio.ini              # 3 env: esp32s3, esp32s3-psram, native
├── include/
│   └── config.h                # ★ จุดเดียวที่แก้ขา/ค่าเวลา/บัฟเฟอร์
├── src/
│   ├── main.cpp                # boot sequence + super loop
│   ├── core/                   # ตรรกะบริสุทธิ์ (ไม่มี Arduino) → unit test ได้
│   │   ├── kit_defs.h/.cpp         ตารางแพด/ไฟล์เสียง/โซน hi-hat
│   │   ├── ulaw.h                  codec G.711 (ตาราง decode สร้างตอน compile)
│   │   ├── wav.h/.cpp              RIFF/WAVE parser
│   │   ├── converter.h/.cpp        downmix + resample (linear)
│   │   ├── sample_loader.h/.cpp    WAV → RAM (resample + u-law)
│   │   ├── sample_bank.h/.cpp      bump allocator ของ pool
│   │   ├── voice_pool.h/.cpp       มิกเซอร์ polyphonic + choke + steal
│   │   ├── trigger.h/.cpp          peak detect / velocity / retrigger mask
│   │   ├── hihat.h/.cpp            โซน pedal + ตรวจจับ chick
│   │   └── mcp3208_frame.h/.cpp    bit-exact SPI frame codec
│   ├── hal/                    # ชั้นฮาร์ดแวร์ (Arduino)
│   │   ├── adc_mcp3208.h/.cpp      SPI3 driver + self test
│   │   ├── audio_out.h/.cpp        I2S + audio task + คิวทริกเกอร์
│   │   ├── sd_manager.h/.cpp       mount + โหลดชุดเสียง
│   │   ├── ui_oled.h/.cpp          หน้าจอ SSD1306 ทุกหน้า
│   │   └── buttons.h/.cpp          debounce + long press
│   └── app/                    # การเชื่อมทุกส่วน
│       ├── shared.h/.cpp           สถานะที่แชร์ด้วย atomic
│       ├── trigger_engine.h/.cpp   trigger task (core 1)
│       ├── control.h/.cpp          ปุ่ม/เมนู/คิท/volume/NVS
│       └── debug.h/.cpp            คำสั่ง serial + สถิติ
├── docs/                       # เอกสาร 01-06 (ภาษาไทย)
├── sdcard/DRUM/{POP,ROCK,HIPHOP,JAZZ}/*.wav   # ชุดเสียงพร้อมใช้ 48 ไฟล์
├── tools/make_test_wavs.py     # สร้างชุดเสียงใหม่/ปรับแต่ง
└── tests/host/                 # unit test + ตรวจไฟล์เสียง (รันบน PC)
```

---

## 4. ติดตั้ง Library

### 4.1 PlatformIO (แนะนำ — ใช้ในโปรเจกต์นี้)

`platformio.ini` ระบุทุกอย่างไว้แล้ว ไม่ต้องติดตั้งอะไรด้วยมือ

```bash
pip install platformio          # ถ้ายังไม่มี
pio pkg install                 # ดึง platform + library ตาม platformio.ini
```

Library ที่ใช้ (ติดตั้งอัตโนมัติ): `Adafruit SSD1306`, `Adafruit GFX`, `Adafruit BusIO`
ที่เหลือ (`SD`, `SPI`, `Wire`, `Preferences`) มาจาก ESP32 core อยู่แล้ว

### 4.2 Arduino IDE (ถ้าไม่ใช้ PlatformIO)

1. Boards Manager → ติดตั้ง **esp32 by Espressif Systems**
2. Library Manager → ติดตั้ง `Adafruit SSD1306`, `Adafruit GFX Library`, `Adafruit BusIO`
3. ตั้งบอร์ดเป็น **ESP32S3 Dev Module** และ
   - Flash Size: **4MB (32Mb)**
   - Flash Mode: **QIO**
   - PSRAM: **Disabled** (หรือ QSPI PSRAM ถ้าบอร์ดมี)
   - USB CDC On Boot: **Enabled**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
4. คัดลอก `src/` + `include/` เข้าโฟลเดอร์ sketch ตามโครงสร้างเดิม

---

## 5. อัปโหลด Firmware

```bash
# 1) ต่อบอร์ดด้วย USB-C
pio run -e esp32s3 -t upload          # คอมไพล์ + อัปโหลด
pio device monitor -b 115200          # ดู log
```

ถ้าอัปโหลดไม่ขึ้น: กด **BOOT** ค้าง → กด **RESET** → ปล่อย BOOT แล้วสั่ง upload อีกครั้ง

คำสั่งที่มีประโยชน์

```bash
pio run                     # คอมไพล์อย่างเดียว
pio run -e esp32s3-psram -t upload   # ถ้าบอร์ดมี PSRAM (pool 1.4 MB)
bash tests/host/run_tests.sh         # ทดสอบ logic บน PC (ไม่ต้องมีบอร์ด)
python3 tools/make_test_wavs.py      # สร้างชุดเสียงทดสอบใหม่
```

**ผลการ build ของโปรเจกต์นี้** (วัดจริง): Flash 385,525 B (12.3% ของ 3 MB), static RAM 29,780 B (9.1%)

---

## 6. วิธีใช้งาน

### ปุ่ม 4 ตัว

| ปุ่ม | หน้า HOME | ในเมนู |
|------|-----------|--------|
| KIT | กดสั้น = ชุดถัดไป, กดค้าง = ชุดก่อนหน้า | ออก |
| MENU | เข้าเมนู | เลือก/ยืนยัน, กดค้าง = ออก |
| VOL− | ลดเสียง (ทีละ 5) | เลื่อนลง / เลือกแพดถัดไป |
| VOL+ | เพิ่มเสียง (ทีละ 5) | เลื่อนขึ้น / เลือกแพดก่อนหน้า |

### หน้าจอ

- **HOME** — `DRUM MODULE`, ชุดเสียง, สถานะ SD/AUDIO, ระดับเสียง (มี bar), โซน hi-hat, จำนวน voice, อัตรา scan
- **NOW PLAYING** (ขึ้นเองเมื่อตี 1.4 วินาที) — ชื่อชุด, ชื่อไฟล์ WAV, PAD, VELOCITY, AUDIO: PLAYING
- **MENU** — TEST PAD / SD INFO / CALIBRATE / DEBUG / EXIT
- **INFO / CAL** — รายละเอียดการ์ด, pool, heap / ผล calibration

### คำสั่ง Serial (115200)

`i` ข้อมูลระบบ · `m` หน่วยความจำ · `s` สถิติ · `c` calibrate · `t` ดู raw ADC 3 วิ · `p0`-`p8` ทดสอบเสียง · `d` เปิด/ปิดสถิติ · `h` help

---

## 7. ชุดเสียงบน SD card

```
/DRUM/POP/     SNARE.wav SNARE_RIM.wav TOM_HIGH.wav TOM_MID.wav TOM_LOW.wav
               FLOOR_TOM.wav HH_CLOSED.wav HH_HALF.wav HH_OPEN.wav
               HH_CHICK.wav CRASH.wav RIDE.wav
/DRUM/ROCK/    (ไฟล์ชุดเดียวกัน 12 ไฟล์)
/DRUM/HIPHOP/
/DRUM/JAZZ/
```

ข้อกำหนดไฟล์: **RIFF/WAVE, PCM, 16-bit, mono หรือ stereo, 22 050–48 000 Hz**
(ระบบ resample ให้เอง) ใช้ชื่อไฟล์ตัวพิมพ์ใหญ่ตามตารางเท่านั้น
ความยาวที่โหลดได้สูงสุดต่อไฟล์กำหนดไว้ที่ `SAMPLES[].maxMs` ใน `src/core/kit_defs.cpp`

---

## 8. สถานะการตรวจสอบ (ตรงไปตรงมา)

### ตรวจแล้วในสภาพแวดล้อมนี้

| รายการ | ผล |
|--------|-----|
| คอมไพล์ firmware (PlatformIO `espressif32@6.9.0` = arduino-esp32 2.0.17) | ✅ ผ่าน ไม่มี error (RAM 9.1%, Flash 12.3%) |
| Unit test ตรรกะหลักบน PC | ✅ **2,348 checks ผ่านทั้งหมด** |
| ตรวจไฟล์เสียง 48 ไฟล์ผ่านโค้ดโหลดจริง + เทียบงบ RAM | ✅ ผ่าน (ชุดใหญ่สุด 131,856 B จาก 150,000 B) |
| การคำนวณ bit alignment ของ MCP3208 (จำลอง DOUT ตาม datasheet ทุกค่า 0–4095) | ✅ ผ่านทั้ง 4,096 ค่า |
| ตัวสร้างชุดเสียง + ไฟล์ WAV 48 ไฟล์ | ✅ สร้างสำเร็จ เล่นได้ตามสเปก |

### **ยังไม่ได้ทดสอบ** (ไม่มีฮาร์ดแวร์ในสภาพแวดล้อมนี้)

- การทำงานบนบอร์ดจริง: I2S/PCM5102A, SPI จริงของ MCP3208, SD จริง, OLED จริง
- ค่า latency และ heap จริง (ต้องวัดด้วยคำสั่ง `m` / `s`)
- สาขาโค้ด arduino-esp32 **3.x** (`i2s_std.h`) — เขียนไว้รองรับและมี `#if` กำกับ แต่คอมไพล์ยืนยันเฉพาะ 2.x
  ถ้าใช้ core 3.x แล้วมีปัญหาที่ `audio_out.cpp` ให้แจ้งได้

ก่อนต่อ piezo เข้า ADC ควรทำ [`docs/06-testing.md`](docs/06-testing.md) ขั้นที่ 5–6 ให้ผ่านก่อน
และ **ห้ามต่อ piezo เข้า ADC โดยตรง** (piezo 27 mm ให้ spike ได้ 20–50 V → ชิปพัง)
