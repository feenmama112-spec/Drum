// ===========================================================================
//  wav.cpp  --  minimal RIFF/WAVE header parser
// ===========================================================================
#include "core/wav.h"

#include <string.h>

namespace drum {
namespace {

uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t rd16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

bool readExact(ByteSource& src, uint8_t* dst, size_t len) { return src.read(dst, len) == len; }

}  // namespace

bool wavParse(ByteSource& src, WavInfo& out) {
  out = WavInfo();

  uint8_t hdr[12];
  if (!src.seek(0) || !readExact(src, hdr, 12)) return false;
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;

  const uint32_t fileSize = src.size();
  uint32_t pos = 12;
  bool haveFmt = false;

  while (pos + 8 <= fileSize) {
    uint8_t ch[8];
    if (!src.seek(pos) || !readExact(src, ch, 8)) return false;
    const uint32_t chunkSize = rd32(ch + 4);
    const uint32_t body = pos + 8;

    if (memcmp(ch, "fmt ", 4) == 0) {
      if (chunkSize < 16 || body + 16 > fileSize) return false;
      uint8_t fmt[16];
      if (!src.seek(body) || !readExact(src, fmt, 16)) return false;
      out.format = rd16(fmt + 0);
      out.channels = rd16(fmt + 2);
      out.sampleRate = rd32(fmt + 4);
      out.blockAlignRaw = rd16(fmt + 12);
      out.bitsPerSample = rd16(fmt + 14);
      // WAVE_FORMAT_EXTENSIBLE: the real format sits in the first 2 bytes of
      // the sub-format GUID at offset 24 of the chunk.
      if (out.format == 0xFFFE && chunkSize >= 40) {
        uint8_t ext[2];
        if (src.seek(body + 24) && readExact(src, ext, 2)) out.format = rd16(ext);
      }
      haveFmt = true;
    } else if (memcmp(ch, "data", 4) == 0) {
      if (!haveFmt) return false;
      out.dataOffset = body;
      // Some writers leave the size field at 0 or 0xFFFFFFFF (streaming); clamp.
      const uint32_t avail = (body < fileSize) ? (fileSize - body) : 0;
      out.dataBytes = (chunkSize == 0 || chunkSize == 0xFFFFFFFFu || chunkSize > avail) ? avail : chunkSize;
      break;
    }

    // Chunks are word aligned: an odd size is followed by one pad byte.
    pos = body + chunkSize + (chunkSize & 1u);
  }

  if (!haveFmt || out.dataOffset == 0) return false;
  if (out.format != 1) return false;
  if (out.channels != 1 && out.channels != 2) return false;
  if (out.bitsPerSample != 16) return false;
  if (out.sampleRate < 8000 || out.sampleRate > 192000) return false;

  out.blockAlign = static_cast<uint32_t>(out.channels) * 2u;
  out.frameCount = out.dataBytes / out.blockAlign;
  if (out.frameCount == 0) return false;

  out.valid = true;
  return src.seek(out.dataOffset);
}

size_t MemoryByteSource::read(uint8_t* dst, size_t len) {
  if (pos_ >= len_) return 0;
  const uint32_t avail = len_ - pos_;
  const uint32_t n = (len < avail) ? static_cast<uint32_t>(len) : avail;
  memcpy(dst, data_ + pos_, n);
  pos_ += n;
  return n;
}

bool MemoryByteSource::seek(uint32_t pos) {
  if (pos > len_) return false;
  pos_ = pos;
  return true;
}

}  // namespace drum
