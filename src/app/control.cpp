// ===========================================================================
//  control.cpp
// ===========================================================================
#include "app/control.h"

#include <math.h>

#include "hal/audio_out.h"
#include "hal/sd_manager.h"
#include "hal/ui_oled.h"
#include "app/trigger_engine.h"
#include "core/sample_bank.h"

namespace drum {

void Control::begin(SdManager* sd, SampleBank* bank, AudioOut* audio, VoicePool* pool,
                    TriggerEngine* trigger, UiOled* ui, Buttons* buttons) {
  sd_ = sd;
  bank_ = bank;
  audio_ = audio;
  pool_ = pool;
  trigger_ = trigger;
  ui_ = ui;
  buttons_ = buttons;
}

// ---------------------------------------------------------------------------
// persistence
// ---------------------------------------------------------------------------
void Control::restore() {
  prefs_.begin(NVS_NAMESPACE, false);

  uint8_t vol = prefs_.getUChar("vol", 75);
  if (vol > 100) vol = 100;
  state().volume.store(vol);

  uint8_t kit = prefs_.getUChar("kit", 1);
  if (kit > 3) kit = 1;
  state().kit.store(kit);

  uint16_t thr[PAD_COUNT];
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    char key[8];
    snprintf(key, sizeof(key), "t%u", p);
    thr[p] = prefs_.getUShort(key, PADS[p].defThreshold);
    if (thr[p] < THRESHOLD_MIN_COUNTS || thr[p] > THRESHOLD_MAX_COUNTS) thr[p] = PADS[p].defThreshold;
  }
  state().init(thr);
  if (trigger_ != nullptr) trigger_->setThresholds(thr);

  applyVolume();
  Serial.printf("[nvs] restored kit=%s vol=%u%% thresholds from NVS\n", KIT_NAMES[kit], vol);
}

void Control::persist() {
  prefs_.putUChar("vol", state().volume.load());
  prefs_.putUChar("kit", state().kit.load());
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    char key[8];
    snprintf(key, sizeof(key), "t%u", p);
    prefs_.putUShort(key, state().threshold[p].load());
  }
}

// ---------------------------------------------------------------------------
// actions
// ---------------------------------------------------------------------------
void Control::applyVolume() {
  const float n = static_cast<float>(state().volume.load()) / 100.0f;
  // Perceptual taper: the top of the range is 0 dB, the bottom is silence.
  const float g = powf(n, 1.8f) * 32768.0f;
  pool_->setMasterVolumeQ15(static_cast<int32_t>(g));
}

void Control::setVolume(uint8_t volume) {
  if (volume > 100) volume = 100;
  state().volume.store(volume);
  applyVolume();
  ui_->markDirty();
  prefs_.putUChar("vol", volume);
}

void Control::loadKit(uint8_t kit, bool showOnScreen) {
  if (kit > 3) kit = 0;
  if (!sd_->mounted()) {
    ui_->showMessage("NO SD CARD", "Insert a FAT32 card", "and reset");
    return;
  }
  if (showOnScreen) ui_->showMessage("LOADING KIT", KIT_NAMES[kit], "please wait");

  // Stop every voice and hold the mixer in silence so the pool can be rewritten
  audio_->beginKitChange();
  const KitReport rep = sd_->loadKit(kit, *bank_);
  pool_->stopAll();
  audio_->endKitChange();

  state().kit.store(kit);
  state().kitLoaded.store(rep.loaded);
  state().kitMissing.store(static_cast<uint8_t>(rep.missing + rep.invalid + rep.noRam));
  state().poolUsed.store(static_cast<uint32_t>(bank_->usedBytes()));
  prefs_.putUChar("kit", kit);
  ui_->markDirty();
}

void Control::startCalibration() {
  state().page.store(UI_CAL);
  state().calProgress.store(0);
  calWasActive_ = true;
  calDone_ = false;
  trigger_->requestCalibration();
  ui_->markDirty();
}

void Control::checkCalibrationFinished() {
  if (!calWasActive_) return;
  if (state().calibrating.load()) return;
  if (state().calProgress.load() < 100) return;
  calWasActive_ = false;
  calDone_ = true;
  persist();
  Serial.println("[ctl] calibration saved to NVS");
  ui_->markDirty();
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------
void Control::onHome(uint8_t id, bool longPress) {
  switch (id) {
    case BTN_KIT: {
      const uint8_t kit = state().kit.load();
      if (longPress) {
        loadKit(static_cast<uint8_t>((kit + 3) & 3));  // previous
      } else {
        loadKit(static_cast<uint8_t>((kit + 1) & 3));  // next
      }
      break;
    }
    case BTN_VOL_UP:
      setVolume(static_cast<uint8_t>(state().volume.load() + (longPress ? 10 : 5)));
      break;
    case BTN_VOL_DOWN: {
      const int v = static_cast<int>(state().volume.load()) - (longPress ? 10 : 5);
      setVolume(static_cast<uint8_t>(v < 0 ? 0 : v));
      break;
    }
    case BTN_MENU:
      state().page.store(UI_MENU);
      state().menuIndex.store(0);
      ui_->markDirty();
      break;
    default:
      break;
  }
}

void Control::onMenu(uint8_t id, bool longPress) {
  uint8_t sel = state().menuIndex.load();

  if (longPress) {  // back
    state().page.store(UI_HOME);
    ui_->markDirty();
    return;
  }

  switch (id) {
    case BTN_VOL_UP:
      state().menuIndex.store(static_cast<uint8_t>((sel + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT));
      ui_->markDirty();
      break;
    case BTN_VOL_DOWN:
      state().menuIndex.store(static_cast<uint8_t>((sel + 1) % MENU_ITEM_COUNT));
      ui_->markDirty();
      break;
    case BTN_KIT:
      state().page.store(UI_HOME);
      ui_->markDirty();
      break;
    case BTN_MENU:
      switch (sel) {
        case MENU_TEST_PAD:
          state().testPad.store(0);
          state().page.store(UI_TEST);
          break;
        case MENU_SD_INFO:
          state().page.store(UI_INFO);
          break;
        case MENU_CALIBRATE:
          startCalibration();
          break;
        case MENU_DEBUG:
          state().debugEnabled.store(!state().debugEnabled.load());
          break;
        case MENU_EXIT:
        default:
          state().page.store(UI_HOME);
          break;
      }
      ui_->markDirty();
      break;
    default:
      break;
  }
}

void Control::onTest(uint8_t id, bool longPress) {
  uint8_t pad = state().testPad.load();
  if (longPress) {
    state().page.store(UI_MENU);
    ui_->markDirty();
    return;
  }
  switch (id) {
    case BTN_VOL_UP:
      state().testPad.store(static_cast<uint8_t>((pad + PAD_COUNT - 1) % PAD_COUNT));
      ui_->markDirty();
      break;
    case BTN_VOL_DOWN:
      state().testPad.store(static_cast<uint8_t>((pad + 1) % PAD_COUNT));
      ui_->markDirty();
      break;
    case BTN_MENU:
      trigger_->testHit(pad, 100);
      break;
    case BTN_KIT:
      state().page.store(UI_MENU);
      ui_->markDirty();
      break;
    default:
      break;
  }
}

void Control::handleEvent(const BtnEvent& ev) {
  const uint8_t page = state().page.load();
  switch (page) {
    case UI_MENU: onMenu(ev.id, ev.longPress); break;
    case UI_TEST: onTest(ev.id, ev.longPress); break;
    case UI_INFO:
    case UI_CAL:
      if (ev.longPress || ev.id == BTN_KIT) {
        state().page.store(UI_HOME);
        ui_->markDirty();
      }
      break;
    case UI_HIT:
    case UI_HOME:
    default: onHome(ev.id, ev.longPress); break;
  }
}

// ---------------------------------------------------------------------------
void Control::update() {
  BtnEvent ev;
  while (buttons_->poll(&ev)) handleEvent(ev);

  checkCalibrationFinished();

  // HOME <-> "NOW PLAYING" page switching
  const uint8_t page = state().page.load();
  if (page == UI_HOME || page == UI_HIT) {
    const uint32_t last = state().lastHitMs.load();
    const bool recent = (last != 0) && ((millis() - last) < HIT_PAGE_HOLD_MS);
    const uint8_t want = recent ? UI_HIT : UI_HOME;
    if (want != page) {
      state().page.store(want);
      ui_->markDirty();
    }
  }
}

}  // namespace drum
