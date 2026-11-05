// ===========================================================================
//  buttons.cpp
// ===========================================================================
#include "hal/buttons.h"

namespace drum {

const uint8_t Buttons::kPins[BTN_COUNT] = {PIN_BTN_KIT, PIN_BTN_MENU, PIN_BTN_VOL_DOWN,
                                           PIN_BTN_VOL_UP};

void Buttons::begin() {
  for (uint8_t i = 0; i < BTN_COUNT; ++i) {
    pinMode(kPins[i], INPUT_PULLUP);
    raw_[i] = HIGH;
    changed_[i] = millis();
  }
}

bool Buttons::poll(BtnEvent* ev) {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < BTN_COUNT; ++i) {
    const uint8_t v = digitalRead(kPins[i]);
    if (v != raw_[i]) {
      raw_[i] = v;
      changed_[i] = now;
      continue;
    }
    // debounce: only accept a level that has been stable long enough
    if ((now - changed_[i]) < BTN_DEBOUNCE_MS) continue;
    const bool down = (v == LOW);

    if (down && !stableDown_[i]) {
      stableDown_[i] = true;
      since_[i] = now;
      longFired_[i] = false;
    } else if (down && stableDown_[i]) {
      // long press fires once, while the button is still held
      if (!longFired_[i] && (now - since_[i]) >= BTN_LONG_PRESS_MS) {
        longFired_[i] = true;
        if (ev != nullptr) {
          ev->id = i;
          ev->longPress = true;
          return true;
        }
      }
    } else if (!down && stableDown_[i]) {
      stableDown_[i] = false;
      const bool wasLong = longFired_[i];
      longFired_[i] = false;
      if (!wasLong && ev != nullptr) {
        ev->id = i;
        ev->longPress = false;
        return true;
      }
    }
  }
  return false;
}

}  // namespace drum
