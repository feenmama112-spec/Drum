// ===========================================================================
//  ulaw.h  --  G.711 u-law codec used to halve the RAM cost of the samples
// ===========================================================================
//  Drum one-shots are short, loud transients.  u-law gives them ~12 bits of
//  effective dynamic range in 8 bits per sample, which is inaudible against
//  the noise floor of a piezo pickup, and doubles the number of seconds that
//  fit into the internal SRAM of the ESP32-S3.
//
//  The decode table is built at compile time (constexpr), so there is no
//  initialisation order problem and the mixer is a single table lookup.
// ===========================================================================
#pragma once

#include <stdint.h>

namespace drum {

namespace ulaw_detail {

constexpr int16_t decodeOne(uint8_t code) {
  const uint8_t u = static_cast<uint8_t>(~code);
  int32_t t = (static_cast<int32_t>(u & 0x0F) << 3) + 0x84;
  t <<= ((u & 0x70) >> 4);
  return static_cast<int16_t>((u & 0x80) ? (0x84 - t) : (t - 0x84));
}

struct Table {
  int16_t v[256];
  constexpr Table() : v() {
    for (int i = 0; i < 256; ++i) v[i] = decodeOne(static_cast<uint8_t>(i));
  }
};

inline constexpr Table kTable{};

}  // namespace ulaw_detail

inline int16_t ulawDecode(uint8_t code) { return ulaw_detail::kTable.v[code]; }

// Encodes one 16-bit PCM sample. Only used while a kit is loaded from the SD
// card, so a small loop is preferable to a 128-entry table in flash.
inline uint8_t ulawEncode(int16_t pcm) {
  int32_t p = pcm;
  uint8_t sign = 0;
  if (p < 0) {
    sign = 0x80;
    p = -p;
  }
  if (p > 32635) p = 32635;  // clip to full scale
  p += 0x84;                 // BIAS

  uint8_t exponent = 7;
  for (uint32_t mask = 0x4000; exponent > 0 && (p & mask) == 0; mask >>= 1) --exponent;

  const uint8_t mantissa = static_cast<uint8_t>((p >> (exponent + 3)) & 0x0F);
  return static_cast<uint8_t>(~(sign | static_cast<uint8_t>(exponent << 4) | mantissa));
}

}  // namespace drum
