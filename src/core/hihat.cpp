// ===========================================================================
//  hihat.cpp
// ===========================================================================
#include "core/hihat.h"

#include "config.h"

namespace drum {

void HiHatPedal::begin() { reset(); }

void HiHatPedal::reset() {
  pct_ = 0.0f;
  filtered_ = 0;
  primed_ = false;
  zone_ = HH_ZONE_CLOSED;
  lastAboveTrippMs_ = 0;
  tripArmed_ = false;
  chickArmed_ = true;
}

bool HiHatPedal::update(uint16_t raw, uint32_t nowMs, uint8_t* chickVelocity) {
  // 1st order low pass - a pedal is a slow signal, this removes ADC noise
  if (!primed_) {
    filtered_ = raw;
    primed_ = true;
  } else {
    const int32_t f = static_cast<int32_t>(filtered_) + ((static_cast<int32_t>(raw) - filtered_) >> 2);
    filtered_ = static_cast<uint16_t>(f < 0 ? 0 : (f > 4095 ? 4095 : f));
  }

  float pct = (static_cast<float>(filtered_) * 100.0f) / 4095.0f;
#if HH_PEDAL_INVERT
  pct = 100.0f - pct;
#endif
  pct_ = pct;

  // ---- zone with hysteresis -------------------------------------------------
  switch (zone_) {
    case HH_ZONE_OPEN:
      if (pct < HH_ZONE_OPEN_PCT - HH_ZONE_HYST_PCT) {
        zone_ = (pct < HH_ZONE_HALF_PCT) ? HH_ZONE_CLOSED : HH_ZONE_HALF;
      }
      break;
    case HH_ZONE_HALF:
      if (pct > HH_ZONE_OPEN_PCT + HH_ZONE_HYST_PCT) {
        zone_ = HH_ZONE_OPEN;
      } else if (pct < HH_ZONE_HALF_PCT - HH_ZONE_HYST_PCT) {
        zone_ = HH_ZONE_CLOSED;
      }
      break;
    case HH_ZONE_CLOSED:
    default:
      if (pct > HH_ZONE_HALF_PCT + HH_ZONE_HYST_PCT) {
        zone_ = (pct > HH_ZONE_OPEN_PCT) ? HH_ZONE_OPEN : HH_ZONE_HALF;
      }
      break;
  }

  // ---- foot "chick" ---------------------------------------------------------
  bool fired = false;
  if (pct >= HH_CHICK_TRIP_PCT) {
    lastAboveTrippMs_ = nowMs;
    tripArmed_ = true;
    chickArmed_ = true;  // the pedal must be released before the next chick
  } else if (tripArmed_ && chickArmed_ && pct <= HH_CHICK_FIRE_PCT) {
    const uint32_t dt = nowMs - lastAboveTrippMs_;
    if (dt >= HH_CHICK_MIN_MS && dt <= HH_CHICK_MAX_MS) {
      fired = true;
      chickArmed_ = false;
      if (chickVelocity != nullptr) *chickVelocity = HH_CHICK_VELOCITY;
    } else if (dt > HH_CHICK_MAX_MS) {
      tripArmed_ = false;  // too slow: the drummer just lowered the pedal
    }
  }
  return fired;
}

}  // namespace drum
