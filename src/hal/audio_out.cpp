// ===========================================================================
//  audio_out.cpp  --  I2S output and the real-time mixer task
// ===========================================================================
//  Supports both Arduino-ESP32 generations:
//    * 2.x (ESP-IDF 4.4, what platform espressif32@6.9.0 installs)
//        -> legacy driver: i2s_driver_install() / i2s_write()
//    * 3.x (ESP-IDF 5.x)
//        -> new driver:    i2s_new_channel() / i2s_channel_write()
//  Both configure exactly the same bus: 16-bit stereo, 3 DMA buffers of 128
//  frames, auto clear on underrun, no MCLK (PCM5102A internal PLL).
// ===========================================================================
#include "hal/audio_out.h"

#include <esp_arduino_version.h>
#include <string.h>

#include "app/shared.h"

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include "driver/i2s_std.h"
#else
#include "driver/i2s.h"
#endif

namespace drum {

bool AudioOut::initI2s() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  i2s_chan_handle_t* tx = new i2s_chan_handle_t();
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = AUDIO_DMA_DESC_NUM;
  chan_cfg.dma_frame_num = AUDIO_BLOCK_FRAMES;
  chan_cfg.auto_clear = true;

  if (i2s_new_channel(&chan_cfg, tx, nullptr) != ESP_OK) {
    Serial.println("[audio] i2s_new_channel failed");
    delete tx;
    return false;
  }

  i2s_std_config_t std_cfg = {};
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE);
  std_cfg.slot_cfg =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;  // PCM5102A PLL: no MCLK needed
  std_cfg.gpio_cfg.bclk = static_cast<gpio_num_t>(PIN_I2S_BCK);
  std_cfg.gpio_cfg.ws = static_cast<gpio_num_t>(PIN_I2S_LRCK);
  std_cfg.gpio_cfg.dout = static_cast<gpio_num_t>(PIN_I2S_DIN);
  std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.ws_inv = false;

  if (i2s_channel_init_std_mode(*tx, &std_cfg) != ESP_OK) {
    Serial.println("[audio] i2s_channel_init_std_mode failed");
    delete tx;
    return false;
  }
  if (i2s_channel_enable(*tx) != ESP_OK) {
    Serial.println("[audio] i2s_channel_enable failed");
    delete tx;
    return false;
  }
  tx_ = tx;
  return true;

#else
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;  // real stereo
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = AUDIO_DMA_DESC_NUM;
  cfg.dma_buf_len = AUDIO_BLOCK_FRAMES;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;  // silence, not the last block, on underrun
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[audio] i2s_driver_install failed");
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("[audio] i2s_set_pin failed");
    return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
#endif
}

bool AudioOut::begin(VoicePool* pool) {
  pool_ = pool;

  if (!initI2s()) return false;

  queue_ = xQueueCreate(16, sizeof(TriggerMsg));
  if (queue_ == nullptr) {
    Serial.println("[audio] trigger queue allocation failed");
    return false;
  }

  ready_ = true;
  if (xTaskCreatePinnedToCore(&AudioOut::taskEntry, "audio", 4096, this, 19, &task_, 0) != pdPASS) {
    Serial.println("[audio] task creation failed");
    ready_ = false;
    return false;
  }
  return true;
}

void AudioOut::taskEntry(void* arg) { static_cast<AudioOut*>(arg)->taskLoop(); }

bool AudioOut::post(const TriggerMsg& msg) {
  if (queue_ == nullptr) return false;
  if (xQueueSend(queue_, &msg, 0) != pdTRUE) {
    ++drops_;
    return false;
  }
  return true;
}

void AudioOut::beginKitChange() {
  if (!ready_) return;
  holdReq_ = true;
  const uint32_t t0 = millis();
  while (!holdAck_ && (millis() - t0) < 60) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void AudioOut::endKitChange() {
  holdReq_ = false;
  holdAck_ = false;
}

uint32_t AudioOut::dmaLatencyUs() const {
  return static_cast<uint32_t>((static_cast<uint64_t>(AUDIO_DMA_DESC_NUM) * AUDIO_BLOCK_FRAMES *
                                1000000ull) /
                               AUDIO_SAMPLE_RATE);
}

void AudioOut::writeSilence() {
  memset(stereo_, 0, sizeof(stereo_));
  size_t written = 0;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  i2s_channel_write(*static_cast<i2s_chan_handle_t*>(tx_), stereo_, sizeof(stereo_), &written,
                    pdMS_TO_TICKS(200));
#else
  i2s_write(I2S_NUM_0, stereo_, sizeof(stereo_), &written, pdMS_TO_TICKS(200));
#endif
}

void AudioOut::taskLoop() {
  for (;;) {
    if (holdReq_) {
      // Kit is being reloaded: silence, stop voices, acknowledge.
      pool_->stopAll();
      holdAck_ = true;
      writeSilence();
      continue;
    }
    holdAck_ = false;

    TriggerMsg msg;
    while (xQueueReceive(queue_, &msg, 0) == pdTRUE) {
      if (msg.chokeGroup != 0) pool_->chokeGroup(msg.chokeGroup);
      pool_->start(static_cast<SampleId>(msg.sample), msg.velocity);
    }

    pool_->render(mono_, AUDIO_BLOCK_FRAMES);

    // mono -> both channels (the module plays mono samples over a stereo DAC)
    for (size_t i = 0; i < AUDIO_BLOCK_FRAMES; ++i) {
      stereo_[2 * i] = mono_[i];
      stereo_[2 * i + 1] = mono_[i];
    }

    size_t written = 0;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const esp_err_t err = i2s_channel_write(*static_cast<i2s_chan_handle_t*>(tx_), stereo_,
                                            sizeof(stereo_), &written, pdMS_TO_TICKS(100));
#else
    const esp_err_t err =
        i2s_write(I2S_NUM_0, stereo_, sizeof(stereo_), &written, pdMS_TO_TICKS(100));
#endif
    if (err != ESP_OK || written != sizeof(stereo_)) {
      ++writeErrors_;
    }
    ++blocks_;

    // published here (not read from another task) so the UI never races the pool
    state().activeVoices.store(pool_->activeCount());
    state().audioBlocks.store(blocks_);
    state().audioErrors.store(writeErrors_);
  }
}

}  // namespace drum
