// ===========================================================================
//  wav.h  --  minimal RIFF/WAVE header parser (pure C++)
// ===========================================================================
//  Supports what drum sample files actually contain: PCM (format 1) or
//  WAVE_FORMAT_EXTENSIBLE (0xFFFE) with 16-bit samples, mono or stereo.
//  Anything else is rejected with a serial warning instead of being played as
//  noise.  Working through a ByteSource keeps the parser unit-testable on a PC
//  and independent of the SD/FAT layer.
// ===========================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace drum {

class ByteSource {
 public:
  virtual ~ByteSource() {}
  // Reads up to len bytes, returns the number actually read.
  virtual size_t read(uint8_t* dst, size_t len) = 0;
  virtual bool seek(uint32_t pos) = 0;
  virtual uint32_t size() const = 0;
};

struct WavInfo {
  uint16_t format = 0;         // 1 = PCM, 0xFFFE = extensible (PCM sub-format)
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t sampleRate = 0;
  uint32_t dataOffset = 0;     // byte offset of the first sample
  uint32_t dataBytes = 0;
  uint32_t frameCount = 0;     // dataBytes / blockAlign
  uint32_t blockAlign = 0;
  uint16_t blockAlignRaw = 0;
  bool valid = false;
};

// Parses the header and leaves the source positioned at dataOffset.
bool wavParse(ByteSource& src, WavInfo& out);

// Convenience wrapper over a memory buffer (host tests).
class MemoryByteSource : public ByteSource {
 public:
  MemoryByteSource(const uint8_t* data, uint32_t len) : data_(data), len_(len) {}
  size_t read(uint8_t* dst, size_t len) override;
  bool seek(uint32_t pos) override;
  uint32_t size() const override { return len_; }

 private:
  const uint8_t* data_;
  uint32_t len_;
  uint32_t pos_ = 0;
};

}  // namespace drum
