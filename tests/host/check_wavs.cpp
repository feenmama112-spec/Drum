// ===========================================================================
//  check_wavs.cpp  --  verifies a generated sample pack through the REAL
//                      firmware load path (parser + resampler + u-law encoder)
// ===========================================================================
//  Build & run:  tests/host/run_tests.sh   (runs after the unit tests)
//
//  This is not a toy check: it uses core/wav.cpp, core/sample_loader.cpp and
//  the per-sample length caps from src/core/kit_defs.cpp, so a pass means the
//  files on the SD card will load and fit into the sample pool on the device.
// ===========================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "config.h"
#include "core/kit_defs.h"
#include "core/sample_loader.h"
#include "core/ulaw.h"
#include "core/wav.h"

using namespace drum;

namespace {

class FileSource : public ByteSource {
 public:
  explicit FileSource(const std::string& path) : f_(path, std::ios::binary) {
    f_.seekg(0, std::ios::end);
    size_ = static_cast<uint32_t>(f_.tellg());
    f_.seekg(0, std::ios::beg);
  }
  bool ok() const { return f_.good() && size_ > 0; }
  size_t read(uint8_t* dst, size_t len) override {
    f_.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(len));
    return static_cast<size_t>(f_.gcount());
  }
  bool seek(uint32_t pos) override {
    f_.clear();
    f_.seekg(pos, std::ios::beg);
    return f_.good();
  }
  uint32_t size() const override { return size_; }

 private:
  std::ifstream f_;
  uint32_t size_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
  const std::string root = (argc > 1) ? argv[1] : "sdcard/DRUM";
  std::printf("=== sample pack check (%s) ===\n", root.c_str());
  std::printf("%-7s %-14s %6s %7s %8s %7s %6s\n", "kit", "file", "ms", "frames", "bytes",
              "peakdB", "ok");
  std::printf("%s\n", std::string(60, '-').c_str());

  int failures = 0;
  size_t worstPool = 0;

  for (uint8_t k = 0; k < 4; ++k) {
    size_t poolUsed = 0;
    std::printf("--- %s ---\n", KIT_NAMES[k]);

    for (uint8_t s = 0; s < S_COUNT; ++s) {
      const std::string path =
          root + "/" + KIT_NAMES[k] + std::string("/") + SAMPLES[s].fileName;
      FileSource src(path);
      bool ok = src.ok();
      WavInfo info;
      if (ok) ok = wavParse(src, info);
      if (!ok) {
        std::printf("%-7s %-14s  FILE MISSING OR INVALID\n", KIT_NAMES[k], SAMPLES[s].fileName);
        ++failures;
        continue;
      }

      const uint32_t maxFrames =
          static_cast<uint32_t>(SAMPLES[s].maxMs) * AUDIO_SAMPLE_RATE / 1000u;
      const bool wide = (AUDIO_16BIT != 0);
      std::vector<uint8_t> buf(pcmBytesFor(maxFrames, wide));
      const LoadResult r =
          loadWavToBuffer(src, info, AUDIO_SAMPLE_RATE, maxFrames, wide, buf.data(), buf.size());
      if (!r.ok) {
        std::printf("%-7s %-14s  DECODE FAILED\n", KIT_NAMES[k], SAMPLES[s].fileName);
        ++failures;
        continue;
      }

      // real peak of the stored data, so a silent or garbage file is caught
      int32_t peakAbs = 0;
      if (wide) {
        const int16_t* p = reinterpret_cast<const int16_t*>(buf.data());
        for (uint32_t i = 0; i < r.frames; ++i) peakAbs = std::max<int32_t>(peakAbs, std::abs(p[i]));
      } else {
        for (uint32_t i = 0; i < r.frames; ++i) {
          const int32_t v = ulawDecode(buf[i]);
          peakAbs = std::max<int32_t>(peakAbs, std::abs(v));
        }
      }
      const double peakDb = 20.0 * std::log10((peakAbs > 0 ? peakAbs : 1) / 32768.0);
      // a usable drum sample peaks between -40 dBFS and 0 dBFS
      const bool levelOk = peakDb > -40.0 && peakDb < 1.0;
      const bool lenOk = r.frames > 0 && r.frames <= maxFrames;
      if (!levelOk || !lenOk) ++failures;

      poolUsed += pcmBytesFor(r.frames, wide);
      std::printf("%-7s %-14s %6u %7u %8u %+7.1f %6s\n", KIT_NAMES[k], SAMPLES[s].fileName,
                  static_cast<unsigned>(r.frames * 1000ull / AUDIO_SAMPLE_RATE),
                  static_cast<unsigned>(r.frames),
                  static_cast<unsigned>(pcmBytesFor(r.frames, wide)), peakDb,
                  (levelOk && lenOk) ? "ok" : "BAD");
    }

    std::printf("%-7s %-14s %28zu bytes  (pool budget %d)\n", KIT_NAMES[k], "TOTAL", poolUsed,
                AUDIO_POOL_BYTES);
    if (poolUsed > worstPool) worstPool = poolUsed;
    if (poolUsed > static_cast<size_t>(AUDIO_POOL_BYTES)) {
      std::printf("  ERROR: kit does not fit into AUDIO_POOL_BYTES\n");
      ++failures;
    }
  }

  std::printf("%s\n", std::string(60, '-').c_str());
  std::printf("worst kit uses %zu of %d bytes (%.0f%%)\n", worstPool, AUDIO_POOL_BYTES,
              100.0 * worstPool / AUDIO_POOL_BYTES);
  std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
