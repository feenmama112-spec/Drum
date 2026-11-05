// ===========================================================================
//  sample_bank.h  --  RAM resident sample storage (pure C++)
// ===========================================================================
//  All samples of one kit live in a single pre-allocated pool and are read
//  with a bump allocator.  The pool is allocated once at boot, so loading a
//  kit never fragments the heap and never fails half-way through.
// ===========================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/kit_defs.h"

namespace drum {

struct SampleSlot {
  const uint8_t* data = nullptr;  // 8-bit u-law or 16-bit PCM
  uint32_t frames = 0;
  bool wide = false;   // true = 16-bit PCM, false = u-law
  bool loaded = false;
};

class SampleBank {
 public:
  // pool/capacity are supplied by the HAL (internal SRAM or PSRAM).
  void begin(uint8_t* pool, size_t capacity);
  void clear();

  // Bump allocation; returns nullptr when the pool is exhausted.
  uint8_t* allocate(size_t bytes);

  void set(SampleId id, const uint8_t* data, uint32_t frames, bool wide);

  const SampleSlot& slot(SampleId id) const { return slots_[id]; }
  const SampleSlot& slot(uint8_t id) const { return slots_[id < S_COUNT ? id : 0]; }

  size_t usedBytes() const { return used_; }
  size_t capacity() const { return capacity_; }
  size_t freeBytes() const { return capacity_ - used_; }
  uint8_t loadedCount() const;
  uint32_t totalSeconds10() const;  // total duration x10, for the UI

 private:
  SampleSlot slots_[S_COUNT];
  uint8_t* pool_ = nullptr;
  size_t capacity_ = 0;
  size_t used_ = 0;
};

}  // namespace drum
