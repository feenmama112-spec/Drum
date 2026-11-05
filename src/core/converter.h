// ===========================================================================
//  converter.h  --  streaming mono downmix + linear resampler (pure C++)
// ===========================================================================
//  Runs once per pad while a kit is loaded from the SD card, never during
//  playback.  Linear interpolation is plenty for drum samples and costs no
//  flash/RAM compared with a polyphase filter.
// ===========================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace drum {

class SampleConverter {
 public:
  void begin(uint32_t srcRate, uint8_t channels, uint32_t dstRate);

  // Feeds one contiguous chunk of interleaved source frames that starts at
  // source frame index `chunkStart`.  Writes at most outCap mono samples and
  // returns how many were written.  `isLast` lets the final sample be held so
  // the tail of the file is not dropped.
  size_t feed(const int16_t* in, size_t inFrames, uint32_t chunkStart, bool isLast,
              int16_t* out, size_t outCap);

  double position() const { return pos_; }
  double ratio() const { return ratio_; }

 private:
  double ratio_ = 1.0;
  uint8_t channels_ = 1;
  double pos_ = 0.0;
  float prev_ = 0.0f;   // last mono frame of the previous chunk
  bool hasPrev_ = false;
};

}  // namespace drum
