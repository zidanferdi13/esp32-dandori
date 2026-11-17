// Pengaturan (spec §5.4): menu on the left, pane on the right. WiFi, Server API, Mesin, Jam and Keamanan
// need the admin PIN once per visit; Tampilan and Tentang are open.
#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cctype>
#include <memory>

#include "../core/wib_time.h"
#include "../platform/clock.h"
#include "../platform/display.h"
#include "../platform/storage.h"
#include "ui_internal.h"

namespace ui {
namespace {

constexpr const char* kFwVersion = "0.1.0";
constexpr int32_t kPaneW = 800 - 224 - 16;

enum class Item { Tampilan, Wifi, Server, Mesin, Jam, Keamanan, Tentang };
struct MenuEntry {
  Item item;
  const char* text;
  bool locked;
};
const MenuEntry kMenu[] = {
    {Item::Tampilan, LV_SYMBOL_EYE_OPEN "  Tampilan", false}, {Item::Wifi, LV_SYMBOL_WIFI "  WiFi", true},
    {Item::Server, LV_SYMBOL_UPLOAD "  Server API", true},    {Item::Mesin, LV_SYMBOL_SETTINGS "  Mesin", true},
    {Item::Jam, LV_SYMBOL_REFRESH "  Jam", true},             {Item::Keamanan, LV_SYMBOL_EDIT "  Keamanan", true},
    {Item::Tentang, LV_SYMBOL_LIST "  Tentang", false},
};

Item s_item = Item::Tampilan;
bool s_unlocked = false;
lv_obj_t* s_pane = nullptr;
std::string s_pinFirst;

void showPane();

void saveSettings() {
  settingsSave(ctx().settings);
  netApplySettings(ctx().settings);
}

lv_obj_t* paneTitle(const std::string& text) {
  lv_obj_t* l = label(s_pane, text, &lv_font_montserrat_20, pal().fg);
  return l;
}

// A tappable row: title + current value, opens `onTap`.
lv_obj_t* row(const std::string& title, const std::string& value, std::function<void()> onTap) {
  lv_obj_t* r = card(s_pane);
  lv_obj_set_size(r, LV_PCT(100), 72);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* t = label(r, title, &lv_font_montserrat_16, pal().muted);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, -4);
  lv_obj_t* v = label(r, value.empty() ? std::string("(kosong)") : value, &lv_font_montserrat_20, pal().fg);
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_set_width(v, 420);
  lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 4);
  if (onTap) {
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* e = label(r, LV_SYMBOL_EDIT, &lv_font_montserrat_20, pal().accent);
    lv_obj_align(e, LV_ALIGN_RIGHT_MID, 0, 0);
    onClick(r, std::move(onTap));
  }
  return r;
}

void editText(const std::string& title, const std::string& initial, bool password,
              std::function<std::string(const std::string&)> onSave) {
  textEditor(title, initial, password, true, 128, std::move(onSave), showPane);
}

// ---------- panes ----------

void segmented(lv_obj_t* parent, const std::vector<std::string>& options, int selected, std::function<void(int)> fn) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, LV_PCT(100), 60);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_gap(box, 8, 0);
  // Layout has not run yet, so use the pane's fixed width rather than lv_obj_get_content_width().
  const int32_t n = static_cast<int32_t>(options.size());
  const int32_t w = (kPaneW - 8 * (n - 1)) / n;
  (void)parent;
  for (size_t i = 0; i < options.size(); ++i) {
    const bool on = static_cast<int>(i) == selected;
    const int index = static_cast<int>(i);
    button(box, options[i], on ? pal().accent : pal().card2, on ? pal().onAccent : pal().fg,
           &lv_font_montserrat_16, w, 56, [fn, index]() { fn(index); });
  }
}

void paneTampilan() {
  Settings& s = ctx().settings;
  paneTitle("Tema");
  segmented(s_pane, {"Light", "Dark", "Auto"}, static_cast<int>(s.theme), [](int i) {
    ctx().settings.theme = static_cast<ThemeMode>(i);
    saveSettings();
    later([]() { uiApplyTheme(); });  // rebuilds this screen (and the clicked button) with the new palette
  });
  paneTitle("Layar mati setelah tidak disentuh");
  static const uint16_t kOff[] = {0, 1, 5, 10, 30};
  int sel = 2;
  for (int i = 0; i < 5; ++i) {
    if (kOff[i] == s.screenOffMin) sel = i;
  }
  segmented(s_pane, {"Tidak", "1 mnt", "5 mnt", "10 mnt", "30 mnt"}, sel, [](int i) {
    ctx().settings.screenOffMin = kOff[i];
    displaySetScreenOffMinutes(kOff[i]);
    saveSettings();
    later(showPane);
  });
}

void paneWifi() {
  const Settings& s = ctx().settings;
  const std::string status = netWifiConnected() ? s.wifiSsid + "  (terhubung, " + netIp() + ")"
                                                : (s.wifiSsid.empty() ? std::string("Belum diatur")
                                                                      : s.wifiSsid + "  (belum terhubung)");  row("Jaringan saat ini", status, nullptr);
  lv_obj_t* scan = button(s_pane, LV_SYMBOL_REFRESH "  Cari jaringan", pal().card2, pal().fg, &lv_font_montserrat_16,
                          LV_PCT(100), 48, nullptr);
  lv_obj_t* list = lv_obj_create(s_pane);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);

  onClick(scan, [list]() {
    lv_obj_clean(list);
    label(list, "Mencari...", &lv_font_montserrat_16, pal().muted);
    netRequestWifiScan();
  });
  hooks().onNet = [list](const NetResult& r) {
    if (r.kind != NetResult::WifiScan) return;
    lv_obj_clean(list);
    if (!r.ok) {
      label(list, r.error.empty() ? "Pemindaian gagal" : r.error, &lv_font_montserrat_16, pal().muted);
    } else if (r.ssids.empty()) {
      label(list, "Tidak ada jaringan ditemukan", &lv_font_montserrat_16, pal().muted);
    }
    for (const std::string& entry : r.ssids) {
      const size_t t1 = entry.find('\t');
      const size_t t2 = entry.find('\t', t1 + 1);
      const std::string ssid = entry.substr(0, t1);
      const std::string rssi = entry.substr(t1 + 1, t2 - t1 - 1);
      const bool secured = entry.substr(t2 + 1) == "1";
      lv_obj_t* b = button(list, ssid + "   " + rssi + " dBm" + (secured ? "   (terkunci)" : ""), pal().card,
                           pal().fg, &lv_font_montserrat_16, LV_PCT(100), 56, nullptr);
      onClick(b, [ssid, secured]() {
        auto apply = [ssid](const std::string& pass) {
          ctx().settings.wifiSsid = ssid;
          ctx().settings.wifiPass = pass;
          saveSettings();
          return std::string();
        };
        if (secured) {
          editText("Password WiFi \"" + ssid + "\"", "", true, apply);
        } else {
          apply("");
          later(showPane);
        }
      });
    }
  };
}

void paneServer() {
  Settings& s = ctx().settings;
  row("Alamat server (base URL)", s.baseUrl, [&s]() {
    editText("Alamat server, contoh http://192.168.0.104:4500", s.baseUrl, false, [](const std::string& v) {
      if (!isValidBaseUrl(v)) return std::string("Alamat tidak valid");
      ctx().settings.baseUrl = v;
      saveSettings();
      return std::string();
    });
  });
  row("Token API", s.token.empty() ? std::string() : std::string(s.token.size(), '*'), [&s]() {
    editText("Token API (kosongkan bila tidak dipakai)", s.token, true, [](const std::string& v) {
      ctx().settings.token = v;
      saveSettings();
      return std::string();
    });
  });

  lv_obj_t* testRow = lv_obj_create(s_pane);
  lv_obj_remove_style_all(testRow);
  lv_obj_set_size(testRow, LV_PCT(100), 52);
  lv_obj_t* test = button(testRow, "Tes koneksi", pal().card2, pal().fg, &lv_font_montserrat_16, 200, 48, nullptr);
  lv_obj_align(test, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* testResult = label(testRow, "", &lv_font_montserrat_16, pal().muted);
  lv_obj_align(testResult, LV_ALIGN_LEFT_MID, 216, 0);
  onClick(test, [testResult]() {
    lv_label_set_text(testResult, "Menghubungi server...");
    netRequestTestConnection(nowEpoch());
  });
  hooks().onNet = [testResult](const NetResult& r) {
    if (r.kind != NetResult::TestConn) return;
    std::string text;
    if (r.httpStatus >= 200 && r.httpStatus < 300) {
      text = "OK (" + std::to_string(r.httpStatus) + ")";
    } else if (r.httpStatus == -2) {
      text = "Alamat server belum diatur";
    } else if (r.httpStatus <= 0) {
      text = "Gagal (tidak terhubung)";
    } else {
      text = "Gagal (kode " + std::to_string(r.httpStatus) + ")";
    }
    lv_label_set_text(testResult, text.c_str());
    lv_obj_set_style_text_color(testResult,
                                r.httpStatus >= 200 && r.httpStatus < 300 ? pal().success : pal().danger, 0);
  };

  const size_t pending = ctx().store.pendingCount();
  lv_obj_t* q = row("Antrian belum terkirim", std::to_string(pending) + " data", nullptr);
  lv_obj_t* resend = button(q, "Kirim ulang", pal().accent, pal().onAccent, &lv_font_montserrat_16, 160, 56, []() {
    ctx().outbox.kick();
    toast("Mengirim ulang antrian...");
  });
  lv_obj_align(resend, LV_ALIGN_RIGHT_MID, 0, 0);
}

void paneMesin() {
  Settings& s = ctx().settings;
  row("Kode mesin", s.machineCode, [&s]() {
    editText("Kode mesin, contoh PALLETIZER-01", s.machineCode, false, [](const std::string& v) {
      std::string code;
      for (char ch : v) {
        if (ch != ' ') code.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
      }
      if (code.empty()) return std::string("Kode mesin wajib diisi");
      if (code.size() > 24) return std::string("Maksimal 24 karakter");
      ctx().settings.machineCode = code;
      saveSettings();
      return std::string();
    });
  });
}

void paneJam() {
  Settings& s = ctx().settings;
  const int64_t last = lastSyncEpoch();
  row("Status", clockSynced() ? "Tersinkron  |  " + hhmmss(nowEpoch()) + " WIB" : std::string("Belum sinkron"),
      nullptr);
  row("Sinkron terakhir", isClockValid(last) ? hhmmss(last) : std::string("-"), nullptr);
  row("Server NTP", s.ntpServer, [&s]() {
    editText("Server NTP, contoh pool.ntp.org atau IP router", s.ntpServer, false, [](const std::string& v) {
      if (v.empty() || v.find(' ') != std::string::npos) return std::string("Alamat tidak valid");
      ctx().settings.ntpServer = v;
      saveSettings();
      return std::string();
    });
  });
}

void pinPad(const std::string& prompt, std::function<void(const std::string&)> onComplete) {
  auto entered = std::make_shared<std::string>();
  paneTitle(prompt);
  lv_obj_t* dots = label(s_pane, "_ _ _ _", &lv_font_montserrat_28, pal().fg);
  numericKeypad(s_pane, 320, 260, [entered, dots, onComplete](char k) {
    if (k == '\b') {
      if (!entered->empty()) entered->pop_back();
    } else if (k == 'c') {
      entered->clear();
    } else if (entered->size() < 4) {
      entered->push_back(k);
    }
    std::string shown;
    for (size_t i = 0; i < 4; ++i) shown += i < entered->size() ? "* " : "_ ";
    lv_label_set_text(dots, shown.c_str());
    if (entered->size() == 4) {
      const std::string value = *entered;
      entered->clear();
      lv_label_set_text(dots, "_ _ _ _");
      onComplete(value);
    }
  });
}

void paneLocked() {
  pinPad("Masukkan PIN admin", [](const std::string& pin) {
    if (pin == ctx().settings.pin) {
      s_unlocked = true;
    } else {
      toast("PIN salah");
    }
    later(showPane);  // the pad that called us is rebuilt
  });
}

void paneKeamanan() {
  pinPad(s_pinFirst.empty() ? "PIN baru (4 angka)" : "Ulangi PIN baru", [](const std::string& pin) {
    if (s_pinFirst.empty()) {
      s_pinFirst = pin;
      toast("Ulangi PIN baru");
      later(showPane);
      return;
    }
    if (pin != s_pinFirst || !isValidPin(pin)) {
      s_pinFirst.clear();
      toast("PIN tidak sama, ulangi dari awal");
      later(showPane);
      return;
    }
    ctx().settings.pin = pin;
    saveSettings();
    s_pinFirst.clear();
    toast("PIN berhasil diganti");
    later(showPane);
  });
}

void paneTentang() {
  const StorageUsage u = storageUsage();
  row("Versi firmware", kFwVersion, nullptr);
  row("MAC", netMac(), nullptr);
  row("Memori bebas", std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024) + " KB internal  |  " +
                          std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024) + " KB PSRAM",
      nullptr);
  row("Penyimpanan", std::to_string(u.usedBytes / 1024) + " / " + std::to_string(u.totalBytes / 1024) + " KB  |  " +
                         std::to_string(ctx().store.pendingCount()) + " antri",
      nullptr);
}

void showPane() {
  if (!s_pane) return;
  lv_obj_clean(s_pane);
  hooks().onNet = nullptr;
  const MenuEntry* entry = nullptr;
  for (const auto& m : kMenu) {
    if (m.item == s_item) entry = &m;
  }
  if (entry && entry->locked && !s_unlocked) {
    paneLocked();
    return;
  }
  switch (s_item) {
    case Item::Tampilan: paneTampilan(); break;
    case Item::Wifi: paneWifi(); break;
    case Item::Server: paneServer(); break;
    case Item::Mesin: paneMesin(); break;
    case Item::Jam: paneJam(); break;
    case Item::Keamanan: paneKeamanan(); break;
    case Item::Tentang: paneTentang(); break;
  }
}

}  // namespace

void lockSettings() {
  s_unlocked = false;
  s_pane = nullptr;
  s_pinFirst.clear();
}

void buildPinLock(lv_obj_t* root) { buildSettings(root); }

void buildSettings(lv_obj_t* root) {
  lv_obj_t* side = lv_obj_create(root);
  lv_obj_set_size(side, 208, LV_PCT(100));
  lv_obj_align(side, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(side, pal().card, 0);
  lv_obj_set_style_radius(side, 0, 0);
  lv_obj_set_style_border_width(side, 0, 0);
  lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(side, 8, 0);
  lv_obj_set_style_pad_row(side, 2, 0);

  button(side, LV_SYMBOL_HOME "  Home", pal().card2, pal().fg, &lv_font_montserrat_16, LV_PCT(100), 48,
         []() { uiGo(Screen::Home); });
  for (const auto& m : kMenu) {
    const bool on = m.item == s_item;
    const Item item = m.item;
    button(side, m.text, on ? pal().accentSoft : pal().card, on ? pal().accent : pal().muted, &lv_font_montserrat_16,
           LV_PCT(100), 48, [item]() {
             s_item = item;
             uiGo(Screen::Settings);  // rebuild to move the highlight
           });
  }

  s_pane = lv_obj_create(root);
  lv_obj_remove_style_all(s_pane);
  lv_obj_set_size(s_pane, kPaneW, LV_PCT(100));
  lv_obj_align(s_pane, LV_ALIGN_RIGHT_MID, -16, 0);
  lv_obj_set_flex_flow(s_pane, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_ver(s_pane, 12, 0);
  lv_obj_set_style_pad_row(s_pane, 10, 0);
  lv_obj_add_flag(s_pane, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      s_pane, [](lv_event_t*) { s_pane = nullptr; }, LV_EVENT_DELETE, nullptr);
  showPane();
}

}  // namespace ui
