// ===========================================================================
//  sample_bank.cpp
// ===========================================================================
#include "core/sample_bank.h"

#include <string.h>

#include "config.h"

namespace drum {

void SampleBank::begin(uint8_t* pool, size_t capacity) {
  pool_ = pool;
  capacity_ = capacity;
  clear();
}

void SampleBank::clear() {
  used_ = 0;
  for (uint8_t i = 0; i < S_COUNT; ++i) slots_[i] = SampleSlot();
}

uint8_t* SampleBank::allocate(size_t bytes) {
  if (pool_ == nullptr) return nullptr;
  // keep 4-byte alignment so 16-bit samples stay word aligned
  const size_t aligned = (bytes + 3u) & ~static_cast<size_t>(3u);
  if (used_ + aligned > capacity_) return nullptr;
  uint8_t* p = pool_ + used_;
  used_ += aligned;
  return p;
}

void SampleBank::set(SampleId id, const uint8_t* data, uint32_t frames, bool wide) {
  if (id >= S_COUNT) return;
  slots_[id].data = data;
  slots_[id].frames = frames;
  slots_[id].wide = wide;
  slots_[id].loaded = (data != nullptr && frames > 0);
}

uint8_t SampleBank::loadedCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < S_COUNT; ++i)
    if (slots_[i].loaded) ++n;
  return n;
}

uint32_t SampleBank::totalSeconds10() const {
  uint64_t frames = 0;
  for (uint8_t i = 0; i < S_COUNT; ++i)
    if (slots_[i].loaded) frames += slots_[i].frames;
  return static_cast<uint32_t>((frames * 10ull) / AUDIO_SAMPLE_RATE);
}

}  // namespace drum
