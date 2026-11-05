// ===========================================================================
//  mcp3208_frame.cpp
// ===========================================================================
#include "core/mcp3208_frame.h"

namespace drum {

void mcp3208BuildCommand(uint8_t channel, uint8_t out[3]) {
  const uint8_t ch = static_cast<uint8_t>(channel & 0x07);
  // byte0: 0 0 0 0 0 | START(1) | SGL(1) | D2
  out[0] = static_cast<uint8_t>(0x06 | ((ch >> 2) & 0x01));
  // byte1: D1 | D0 | don't care x6
  out[1] = static_cast<uint8_t>((ch & 0x03) << 6);
  // byte2: don't care (the conversion result is shifted in here)
  out[2] = 0x00;
}

uint16_t mcp3208Decode(const uint8_t in[3]) {
  return static_cast<uint16_t>(((static_cast<uint16_t>(in[1]) & 0x1F) << 7) |
                               (static_cast<uint16_t>(in[2]) >> 1));
}

}  // namespace drum
