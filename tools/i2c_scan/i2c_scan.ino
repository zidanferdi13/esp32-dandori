// Minimal I2C scanner for the Waveshare ESP32-S3-Touch-LCD-7C.
// Expected on a healthy board: 0x24 (IO expander) and 0x5D (GT911).
// The older non-C board uses SDA 8 / SCL 9 instead.
#include <Wire.h>

constexpr int kSda = 47;
constexpr int kScl = 48;

// Idle I2C lines must read 1 (board pull-ups). 0 = something holds the line low.
void printLines(const char* when) {
  pinMode(kSda, INPUT_PULLUP);
  pinMode(kScl, INPUT_PULLUP);
  delay(2);
  Serial.printf("%s: SDA=%d SCL=%d\n", when, digitalRead(kSda), digitalRead(kScl));
}

void setup() {
  Serial.begin(115200);
  delay(3000);  // give the USB host time to attach
  printLines("lines before Wire");
  Wire.begin(kSda, kScl);
}

void loop() {
  int found = 0;
  int errs[6] = {};  // Wire error codes: 2 = NACK (nobody there), 4/5 = bus error/timeout
  Serial.printf("[%lu ms] scan:", millis());
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    const uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf(" 0x%02X", addr);
      ++found;
    } else if (err < 6) {
      ++errs[err];
    }
  }
  Serial.printf("  (%d found) err2=%d err4=%d err5=%d\n", found, errs[2], errs[4], errs[5]);
  delay(2000);
}
