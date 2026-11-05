// ===========================================================================
//  sample_loader.h  --  WAV -> RAM conversion loop (pure C++)
// ===========================================================================
//  Reads a parsed WAV through a ByteSource, downmixes to mono, resamples to
//  the engine rate, encodes to u-law (or keeps 16-bit) and writes straight
//  into the sample pool.  No dynamic allocation, bounded stack, and it can be
//  unit tested with a MemoryByteSource.
// ===========================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/wav.h"

namespace drum {

struct LoadResult {
  uint32_t frames = 0;      // frames actually stored
  uint32_t srcFrames = 0;   // frames in the file
  bool truncated = false;   // stopped because of maxFrames / buffer size
  bool ok = false;
};

// dst receives frames*bytesPerSample bytes.  maxFrames caps the stored length
// (0 = no cap beyond the destination size).
LoadResult loadWavToBuffer(ByteSource& src, const WavInfo& info, uint32_t dstRate,
                           uint32_t maxFrames, bool wide, uint8_t* dst, size_t dstCap);

// Bytes needed to store `frames` frames in the selected format.
size_t pcmBytesFor(uint32_t frames, bool wide);

// Peak level of a loaded sample as a Q15 normalisation factor (1.0 = 32767).
// Used by the serial diagnostics to spot silent or clipped sample files.
int16_t computePeakNormaliseQ15(const uint8_t* data, uint32_t frames, bool wide);

}  // namespace drum
