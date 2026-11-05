// ===========================================================================
//  hihat.h  --  hi-hat pedal position, zone and "chick" detection (pure C++)
// ===========================================================================
//  Position comes from a potentiometer or Hall sensor on the pedal, read by
//  the ADC.  0 % = pedal fully down (closed), 100 % = released (open).
//  Zones have hysteresis so a pedal resting on a border cannot flap between
//  two samples while the drummer keeps time.
// ===========================================================================
#pragma once

#include <stdint.h>

#include "core/kit_defs.h"

namespace drum {

class HiHatPedal {
 public:
  void begin();
  void reset();

  // Feeds a raw 0..4095 pedal reading.  Returns true when the pedal was
  // stepped down fast enough to count as a foot "chick"; the velocity for it
  // is written to *chickVelocity.
  bool update(uint16_t raw, uint32_t nowMs, uint8_t* chickVelocity);

  float positionPct() const { return pct_; }
  HiHatZone zone() const { return zone_; }
  SampleId sampleForZone() const { return hiHatSampleForPosition(pct_); }
  uint16_t rawFiltered() const { return filtered_; }

 private:
  float pct_ = 0.0f;
  uint16_t filtered_ = 0;
  bool primed_ = false;
  HiHatZone zone_ = HH_ZONE_CLOSED;
  uint32_t lastAboveTrippMs_ = 0;
  bool tripArmed_ = false;
  bool chickArmed_ = true;
};

}  // namespace drum
