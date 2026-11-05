// ===========================================================================
//  sd_manager.h  --  SD card mount and kit loading
// ===========================================================================
//  A kit is loaded once, when it is selected: every WAV is decoded to mono at
//  the engine rate and stored in the sample pool.  Nothing is read from the
//  card while playing, so a slow card cannot cause a click, and the "SD card
//  must not disturb piezo detection" requirement is satisfied by construction.
// ===========================================================================
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"
#include "core/sample_bank.h"
#include "core/wav.h"

namespace drum {

struct KitReport {
  uint8_t loaded = 0;
  uint8_t missing = 0;
  uint8_t invalid = 0;
  uint8_t noRam = 0;
  uint32_t bytes = 0;
  uint32_t totalFrames = 0;
  char error[40] = {0};
};

class FileByteSource : public ByteSource {
 public:
  explicit FileByteSource(File& f) : f_(f) {}
  size_t read(uint8_t* dst, size_t len) override { return f_.read(dst, len); }
  bool seek(uint32_t pos) override { return f_.seek(pos); }
  uint32_t size() const override { return f_.size(); }

 private:
  File& f_;
};

class SdManager {
 public:
  bool begin();
  bool mounted() const { return mounted_; }
  void end();

  // Loads /DRUM/<KIT>/*.wav into the bank.  Safe to call only while the audio
  // engine is held (AudioOut::beginKitChange()).
  KitReport loadKit(uint8_t kitIndex, SampleBank& bank, bool verbose = true);

  uint64_t cardSizeBytes();
  uint64_t cardUsedBytes();
  const char* typeName() const { return typeName_; }

  // Scans /DRUM and reports how many of the four kits are complete.
  uint8_t countCompleteKits();

 private:
  bool openFile(const char* path, File& out);

  SPIClass spi_{FSPI};
  bool mounted_ = false;
  const char* typeName_ = "none";
};

}  // namespace drum
