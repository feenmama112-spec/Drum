// ===========================================================================
//  ESP32-S3 DIY ELECTRONIC DRUM MODULE
// ---------------------------------------------------------------------------
//  main.cpp - boot sequence and the non-real-time super loop.
//
//  Task map
//    audioTask  (core 0, prio 19) : I2S output + voice mixer  [hal/audio_out]
//    triggerTask(core 1, prio 18) : MCP3208 scan + hit detect [app/trigger_engine]
//    loopTask   (core 1, prio  1) : buttons, OLED, serial     [this file]
//
//  Boot order matters:
//    1. buttons + display      (so the user sees progress)
//    2. NVS                    (kit index, volume, thresholds)
//    3. SD card                (sample source)
//    4. sample pool allocation (before anything else grabs the heap)
//    5. kit load               (audio task not started yet -> no race)
//    6. I2S + mixer task
//    7. ADC + trigger task     (sounds can start flowing)
// ===========================================================================
#include <Arduino.h>
#include <esp_heap_caps.h>

#include "app/control.h"
#include "app/debug.h"
#include "app/shared.h"
#include "app/trigger_engine.h"
#include "config.h"
#include "core/hihat.h"
#include "core/sample_bank.h"
#include "core/voice_pool.h"
#include "hal/adc_mcp3208.h"
#include "hal/audio_out.h"
#include "hal/buttons.h"
#include "hal/sd_manager.h"
#include "hal/ui_oled.h"

using namespace drum;

namespace {

Mcp3208Bus s_adc;
AudioOut s_audio;
VoicePool s_pool;
SampleBank s_bank;
SdManager s_sd;
HiHatPedal s_hh;
TriggerEngine s_trigger;
UiOled s_ui;
Buttons s_buttons;
Control s_control;
DebugPort s_debug;

uint8_t* s_poolMem = nullptr;
size_t s_poolBytes = 0;

// Allocates the sample pool.  PSRAM is used only when the build enables it and
// the chip really has some; otherwise internal SRAM, with a smaller fallback so
// the module still plays (shortened samples are better than no module).
size_t allocatePool() {
  const size_t candidates[3] = {AUDIO_POOL_BYTES, (AUDIO_POOL_BYTES * 3) / 4, AUDIO_POOL_BYTES / 2};

  for (uint8_t i = 0; i < 3; ++i) {
    const size_t want = candidates[i];
#if BOARD_HAS_PSRAM
    void* p = heap_caps_malloc(want, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p != nullptr) {
      s_poolMem = static_cast<uint8_t*>(p);
      Serial.printf("[ram] sample pool %u B in PSRAM\n", static_cast<unsigned>(want));
      return want;
    }
#endif
    void* p = heap_caps_malloc(want, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p != nullptr) {
      s_poolMem = static_cast<uint8_t*>(p);
      Serial.printf("[ram] sample pool %u B in internal SRAM (free heap %u)\n",
                    static_cast<unsigned>(want), static_cast<unsigned>(ESP.getFreeHeap()));
      return want;
    }
    Serial.printf("[ram] %u B not available, trying smaller pool\n", static_cast<unsigned>(want));
  }
  return 0;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==================================================");
  Serial.println("  ESP32-S3 DIY ELECTRONIC DRUM MODULE  v1.0");
  Serial.println("==================================================");
  Serial.printf("cpu %lu MHz  flash %lu MB  heap %lu kB  cores %d\n",
                static_cast<unsigned long>(getCpuFrequencyMhz()),
                static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)),
                static_cast<unsigned long>(ESP.getFreeHeap() / 1024), portNUM_PROCESSORS);

  // 1. user interface -------------------------------------------------------
  s_buttons.begin();
  const bool uiOk = s_ui.begin();
  s_ui.bind(&s_sd, &s_audio, &s_pool, &s_bank);
  if (!uiOk) Serial.println("[ui] running headless, OLED not detected");
  s_ui.showMessage("DRUM MODULE", "starting...", "v1.0");

  // 2. persisted settings ---------------------------------------------------
  s_control.begin(&s_sd, &s_bank, &s_audio, &s_pool, &s_trigger, &s_ui, &s_buttons);
  s_control.restore();

  // 3. storage --------------------------------------------------------------
  s_ui.showMessage("SD CARD", "mounting...");
  const bool sdOk = s_sd.begin();
  state().sdOk.store(sdOk);

  // 4. sample RAM -----------------------------------------------------------
  s_poolBytes = allocatePool();
  if (s_poolBytes == 0) {
    Serial.println("[ram] FATAL: no RAM for the sample pool - audio disabled");
    s_ui.showMessage("AUDIO OFF", "not enough RAM");
  }
  s_bank.begin(s_poolMem, s_poolBytes);
  state().poolCapacity.store(static_cast<uint32_t>(s_poolBytes));

  // 5. kit ------------------------------------------------------------------
  if (sdOk && s_poolBytes > 0 && KIT_AUTOLOAD_ON_BOOT) {
    s_ui.showMessage("LOADING KIT", KIT_NAMES[state().kit.load()]);
    const KitReport rep = s_sd.loadKit(state().kit.load(), s_bank);
    state().kitLoaded.store(rep.loaded);
    state().kitMissing.store(static_cast<uint8_t>(rep.missing + rep.invalid + rep.noRam));
    state().poolUsed.store(static_cast<uint32_t>(s_bank.usedBytes()));
    if (rep.loaded == 0) {
      s_ui.showMessage("NO SAMPLES", "copy /DRUM/<KIT>/", "*.wav to the card");
    }
  }

  // 6. audio ----------------------------------------------------------------
  const bool audioOk = s_audio.begin(&s_pool);
  state().audioOk.store(audioOk);
  Serial.printf("[audio] %s  %d Hz  %d frames  latency %.1f ms\n", audioOk ? "OK" : "FAILED",
                AUDIO_SAMPLE_RATE, AUDIO_BLOCK_FRAMES, s_audio.dmaLatencyUs() / 1000.0);

  // 7. sensors --------------------------------------------------------------
  s_adc.begin();
  const bool adcOk = s_adc.selfTest();
  state().adcOk.store(adcOk);
  if (!adcOk) {
    Serial.println("[adc] WARNING: both MCP3208 read 0 or 4095 on all channels");
    Serial.println("[adc]          check SCK/MOSI/MISO, both CS lines and 3V3");
  }

  s_hh.begin();
  s_trigger.begin(&s_adc, &s_audio, &s_pool, &s_hh);
  {
    uint16_t thr[PAD_COUNT];
    for (uint8_t p = 0; p < PAD_COUNT; ++p) thr[p] = state().threshold[p].load();
    s_trigger.setThresholds(thr);
  }
  if (!s_trigger.startTask()) Serial.println("[trg] FATAL: trigger task could not start");

  s_debug.begin(&s_sd, &s_bank, &s_trigger, &s_pool, &s_audio, &s_adc);

  state().page.store(UI_HOME);
  s_ui.markDirty();
  s_debug.printInfo();
  Serial.println("[sys] ready - hit a pad");
}

void loop() {
  s_control.update();
  s_debug.poll();

  // The display is a pure reader of the shared state, refreshed at UI_REFRESH_MS
  static uint32_t lastUi = 0;
  if (millis() - lastUi >= UI_REFRESH_MS) {
    lastUi = millis();
    s_ui.markDirty();
  }
  s_ui.render();

  delay(2);  // give the idle task and the trigger task their share
}
