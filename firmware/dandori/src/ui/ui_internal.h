// Shared helpers for the screen files. Only the main loop calls into these.
#pragma once
#include <lvgl.h>

#include <functional>
#include <string>

#include "ui.h"

namespace ui {

struct Palette {
  lv_color_t bg, card, card2, fg, muted, accent, accentSoft;
};

constexpr uint32_t kGreen = 0x1F9D55;
constexpr uint32_t kRed = 0xD63C3C;
constexpr uint32_t kAmber = 0xF2C94C;
constexpr int kStatusBarH = 44;

AppContext& ctx();
const Palette& pal();
bool dark();

// Per-screen hooks, set by a screen's builder and cleared on navigation.
struct Hooks {
  std::function<void(uint32_t nowMs)> tick;
  std::function<void(const NetResult&)> onNet;
};
Hooks& hooks();

Screen current();
void persistCounter();  // counterSave(counter.snapshot()); call after every counter change
std::string prodDateNow();  // production date, or "" while the clock is not synced
void toast(const std::string& text);
void setStatusBarAlert(bool alert);  // red status bar while downtime runs

// Widgets
lv_obj_t* label(lv_obj_t* parent, const std::string& text, const lv_font_t* font, lv_color_t color);
lv_obj_t* button(lv_obj_t* parent, const std::string& text, lv_color_t bg, lv_color_t fg, const lv_font_t* font,
                 int32_t w, int32_t h, std::function<void()> onClick);
lv_obj_t* card(lv_obj_t* parent);
void setEnabled(lv_obj_t* btn, bool enabled);
void onClick(lv_obj_t* obj, std::function<void()> fn);
// Runs fn on the next LVGL cycle. Use it when a click handler would delete its own widget.
void later(std::function<void()> fn);
// Keys 1-9, BACKSPACE, 0, HAPUS. onKey gets '0'..'9', '\b' (backspace) or 'c' (clear).
lv_obj_t* numericKeypad(lv_obj_t* parent, int32_t w, int32_t h, std::function<void(char)> onKey);
// Full-screen editor over the top layer, with the on-screen keyboard. onSave returns an error text, or "" to
// accept. onClose runs after the overlay is dismissed, whatever the reason.
void textEditor(const std::string& title, const std::string& initial, bool password, bool oneLine, size_t maxLen,
                std::function<std::string(const std::string&)> onSave, std::function<void()> onClose);
void confirm(const std::string& text, const std::string& yes, std::function<void()> onYes);

// Screen builders (one file each). `root` is the content area under the status bar.
void buildHome(lv_obj_t* root);
void buildMoSelect(lv_obj_t* root);
void buildCounting(lv_obj_t* root);
void buildCodeInput(lv_obj_t* root);
void buildResult(lv_obj_t* root);
void buildHistory(lv_obj_t* root);
void buildPinLock(lv_obj_t* root);
void buildSettings(lv_obj_t* root);

// Downtime flow state shared between screens.
void resetCodeEntry();
void setResultEventId(const std::string& id);
void lockSettings();

}  // namespace ui
