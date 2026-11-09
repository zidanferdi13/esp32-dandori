#include "display.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <lvgl.h>

#include "../../board_config.h"

namespace {

constexpr int kDrawBufLines = 48;
constexpr uint8_t kGt911AddrA = 0x5D;
constexpr uint8_t kGt911AddrB = 0x14;
constexpr uint16_t kGt911RegStatus = 0x814E;  // bit7 = data ready, low nibble = touch count
constexpr uint16_t kGt911RegPoint1 = 0x8150;  // x lo, x hi, y lo, y hi

esp_lcd_panel_handle_t s_panel = nullptr;
uint8_t s_touchAddr = 0;  // 0 = GT911 not found

uint8_t s_exio = 0;
bool s_backlightOn = false;
bool s_swallowTouch = false;  // wake-up touch: report released until the finger lifts
uint16_t s_screenOffMin = 0;
int16_t s_lastX = 0, s_lastY = 0;
bool s_lastPressed = false;

void ch422gWrite(uint8_t command, uint8_t value) {
  Wire.beginTransmission(command);
  Wire.write(value);
  Wire.endTransmission();
}

void exioSet(uint8_t bit, bool high) {
  s_exio = high ? (s_exio | bit) : (s_exio & ~bit);
  ch422gWrite(board::kCh422gOutput, s_exio);
}

bool gt911Write(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(s_touchAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool gt911Read(uint8_t addr, uint16_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, static_cast<uint8_t>(len)) != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
  return true;
}

// Reads the first touch point. Keeps reporting the last state between GT911 updates.
bool gt911Point(int16_t& x, int16_t& y) {
  uint8_t status = 0;
  if (!gt911Read(s_touchAddr, kGt911RegStatus, &status, 1)) return s_lastPressed;
  if (status & 0x80) {
    const uint8_t count = status & 0x0F;
    s_lastPressed = false;
    uint8_t p[4];
    if (count > 0 && gt911Read(s_touchAddr, kGt911RegPoint1, p, sizeof(p))) {
      s_lastX = static_cast<int16_t>(p[0] | (p[1] << 8));
      s_lastY = static_cast<int16_t>(p[2] | (p[3] << 8));
      s_lastPressed = true;
    }
    gt911Write(kGt911RegStatus, 0);  // release the buffer
  }
  x = s_lastX;
  y = s_lastY;
  return s_lastPressed;
}

bool beginTouch() {
  // GT911 reset with INT low selects I2C address 0x5D.
  pinMode(board::kTouchInt, OUTPUT);
  digitalWrite(board::kTouchInt, LOW);
  exioSet(board::kExioTouchReset, false);
  delay(100);
  exioSet(board::kExioTouchReset, true);
  delay(200);
  pinMode(board::kTouchInt, INPUT);

  uint8_t probe = 0;
  for (uint8_t addr : {kGt911AddrA, kGt911AddrB}) {
    if (gt911Read(addr, kGt911RegStatus, &probe, 1)) {
      s_touchAddr = addr;
      Serial.printf("display: GT911 at 0x%02X\n", addr);
      return true;
    }
  }
  return false;
}

bool beginPanel() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = board::kPclkHz;
  cfg.timings.h_res = board::kScreenW;
  cfg.timings.v_res = board::kScreenH;
  cfg.timings.hsync_pulse_width = board::kHsyncPulseWidth;
  cfg.timings.hsync_back_porch = board::kHsyncBackPorch;
  cfg.timings.hsync_front_porch = board::kHsyncFrontPorch;
  cfg.timings.vsync_pulse_width = board::kVsyncPulseWidth;
  cfg.timings.vsync_back_porch = board::kVsyncBackPorch;
  cfg.timings.vsync_front_porch = board::kVsyncFrontPorch;
  cfg.timings.flags.pclk_active_neg = board::kPclkActiveNeg;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  cfg.bounce_buffer_size_px = board::kBounceBufferPx;
  cfg.dma_burst_size = 64;
  cfg.hsync_gpio_num = board::kPinHsync;
  cfg.vsync_gpio_num = board::kPinVsync;
  cfg.de_gpio_num = board::kPinDe;
  cfg.pclk_gpio_num = board::kPinPclk;
  cfg.disp_gpio_num = -1;
  // RGB565 data lines: D0-D4 = B, D5-D10 = G, D11-D15 = R.
  int d = 0;
  for (int pin : board::kPinB) cfg.data_gpio_nums[d++] = pin;
  for (int pin : board::kPinG) cfg.data_gpio_nums[d++] = pin;
  for (int pin : board::kPinR) cfg.data_gpio_nums[d++] = pin;
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &s_panel) != ESP_OK) return false;
  if (esp_lcd_panel_reset(s_panel) != ESP_OK) return false;
  return esp_lcd_panel_init(s_panel) == ESP_OK;
}

uint32_t tickMillis() { return millis(); }

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  // Copies into the driver's PSRAM frame buffer; the buffer can be reused right after.
  esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
  lv_display_flush_ready(disp);
}

void touchReadCb(lv_indev_t*, lv_indev_data_t* data) {
  data->state = LV_INDEV_STATE_RELEASED;
  if (!s_touchAddr) return;

  int16_t x = 0, y = 0;
  const bool pressed = gt911Point(x, y);

  if (!s_backlightOn) {
    if (pressed) {
      displaySetBacklight(true);
      s_swallowTouch = true;
      lv_display_trigger_activity(nullptr);
    }
    return;
  }
  if (s_swallowTouch) {
    if (!pressed) s_swallowTouch = false;
    return;
  }
  if (pressed) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
}

}  // namespace

bool displayBegin() {
  Wire.begin(board::kI2cSda, board::kI2cScl, board::kI2cHz);
  ch422gWrite(board::kCh422gMode, board::kCh422gModeOutputEnable);
  s_exio = board::kExioTouchReset | board::kExioLcdReset | board::kExioSdCs;  // backlight off
  ch422gWrite(board::kCh422gOutput, s_exio);

  if (!beginPanel()) {
    Serial.println("display: RGB panel init failed");
    return false;
  }
  if (!beginTouch()) {
    Serial.print("display: GT911 not found; I2C devices:");
    for (uint8_t addr = 1; addr < 127; ++addr) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", addr);
    }
    Serial.println();
  }

  lv_init();
  lv_tick_set_cb(tickMillis);

  const size_t bufBytes = board::kScreenW * kDrawBufLines * sizeof(uint16_t);
  void* buf1 = heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
  void* buf2 = heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
  if (!buf1 || !buf2) {
    Serial.println("display: draw buffer alloc failed");
    return false;
  }
  lv_display_t* disp = lv_display_create(board::kScreenW, board::kScreenH);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, buf1, buf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchReadCb);

  displaySetBacklight(true);
  return true;
}

void displayTick() {
  lv_timer_handler();
  if (s_screenOffMin > 0 && s_backlightOn &&
      lv_display_get_inactive_time(nullptr) > static_cast<uint32_t>(s_screenOffMin) * 60000UL) {
    displaySetBacklight(false);
  }
}

void displaySetBacklight(bool on) {
  s_backlightOn = on;
  exioSet(board::kExioBacklight, on);
}

bool displayBacklightOn() { return s_backlightOn; }

void displaySetScreenOffMinutes(uint16_t minutes) { s_screenOffMin = minutes; }
