// ===========================================================================
//  converter.cpp
// ===========================================================================
#include "core/converter.h"

#include <math.h>

namespace drum {
namespace {

// Reads frame `i` of the interleaved input as mono.
inline float monoAt(const int16_t* in, size_t i, uint8_t channels) {
  if (channels == 2) {
    return (static_cast<float>(in[2 * i]) + static_cast<float>(in[2 * i + 1])) * 0.5f;
  }
  return static_cast<float>(in[i]);
}

}  // namespace

void SampleConverter::begin(uint32_t srcRate, uint8_t channels, uint32_t dstRate) {
  ratio_ = (dstRate > 0) ? (static_cast<double>(srcRate) / static_cast<double>(dstRate)) : 1.0;
  channels_ = (channels == 2) ? 2 : 1;
  pos_ = 0.0;
  prev_ = 0.0f;
  hasPrev_ = false;
}

size_t SampleConverter::feed(const int16_t* in, size_t inFrames, uint32_t chunkStart, bool isLast,
                             int16_t* out, size_t outCap) {
  if (in == nullptr || inFrames == 0 || out == nullptr || outCap == 0) return 0;

  const int64_t start = static_cast<int64_t>(chunkStart);
  const int64_t end = start + static_cast<int64_t>(inFrames);
  size_t written = 0;

  while (written < outCap) {
    const int64_t i = static_cast<int64_t>(pos_);
    if (i >= end) break;

    float a = 0.0f;
    float b = 0.0f;

    if (i < start) {
      // The output position sits one frame before this chunk: the sample it
      // needs is the last frame of the previous chunk, which we carried over.
      // Without this, a file whose length is not a multiple of the chunk size
      // would stall at every chunk boundary.
      a = hasPrev_ ? prev_ : monoAt(in, 0, channels_);
      b = monoAt(in, 0, channels_);
    } else {
      const size_t i0 = static_cast<size_t>(i - start);
      if (i0 >= inFrames) break;
      a = monoAt(in, i0, channels_);
      if (i0 + 1 < inFrames) {
        b = monoAt(in, i0 + 1, channels_);
      } else if (isLast) {
        b = a;  // hold the last frame at end of file
      } else {
        break;  // needs the next chunk, keep the position for the next call
      }
    }

    const float frac = static_cast<float>(pos_ - static_cast<double>(i));
    const float v = a + (b - a) * frac;
    int32_t s = static_cast<int32_t>(lrintf(v));
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    out[written++] = static_cast<int16_t>(s);

    pos_ += ratio_;
  }

  prev_ = monoAt(in, inFrames - 1, channels_);
  hasPrev_ = true;
  return written;
}

}  // namespace drum
