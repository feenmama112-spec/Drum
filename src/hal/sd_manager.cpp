// ===========================================================================
//  sd_manager.cpp
// ===========================================================================
#include "hal/sd_manager.h"

#include <math.h>

#include "core/sample_loader.h"

namespace drum {
namespace {
constexpr uint32_t kSdFreqHz = 20000000;  // 20 MHz is reliable on a breadboard
constexpr uint8_t kMaxOpenFiles = 4;
}  // namespace

bool SdManager::begin() {
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  spi_.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  mounted_ = SD.begin(PIN_SD_CS, spi_, kSdFreqHz, "/sd", kMaxOpenFiles, false);
  if (!mounted_) {
    typeName_ = "none";
    Serial.println("[sd] mount failed - check card (FAT32), wiring and 3V3");
    return false;
  }

  const uint8_t t = SD.cardType();
  switch (t) {
    case CARD_MMC:  typeName_ = "MMC";   break;
    case CARD_SD:   typeName_ = "SDSC";  break;
    case CARD_SDHC: typeName_ = "SDHC";  break;
    default:        typeName_ = "UNKNOWN"; break;
  }
  Serial.printf("[sd] mounted %s, %llu MB total\n", typeName_,
                static_cast<unsigned long long>(SD.totalBytes() / (1024ull * 1024ull)));

  if (!SD.exists("/DRUM")) {
    Serial.println("[sd] warning: /DRUM folder not found - copy the sample kits first");
  }
  return true;
}

void SdManager::end() {
  if (mounted_) SD.end();
  mounted_ = false;
}

uint64_t SdManager::cardSizeBytes() { return mounted_ ? SD.totalBytes() : 0; }
uint64_t SdManager::cardUsedBytes() { return mounted_ ? SD.usedBytes() : 0; }

KitReport SdManager::loadKit(uint8_t kitIndex, SampleBank& bank, bool verbose) {
  KitReport rep;
  bank.clear();
  if (kitIndex > 3) kitIndex = 0;
  if (!mounted_) {
    snprintf(rep.error, sizeof(rep.error), "no card");
    return rep;
  }

  for (uint8_t s = 0; s < S_COUNT; ++s) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%s", KIT_FOLDERS[kitIndex], SAMPLES[s].fileName);

    File f = SD.open(path, FILE_READ);
    if (!f) {
      ++rep.missing;
      if (verbose) Serial.printf("[kit] missing  %s\n", path);
      continue;
    }

    FileByteSource src(f);
    WavInfo info;
    if (!wavParse(src, info)) {
      ++rep.invalid;
      if (verbose) Serial.printf("[kit] invalid WAV (need 16-bit PCM mono/stereo) %s\n", path);
      f.close();
      continue;
    }

    // exact number of output frames this file will produce
    const uint32_t maxFrames = static_cast<uint32_t>(SAMPLES[s].maxMs) * AUDIO_SAMPLE_RATE / 1000u;
    uint64_t outFrames = (static_cast<uint64_t>(info.frameCount) * AUDIO_SAMPLE_RATE) / info.sampleRate;
    if (outFrames > maxFrames) outFrames = maxFrames;
    if (outFrames == 0) {
      ++rep.invalid;
      f.close();
      continue;
    }

    const bool wide = (AUDIO_16BIT != 0);
    const size_t bytes = pcmBytesFor(static_cast<uint32_t>(outFrames), wide);
    uint8_t* dst = bank.allocate(bytes);
    if (dst == nullptr) {
      ++rep.noRam;
      if (verbose) Serial.printf("[kit] out of sample RAM at %s\n", path);
      f.close();
      continue;
    }

    const LoadResult r =
        loadWavToBuffer(src, info, AUDIO_SAMPLE_RATE, maxFrames, wide, dst, bytes);
    f.close();

    if (!r.ok) {
      ++rep.invalid;
      if (verbose) Serial.printf("[kit] decode failed %s\n", path);
      continue;
    }

    bank.set(static_cast<SampleId>(s), dst, r.frames, wide);
    ++rep.loaded;
    rep.bytes += bytes;
    rep.totalFrames += r.frames;

    if (verbose) {
      const int16_t q15 = computePeakNormaliseQ15(dst, r.frames, wide);
      const int peakDb = (q15 > 0) ? static_cast<int>(20.0f * log10f(32767.0f / q15)) : 0;
      Serial.printf("[kit] %-14s %5u ms  %6u B  peak %+d dBFS\n", SAMPLES[s].fileName,
                    static_cast<unsigned>(r.frames * 1000ull / AUDIO_SAMPLE_RATE),
                    static_cast<unsigned>(bytes), peakDb);
    }
  }

  if (verbose) Serial.printf("[kit] %s: %u/12 samples, %u kB, %.1f s\n", KIT_NAMES[kitIndex],
                             rep.loaded, static_cast<unsigned>(rep.bytes / 1024),
                             static_cast<double>(rep.totalFrames) / AUDIO_SAMPLE_RATE);
  return rep;
}

uint8_t SdManager::countCompleteKits() {
  uint8_t complete = 0;
  for (uint8_t k = 0; k < 4; ++k) {
    uint8_t found = 0;
    for (uint8_t s = 0; s < S_COUNT; ++s) {
      char path[48];
      snprintf(path, sizeof(path), "%s/%s", KIT_FOLDERS[k], SAMPLES[s].fileName);
      if (SD.exists(path)) ++found;
    }
    if (found == S_COUNT) ++complete;
  }
  return complete;
}

}  // namespace drum
