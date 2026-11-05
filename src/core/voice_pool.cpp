// ===========================================================================
//  voice_pool.cpp
// ===========================================================================
#include "core/voice_pool.h"

#include <math.h>
#include <string.h>

#include "config.h"
#include "core/ulaw.h"

namespace drum {
namespace {

inline int16_t softClip(int32_t x) {
  constexpr int32_t kKnee = 24000;
  if (x > kKnee) {
    x = kKnee + (x - kKnee) / 4;
  } else if (x < -kKnee) {
    x = -kKnee + (x + kKnee) / 4;
  }
  if (x > 32767) x = 32767;
  if (x < -32768) x = -32768;
  return static_cast<int16_t>(x);
}

// MIDI velocity 1..127 -> Q15 amplitude.  The exponent makes soft strokes
// quieter than a plain linear map, which matches how an acoustic drum behaves.
int32_t velocityGainQ15(uint8_t v) {
  if (v == 0) return 0;
  if (v > 127) v = 127;
  const float norm = static_cast<float>(v) / 127.0f;
  const float g = powf(norm, AUDIO_VELOCITY_GAMMA);
  int32_t q = static_cast<int32_t>(g * 32768.0f + 0.5f);
  if (q > 32768) q = 32768;
  return q;
}

}  // namespace

void VoicePool::begin(const SampleBank* bank) {
  bank_ = bank;
  stopAll();
  started_ = stolen_ = underrun_ = 0;
}

void VoicePool::setMasterVolumeQ15(int32_t g) {
  if (g < 0) g = 0;
  if (g > 32768) g = 32768;
  masterQ15_ = g;
}

int VoicePool::findFree() const {
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i)
    if (!voices_[i].active) return i;
  return -1;
}

int VoicePool::stealOldest() const {
  int best = 0;
  uint32_t bestPos = 0;
  bool found = false;
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
    if (!voices_[i].active) continue;
    if (!found || voices_[i].pos > bestPos) {
      bestPos = voices_[i].pos;
      best = i;
      found = true;
    }
  }
  return found ? best : -1;
}

bool VoicePool::start(SampleId s, uint8_t midiVelocity) {
  if (bank_ == nullptr || s >= S_COUNT) return false;
  const SampleSlot& sl = bank_->slot(s);
  if (!sl.loaded || sl.frames == 0) return false;

  const uint8_t pad = static_cast<uint8_t>(padForSample(s));
  const uint8_t maxInst = PADS[pad].maxVoices;

  // Count the instances this pad already owns; if it is at its limit, reuse
  // its oldest voice instead of eating a voice from another pad.
  int instances = 0;
  int reuse = -1;
  uint32_t reusePos = 0;
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
    if (!voices_[i].active || voices_[i].pad != pad) continue;
    ++instances;
    if (reuse < 0 || voices_[i].pos > reusePos) {
      reusePos = voices_[i].pos;
      reuse = i;
    }
  }

  int slot = -1;
  if (instances >= maxInst && reuse >= 0) {
    slot = reuse;  // natural re-strike of the same pad
  } else {
    slot = findFree();
    if (slot < 0) {
      slot = stealOldest();
      ++stolen_;
    }
  }
  if (slot < 0) return false;

  Voice& v = voices_[slot];
  v.sample = static_cast<uint8_t>(s);
  v.pad = pad;
  v.chokeGroup = PADS[pad].chokeGroup;
  v.active = true;
  v.pos = 0;
  v.chokeFade = 0;
  v.gainQ15 = velocityGainQ15(midiVelocity);

  ++started_;
  lastSample_ = s;
  lastVelocity_ = midiVelocity;
  return true;
}

void VoicePool::chokeGroup(uint8_t group) {
  if (group == 0) return;
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
    if (voices_[i].active && voices_[i].chokeGroup == group) voices_[i].chokeFade = 64;
  }
}

void VoicePool::stopAll() {
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
    voices_[i].active = false;
    voices_[i].pos = 0;
    voices_[i].chokeFade = 0;
  }
  active_ = 0;
}

uint8_t VoicePool::activeCount() const {
  uint8_t n = 0;
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i)
    if (voices_[i].active) ++n;
  return n;
}

uint16_t VoicePool::activeMask() const {
  uint16_t m = 0;
  for (int i = 0; i < AUDIO_MAX_VOICES; ++i)
    if (voices_[i].active) m |= static_cast<uint16_t>(1u << i);
  return m;
}

void VoicePool::render(int16_t* out, size_t frames) {
  if (out == nullptr || frames == 0) return;

  if (activeCount() == 0) {
    memset(out, 0, frames * sizeof(int16_t));
    return;
  }

  for (size_t i = 0; i < frames; ++i) {
    int32_t acc = 0;
    for (int vi = 0; vi < AUDIO_MAX_VOICES; ++vi) {
      Voice& v = voices_[vi];
      if (!v.active) continue;

      const SampleSlot& sl = bank_->slot(v.sample);
      if (v.pos >= sl.frames || sl.data == nullptr) {
        v.active = false;
        continue;
      }

      int32_t s = sl.wide ? reinterpret_cast<const int16_t*>(sl.data)[v.pos]
                          : static_cast<int32_t>(ulawDecode(sl.data[v.pos]));

      int32_t g = v.gainQ15;
      if (v.chokeFade > 0) {
        g = (g * static_cast<int32_t>(v.chokeFade)) >> 6;
        if (--v.chokeFade == 0) {
          v.active = false;
        }
      }
      acc += (s * g) >> 15;
      ++v.pos;

      if (v.pos >= sl.frames && v.chokeFade == 0) v.active = false;
    }

    acc = (acc * masterQ15_) >> 15;
    out[i] = softClip(acc);
  }
}

}  // namespace drum
