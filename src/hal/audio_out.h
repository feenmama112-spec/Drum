// ===========================================================================
//  audio_out.h  --  I2S output + the real-time mixer task
// ===========================================================================
//  PCM5102A is used as a 3-wire I2S DAC (its SCK pin tied to GND, the internal
//  PLL derives the master clock from BCK), so only BCK/LRCK/DIN are wired.
//
//  Task layout
//    core 0, prio 19 : audioTask  - renders one 128 frame block, writes it to
//                       the I2S DMA queue and blocks there, which paces the
//                       engine at exactly the sample rate.
//    core 1, prio 18 : triggerTask (see trigger_engine.h)
//    core 1, prio  1 : Arduino loopTask - UI, buttons, serial
//
//  Latency budget: 3 DMA blocks of 128 frames at 22 050 Hz = 17.4 ms of
//  buffering, plus ~2-4 ms of trigger detection: ~20 ms end to end.
//
//  I2S API: the legacy driver (driver/i2s.h) is used for arduino-esp32 2.x
//  because that is what PlatformIO ships with platform espressif32@6.9.0.
//  With arduino-esp32 3.x the same file switches to driver/i2s_std.h
//  automatically (see the #if in audio_out.cpp) - nothing else changes.
// ===========================================================================
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config.h"
#include "core/voice_pool.h"

namespace drum {

struct TriggerMsg {
  uint8_t sample = 0;    // SampleId
  uint8_t velocity = 0;  // 1..127
  uint8_t chokeGroup = 0;
};

class AudioOut {
 public:
  bool begin(VoicePool* pool);

  // Thread safe: called from the trigger task.
  bool post(const TriggerMsg& msg);

  // Kit switching: the audio task stops every voice and outputs silence so the
  // sample pool can be rewritten safely.  Blocks for at most ~60 ms.
  void beginKitChange();
  void endKitChange();

  bool ready() const { return ready_; }
  uint32_t writeErrors() const { return writeErrors_; }
  uint32_t blocksRendered() const { return blocks_; }
  uint32_t queueDrops() const { return drops_; }
  uint32_t dmaLatencyUs() const;

 private:
  static void taskEntry(void* arg);
  void taskLoop();
  void writeSilence();
  bool initI2s();

  VoicePool* pool_ = nullptr;
  void* tx_ = nullptr;  // i2s_chan_handle_t* on core 3.x, unused on core 2.x
  QueueHandle_t queue_ = nullptr;
  TaskHandle_t task_ = nullptr;

  volatile bool holdReq_ = false;
  volatile bool holdAck_ = false;
  bool ready_ = false;

  int16_t mono_[AUDIO_BLOCK_FRAMES] = {0};
  int16_t stereo_[AUDIO_BLOCK_FRAMES * 2] = {0};

  uint32_t writeErrors_ = 0;
  uint32_t blocks_ = 0;
  uint32_t drops_ = 0;
};

}  // namespace drum
