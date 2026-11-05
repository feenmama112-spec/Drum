// ===========================================================================
//  adc_mcp3208.cpp
// ===========================================================================
#include "hal/adc_mcp3208.h"

namespace drum {

bool Mcp3208Bus::begin() {
  for (uint8_t i = 0; i < ADC_CHIPS; ++i) {
    pinMode(cs_[i], OUTPUT);
    digitalWrite(cs_[i], HIGH);  // CS must idle high or the chip stays shut down
  }
  spi_.begin(PIN_ADC_SCK, PIN_ADC_MISO, PIN_ADC_MOSI, -1);
  rateWindowStart_ = millis();
  return true;
}

uint16_t Mcp3208Bus::read(uint8_t chip, uint8_t channel) {
  if (chip >= ADC_CHIPS) return 0;

  uint8_t tx[3];
  uint8_t rx[3] = {0, 0, 0};
  mcp3208BuildCommand(channel, tx);

  spi_.beginTransaction(settings_);
  digitalWrite(cs_[chip], LOW);
  spi_.transferBytes(tx, rx, 3);
  digitalWrite(cs_[chip], HIGH);
  spi_.endTransaction();

  return mcp3208Decode(rx);
}

void Mcp3208Bus::scanAll(uint16_t out[ADC_CHIPS][ADC_CHANNELS]) {
  for (uint8_t chip = 0; chip < ADC_CHIPS; ++chip) {
    for (uint8_t ch = 0; ch < ADC_CHANNELS; ++ch) {
      out[chip][ch] = read(chip, ch);
    }
  }
  ++scans_;
  ++rateWindowScans_;
}

void Mcp3208Bus::noteScanTime(uint32_t us, uint32_t nowMs) {
  // exponential average, cheap and stable enough for the display
  usPerScan_ = (usPerScan_ == 0) ? us : (usPerScan_ * 7 + us) / 8;
  if (nowMs - rateWindowStart_ >= 1000) {
    rateHz_ = rateWindowScans_;
    rateWindowScans_ = 0;
    rateWindowStart_ = nowMs;
  }
}

bool Mcp3208Bus::selfTest() {
  // A working piezo input idles near 0 V.  If both chips read a constant
  // 0 or a constant 4095 on every channel, the SPI wiring (or MISO) is open.
  uint16_t a[ADC_CHIPS][ADC_CHANNELS];
  scanAll(a);
  scanAll(a);

  uint8_t zeros = 0;
  uint8_t full = 0;
  for (uint8_t c = 0; c < ADC_CHIPS; ++c) {
    for (uint8_t ch = 0; ch < ADC_CHANNELS; ++ch) {
      if (a[c][ch] == 0) ++zeros;
      if (a[c][ch] >= 4094) ++full;
    }
  }
  // 16 channels all railed means the bus is not answering
  return !(zeros == ADC_CHIPS * ADC_CHANNELS || full == ADC_CHIPS * ADC_CHANNELS);
}

}  // namespace drum
