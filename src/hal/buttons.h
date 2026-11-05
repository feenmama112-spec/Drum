// ===========================================================================
//  buttons.h  --  non blocking debounced buttons with long press
// ===========================================================================
#pragma once

#include <Arduino.h>

#include "config.h"

namespace drum {

enum BtnId : uint8_t { BTN_KIT = 0, BTN_MENU, BTN_VOL_DOWN, BTN_VOL_UP, BTN_COUNT };

struct BtnEvent {
  uint8_t id = 0;
  bool longPress = false;
};

class Buttons {
 public:
  void begin();

  // Call from loop(); returns true and fills *ev for each finished press.
  bool poll(BtnEvent* ev);

  bool isDown(uint8_t id) const { return raw_[id] == LOW; }
  uint32_t heldMs(uint8_t id) const { return raw_[id] == LOW ? (millis() - since_[id]) : 0; }

 private:
  static const uint8_t kPins[BTN_COUNT];
  uint8_t raw_[BTN_COUNT] = {HIGH, HIGH, HIGH, HIGH};
  bool stableDown_[BTN_COUNT] = {false, false, false, false};
  bool longFired_[BTN_COUNT] = {false, false, false, false};
  uint32_t since_[BTN_COUNT] = {0, 0, 0, 0};
  uint32_t changed_[BTN_COUNT] = {0, 0, 0, 0};
};

}  // namespace drum
