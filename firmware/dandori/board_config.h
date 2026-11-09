// Waveshare ESP32-S3-Touch-LCD-7 (800x480, non-"B" variant).
// NOT yet verified on hardware: compare against Waveshare's official demo during the device check.
#pragma once
#include <stdint.h>

namespace board {

constexpr int kScreenW = 800;
constexpr int kScreenH = 480;

// Shared I2C bus: GT911 touch + CH422G IO expander.
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;
constexpr uint32_t kI2cHz = 400000;

// CH422G: the "address" is the command. 0x24 = mode register, 0x38 = output register (EXIO0..7).
constexpr uint8_t kCh422gMode = 0x24;
constexpr uint8_t kCh422gOutput = 0x38;
constexpr uint8_t kCh422gModeOutputEnable = 0x01;
constexpr uint8_t kExioTouchReset = 1 << 1;
constexpr uint8_t kExioBacklight = 1 << 2;
constexpr uint8_t kExioLcdReset = 1 << 3;
constexpr uint8_t kExioSdCs = 1 << 4;

// GT911: INT held low during reset selects address 0x5D (0x14 otherwise).
constexpr int kTouchInt = 4;

// RGB565 parallel panel.
constexpr int kPinDe = 5, kPinVsync = 3, kPinHsync = 46, kPinPclk = 7;
constexpr int kPinR[5] = {1, 2, 42, 41, 40};
constexpr int kPinG[6] = {39, 0, 45, 48, 47, 21};
constexpr int kPinB[5] = {14, 38, 18, 17, 10};

constexpr uint16_t kHsyncPolarity = 0, kHsyncFrontPorch = 8, kHsyncPulseWidth = 4, kHsyncBackPorch = 8;
constexpr uint16_t kVsyncPolarity = 0, kVsyncFrontPorch = 8, kVsyncPulseWidth = 4, kVsyncBackPorch = 8;
constexpr uint16_t kPclkActiveNeg = 1;
constexpr int32_t kPclkHz = 16000000;
// Raise (e.g. 800 * 10) if the picture jitters while WiFi is busy.
constexpr size_t kBounceBufferPx = 0;

}  // namespace board
