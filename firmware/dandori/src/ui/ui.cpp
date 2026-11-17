#include "ui.h"

#include <Arduino.h>

#include "../core/wib_time.h"
#include "../platform/clock.h"
#include "../platform/storage.h"
#include "ui_internal.h"

namespace ui {
namespace {

constexpr uint32_t kRestoreWaitMs = 20000;  // wait this long for NTP before resuming a timer without a clock

const Palette kDarkPalette = {
    lv_color_hex(0x101418), lv_color_hex(0x171D23), lv_color_hex(0x232C37), lv_color_hex(0xECE7DE),
    lv_color_hex(0xA7B0BA), lv_color_hex(0x2364C4), lv_color_hex(0x232C37),
    lv_color_hex(0xE8776B), lv_color_hex(0x58B98A), lv_color_hex(0xF2B75C), lv_color_hex(0xF3F5F8)};
const Palette kLightPalette = {
    lv_color_hex(0xF1EEE8), lv_color_hex(0xFAF8F4), lv_color_hex(0xE4DED4), lv_color_hex(0x16191D),
    lv_color_hex(0x5A626B), lv_color_hex(0x2364C4), lv_color_hex(0xE4DED4),
    lv_color_hex(0xB23A2C), lv_color_hex(0x1F6B45), lv_color_hex(0x8A5A00), lv_color_hex(0xF3F5F8)};

AppContext* s_ctx = nullptr;
Palette s_pal = kDarkPalette;
Hooks s_hooks;
Screen s_current = Screen::Home;
Screen s_pending = Screen::Home;
bool s_goScheduled = false;

lv_obj_t* s_statusBar = nullptr;
lv_obj_t* s_statusLeft = nullptr;
lv_obj_t* s_statusRight = nullptr;
lv_obj_t* s_content = nullptr;
bool s_statusAlert = false;

bool s_restorePending = false;
CounterSnapshot s_restoreSnap;
uint32_t s_bootMs = 0;
uint32_t s_lastSecondTick = 0;

void buildStatusBar(lv_obj_t* scr) {
  s_statusBar = lv_obj_create(scr);
  lv_obj_remove_style_all(s_statusBar);
  lv_obj_set_size(s_statusBar, LV_PCT(100), kStatusBarH);
  lv_obj_set_style_bg_opa(s_statusBar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(s_statusBar, 14, 0);
  lv_obj_remove_flag(s_statusBar, LV_OBJ_FLAG_SCROLLABLE);
  s_statusLeft = label(s_statusBar, "", &lv_font_montserrat_16, s_pal.fg);
  lv_obj_set_width(s_statusLeft, 320);
  lv_label_set_long_mode(s_statusLeft, LV_LABEL_LONG_DOT);
  lv_obj_align(s_statusLeft, LV_ALIGN_LEFT_MID, 0, 0);
  s_statusRight = label(s_statusBar, "", &lv_font_montserrat_16, s_pal.muted);
  lv_obj_set_width(s_statusRight, 420);
  lv_label_set_long_mode(s_statusRight, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(s_statusRight, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(s_statusRight, LV_ALIGN_RIGHT_MID, 0, 0);
  setStatusBarAlert(s_statusAlert);
}

void refreshStatusBar() {
  if (!s_statusLeft) return;
  const AppContext& c = ctx();
  std::string left = c.settings.machineCode;
  lv_label_set_text(s_statusLeft, left.c_str());

  std::string right;
  const int64_t now = nowEpoch();
  if (clockSynced()) right += "Shift " + std::to_string(shiftOf(now)) + "    ";
  right += netWifiConnected() ? LV_SYMBOL_WIFI " Online" : LV_SYMBOL_WARNING " Offline";
  const size_t pending = c.store.pendingCount();
  if (pending > 0) right += std::string("    " LV_SYMBOL_UPLOAD " ") + std::to_string(pending);
  right += "    ";
  right += clockSynced() ? hhmmss(now).substr(0, 5) : std::string("--:--");
  lv_label_set_text(s_statusRight, right.c_str());
}

void rebuildScreen() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, s_pal.bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  buildStatusBar(scr);

  s_content = lv_obj_create(scr);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, LV_PCT(100), lv_display_get_vertical_resolution(nullptr) - kStatusBarH);
  lv_obj_set_pos(s_content, 0, kStatusBarH);
  lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  s_hooks = Hooks{};
  setStatusBarAlert(false);
  if (s_restorePending) {
    lv_obj_t* l = label(s_content, "Memulihkan pencatatan downtime\nMenunggu sinkronisasi jam",
                        &lv_font_montserrat_28, s_pal.fg);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
  } else {
    switch (s_current) {
      case Screen::Home: buildHome(s_content); break;
      case Screen::MoSelect: buildMoSelect(s_content); break;
      case Screen::Counting: buildCounting(s_content); break;
      case Screen::CodeInput: buildCodeInput(s_content); break;
      case Screen::Result: buildResult(s_content); break;
      case Screen::History: buildHistory(s_content); break;
      case Screen::PinLock: buildPinLock(s_content); break;
      case Screen::Settings: buildSettings(s_content); break;
    }
  }
  refreshStatusBar();
}

void doGo(void*) {
  s_goScheduled = false;
  const bool leavingSettings = (s_current == Screen::Settings || s_current == Screen::PinLock) &&
                               s_pending != Screen::Settings && s_pending != Screen::PinLock;
  if (leavingSettings) lockSettings();
  s_current = s_pending;
  rebuildScreen();
}

Screen screenForState(CounterState s) {
  switch (s) {
    case CounterState::Running: return Screen::Counting;
    case CounterState::AwaitCode: return Screen::CodeInput;
    default: return Screen::Home;
  }
}

void finishRestore(bool clockOk) {
  AppContext& c = ctx();
  if (clockOk) {
    c.counter.restore(s_restoreSnap, millis(), nowEpoch());
  } else {
    c.counter.restore(s_restoreSnap, s_bootMs, 0);
  }
  s_restorePending = false;
  persistCounter();
  if (c.counter.state() == CounterState::Running) {
    toast(c.counter.estimated() ? std::string("Pencatatan dilanjutkan (durasi perkiraan)")
                                : "Pencatatan dilanjutkan sejak " + hhmmss(c.counter.startEpoch()));
  }
  uiGo(screenForState(c.counter.state()));
}

void toastDone(lv_timer_t* t) {
  lv_obj_delete(static_cast<lv_obj_t*>(lv_timer_get_user_data(t)));
  lv_timer_delete(t);
}

}  // namespace

AppContext& ctx() { return *s_ctx; }
const Palette& pal() { return s_pal; }
bool dark() { return ctx().lastDark; }
Hooks& hooks() { return s_hooks; }
Screen current() { return s_current; }

void persistCounter() { counterSave(ctx().counter.snapshot()); }

std::string prodDateNow() { return clockSynced() ? productionDate(nowEpoch()) : std::string(); }

void toast(const std::string& text) {
  lv_obj_t* box = lv_obj_create(lv_layer_top());
  lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(box, dark() ? s_pal.card2 : s_pal.fg, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 16, 0);
  label(box, text, &lv_font_montserrat_20, dark() ? s_pal.fg : s_pal.card);
  lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_timer_t* t = lv_timer_create(toastDone, 3500, box);
  lv_timer_set_repeat_count(t, 1);
}

void setStatusBarAlert(bool alert) {
  s_statusAlert = alert;
  if (!s_statusBar) return;
  lv_obj_set_style_bg_color(s_statusBar, alert ? s_pal.danger : s_pal.card, 0);
  lv_obj_set_style_text_color(s_statusLeft, alert ? s_pal.onAccent : s_pal.fg, 0);
  lv_obj_set_style_text_color(s_statusRight, alert ? s_pal.onAccent : s_pal.muted, 0);
}

lv_obj_t* label(lv_obj_t* parent, const std::string& text, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text.c_str());
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

void onClick(lv_obj_t* obj, std::function<void()> fn) {
  auto* holder = new std::function<void()>(std::move(fn));
  lv_obj_add_event_cb(
      obj, [](lv_event_t* e) { (*static_cast<std::function<void()>*>(lv_event_get_user_data(e)))(); },
      LV_EVENT_CLICKED, holder);
  lv_obj_add_event_cb(
      obj, [](lv_event_t* e) { delete static_cast<std::function<void()>*>(lv_event_get_user_data(e)); },
      LV_EVENT_DELETE, holder);
}

void later(std::function<void()> fn) {
  lv_async_call(
      [](void* p) {
        auto* f = static_cast<std::function<void()>*>(p);
        (*f)();
        delete f;
      },
      new std::function<void()>(std::move(fn)));
}

lv_obj_t* button(lv_obj_t* parent, const std::string& text, lv_color_t bg, lv_color_t fg, const lv_font_t* font,
                 int32_t w, int32_t h, std::function<void()> fn) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 8, 0);
  lv_obj_set_style_bg_color(b, lv_color_darken(bg, LV_OPA_20), LV_STATE_PRESSED);
  lv_obj_set_style_outline_color(b, dark() ? s_pal.warning : s_pal.accent, LV_STATE_FOCUS_KEY);
  lv_obj_set_style_outline_width(b, 3, LV_STATE_FOCUS_KEY);
  lv_obj_set_style_outline_pad(b, 3, LV_STATE_FOCUS_KEY);
  lv_obj_set_style_bg_color(b, s_pal.card2, LV_STATE_DISABLED);
  lv_obj_add_flag(b, LV_OBJ_FLAG_STATE_TRICKLE);  // disabled/pressed reach the label so its colour follows
  lv_obj_t* l = label(b, text, font, fg);
  lv_obj_center(l);
  if (fn) onClick(b, std::move(fn));
  return b;
}

lv_obj_t* card(lv_obj_t* parent) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_set_style_bg_color(c, s_pal.card, 0);
  lv_obj_set_style_border_color(c, s_pal.card2, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, 8, 0);
  lv_obj_set_style_pad_all(c, 12, 0);
  return c;
}

void setEnabled(lv_obj_t* btn, bool enabled) {
  if (enabled) {
    lv_obj_remove_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
  } else {
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(btn, s_pal.muted, LV_STATE_DISABLED);
    for (uint32_t i = 0; i < lv_obj_get_child_count(btn); ++i)
      lv_obj_set_style_text_color(lv_obj_get_child(btn, i), s_pal.muted, LV_STATE_DISABLED);
  }
}

lv_obj_t* numericKeypad(lv_obj_t* parent, int32_t w, int32_t h, std::function<void(char)> onKey) {
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, w, h);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(grid, 8, 0);
  static const char* kKeys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", LV_SYMBOL_BACKSPACE, "0", "HAPUS"};
  static const char kCodes[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '\b', '0', 'c'};
  const int32_t kw = (w - 16) / 3;
  const int32_t kh = (h - 24) / 4;
  for (int i = 0; i < 12; ++i) {
    const char code = kCodes[i];
    const bool special = code == '\b' || code == 'c';
    button(grid, kKeys[i], special ? s_pal.card2 : s_pal.card, special ? s_pal.danger : s_pal.fg,
           special && code == 'c' ? &lv_font_montserrat_20 : &lv_font_montserrat_28, kw, kh,
           [onKey, code]() { onKey(code); });
  }
  return grid;
}

void textEditor(const std::string& title, const std::string& initial, bool password, bool oneLine, size_t maxLen,
                std::function<std::string(const std::string&)> onSave, std::function<void()> onClose) {
  lv_obj_t* ov = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, s_pal.bg, 0);
  lv_obj_set_style_pad_all(ov, 0, 0);
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = label(ov, title, &lv_font_montserrat_20, s_pal.fg);
  lv_obj_set_width(t, 768);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 8);
  lv_obj_t* err = label(ov, "", &lv_font_montserrat_16, s_pal.danger);
  lv_obj_set_width(err, 768);
  lv_obj_align(err, LV_ALIGN_TOP_LEFT, 16, 172);

  lv_obj_t* ta = lv_textarea_create(ov);
  lv_textarea_set_one_line(ta, oneLine);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_max_length(ta, static_cast<uint32_t>(maxLen));
  lv_textarea_set_text(ta, initial.c_str());
  lv_obj_set_style_text_font(ta, oneLine ? &lv_font_montserrat_28 : &lv_font_montserrat_20, 0);
  lv_obj_set_size(ta, 768, oneLine ? 64 : 100);
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t* kb = lv_keyboard_create(ov);
  lv_obj_set_size(kb, LV_PCT(100), 280);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, ta);

  auto* finish = new std::function<void(bool)>([ov, ta, err, onSave, onClose](bool save) {
    if (save) {
      const std::string msg = onSave(lv_textarea_get_text(ta));
      if (!msg.empty()) {
        lv_label_set_text(err, msg.c_str());
        return;
      }
    }
    lv_obj_delete_async(ov);
    onClose();
  });
  lv_obj_add_event_cb(
      kb,
      [](lv_event_t* e) {
        auto* fn = static_cast<std::function<void(bool)>*>(lv_event_get_user_data(e));
        switch (lv_event_get_code(e)) {
          case LV_EVENT_READY: (*fn)(true); break;
          case LV_EVENT_CANCEL: (*fn)(false); break;
          case LV_EVENT_DELETE: delete fn; break;
          default: break;
        }
      },
      LV_EVENT_ALL, finish);
  lv_obj_t* hint = label(ov, LV_SYMBOL_OK " = Simpan     " LV_SYMBOL_KEYBOARD " = Batal", &lv_font_montserrat_16,
                         s_pal.muted);
  lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 16, oneLine ? 112 : 148);
}

void confirm(const std::string& text, const std::string& yes, std::function<void()> onYes) {
  lv_obj_t* mbox = lv_msgbox_create(nullptr);
  lv_obj_set_style_text_font(mbox, &lv_font_montserrat_20, 0);
  lv_msgbox_add_text(mbox, text.c_str());
  lv_obj_t* no = lv_msgbox_add_footer_button(mbox, "Tetap isi");
  lv_obj_t* ok = lv_msgbox_add_footer_button(mbox, yes.c_str());
  lv_obj_set_style_bg_color(ok, lv_color_hex(kRed), 0);
  lv_obj_set_height(no, 56);
  lv_obj_set_height(ok, 56);
  onClick(no, [mbox]() { lv_msgbox_close_async(mbox); });
  onClick(ok, [mbox, onYes]() {
    lv_msgbox_close_async(mbox);
    onYes();
  });
}

}  // namespace ui

using namespace ui;

void uiBegin(AppContext& c) {
  s_ctx = &c;
  s_bootMs = millis();
  c.lastDark = resolveDark(c.settings.theme, nowEpoch(), c.settings.theme != ThemeMode::Light);

  CounterSnapshot snap;
  Screen first = Screen::Home;
  if (counterLoad(snap)) {
    if (snap.state == CounterState::Running && isClockValid(snap.startEpoch) && !clockSynced()) {
      s_restoreSnap = snap;
      s_restorePending = true;  // resolved in uiTick once NTP syncs or the wait runs out
    } else {
      c.counter.restore(snap, millis(), nowEpoch());
      first = screenForState(c.counter.state());
      if (c.counter.state() == CounterState::Running) {
        s_restoreSnap = snap;
        toast(c.counter.estimated() ? std::string("Pencatatan dilanjutkan (durasi perkiraan)")
                                    : "Pencatatan dilanjutkan sejak " + hhmmss(c.counter.startEpoch()));
      }
    }
  }
  s_current = first;
  uiApplyTheme();
}

void uiGo(Screen s) {
  s_pending = s;
  if (!s_goScheduled) {
    s_goScheduled = true;
    lv_async_call(doGo, nullptr);  // never rebuild inside the event that asked for it
  }
}

void uiTick(uint32_t nowMs) {
  if (s_restorePending) {
    if (clockSynced()) {
      finishRestore(true);
    } else if (nowMs - s_bootMs > kRestoreWaitMs) {
      finishRestore(false);
    }
  }
  if (s_hooks.tick) s_hooks.tick(nowMs);
  if (nowMs - s_lastSecondTick < 1000) return;
  s_lastSecondTick = nowMs;
  refreshStatusBar();

  AppContext& c = ctx();
  if (resolveDark(c.settings.theme, nowEpoch(), c.lastDark) != c.lastDark) uiApplyTheme();
}

void uiOnNetResult(const NetResult& r) {
  if (s_hooks.onNet) s_hooks.onNet(r);
  refreshStatusBar();
}

void uiApplyTheme() {
  AppContext& c = ctx();
  c.lastDark = resolveDark(c.settings.theme, nowEpoch(), c.lastDark);
  s_pal = c.lastDark ? kDarkPalette : kLightPalette;
  lv_display_t* disp = lv_display_get_default();
  lv_theme_t* th = lv_theme_default_init(disp, s_pal.accent, lv_color_hex(kRed), c.lastDark, &lv_font_montserrat_16);
  lv_display_set_theme(disp, th);
  rebuildScreen();
}
