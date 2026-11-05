// ===========================================================================
//  config.h  --  hardware map and tunables for the ESP32-S3 drum module
// ===========================================================================
//  Every pin below is checked against the ESP32-S3 Super Mini pinout:
//    * GPIO 26..32 carry the in-package flash      -> never used
//    * GPIO 19/20 are the native USB pads          -> not exposed, not used
//    * GPIO 43/44 are UART0 (serial debug)         -> kept free
//    * GPIO 0    is BOOT, 45/46 are boot strapping -> never used
//    * GPIO 48   drives the on-board RGB LED       -> never used
//  See docs/03-gpio-map.md for the full justification table.
// ===========================================================================
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// 1. PIN MAP
// ---------------------------------------------------------------------------

// --- MicroSD card (SPI2 / FSPI, native IO-MUX pins 10..13) -----------------
#define PIN_SD_SCK        12   // FSPICLK
#define PIN_SD_MISO       13   // FSPIQ
#define PIN_SD_MOSI       11   // FSPID
#define PIN_SD_CS         10   // FSPICS0
#define PIN_SD_DETECT    (-1)  // optional card-detect switch, -1 = not wired

// --- MCP3208 x2 ADC (SPI3 / HSPI, dedicated bus) ---------------------------
// Chip A (chip index 0) : 8 drum pads
// Chip B (chip index 1) : hi-hat pedal + spares
#define PIN_ADC_SCK        4
#define PIN_ADC_MISO       6
#define PIN_ADC_MOSI       5
#define PIN_ADC_CS0        7   // chip 0  (pads)
#define PIN_ADC_CS1       21   // chip 1  (pedal / aux)

// --- PCM5102A I2S DAC (3-wire, SCK pin of the DAC tied to GND) -------------
#define PIN_I2S_BCK       15
#define PIN_I2S_LRCK      16
#define PIN_I2S_DIN       17

// --- SSD1306 128x64 OLED (I2C) --------------------------------------------
#define PIN_I2C_SDA        8
#define PIN_I2C_SCL        9
#define OLED_I2C_ADDR    0x3C
#define OLED_WIDTH        128
#define OLED_HEIGHT        64

// --- Front panel buttons (active LOW, internal pull-up) --------------------
#define PIN_BTN_KIT        1   // short: next kit      long: previous kit
#define PIN_BTN_MENU       2   // short: enter/next    long: back / exit
#define PIN_BTN_VOL_DOWN  14   // volume -  / menu value -
#define PIN_BTN_VOL_UP    18   // volume +  / menu value +  / test hit
#define BTN_ACTIVE_LEVEL LOW
#define BTN_LONG_PRESS_MS 700
#define BTN_DEBOUNCE_MS    25

// ---------------------------------------------------------------------------
// 2. AUDIO ENGINE
// ---------------------------------------------------------------------------
//  The audio path is: WAV on SD -> (load time) linear resample to
//  AUDIO_SAMPLE_RATE, mono, 8-bit u-law (or 16-bit PCM) -> internal SRAM.
//  Playback never touches the SD card, so there is no way for a card stall to
//  interrupt the sound.  u-law is used because it halves the RAM cost of the
//  samples at a quality that is transparent for drum one-shots.
// ---------------------------------------------------------------------------
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE       22050      // Hz, mono, duplicated to L/R
#endif
#define AUDIO_BLOCK_FRAMES        128      // frames per mixer block (~5.8 ms)
#define AUDIO_DMA_DESC_NUM          3      // queued DMA blocks -> ~17 ms latency
#ifndef AUDIO_POOL_BYTES
#define AUDIO_POOL_BYTES       150000      // sample RAM budget (internal SRAM)
#endif
#define AUDIO_MAX_VOICES           12      // simultaneous voices
#define AUDIO_PAD_INSTANCES         2      // voices a single pad may use
#define AUDIO_VELOCITY_GAMMA      1.4f     // MIDI velocity 1..127 -> amplitude

// 8-bit u-law halves the RAM per sample.  Compile with -DAUDIO_16BIT=1 to keep
// full 16-bit PCM instead (same code path, twice the memory, ~2.2 s of audio).
#ifndef AUDIO_16BIT
#define AUDIO_16BIT                 0
#endif

// ---------------------------------------------------------------------------
// 3. TRIGGER ENGINE
// ---------------------------------------------------------------------------
#define ADC_SPI_HZ             1000000    // MCP3208: 1 MHz max @ 3.3 V (datasheet)
#define ADC_SCANS_PER_YIELD          3    // scans between vTaskDelay(1)
#define HIT_PEAK_WINDOW_MS           6    // max time to wait for the piezo peak
#define HIT_MIN_SAMPLES              2    // samples above threshold before firing
#define CAL_SAMPLES_MS             800    // noise-floor measurement time
#define CAL_MARGIN_COUNTS           45    // threshold = floor * mult + margin
#define CAL_MULTIPLIER             2.4f
#define THRESHOLD_MIN_COUNTS        40    // never accept a lower threshold
#define THRESHOLD_MAX_COUNTS      1200

// hi-hat pedal
//   zone thresholds and the chick timing live in src/core/kit_defs.h so the
//   pure logic module stays self contained; only board level options are here.
#define HH_CHICK_VELOCITY          100    // fixed velocity for the foot chick
#define HH_PEDAL_INVERT              0    // 1 if your pedal reads backwards

// ---------------------------------------------------------------------------
// 4. CONTROL / UI
// ---------------------------------------------------------------------------
#define UI_REFRESH_MS               40     // 25 fps
#define HIT_PAGE_HOLD_MS          1400     // "NOW PLAYING" page duration
#define DEBUG_STATS_MS            1000     // serial statistics period
#define KIT_AUTOLOAD_ON_BOOT         1

// NVS namespace used for calibration / volume / kit persistence
#define NVS_NAMESPACE          "drum"

// ---------------------------------------------------------------------------
// 5. COMPILE-TIME SANITY CHECKS  (fail the build, not the drummer)
// ---------------------------------------------------------------------------
#if AUDIO_SAMPLE_RATE < 16000 || AUDIO_SAMPLE_RATE > 48000
#error "AUDIO_SAMPLE_RATE must be 16000..48000 (SD bandwidth / I2S PLL limit)"
#endif
#if AUDIO_BLOCK_FRAMES < 32 || (AUDIO_BLOCK_FRAMES & 31) != 0
#error "AUDIO_BLOCK_FRAMES must be a multiple of 32"
#endif
#if AUDIO_DMA_DESC_NUM < 2
#error "AUDIO_DMA_DESC_NUM must be >= 2"
#endif
#if AUDIO_MAX_VOICES < 4 || AUDIO_MAX_VOICES > 24
#error "AUDIO_MAX_VOICES must be 4..24"
#endif
#if AUDIO_POOL_BYTES > 400000
#error "AUDIO_POOL_BYTES too large for internal SRAM without PSRAM"
#endif
#if PIN_ADC_CS0 == PIN_ADC_CS1
#error "The two MCP3208 need separate chip selects"
#endif
