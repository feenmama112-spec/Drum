// ===========================================================================
//  shared.h  --  state shared between the trigger task, audio task and the UI
// ===========================================================================
//  Only plain atomics (lock free 32-bit on the ESP32-S3) so no task can block
//  another.  The UI is a pure reader, which keeps the real-time path free of
//  display work.
// ===========================================================================
#pragma once

#include <atomic>

#include "config.h"
#include "core/kit_defs.h"

namespace drum {

enum UiPage : uint8_t {
  UI_BOOT = 0,
  UI_HOME,
  UI_HIT,
  UI_MENU,
  UI_TEST,
  UI_INFO,
  UI_CAL,
  UI_PAGE_COUNT
};

enum MenuItem : uint8_t {
  MENU_TEST_PAD = 0,
  MENU_SD_INFO,
  MENU_CALIBRATE,
  MENU_DEBUG,
  MENU_EXIT,
  MENU_ITEM_COUNT
};

struct SharedState {
  // control
  std::atomic<uint8_t> page{UI_BOOT};
  std::atomic<uint8_t> kit{1};      // default ROCK
  std::atomic<uint8_t> volume{75};  // 0..100
  std::atomic<uint8_t> menuIndex{0};
  std::atomic<uint8_t> testPad{0};
  std::atomic<bool> debugEnabled{true};

  // health
  std::atomic<bool> sdOk{false};
  std::atomic<bool> audioOk{false};
  std::atomic<bool> adcOk{false};
  std::atomic<bool> calibrating{false};
  std::atomic<uint8_t> calProgress{0};

  // kit / memory
  std::atomic<uint8_t> kitLoaded{0};
  std::atomic<uint8_t> kitMissing{0};
  std::atomic<uint32_t> poolUsed{0};
  std::atomic<uint32_t> poolCapacity{0};

  // trigger
  std::atomic<uint16_t> threshold[PAD_COUNT]{};
  std::atomic<uint16_t> lastPeak[PAD_COUNT]{};
  std::atomic<uint8_t> lastPad{0};
  std::atomic<uint8_t> lastVelocity{0};
  std::atomic<uint8_t> lastSample{0};
  std::atomic<uint32_t> lastHitMs{0};
  std::atomic<uint32_t> hitCount{0};
  std::atomic<uint32_t> adcRateHz{0};
  std::atomic<uint32_t> adcUsPerScan{0};
  std::atomic<uint32_t> triggerLatencyUs{0};

  // audio
  std::atomic<uint8_t> activeVoices{0};
  std::atomic<uint32_t> audioBlocks{0};
  std::atomic<uint32_t> audioErrors{0};

  // hi-hat
  std::atomic<uint16_t> hhRaw{0};
  std::atomic<float> hhPct{0.0f};
  std::atomic<uint8_t> hhZone{0};

  void init(uint16_t defaultThreshold[PAD_COUNT]);
};

SharedState& state();

}  // namespace drum
