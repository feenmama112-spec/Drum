// ===========================================================================
//  kit_defs.cpp  --  pad / sample tables
// ===========================================================================
#include "core/kit_defs.h"

namespace drum {

// ADC channel map over the two MCP3208 chips.
//
//   chip 0 (CS = PIN_ADC_CS0) : all nine drum pads
//     CH0 SNARE      CH1 SNARE RIM   CH2 TOM HIGH   CH3 TOM MID
//     CH4 TOM LOW    CH5 FLOOR TOM   CH6 HI-HAT     CH7 CRASH
//   chip 1 (CS = PIN_ADC_CS1) : slow / auxiliary signals
//     CH0 RIDE       CH1 HI-HAT PEDAL   CH2..CH7 spare
//
// Putting every pad on chip 0 also means the 1-chip (8 pad) budget build only
// has to move RIDE and the pedal - see docs/02-wiring.md.
const PadDef PADS[PAD_COUNT] = {
    // name          chip ch  threshold mask voices choke
    {"SNARE",        0, 0, 120, 22, 2, 0},
    {"SNARE RIM",    0, 1, 100, 22, 2, 0},
    {"TOM HIGH",     0, 2, 110, 25, 2, 0},
    {"TOM MID",      0, 3, 110, 25, 2, 0},
    {"TOM LOW",      0, 4, 110, 25, 2, 0},
    {"FLOOR TOM",    0, 5, 110, 25, 2, 0},
    {"HI-HAT",       0, 6,  90, 18, 2, 1},
    {"CRASH",        0, 7, 130, 40, 2, 0},
    {"RIDE",         1, 0, 120, 40, 2, 0},
};

// maxMs caps how much of each file is cached in RAM.  Sum of maxMs x 22.05
// bytes/ms (u-law) is 143 kB, just below the 150 kB AUDIO_POOL_BYTES budget.
const SampleDef SAMPLES[S_COUNT] = {
    {"SNARE.wav",     "SNA",  500},
    {"SNARE_RIM.wav", "RIM",  350},
    {"TOM_HIGH.wav",  "TOMH", 450},
    {"TOM_MID.wav",   "TOMM", 450},
    {"TOM_LOW.wav",   "TOML", 500},
    {"FLOOR_TOM.wav", "FTOM", 600},
    {"HH_CLOSED.wav", "HHC",  200},
    {"HH_HALF.wav",   "HHH",  300},
    {"HH_OPEN.wav",   "HHO",  800},
    {"HH_CHICK.wav",  "CHK",  150},
    {"CRASH.wav",     "CRSH", 1000},
    {"RIDE.wav",      "RIDE", 1200},
};

const char* const KIT_NAMES[4]   = {"POP", "ROCK", "HIPHOP", "JAZZ"};
const char* const KIT_FOLDERS[4] = {"/DRUM/POP", "/DRUM/ROCK", "/DRUM/HIPHOP", "/DRUM/JAZZ"};

const char* padName(PadId p) {
  if (p >= PAD_COUNT) return "?";
  return PADS[p].name;
}

const char* sampleName(SampleId s) {
  if (s >= S_COUNT) return "?";
  return SAMPLES[s].shortName;
}

const char* sampleFile(SampleId s) {
  if (s >= S_COUNT) return "";
  return SAMPLES[s].fileName;
}

PadId padForSample(SampleId s) {
  switch (s) {
    case S_SNARE:     return PAD_SNARE;
    case S_SNARE_RIM: return PAD_SNARE_RIM;
    case S_TOM_HIGH:  return PAD_TOM_HIGH;
    case S_TOM_MID:   return PAD_TOM_MID;
    case S_TOM_LOW:   return PAD_TOM_LOW;
    case S_FLOOR_TOM: return PAD_FLOOR_TOM;
    case S_HH_CLOSED:
    case S_HH_HALF:
    case S_HH_OPEN:
    case S_HH_CHICK:  return PAD_HI_HAT;
    case S_CRASH:     return PAD_CRASH;
    case S_RIDE:      return PAD_RIDE;
    default:          return PAD_SNARE;
  }
}

SampleId padDefaultSample(PadId p) {
  switch (p) {
    case PAD_SNARE:     return S_SNARE;
    case PAD_SNARE_RIM: return S_SNARE_RIM;
    case PAD_TOM_HIGH:  return S_TOM_HIGH;
    case PAD_TOM_MID:   return S_TOM_MID;
    case PAD_TOM_LOW:   return S_TOM_LOW;
    case PAD_FLOOR_TOM: return S_FLOOR_TOM;
    case PAD_HI_HAT:    return S_HH_CLOSED;  // overridden by the pedal zone
    case PAD_CRASH:     return S_CRASH;
    case PAD_RIDE:      return S_RIDE;
    default:            return S_SNARE;
  }
}

SampleId hiHatSampleForPosition(float positionPct) {
  if (positionPct >= HH_ZONE_OPEN_PCT) return S_HH_OPEN;
  if (positionPct >= HH_ZONE_HALF_PCT) return S_HH_HALF;
  return S_HH_CLOSED;
}

const char* hiHatZoneName(HiHatZone z) {
  switch (z) {
    case HH_ZONE_OPEN: return "OPEN";
    case HH_ZONE_HALF: return "HALF";
    default:           return "CLOSED";
  }
}

}  // namespace drum
