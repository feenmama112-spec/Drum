// ===========================================================================
//  debug.cpp
// ===========================================================================
#include "app/debug.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "hal/adc_mcp3208.h"
#include "hal/audio_out.h"
#include "hal/sd_manager.h"
#include "core/sample_bank.h"
#include "app/trigger_engine.h"

namespace drum {

void DebugPort::begin(SdManager* sd, SampleBank* bank, TriggerEngine* trigger, VoicePool* pool,
                      AudioOut* audio, Mcp3208Bus* adc) {
  sd_ = sd;
  bank_ = bank;
  trigger_ = trigger;
  pool_ = pool;
  audio_ = audio;
  adc_ = adc;
  printHelp();
}

void DebugPort::printHelp() {
  Serial.println();
  Serial.println("commands:  i = info      m = memory     s = stats");
  Serial.println("           c = calibrate t = raw ADC dump for 3 s");
  Serial.println("           p<n> = test pad n (0-8)      h = this help");
}

void DebugPort::printStats() {
  Serial.printf("[stat] hit %lu  voices %u/%d  pool %lu/%lu B  adc %lu Hz (%lu us)  queue drop %lu\n",
                static_cast<unsigned long>(state().hitCount.load()),
                static_cast<unsigned>(pool_->activeCount()), AUDIO_MAX_VOICES,
                static_cast<unsigned long>(bank_->usedBytes()),
                static_cast<unsigned long>(bank_->capacity()),
                static_cast<unsigned long>(state().adcRateHz.load()),
                static_cast<unsigned long>(state().adcUsPerScan.load()),
                static_cast<unsigned long>(audio_->queueDrops()));
  Serial.printf("[stat] heap %lu B  min %lu B  blocks %lu  i2s err %lu  hh %s %.0f%%\n",
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<unsigned long>(audio_->blocksRendered()),
                static_cast<unsigned long>(audio_->writeErrors()),
                hiHatZoneName(static_cast<HiHatZone>(state().hhZone.load())),
                state().hhPct.load());
}

void DebugPort::printInfo() {
  Serial.println();
  Serial.printf("  kit        %s (%u/12 samples, %u missing)\n", KIT_NAMES[state().kit.load()],
                static_cast<unsigned>(state().kitLoaded.load()),
                static_cast<unsigned>(state().kitMissing.load()));
  Serial.printf("  volume     %u%%\n", static_cast<unsigned>(state().volume.load()));
  Serial.printf("  sd         %s  %llu MB\n", sd_->mounted() ? sd_->typeName() : "NOT MOUNTED",
                static_cast<unsigned long long>(sd_->cardSizeBytes() >> 20));
  Serial.printf("  audio      %s  %d Hz  %d frames/block  %d DMA blocks  %.1f ms latency\n",
                audio_->ready() ? "OK" : "FAIL", AUDIO_SAMPLE_RATE, AUDIO_BLOCK_FRAMES,
                AUDIO_DMA_DESC_NUM, audio_->dmaLatencyUs() / 1000.0);
  Serial.printf("  sample fmt %s\n", (AUDIO_16BIT != 0) ? "16-bit PCM" : "8-bit u-law");
  Serial.printf("  adc        %s  %lu Hz scan\n", adc_->selfTest() ? "OK" : "NO RESPONSE",
                static_cast<unsigned long>(state().adcRateHz.load()));

  Serial.println("  pad        adc     threshold  peak   hits");
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    Serial.printf("  %-10s %u:%u     %4u     %4u\n", PADS[p].name, PADS[p].adcChip,
                  PADS[p].adcChannel, static_cast<unsigned>(state().threshold[p].load()),
                  static_cast<unsigned>(state().lastPeak[p].load()));
  }
}

void DebugPort::printMemory() {
  Serial.printf("[mem] free %lu  min %lu  largest block %lu\n",
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  Serial.printf("[mem] sample pool %lu / %lu B (%.0f%%), %u samples, %.1f s of audio\n",
                static_cast<unsigned long>(bank_->usedBytes()),
                static_cast<unsigned long>(bank_->capacity()),
                bank_->capacity() ? (100.0 * bank_->usedBytes() / bank_->capacity()) : 0.0,
                static_cast<unsigned>(bank_->loadedCount()),
                static_cast<double>(bank_->totalSeconds10()) / 10.0);
  Serial.printf("[mem] flash sketch %lu / %lu B\n",
                static_cast<unsigned long>(ESP.getSketchSize()),
                static_cast<unsigned long>(ESP.getFreeSketchSpace() + ESP.getSketchSize()));
}

void DebugPort::handleLine(char* line) {
  if (line[0] == 0) return;
  switch (line[0]) {
    case 'i': printInfo(); break;
    case 'm': printMemory(); break;
    case 's': printStats(); break;
    case 'h': printHelp(); break;
    case 'c':
      Serial.println("[dbg] calibration requested");
      trigger_->requestCalibration();
      break;
    case 't':
      rawDumpUntil_ = millis() + 3000;
      Serial.println("[dbg] raw ADC dump for 3 s ...");
      break;
    case 'd':
      state().debugEnabled.store(!state().debugEnabled.load());
      Serial.printf("[dbg] periodic stats %s\n", state().debugEnabled.load() ? "on" : "off");
      break;
    case 'p': {
      const int pad = atoi(line + 1);
      if (pad >= 0 && pad < PAD_COUNT) {
        trigger_->testHit(static_cast<uint8_t>(pad), 100);
        Serial.printf("[dbg] test hit %s\n", PADS[pad].name);
      } else {
        Serial.println("[dbg] usage: p0 .. p8");
      }
      break;
    }
    default:
      Serial.printf("[dbg] unknown command '%c'\n", line[0]);
      break;
  }
}

void DebugPort::poll() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      line_[len_] = 0;
      handleLine(line_);
      len_ = 0;
    } else if (len_ < sizeof(line_) - 1) {
      line_[len_++] = c;
    }
  }

  const uint32_t now = millis();
  if (state().debugEnabled.load() && (now - lastStats_) >= DEBUG_STATS_MS) {
    lastStats_ = now;
    printStats();
  }

  if (rawDumpUntil_ != 0 && now < rawDumpUntil_) {
    static uint32_t last = 0;
    if (now - last >= 100) {
      last = now;
      Serial.printf("[adc] ");
      for (uint8_t p = 0; p < PAD_COUNT; ++p) {
        Serial.printf("%s=%u ", PADS[p].name, static_cast<unsigned>(state().lastPeak[p].load()));
      }
      Serial.printf("pedal=%u\n", static_cast<unsigned>(state().hhRaw.load()));
    }
  } else if (rawDumpUntil_ != 0) {
    rawDumpUntil_ = 0;
    Serial.println("[dbg] raw dump finished");
  }
}

}  // namespace drum
