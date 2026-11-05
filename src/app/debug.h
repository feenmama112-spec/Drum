// ===========================================================================
//  debug.h  --  serial diagnostics and statistics
// ===========================================================================
#pragma once

#include <Arduino.h>

#include "app/shared.h"
#include "config.h"

namespace drum {

class SdManager;
class SampleBank;
class TriggerEngine;
class VoicePool;
class AudioOut;
class Mcp3208Bus;

class DebugPort {
 public:
  void begin(SdManager* sd, SampleBank* bank, TriggerEngine* trigger, VoicePool* pool,
             AudioOut* audio, Mcp3208Bus* adc);
  void poll();
  void printStats();
  void printInfo();
  void printMemory();
  void printHelp();

 private:
  void handleLine(char* line);

  SdManager* sd_ = nullptr;
  SampleBank* bank_ = nullptr;
  TriggerEngine* trigger_ = nullptr;
  VoicePool* pool_ = nullptr;
  AudioOut* audio_ = nullptr;
  Mcp3208Bus* adc_ = nullptr;

  char line_[40] = {0};
  uint8_t len_ = 0;
  uint32_t lastStats_ = 0;
  uint32_t rawDumpUntil_ = 0;
};

}  // namespace drum
