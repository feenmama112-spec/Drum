// ===========================================================================
//  voice_pool.h  --  polyphonic mixer over RAM resident samples (pure C++)
// ===========================================================================
//  Because every sample is already in RAM, starting a voice is a few integer
//  stores: no SD access, no allocation and no blocking, which is what makes
//  simultaneous pads and low latency possible on a 4 MB / no-PSRAM board.
// ===========================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "core/kit_defs.h"
#include "core/sample_bank.h"

namespace drum {

struct Voice {
  uint8_t sample = 0;
  uint8_t pad = 0;
  uint8_t chokeGroup = 0;
  bool active = false;
  uint32_t pos = 0;
  int32_t gainQ15 = 32768;
  uint8_t chokeFade = 0;  // 64 -> 0 : fade out over 64 frames (click free)
};

class VoicePool {
 public:
  void begin(const SampleBank* bank);
  void setMasterVolumeQ15(int32_t g);
  int32_t masterVolumeQ15() const { return masterQ15_; }

  // Returns false when the sample is not loaded in RAM.
  bool start(SampleId s, uint8_t midiVelocity);

  void chokeGroup(uint8_t group);  // fade out every voice of that choke group
  void stopAll();

  uint8_t activeCount() const;
  uint16_t activeMask() const;
  uint32_t startedCount() const { return started_; }
  uint32_t stolenCount() const { return stolen_; }
  uint32_t underrunCount() const { return underrun_; }

  // Fills `frames` mono samples (already mixed, saturated to int16).
  void render(int16_t* out, size_t frames);

  // The most recent trigger, for the OLED "NOW PLAYING" page.
  SampleId lastSample() const { return lastSample_; }
  uint8_t lastVelocity() const { return lastVelocity_; }
  uint8_t lastPanic() const { return 0; }  // placeholder for UI symmetry

 private:
  int findFree() const;
  int stealOldest() const;

  Voice voices_[AUDIO_MAX_VOICES];
  const SampleBank* bank_ = nullptr;
  int32_t masterQ15_ = 32768;
  uint16_t active_ = 0;
  uint32_t started_ = 0;
  uint32_t stolen_ = 0;
  uint32_t underrun_ = 0;
  SampleId lastSample_ = S_SNARE;
  uint8_t lastVelocity_ = 0;
};

}  // namespace drum
