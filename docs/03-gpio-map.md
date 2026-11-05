# 03 — ตาราง GPIO (ไม่ชนกัน)

อ้างอิงจาก pinout จริงของ **ESP32-S3 Super Mini** (38 ขา, 35 GPIO: GPIO1–18, 21, 33–48)
บอร์ดรุ่นนี้มี 18 ขาเป็น header ด้านข้าง (GPIO1–13 + TX/RX + ไฟ) และ 20 ขาเป็น pad ด้านล่าง

## 1. ตารางที่ใช้จริงในโปรเจกต์นี้

| GPIO | สัญญาณ | ทิศทาง | อยู่ที่ | หมายเหตุ |
|-----:|--------|--------|---------|----------|
| 12 | SD_SCK | out | header | FSPICLK (IO-MUX ตรงของ SPI2) |
| 13 | SD_MISO | in | header | FSPIQ |
| 11 | SD_MOSI | out | header | FSPID |
| 10 | SD_CS | out | header | FSPICS0, active low |
| 4 | ADC_SCK | out | header | SPI3 (HSPI) แยกบัสจาก SD |
| 6 | ADC_MISO | in | header | ต่อจาก DOUT ของ MCP3208 ทั้งสองตัว |
| 5 | ADC_MOSI | out | header | ต่อเข้า DIN ของ MCP3208 ทั้งสองตัว |
| 7 | ADC_CS0 | out | header | chip 0 = แพดทั้ง 9 |
| 21 | ADC_CS1 | out | **pad ด้านล่าง** | chip 1 = pedal + สำรอง |
| 15 | I2S_BCK | out | **pad ด้านล่าง** | → PCM5102A BCK |
| 16 | I2S_LRCK | out | **pad ด้านล่าง** | → PCM5102A LRCK |
| 17 | I2S_DIN | out | **pad ด้านล่าง** | → PCM5102A DIN |
| 8 | I2C_SDA | bidir | header | OLED SDA |
| 9 | I2C_SCL | out | header | OLED SCL |
| 1 | BTN_KIT | in | header | pull-up ภายใน, กด = LOW |
| 2 | BTN_MENU | in | header | pull-up ภายใน |
| 14 | BTN_VOL_DOWN | in | **pad ด้านล่าง** | pull-up ภายใน |
| 18 | BTN_VOL_UP | in | **pad ด้านล่าง** | pull-up ภายใน |
| 38 | SD_DETECT | in | **pad ด้านล่าง** | ไม่ได้ต่อในโค้ด (`PIN_SD_DETECT -1`) |

**สรุป: ใช้ 18 GPIO จาก 35 ที่มี** เหลืออีก 17 ขาไว้ต่อเพิ่ม (จอ SPI, encoder, MIDI ฯลฯ)

## 2. ขาที่ห้ามใช้ (และเหตุผล)

| GPIO | เหตุผล |
|-----:|--------|
| 26–32 | ต่ออยู่กับ flash ภายในชิป — **ไม่ถูกนำออกมาเป็นขา** บนบอร์ดนี้ |
| 19, 20 | USB D-/D+ (native USB) — ไม่ถูกนำออกมา |
| 43, 44 | UART0 TX/RX = serial debug/flash → เก็บไว้ดู log |
| 0 | ปุ่ม BOOT (strapping) |
| 3 | strapping (เลือก JTAG source) — ใช้ได้แต่เสี่ยงถ้ากดค้างตอนบูต |
| 45 | strapping (VDD_SPI 1.8/3.3 V) — ห้ามดึงตอนบูต |
| 46 | strapping (boot mode + ROM log) — เป็น input only |
| 48 | LED RGB ออนบอร์ด (แชร์กับ LED แดง) |
| 39, 40, 41, 42 | JTAG (TCK/TDO/TDI/TMS) — ใช้ได้ถ้าไม่ต้อง debug ด้วย JTAG |
| 33–37 | บนชิปรุ่น octal-PSRAM ขาเหล่านี้ถูกใช้ → เลี่ยงเพื่อความเข้ากันได้ |

## 3. ทำไมเลือกขาเหล่านี้ (เหตุผลเชิงเทคนิค)

1. **SD ใช้ GPIO 10–13** ซึ่งเป็นขา IO-MUX ของ FSPI โดยตรง → เร็วที่สุดและไม่ต้องพึ่ง GPIO matrix
2. **ADC ใช้ SPI3 แยกบัสโดยสิ้นเชิง** (คนละ controller กับ SD) → การอ่าน SD ตอนโหลดคิทไม่สามารถหน่วงการ scan piezo ได้ตามข้อกำหนด
3. **I2S อยู่ห่างจาก SPI** เพื่อลด crosstalk บน PCB และเดินสายสั้นไปยัง DAC
4. **ปุ่มทั้งหมดใช้ pull-up ภายใน** ไม่ต้องเพิ่มตัวต้านทานภายนอก และอ่านด้วย digitalRead ธรรมดา
5. **ไม่ใช้ GPIO3/45/46 เลย** → บูตได้เสมอแม้กดปุ่มค้างตอนเปิดเครื่อง

## 4. ตาราง ADC → แพด

| MCP3208 | CH | สัญญาณ | PadId |
|---------|----|--------|-------|
| 0 (CS=7) | 0 | Snare | 0 |
| 0 | 1 | Snare Rim | 1 |
| 0 | 2 | High Tom | 2 |
| 0 | 3 | Mid Tom | 3 |
| 0 | 4 | Low Tom | 4 |
| 0 | 5 | Floor Tom | 5 |
| 0 | 6 | Hi-hat | 6 |
| 0 | 7 | Crash | 7 |
| 1 (CS=21) | 0 | Ride | 8 |
| 1 | 1 | Hi-hat pedal | (ไม่ใช่ pad) |
| 1 | 2–7 | สำรอง / ปุ่มแบบ analog ladder | — |

แก้ไขตารางนี้ได้ที่ `src/core/kit_defs.cpp` (`PADS[]`) และ `PEDAL_CHIP/PEDAL_CHANNEL`

## 5. ถ้าจะใช้บอร์ดอื่น

ตารางขาอยู่ที่ `include/config.h` ที่เดียว ทั้งโปรเจกต์อ้างถึงค่าคงที่ `PIN_*`
ถ้าย้ายไป ESP32-S3 DevKitC หรือบอร์ดที่มี PSRAM ให้แก้แค่ไฟล์นั้น (และห้ามชนกับขา flash 26–32)
