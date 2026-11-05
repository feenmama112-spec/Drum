// ===========================================================================
//  ui_oled.cpp
// ===========================================================================
#include "hal/ui_oled.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal/audio_out.h"
#include "hal/sd_manager.h"

namespace drum {
namespace {
constexpr uint8_t kRowH = 9;
}

bool UiOled::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  if (!display_.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("[ui] SSD1306 not found - check SDA/SCL and the address (0x3C/0x3D)");
    ok_ = false;
    return false;
  }
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.cp437(true);
  display_.display();
  ok_ = true;
  return true;
}

void UiOled::bind(SdManager* sd, AudioOut* audio, VoicePool* pool, SampleBank* bank) {
  sd_ = sd;
  audio_ = audio;
  pool_ = pool;
  bank_ = bank;
}

void UiOled::text(uint8_t row, const char* fmt, ...) {
  char buf[32];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  display_.setCursor(0, row * kRowH);
  display_.print(buf);
}

void UiOled::header(const char* title, const char* right) {
  display_.fillRect(0, 0, OLED_WIDTH, 8, SSD1306_WHITE);
  display_.setTextColor(SSD1306_BLACK);
  display_.setCursor(1, 0);
  display_.print(title);
  if (right != nullptr) {
    const int16_t w = strlen(right) * 6;
    display_.setCursor(OLED_WIDTH - w - 1, 0);
    display_.print(right);
  }
  display_.setTextColor(SSD1306_WHITE);
}

void UiOled::bar(uint8_t y, uint8_t pct) {
  if (pct > 100) pct = 100;
  const uint8_t w = 60;
  display_.drawRect(64, y, w, 7, SSD1306_WHITE);
  display_.fillRect(65, y + 1, static_cast<int16_t>((w - 2) * pct / 100), 5, SSD1306_WHITE);
}

void UiOled::showMessage(const char* title, const char* line1, const char* line2) {
  strncpy(msgTitle_, title ? title : "", sizeof(msgTitle_) - 1);
  strncpy(msgLine1_, line1 ? line1 : "", sizeof(msgLine1_) - 1);
  strncpy(msgLine2_, line2 ? line2 : "", sizeof(msgLine2_) - 1);
  messageMode_ = true;
  dirty_ = true;
  render();
}

void UiOled::render() {
  if (!ok_) return;

  if (messageMode_) {
    display_.clearDisplay();
    header(msgTitle_, nullptr);
    text(2, "%s", msgLine1_);
    text(4, "%s", msgLine2_);
    display_.display();
    messageMode_ = false;
    return;
  }

  const uint8_t page = state().page.load();
  if (page == lastPage_ && !dirty_) return;
  lastPage_ = page;
  dirty_ = false;

  display_.clearDisplay();
  switch (page) {
    case UI_HIT:  drawHit();  break;
    case UI_MENU: drawMenu(); break;
    case UI_TEST: drawTest(); break;
    case UI_INFO: drawInfo(); break;
    case UI_CAL:  drawCal();  break;
    case UI_HOME:
    default:      drawHome(); break;
  }
  display_.display();
}

void UiOled::drawHome() {
  const uint8_t kit = state().kit.load();
  header("DRUM MODULE", KIT_NAMES[kit < 4 ? kit : 0]);

  text(1, "KIT   %s", KIT_NAMES[kit < 4 ? kit : 0]);
  text(2, "SD    %s", state().sdOk.load() ? "OK" : "ERROR");
  text(3, "AUDIO %s", state().audioOk.load() ? "OK" : "ERROR");

  text(4, "VOL   %u%%", static_cast<unsigned>(state().volume.load()));
  bar(4 * kRowH, state().volume.load());

  const HiHatZone z = static_cast<HiHatZone>(state().hhZone.load());
  text(5, "HH %s %2u%%", hiHatZoneName(z), static_cast<unsigned>(state().hhPct.load()));

  text(6, "VOICES %u   ADC %luk", static_cast<unsigned>(state().activeVoices.load()),
       static_cast<unsigned long>(state().adcRateHz.load() / 1000));
}

void UiOled::drawHit() {
  const uint8_t kit = state().kit.load();
  const uint8_t sample = state().lastSample.load();
  const uint8_t pad = state().lastPad.load();

  char right[12];
  snprintf(right, sizeof(right), "%s KIT", KIT_NAMES[kit < 4 ? kit : 0]);
  header("NOW PLAYING", nullptr);

  display_.setTextSize(2);
  display_.setCursor(0, 11);
  display_.print(right);
  display_.setTextSize(1);

  // The WAV file name, exactly as the spec asks for
  text(3, "%s", sample < S_COUNT ? sampleFile(static_cast<SampleId>(sample)) : "---");

  text(4, "PAD: %s", pad < PAD_COUNT ? padName(static_cast<PadId>(pad)) : "---");
  text(5, "VELOCITY: %u", static_cast<unsigned>(state().lastVelocity.load()));
  text(6, "AUDIO: %s", state().activeVoices.load() > 0 ? "PLAYING" : "IDLE");
}

void UiOled::drawMenu() {
  header("MENU", "HOLD=BACK");
  static const char* labels[MENU_ITEM_COUNT] = {"TEST PAD", "SD INFO", "CALIBRATE", "DEBUG", "EXIT"};
  const uint8_t sel = state().menuIndex.load();

  for (uint8_t i = 0; i < MENU_ITEM_COUNT; ++i) {
    const uint8_t row = static_cast<uint8_t>(1 + i);
    if (i == sel) {
      display_.fillRect(0, row * kRowH - 1, OLED_WIDTH, kRowH, SSD1306_WHITE);
      display_.setTextColor(SSD1306_BLACK);
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%c %s", i == sel ? '>' : ' ', labels[i]);
    display_.setCursor(2, row * kRowH);
    display_.print(buf);
    if (i == sel) display_.setTextColor(SSD1306_WHITE);
  }
  if (sel == MENU_DEBUG) {
    display_.setCursor(96, 4 * kRowH);
    display_.print(state().debugEnabled.load() ? "ON" : "OFF");
  }
}

void UiOled::drawTest() {
  header("TEST PAD", "HOLD=BACK");
  const uint8_t sel = state().testPad.load();
  text(1, "SELECT: %s", padName(static_cast<PadId>(sel)));
  text(3, "PRESS MENU TO PLAY");
  text(5, "KIT %s", KIT_NAMES[state().kit.load() % 4]);
  text(6, "VEL %u   VOICES %u", 100, static_cast<unsigned>(state().activeVoices.load()));
}

void UiOled::drawInfo() {
  header("SD INFO", nullptr);
  if (sd_ == nullptr || !sd_->mounted()) {
    text(2, "NO CARD");
    text(4, "Insert a FAT32 card");
    text(5, "with /DRUM/<KIT>/");
    return;
  }
  text(1, "CARD %s", sd_->typeName());
  text(2, "SIZE %llu MB", static_cast<unsigned long long>(sd_->cardSizeBytes() >> 20));
  text(3, "USED %llu MB", static_cast<unsigned long long>(sd_->cardUsedBytes() >> 20));
  text(4, "SAMPLES %u/12", static_cast<unsigned>(state().kitLoaded.load()));
  text(5, "POOL %uk / %uk", static_cast<unsigned>(state().poolUsed.load() / 1024),
       static_cast<unsigned>(state().poolCapacity.load() / 1024));
  text(6, "HEAP %uk", static_cast<unsigned>(ESP.getFreeHeap() / 1024));
}

void UiOled::drawCal() {
  header("CALIBRATE", nullptr);
  if (state().calibrating.load()) {
    text(2, "BE QUIET...");
    bar(4 * kRowH, state().calProgress.load());
    text(6, "%u%%", static_cast<unsigned>(state().calProgress.load()));
    return;
  }
  text(1, "MEASURED THRESHOLDS");
  const char* shortNames[PAD_COUNT] = {"SNA", "RIM", "TMH", "TMM", "TML", "FTM", "HH", "CRS", "RID"};
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    const uint8_t row = static_cast<uint8_t>(2 + p / 3);
    const uint8_t col = static_cast<uint8_t>((p % 3) * 42);
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%u", shortNames[p],
             static_cast<unsigned>(state().threshold[p].load()));
    display_.setCursor(col, row * kRowH);
    display_.print(buf);
  }
  text(6, "HOLD MENU: BACK");
}

}  // namespace drum
