// ===========================================================================
//  adc_mcp3208.h  --  SPI driver for the two MCP3208 12-bit ADCs
// ===========================================================================
//  The ADC bus is private to the trigger task: the SD card sits on its own SPI
//  controller, so an SD write can never delay an ADC scan (a requirement of
//  the spec - "SD access must not disturb piezo detection").
//
//  Bus:  ESP32-S3 SPI3 (HSPI), mode 0, 1 MHz  (MCP3208 is rated 1 MHz at
//        2.7 V / 2 MHz at 5 V; we power it from 3.3 V so 1 MHz is the safe
//        number).  24 clocks per channel = 24 us, so a full 16 channel scan
//        costs ~400 us, which leaves the trigger task idle most of the time.
// ===========================================================================
#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "config.h"
#include "core/mcp3208_frame.h"

namespace drum {

constexpr uint8_t ADC_CHIPS = 2;
constexpr uint8_t ADC_CHANNELS = 8;

class Mcp3208Bus {
 public:
  bool begin();

  // Reads all 16 channels.  Raw values are 0..4095 with VREF = 3.3 V.
  void scanAll(uint16_t out[ADC_CHIPS][ADC_CHANNELS]);

  uint16_t read(uint8_t chip, uint8_t channel);

  // A stuck-low or stuck-high bus is the most common wiring mistake; used by
  // the self test page and the serial diagnostics.
  bool selfTest();

  uint32_t scans() const { return scans_; }
  uint32_t microsPerScan() const { return usPerScan_; }
  float scanRateHz() const { return rateHz_; }

  void noteScanTime(uint32_t us, uint32_t nowMs);

 private:
  SPIClass spi_{HSPI};
  const uint8_t cs_[ADC_CHIPS] = {PIN_ADC_CS0, PIN_ADC_CS1};
  const SPISettings settings_{ADC_SPI_HZ, MSBFIRST, SPI_MODE0};
  uint32_t scans_ = 0;
  uint32_t usPerScan_ = 0;
  uint32_t rateHz_ = 0;
  uint32_t rateWindowStart_ = 0;
  uint32_t rateWindowScans_ = 0;
};

}  // namespace drum
