// ===========================================================================
//  mcp3208_frame.h  --  bit-exact MCP3208 SPI frame codec (pure C++)
// ===========================================================================
//  Why this is a separate, unit-tested module: the 12-bit result is not
//  aligned to the byte boundaries of the SPI transfer, and getting the shift
//  wrong silently produces plausible-but-wrong velocity readings.
//
//  Datasheet "Figure 6-1: SPI communication using 8-bit segments (Mode 0,0)"
//  shows the MCU sending five leading zeros, then START, SGL/DIFF, D2, D1, D0:
//
//    clock           1  2  3  4  5  6  7  8  9 10 11 12 13 ... 16 17 ... 23 24
//    DIN             -  -  -  -  -  S SG D2 D1 D0  X  X  X      X  X      X  X
//    DOUT            high-Z...............   null B11 B10 ...  B7 B6 ...  B0 B1*
//                                                                       (*LSB-first echo)
//
//  Therefore, after a 3-byte transfer:
//    rx[1] bit4..bit0 = B11 B10 B9 B8 B7      (bit7/bit6 = high-Z, bit5 = null)
//    rx[2] bit7..bit1 = B6 B5 B4 B3 B2 B1 B0  (bit0 = LSB-first echo)
//
//    code = ((rx[1] & 0x1F) << 7) | (rx[2] >> 1)
// ===========================================================================
#pragma once

#include <stdint.h>

namespace drum {

constexpr uint16_t MCP3208_MAX_CODE = 4095;

// Fills out[3] with the command for a single-ended conversion on channel 0..7.
void mcp3208BuildCommand(uint8_t channel, uint8_t out[3]);

// Extracts the 0..4095 conversion result from the three received bytes.
uint16_t mcp3208Decode(const uint8_t in[3]);

}  // namespace drum
