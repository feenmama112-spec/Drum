// ===========================================================================
//  ui_oled.h  --  SSD1306 128x64 user interface
// ===========================================================================
//  Pages (see app/shared.h UiPage):
//    HOME  : DRUM MODULE / KIT / SD / AUDIO / VOLUME / hi-hat / scan rate
//    HIT   : "NOW PLAYING" with the WAV file name, pad, velocity (spec layout)
//    MENU  : test pad / SD info / calibrate / debug / exit
//    TEST  : pick a pad and fire a test hit at a fixed velocity
//    INFO  : card, sample pool and heap usage
//    CAL   : calibration progress and the resulting thresholds
// ===========================================================================
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "app/shared.h"
#include "config.h"
#include "core/kit_defs.h"

namespace drum {

class SdManager;
class AudioOut;
class VoicePool;
class SampleBank;

class UiOled {
 public:
  bool begin();
  void bind(SdManager* sd, AudioOut* audio, VoicePool* pool, SampleBank* bank);

  void render();
  void markDirty() { dirty_ = true; }

  // Full screen message used during boot and kit loading
  void showMessage(const char* title, const char* line1, const char* line2 = nullptr);

  bool available() const { return ok_; }

 private:
  void drawHome();
  void drawHit();
  void drawMenu();
  void drawTest();
  void drawInfo();
  void drawCal();
  void text(uint8_t row, const char* fmt, ...);
  void header(const char* title, const char* right);
  void bar(uint8_t y, uint8_t pct);

  Adafruit_SSD1306 display_{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
  bool ok_ = false;
  bool dirty_ = true;
  uint8_t lastPage_ = 255;

  SdManager* sd_ = nullptr;
  AudioOut* audio_ = nullptr;
  VoicePool* pool_ = nullptr;
  SampleBank* bank_ = nullptr;

  char msgTitle_[20] = {0};
  char msgLine1_[24] = {0};
  char msgLine2_[24] = {0};
  bool messageMode_ = false;
};

}  // namespace drum
