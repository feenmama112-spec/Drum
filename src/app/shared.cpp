// ===========================================================================
//  shared.cpp
// ===========================================================================
#include "app/shared.h"

namespace drum {
namespace {
SharedState g_state;
}

SharedState& state() { return g_state; }

void SharedState::init(uint16_t defaultThreshold[PAD_COUNT]) {
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    threshold[p].store(defaultThreshold[p]);
    lastPeak[p].store(0);
  }
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    if (threshold[p].load() == 0) threshold[p].store(PADS[p].defThreshold);
  }
}

}  // namespace drum
