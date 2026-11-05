// ===========================================================================
//  trigger.h  --  piezo hit detection, peak tracking and velocity (pure C++)
// ===========================================================================
//  One TriggerDetector per pad.  Fed with raw 12-bit ADC samples at the scan
//  rate (~1.7 kHz per channel).
//
//  A piezo hit is a spike that rises in well under a millisecond and then
//  decays.  The detector waits for the signal to fall back after the peak (or
//  for HIT_PEAK_WINDOW_MS to expire) so the velocity uses the true peak and
//  not the first sample above the threshold - which would make every hit
//  velocity 1.
// ===========================================================================
#pragma once

#include <stdint.h>

namespace drum {

struct TriggerParams {
  uint16_t threshold = 100;  // raw ADC counts
  uint16_t maskMs = 25;      // retrigger masking
  float gamma = 0.65f;       // < 1 gives more resolution at low velocity
};

struct TriggerEvent {
  uint8_t pad = 0;
  uint8_t velocity = 1;  // 1..127
  uint16_t peak = 0;     // raw ADC peak
  uint32_t timeMs = 0;
};

class TriggerDetector {
 public:
  void configure(const TriggerParams& p) { params_ = p; reset(); }
  const TriggerParams& params() const { return params_; }
  void setThreshold(uint16_t t) { params_.threshold = t; }
  void reset();

  // Feed one sample.  Returns true when a complete hit is available in *ev.
  bool update(uint16_t raw, uint32_t nowMs, TriggerEvent* ev);

  uint16_t lastPeak() const { return lastPeak_; }
  uint32_t hitCount() const { return hits_; }
  bool busy() const { return state_ != STATE_IDLE; }

 private:
  enum State : uint8_t { STATE_IDLE = 0, STATE_CAPTURE = 1 };

  TriggerParams params_;
  State state_ = STATE_IDLE;
  uint16_t peak_ = 0;
  uint16_t lastPeak_ = 0;
  uint8_t aboveCount_ = 0;
  uint8_t flatCount_ = 0;
  uint32_t startMs_ = 0;
  uint32_t lastHitMs_ = 0;
  bool everHit_ = false;
  uint32_t hits_ = 0;
};

// Maps a raw peak to a MIDI velocity using the calibrated threshold.
uint8_t peakToVelocity(uint16_t peak, uint16_t threshold, float gamma);

}  // namespace drum
