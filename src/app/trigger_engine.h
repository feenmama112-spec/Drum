// ===========================================================================
//  trigger_engine.h  --  the real-time piezo scanning task
// ===========================================================================
//  Runs on core 1 at priority 18 and owns the ADC bus.  It only does three
//  things: scan the two MCP3208s, run the per-pad detectors, and push trigger
//  messages into the audio queue.  No SD, no display, no allocation.
// ===========================================================================
#pragma once

#include <Arduino.h>

#include "app/shared.h"
#include "config.h"
#include "core/hihat.h"
#include "core/trigger.h"
#include "hal/adc_mcp3208.h"

namespace drum {

class AudioOut;
class VoicePool;

class TriggerEngine {
 public:
  void begin(Mcp3208Bus* adc, AudioOut* audio, VoicePool* pool, HiHatPedal* hh);
  bool startTask();

  void setThresholds(const uint16_t values[PAD_COUNT]);
  void requestCalibration() { calRequested_ = true; }
  uint32_t hits() const { return hits_; }

  // Used by the TEST PAD menu entry (called from the UI task).
  void testHit(uint8_t pad, uint8_t velocity);

 private:
  static void taskEntry(void* arg);
  void taskLoop();
  void runCalibration();
  void fire(uint8_t pad, uint8_t velocity);

  Mcp3208Bus* adc_ = nullptr;
  AudioOut* audio_ = nullptr;
  VoicePool* pool_ = nullptr;
  HiHatPedal* hh_ = nullptr;

  TriggerDetector det_[PAD_COUNT];
  uint16_t raw_[ADC_CHIPS][ADC_CHANNELS] = {};
  volatile bool calRequested_ = false;
  TaskHandle_t task_ = nullptr;
  uint32_t hits_ = 0;
};

}  // namespace drum
