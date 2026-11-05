// ===========================================================================
//  trigger_engine.cpp
// ===========================================================================
#include "app/trigger_engine.h"

#include "hal/audio_out.h"

namespace drum {

void TriggerEngine::begin(Mcp3208Bus* adc, AudioOut* audio, VoicePool* pool, HiHatPedal* hh) {
  adc_ = adc;
  audio_ = audio;
  pool_ = pool;
  hh_ = hh;

  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    TriggerParams params;
    params.threshold = PADS[p].defThreshold;
    params.maskMs = PADS[p].maskMs;
    // cymbals need a softer curve, drums a steeper one
    params.gamma = (p == PAD_HI_HAT) ? 0.55f : ((p == PAD_CRASH || p == PAD_RIDE) ? 0.7f : 0.65f);
    det_[p].configure(params);
    state().threshold[p].store(params.threshold);
  }
}

void TriggerEngine::setThresholds(const uint16_t values[PAD_COUNT]) {
  for (uint8_t p = 0; p < PAD_COUNT; ++p) {
    if (values[p] == 0) continue;
    det_[p].setThreshold(values[p]);
    state().threshold[p].store(values[p]);
  }
}

bool TriggerEngine::startTask() {
  const BaseType_t ok =
      xTaskCreatePinnedToCore(&TriggerEngine::taskEntry, "trigger", 4096, this, 18, &task_, 1);
  return ok == pdPASS;
}

void TriggerEngine::taskEntry(void* arg) { static_cast<TriggerEngine*>(arg)->taskLoop(); }

void TriggerEngine::fire(uint8_t pad, uint8_t velocity) {
  if (pad >= PAD_COUNT) return;
  SampleId s = (pad == PAD_HI_HAT) ? hh_->sampleForZone() : padDefaultSample(static_cast<PadId>(pad));

  TriggerMsg msg;
  msg.sample = static_cast<uint8_t>(s);
  msg.velocity = velocity;
  msg.chokeGroup = PADS[pad].chokeGroup;  // the audio task chokes before it starts
  audio_->post(msg);

  state().lastPad.store(pad);
  state().lastSample.store(msg.sample);
  state().lastVelocity.store(velocity);
  state().lastHitMs.store(millis());
  state().hitCount.store(state().hitCount.load() + 1);
  ++hits_;
}

void TriggerEngine::testHit(uint8_t pad, uint8_t velocity) {
  if (pad >= PAD_COUNT) return;
  fire(pad, velocity);
}

void TriggerEngine::runCalibration() {
  TriggerParams p;
  (void)p;
  state().calibrating.store(true);
  state().calProgress.store(0);

  uint16_t floorMax[PAD_COUNT] = {0};
  const uint32_t start = millis();

  while ((millis() - start) < CAL_SAMPLES_MS) {
    adc_->scanAll(raw_);
    for (uint8_t i = 0; i < PAD_COUNT; ++i) {
      const uint16_t v = raw_[PADS[i].adcChip][PADS[i].adcChannel];
      if (v > floorMax[i]) floorMax[i] = v;
    }
    const uint32_t elapsed = millis() - start;
    state().calProgress.store(static_cast<uint8_t>(elapsed * 100u / CAL_SAMPLES_MS));
    vTaskDelay(1);
  }

  for (uint8_t i = 0; i < PAD_COUNT; ++i) {
    float t = static_cast<float>(floorMax[i]) * CAL_MULTIPLIER + CAL_MARGIN_COUNTS;
    if (t < THRESHOLD_MIN_COUNTS) t = THRESHOLD_MIN_COUNTS;
    if (t > THRESHOLD_MAX_COUNTS) t = THRESHOLD_MAX_COUNTS;
    const uint16_t thr = static_cast<uint16_t>(t);
    det_[i].setThreshold(thr);
    det_[i].reset();
    state().threshold[i].store(thr);
    Serial.printf("[cal] %-10s floor %4u -> threshold %4u\n", PADS[i].name,
                  static_cast<unsigned>(floorMax[i]), static_cast<unsigned>(thr));
  }

  state().calProgress.store(100);
  state().calibrating.store(false);
  calRequested_ = false;
  Serial.println("[cal] done");
}

void TriggerEngine::taskLoop() {
  uint8_t yieldCounter = 0;

  for (;;) {
    if (calRequested_) {
      runCalibration();
      continue;
    }

    const uint32_t now = millis();
    const uint32_t t0 = micros();
    adc_->scanAll(raw_);
    const uint32_t dtus = micros() - t0;
    adc_->noteScanTime(dtus, now);
    state().adcUsPerScan.store(dtus);
    state().adcRateHz.store(static_cast<uint32_t>(adc_->scanRateHz()));

    // ---- hi-hat pedal (also produces the foot "chick") --------------------
    uint8_t chickVelocity = 0;
    const bool chick = hh_->update(raw_[PEDAL_CHIP][PEDAL_CHANNEL], now, &chickVelocity);
    state().hhPct.store(hh_->positionPct());
    state().hhZone.store(static_cast<uint8_t>(hh_->zone()));
    state().hhRaw.store(hh_->rawFiltered());

    if (chick) {
      TriggerMsg msg;
      msg.sample = static_cast<uint8_t>(S_HH_CHICK);
      msg.velocity = chickVelocity;
      msg.chokeGroup = PADS[PAD_HI_HAT].chokeGroup;
      audio_->post(msg);
      state().lastPad.store(PAD_HI_HAT);
      state().lastSample.store(S_HH_CHICK);
      state().lastVelocity.store(chickVelocity);
      state().lastHitMs.store(now);
      state().hitCount.store(state().hitCount.load() + 1);
    }

    // ---- pads -------------------------------------------------------------
    for (uint8_t pad = 0; pad < PAD_COUNT; ++pad) {
      const uint16_t v = raw_[PADS[pad].adcChip][PADS[pad].adcChannel];
      TriggerEvent ev;
      if (det_[pad].update(v, now, &ev)) {
        state().lastPeak[pad].store(ev.peak);
        fire(pad, ev.velocity);
      }
    }

    // Yield often enough to keep the idle task (and the watchdog) happy while
    // still scanning every channel well above 1 kHz.
    if (++yieldCounter >= ADC_SCANS_PER_YIELD) {
      yieldCounter = 0;
      vTaskDelay(1);
    }
  }
}

}  // namespace drum
