// ===========================================================================
//  sample_loader.cpp
// ===========================================================================
#include "core/sample_loader.h"

#include <math.h>
#include <string.h>

#include "core/converter.h"
#include "core/ulaw.h"

namespace drum {
namespace {

constexpr size_t kChunkFrames = 512;
// Worst case upsample ratio is 48000/8000 = 6x, so one chunk can produce
// 512*6 = 3072 output frames.  Static (not stack) to keep the loader cheap on
// the Arduino loop task.
constexpr size_t kMonoCap = 512 * 6 + 16;

int16_t sIn[kChunkFrames * 2];
int16_t sMono[kMonoCap];

}  // namespace

size_t pcmBytesFor(uint32_t frames, bool wide) { return static_cast<size_t>(frames) * (wide ? 2u : 1u); }

LoadResult loadWavToBuffer(ByteSource& src, const WavInfo& info, uint32_t dstRate, uint32_t maxFrames,
                           bool wide, uint8_t* dst, size_t dstCap) {
  LoadResult r;
  if (!info.valid || dst == nullptr || dstCap == 0 || info.blockAlign == 0) return r;

  const size_t bps = wide ? 2u : 1u;
  uint32_t limit = static_cast<uint32_t>(dstCap / bps);
  if (maxFrames > 0 && maxFrames < limit) limit = maxFrames;
  if (limit == 0) return r;

  SampleConverter conv;
  conv.begin(info.sampleRate, static_cast<uint8_t>(info.channels), dstRate);

  uint32_t written = 0;
  uint32_t chunkStart = 0;
  int16_t* dst16 = reinterpret_cast<int16_t*>(dst);

  while (written < limit && chunkStart < info.frameCount) {
    size_t want = kChunkFrames;
    if (chunkStart + want > info.frameCount) want = info.frameCount - chunkStart;

    const size_t gotBytes = src.read(reinterpret_cast<uint8_t*>(sIn), want * info.blockAlign);
    const size_t gotFrames = gotBytes / info.blockAlign;
    if (gotFrames == 0) break;

    const bool isLast = (chunkStart + gotFrames) >= info.frameCount;
    const size_t outFrames = conv.feed(sIn, gotFrames, chunkStart, isLast, sMono, kMonoCap);

    uint32_t n = static_cast<uint32_t>(outFrames);
    if (n > limit - written) n = limit - written;
    for (uint32_t k = 0; k < n; ++k) {
      if (wide) {
        dst16[written + k] = sMono[k];
      } else {
        dst[written + k] = ulawEncode(sMono[k]);
      }
    }
    written += n;
    chunkStart += static_cast<uint32_t>(gotFrames);

    if (isLast) break;
    if (outFrames == 0 && gotBytes < want * info.blockAlign) break;  // short read
  }

  r.frames = written;
  r.srcFrames = info.frameCount;
  r.truncated = (written >= limit) && (chunkStart < info.frameCount);
  r.ok = written > 0;
  return r;
}

int16_t computePeakNormaliseQ15(const uint8_t* data, uint32_t frames, bool wide) {
  if (data == nullptr || frames == 0) return 32768;
  int32_t peak = 0;
  if (wide) {
    const int16_t* p = reinterpret_cast<const int16_t*>(data);
    for (uint32_t i = 0; i < frames; ++i) {
      const int32_t v = p[i] < 0 ? -p[i] : p[i];
      if (v > peak) peak = v;
    }
  } else {
    for (uint32_t i = 0; i < frames; ++i) {
      const int32_t v = ulawDecode(data[i]);
      const int32_t a = v < 0 ? -v : v;
      if (a > peak) peak = a;
    }
  }
  if (peak < 16) return 32767;  // essentially silent - do not amplify noise
  const float q = 32000.0f / static_cast<float>(peak);
  int32_t out = static_cast<int32_t>(q * 32768.0f + 0.5f);
  // 32767 and not 32768: the result is stored in an int16_t
  if (out > 32767) out = 32767;
  if (out < 1) out = 1;
  return static_cast<int16_t>(out);
}

}  // namespace drum
