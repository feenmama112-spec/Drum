// ===========================================================================
//  kit_defs.h  --  pad / sample tables (pure C++, no Arduino dependency)
// ===========================================================================
#pragma once

#include <stdint.h>

namespace drum {

// The nine physical pads.  Hi-hat is ONE pad that plays one of three samples
// depending on the pedal, so 9 piezos give the ten "channels" of the spec.
enum PadId : uint8_t {
  PAD_SNARE = 0,
  PAD_SNARE_RIM,
  PAD_TOM_HIGH,
  PAD_TOM_MID,
  PAD_TOM_LOW,
  PAD_FLOOR_TOM,
  PAD_HI_HAT,
  PAD_CRASH,
  PAD_RIDE,
  PAD_COUNT
};

// The twelve WAV files that make up one sound kit.
enum SampleId : uint8_t {
  S_SNARE = 0,
  S_SNARE_RIM,
  S_TOM_HIGH,
  S_TOM_MID,
  S_TOM_LOW,
  S_FLOOR_TOM,
  S_HH_CLOSED,
  S_HH_HALF,
  S_HH_OPEN,
  S_HH_CHICK,
  S_CRASH,
  S_RIDE,
  S_COUNT
};

struct PadDef {
  const char* name;       // shown on the OLED
  uint8_t     adcChip;    // 0 or 1
  uint8_t     adcChannel; // 0..7 on that chip
  uint16_t    defThreshold;
  uint8_t     maskMs;     // retrigger masking after a hit
  uint8_t     maxVoices;  // polyphony per pad
  uint8_t     chokeGroup; // 0 = none, 1 = hi-hat cymbal
};

struct SampleDef {
  const char* fileName;   // file in /DRUM/<KIT>/
  const char* shortName;  // OLED label
  uint16_t    maxMs;      // truncate longer files to this length
};

extern const PadDef    PADS[PAD_COUNT];
extern const SampleDef SAMPLES[S_COUNT];
extern const char* const KIT_NAMES[4];
extern const char* const KIT_FOLDERS[4];

// Analog inputs that are not pads
constexpr uint8_t PEDAL_CHIP    = 1;
constexpr uint8_t PEDAL_CHANNEL = 1;

// Hi-hat pedal zones, in percent of travel (0 = fully down, 100 = up).
// Hysteresis prevents the zone flapping when the pedal is parked on a border.
constexpr float   HH_ZONE_OPEN_PCT     = 70.0f;
constexpr float   HH_ZONE_HALF_PCT     = 32.0f;
constexpr float   HH_ZONE_HYST_PCT      = 6.0f;
constexpr float   HH_CHICK_TRIP_PCT    = 55.0f;
constexpr float   HH_CHICK_FIRE_PCT    = 25.0f;
constexpr uint32_t HH_CHICK_MIN_MS     = 40;
constexpr uint32_t HH_CHICK_MAX_MS    = 400;

const char* padName(PadId p);
const char* sampleName(SampleId s);
const char* sampleFile(SampleId s);

// Which pad produced a given sample (hi-hat zones map back to the hi-hat pad).
PadId padForSample(SampleId s);

// The sample a pad plays when the pedal position is not involved.
SampleId padDefaultSample(PadId p);

// Map the pedal position to the hi-hat sample that should be played.
SampleId hiHatSampleForPosition(float positionPct);

enum HiHatZone : uint8_t { HH_ZONE_CLOSED = 0, HH_ZONE_HALF = 1, HH_ZONE_OPEN = 2 };
const char* hiHatZoneName(HiHatZone z);

}  // namespace drum
