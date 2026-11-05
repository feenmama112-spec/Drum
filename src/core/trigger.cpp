// ===========================================================================
//  trigger.cpp
// ===========================================================================
#include "core/trigger.h"

#include <math.h>

#include "config.h"

namespace drum {
namespace {
constexpr uint16_t kFullScale = 4095;
}

uint8_t peakToVelocity(uint16_t peak, uint16_t threshold, float gamma) {
  if (peak <= threshold) return 1;
  const float span = static_cast<float>(kFullScale - threshold);
  if (span <= 0.0f) return 1;
  float lin = static_cast<float>(peak - threshold) / span;
  if (lin < 0.0f) lin = 0.0f;
  if (lin > 1.0f) lin = 1.0f;
  const float shaped = powf(lin, gamma);
  int v = 1 + static_cast<int>(shaped * 126.0f + 0.5f);
  if (v < 1) v = 1;
  if (v > 127) v = 127;
  return static_cast<uint8_t>(v);
}

void TriggerDetector::reset() {
  state_ = STATE_IDLE;
  peak_ = 0;
  aboveCount_ = 0;
  flatCount_ = 0;
  startMs_ = 0;
  lastHitMs_ = 0;
  everHit_ = false;
}

bool TriggerDetector::update(uint16_t raw, uint32_t nowMs, TriggerEvent* ev) {
  switch (state_) {
    case STATE_IDLE: {
      if (raw <= params_.threshold) return false;
      // retrigger masking: a bouncing piezo or a double stroke inside the mask
      // window must not produce a second note
      if (everHit_ && (nowMs - lastHitMs_) < params_.maskMs) return false;

      state_ = STATE_CAPTURE;
      startMs_ = nowMs;
      peak_ = raw;
      aboveCount_ = 1;
      flatCount_ = 0;
      return false;
    }

    case STATE_CAPTURE: {
      if (raw > params_.threshold) {
        if (aboveCount_ < 255) ++aboveCount_;
      }

      if (raw > peak_) {
        peak_ = raw;
        flatCount_ = 0;
      } else {
        if (flatCount_ < 255) ++flatCount_;
      }

      const bool elapsed = (nowMs - startMs_) >= HIT_PEAK_WINDOW_MS;
      // peak confirmed: enough samples seen and the signal is already falling
      const bool fallen = aboveCount_ >= HIT_MIN_SAMPLES && flatCount_ >= 2;
      if (!elapsed && !fallen) return false;

      lastPeak_ = peak_;
      if (ev != nullptr) {
        ev->pad = 0;  // filled in by the caller
        ev->peak = peak_;
        ev->velocity = peakToVelocity(peak_, params_.threshold, params_.gamma);
        ev->timeMs = nowMs;
      }
      ++hits_;
      everHit_ = true;
      lastHitMs_ = nowMs;
      state_ = STATE_IDLE;
      return true;
    }
  }
  return false;
}

}  // namespace drum
