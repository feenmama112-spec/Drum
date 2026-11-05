// ===========================================================================
//  control.h  --  buttons, menu, kit selection, volume, calibration glue
// ===========================================================================
#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "app/shared.h"
#include "config.h"
#include "hal/buttons.h"

namespace drum {

class AudioOut;
class SampleBank;
class SdManager;
class TriggerEngine;
class UiOled;
class VoicePool;

class Control {
 public:
  void begin(SdManager* sd, SampleBank* bank, AudioOut* audio, VoicePool* pool,
             TriggerEngine* trigger, UiOled* ui, Buttons* buttons);

  // Call as often as possible from loop(); it never blocks longer than the
  // ~1 s a kit change needs to read 12 files.
  void update();

  void restore();  // load volume / kit / thresholds from NVS
  void persist();

  void loadKit(uint8_t kit, bool showOnScreen = true);
  void setVolume(uint8_t volume);
  void startCalibration();

 private:
  void handleEvent(const BtnEvent& ev);
  void onHome(uint8_t id, bool longPress);
  void onMenu(uint8_t id, bool longPress);
  void onTest(uint8_t id, bool longPress);
  void applyVolume();
  void checkCalibrationFinished();

  SdManager* sd_ = nullptr;
  SampleBank* bank_ = nullptr;
  AudioOut* audio_ = nullptr;
  VoicePool* pool_ = nullptr;
  TriggerEngine* trigger_ = nullptr;
  UiOled* ui_ = nullptr;
  Buttons* buttons_ = nullptr;
  Preferences prefs_;

  bool calWasActive_ = false;
  volatile bool calDone_ = false;
};

}  // namespace drum
