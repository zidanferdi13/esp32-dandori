// Waveshare ESP32-S3-Touch-LCD-7C (800x480, 32MB flash / 16MB octal PSRAM).
// Pinout from waveshareteam/ESP32-S3-Touch-LCD-7C, example/arduino/06_TOUCH.
#pragma once
#include <stdint.h>

namespace board {

constexpr int kScreenW = 800;
constexpr int kScreenH = 480;

// Shared I2C bus: GT911 touch + 16-bit IO expander at 0x24.
constexpr int kI2cSda = 47;
constexpr int kI2cScl = 48;
constexpr uint32_t kI2cHz = 400000;

// IO expander 0x24: write [command, value low byte, value high byte].
constexpr uint8_t kExpanderAddr = 0x24;
constexpr uint8_t kExpanderMode = 0x02;    // 1 = output, 0 = input
constexpr uint8_t kExpanderOutput = 0x03;
constexpr uint16_t kIoTouchReset = 1u << 1;  // IO1
constexpr uint16_t kIoBacklight = 1u << 2;   // IO2
constexpr uint16_t kIoPa = 1u << 3;          // IO3, audio amplifier enable
constexpr uint16_t kIoSdCs = 1u << 4;        // IO4

// GT911: INT held low during reset selects address 0x5D (0x14 otherwise).
constexpr int kTouchInt = 4;

// RGB565 parallel panel.
constexpr int kPinDe = 5, kPinVsync = 3, kPinHsync = 46, kPinPclk = 7;
constexpr int kPinR[5] = {1, 2, 42, 41, 40};
constexpr int kPinG[6] = {39, 0, 45, 9, 8, 21};
constexpr int kPinB[5] = {14, 38, 18, 17, 10};

constexpr uint16_t kHsyncPolarity = 0, kHsyncFrontPorch = 8, kHsyncPulseWidth = 4, kHsyncBackPorch = 8;
constexpr uint16_t kVsyncPolarity = 0, kVsyncFrontPorch = 8, kVsyncPulseWidth = 4, kVsyncBackPorch = 8;
constexpr uint16_t kPclkActiveNeg = 1;
// Restore the pixel clock at which this panel previously displayed an image.
constexpr int32_t kPclkHz = 16000000;
// Two internal DMA buffers; these absorb short bandwidth spikes, not flash-cache outages.
constexpr size_t kBounceBufferPx = 800 * 20;

}  // namespace board
