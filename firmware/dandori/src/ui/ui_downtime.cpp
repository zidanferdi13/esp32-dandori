// Downtime flow (spec §4, §5.2): Pilih MO -> Counting -> Input kode -> Hasil.
#include <Arduino.h>

#include <memory>

#include "../core/problem_codes.h"
#include "../core/wib_time.h"
#include "../platform/clock.h"
#include "../platform/storage.h"
#include "ui_internal.h"

namespace ui {
namespace {

constexpr uint32_t kResultWaitMs = 8000;
constexpr uint32_t kResultHoldMs = 3000;

std::string s_code;         // keypad input, survives a theme rebuild
std::string s_catatan;      // operator note, survives a theme rebuild
std::string s_resultId;     // event shown on the Result screen
int s_moSelected = -1;

// Notes may contain newlines; single-line labels need them collapsed.
std::string oneLineText(const std::string& s) {
  std::string out = s;
  for (char& ch : out) {
    if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
  }
  return out;
}

std::string durationText(uint32_t sec) {
  return std::to_string(sec / 60) + " mnt " + std::to_string(sec % 60) + " dtk";
}

std::string clockText(uint32_t sec) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", static_cast<unsigned>(sec / 3600),
           static_cast<unsigned>((sec / 60) % 60), static_cast<unsigned>(sec % 60));
  return buf;
}

lv_obj_t* backButton(lv_obj_t* root, Screen target) {
  lv_obj_t* b = button(root, LV_SYMBOL_LEFT " Kembali", pal().card2, pal().fg, &lv_font_montserrat_16, 140, 44,
                       [target]() { uiGo(target); });
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 16, 12);
  return b;
}

// ---------- Pilih MO ----------

void showMoList(lv_obj_t* area, lv_obj_t* chooseBtn) {
  lv_obj_clean(area);
  const auto& mos = ctx().lastMoList;
  if (mos.empty()) {
    lv_obj_t* l = label(area, "Tidak ada MO tersedia", &lv_font_montserrat_20, pal().muted);
    lv_obj_center(l);
    return;
  }
  lv_obj_t* list = lv_obj_create(area);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  for (size_t i = 0; i < mos.size(); ++i) {
    lv_obj_t* row = card(list);
    lv_obj_set_size(row, LV_PCT(100), mos[i].produk.empty() ? 72 : 80);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    if (static_cast<int>(i) == s_moSelected) {
      lv_obj_set_style_border_color(row, pal().accent, 0);
      lv_obj_set_style_border_width(row, 2, 0);
      lv_obj_set_style_bg_color(row, pal().accentSoft, 0);
      lv_obj_t* check = label(row, LV_SYMBOL_OK, &lv_font_montserrat_20, pal().accent);
      lv_obj_align(check, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    lv_obj_t* mo = label(row, mos[i].mo, &lv_font_montserrat_20, pal().fg);
    lv_obj_set_width(mo, 660);
    lv_label_set_long_mode(mo, LV_LABEL_LONG_DOT);
    lv_obj_align(mo, LV_ALIGN_TOP_LEFT, 0, 0);
    if (!mos[i].produk.empty()) {
      lv_obj_t* p = label(row, mos[i].produk, &lv_font_montserrat_16, pal().muted);
      lv_obj_set_width(p, 660);
      lv_label_set_long_mode(p, LV_LABEL_LONG_DOT);
      lv_obj_align(p, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    const int index = static_cast<int>(i);
    onClick(row, [area, chooseBtn, index]() {
      s_moSelected = index;
      setEnabled(chooseBtn, true);
      later([area, chooseBtn]() { showMoList(area, chooseBtn); });  // the clicked row is rebuilt
    });
  }
}

void showMoMessage(lv_obj_t* area, const std::string& text, lv_color_t color) {
  lv_obj_clean(area);
  lv_obj_t* l = label(area, text, &lv_font_montserrat_20, color);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
}

void requestMoList(lv_obj_t* area, lv_obj_t* chooseBtn) {
  lv_obj_clean(area);
  lv_obj_t* sp = lv_spinner_create(area);
  lv_obj_set_size(sp, 64, 64);
  lv_obj_align(sp, LV_ALIGN_LEFT_MID, 0, -12);
  lv_obj_t* l = label(area, "Mengambil daftar MO...", &lv_font_montserrat_20, pal().muted);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 88, -12);
  lv_obj_t* hint = label(area, "Pastikan WiFi dan alamat server sudah diatur.", &lv_font_montserrat_16, pal().muted);
  lv_obj_align(hint, LV_ALIGN_LEFT_MID, 88, 24);
  setEnabled(chooseBtn, false);
  s_moSelected = -1;
  netRequestMoList(nowEpoch());
}

}  // namespace

void buildMoSelect(lv_obj_t* root) {
  const bool hasMo = !ctx().counter.activeMo().empty();
  backButton(root, hasMo ? Screen::Counting : Screen::Home);
  lv_obj_t* title = label(root, "Pilih MO yang sedang jalan", &lv_font_montserrat_28, pal().fg);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 172, 20);

  lv_obj_t* area = lv_obj_create(root);
  lv_obj_remove_style_all(area);
  lv_obj_set_size(area, 768, 248);
  lv_obj_align(area, LV_ALIGN_TOP_MID, 0, 80);

  lv_obj_t* chooseBtn = button(root, "PILIH MO INI", pal().accent, pal().onAccent, &lv_font_montserrat_20, 512, 72,
                               nullptr);
  lv_obj_align(chooseBtn, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
  setEnabled(chooseBtn, false);
  onClick(chooseBtn, []() {
    AppContext& c = ctx();
    if (s_moSelected < 0 || s_moSelected >= static_cast<int>(c.lastMoList.size())) return;
    if (c.counter.setActiveMo(c.lastMoList[s_moSelected].mo)) {
      persistCounter();
      uiGo(Screen::Counting);
    }
  });

  lv_obj_t* refresh = button(root, LV_SYMBOL_REFRESH " Refresh", pal().card2, pal().fg, &lv_font_montserrat_20, 240,
                             72, nullptr);
  lv_obj_align(refresh, LV_ALIGN_BOTTOM_LEFT, 16, -16);
  onClick(refresh, [area, chooseBtn]() { requestMoList(area, chooseBtn); });

  hooks().onNet = [area, chooseBtn](const NetResult& r) {
    if (r.kind != NetResult::MoList) return;
    if (r.ok) {
      ctx().lastMoList = r.mos;  // a failure never touches the list or the active MO
      showMoList(area, chooseBtn);
    } else if (r.httpStatus == -2) {
      showMoMessage(area, "Alamat server belum diatur\n(Pengaturan > Server API)", pal().danger);
    } else if (r.httpStatus <= 0) {
      showMoMessage(area, "Daftar MO gagal dimuat.\nPeriksa koneksi lalu coba lagi.", pal().danger);
    } else {
      showMoMessage(area, "Gagal ambil daftar MO (kode " + std::to_string(r.httpStatus) + ")", pal().danger);
    }
  };
  requestMoList(area, chooseBtn);
}

// ---------- Counting ----------

void buildCounting(lv_obj_t* root) {
  AppContext& c = ctx();
  const CounterState st = c.counter.state();
  const bool running = st == CounterState::Running;
  const std::string& mo = c.counter.activeMo();

  if (st == CounterState::AwaitCode) {  // reached via Home while a code is pending
    uiGo(Screen::CodeInput);
    return;
  }

  if (running) {
    setStatusBarAlert(true);
  } else {
    backButton(root, Screen::Home);
  }

  lv_obj_t* moLbl = label(root, mo.empty() ? std::string("MO: belum dipilih") : "MO: " + mo,
                          &lv_font_montserrat_16, pal().muted);
  lv_obj_set_width(moLbl, 460);
  lv_label_set_long_mode(moLbl, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(moLbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(moLbl, LV_ALIGN_TOP_RIGHT, -16, 24);

  lv_obj_t* caption = label(root, running ? "DOWNTIME DICATAT" : "SIAP MENCATAT", &lv_font_montserrat_20,
                            running ? pal().danger : pal().muted);
  lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 92);

  lv_obj_t* timer = label(root, clockText(c.counter.elapsedSec(millis())), &lv_font_montserrat_48,
                          running ? pal().danger : pal().muted);
  lv_obj_set_style_transform_scale(timer, 400, 0);  // 48px font scaled to ~75px
  lv_obj_align(timer, LV_ALIGN_TOP_MID, 0, 126);
  lv_obj_set_style_transform_pivot_x(timer, LV_PCT(50), 0);
  lv_obj_set_style_transform_pivot_y(timer, LV_PCT(50), 0);

  const std::string since = c.counter.startEpoch() ? "Mulai " + hhmmss(c.counter.startEpoch()) + " WIB"
                                                   : std::string("Mulai --:--:-- WIB");
  lv_obj_t* s = label(root, since + (c.counter.estimated() ? "   (durasi perkiraan)" : ""),
                      &lv_font_montserrat_20, running ? pal().danger : pal().muted);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 228);

  if (running) {
    lv_obj_t* stop = button(root, LV_SYMBOL_STOP "  Selesai catat downtime", pal().danger, pal().onAccent,
                            &lv_font_montserrat_28, 752, 96, []() {
                              AppContext& cc = ctx();
                              if (cc.counter.stop(millis(), nowEpoch())) {
                                persistCounter();
                                resetCodeEntry();
                                uiGo(Screen::CodeInput);
                              }
                            });
    lv_obj_align(stop, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_t* foot = label(root, "Ganti MO dikunci selama pencatatan berjalan. Bukan kontrol mesin.",
                           &lv_font_montserrat_16, pal().muted);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -28);
  } else if (st == CounterState::NoMo) {
    lv_obj_t* pick = button(root, "Pilih MO dulu", pal().accent, pal().onAccent, &lv_font_montserrat_28, 752, 96,
                            []() { uiGo(Screen::MoSelect); });
    lv_obj_align(pick, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_t* foot = label(root, "Pencatatan downtime, bukan kontrol mesin", &lv_font_montserrat_16, pal().muted);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -28);
  } else {
    lv_obj_t* start = button(root, LV_SYMBOL_PLAY "  Mulai catat downtime", pal().accent, pal().onAccent,
                             &lv_font_montserrat_28, 752, 96, []() {
                               if (ctx().counter.start(millis(), nowEpoch())) {
                                 persistCounter();
                                 uiGo(Screen::Counting);
                               }
                             });
    lv_obj_align(start, LV_ALIGN_BOTTOM_MID, 0, -72);

    const DaySummary d = c.store.summary(prodDateNow());
    lv_obj_t* sum = label(root, "Hari ini: " + std::to_string(d.count) + " kejadian  |  " + durationText(d.totalSec),
                          &lv_font_montserrat_16, pal().muted);
    lv_obj_align(sum, LV_ALIGN_BOTTOM_LEFT, 20, -26);
    lv_obj_t* change = button(root, "Ganti MO", pal().card2, pal().fg, &lv_font_montserrat_16, 160, 48,
                              []() { uiGo(Screen::MoSelect); });
    lv_obj_align(change, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
  }

  if (running) {
    hooks().tick = [timer](uint32_t nowMs) {
      static uint32_t shown = UINT32_MAX;
      const uint32_t sec = ctx().counter.elapsedSec(nowMs);
      if (sec == shown) return;  // the label is built with the current value, so a stale `shown` is harmless
      shown = sec;
      lv_label_set_text(timer, clockText(sec).c_str());
    };
  }
}

// ---------- Input kode ----------

void resetCodeEntry() {
  s_code.clear();
  s_catatan.clear();
}

void buildCodeInput(lv_obj_t* root) {
  AppContext& c = ctx();
  if (c.counter.state() != CounterState::AwaitCode) {
    uiGo(Screen::Counting);
    return;
  }
  const uint32_t total = c.counter.elapsedSec(millis());
  lv_obj_t* dur = label(root, "Durasi: " + durationText(total), &lv_font_montserrat_20, pal().fg);
  lv_obj_align(dur, LV_ALIGN_TOP_LEFT, 16, 14);
  if (c.counter.estimated()) {
    lv_obj_t* est = label(root, "(durasi perkiraan)", &lv_font_montserrat_16, pal().muted);
    lv_obj_align(est, LV_ALIGN_TOP_LEFT, 16, 42);
  }
  if (c.counter.startEpoch() && c.counter.stopEpoch()) {
    const std::string range = hhmmss(c.counter.startEpoch()) + " - " + hhmmss(c.counter.stopEpoch());
    lv_obj_t* r = label(root, "Rentang waktu: " + range, &lv_font_montserrat_20, pal().muted);
    lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -16, 14);
  }

  lv_obj_t* left = lv_obj_create(root);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, 376, 344);
  lv_obj_align(left, LV_ALIGN_TOP_LEFT, 16, 76);

  label(left, "Kode problem", &lv_font_montserrat_20, pal().muted);
  lv_obj_t* codeBox = card(left);
  lv_obj_set_size(codeBox, 376, 64);
  lv_obj_align(codeBox, LV_ALIGN_TOP_LEFT, 0, 28);
  lv_obj_set_style_border_color(codeBox, pal().accent, 0);
  lv_obj_set_style_border_width(codeBox, 2, 0);
  lv_obj_t* codeLbl = label(codeBox, "", &lv_font_montserrat_48, pal().fg);
  lv_obj_center(codeLbl);

  lv_obj_t* preview = card(left);
  lv_obj_set_size(preview, 376, 80);
  lv_obj_align(preview, LV_ALIGN_TOP_LEFT, 0, 100);
  lv_obj_t* pvTop = label(preview, "", &lv_font_montserrat_16, pal().warning);
  lv_obj_set_width(pvTop, 352);
  lv_label_set_long_mode(pvTop, LV_LABEL_LONG_DOT);
  lv_obj_align(pvTop, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t* pvSub = label(preview, "", &lv_font_montserrat_20, pal().fg);
  lv_obj_set_width(pvSub, 352);
  lv_label_set_long_mode(pvSub, LV_LABEL_LONG_DOT);
  lv_obj_align(pvSub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t* noteRow = card(left);
  lv_obj_set_size(noteRow, 376, 48);
  lv_obj_align(noteRow, LV_ALIGN_TOP_LEFT, 0, 188);
  lv_obj_remove_flag(noteRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_ver(noteRow, 2, 0);
  lv_obj_add_flag(noteRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* noteCap = label(noteRow, "Catatan (opsional)", &lv_font_montserrat_16, pal().muted);
  lv_obj_align(noteCap, LV_ALIGN_TOP_LEFT, 0, -4);
  lv_obj_t* noteVal = label(noteRow, "", &lv_font_montserrat_16, pal().fg);
  lv_label_set_long_mode(noteVal, LV_LABEL_LONG_DOT);
  lv_obj_set_width(noteVal, 320);
  lv_obj_align(noteVal, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* noteIcon = label(noteRow, LV_SYMBOL_EDIT, &lv_font_montserrat_20, pal().accent);
  lv_obj_align(noteIcon, LV_ALIGN_RIGHT_MID, 0, 0);
  onClick(noteRow, []() {
    textEditor("Catatan downtime (opsional)", s_catatan, false, false, kMaxCatatanLen,
               [](const std::string& v) {
                 s_catatan = v;
                 return std::string();
               },
               []() { uiGo(Screen::CodeInput); });
  });

  lv_obj_t* cancel = button(left, "Buang", pal().card2, pal().fg, &lv_font_montserrat_20, 112, 96, []() {
    confirm("Buang catatan downtime ini?\nData ini tidak akan disimpan.", "Buang catatan", []() {
      if (ctx().counter.cancel()) {
        persistCounter();
        resetCodeEntry();
        uiGo(Screen::Counting);
      }
    });
  });
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t* send = button(left, LV_SYMBOL_OK "  Simpan & kirim", pal().accent, pal().onAccent,
                          &lv_font_montserrat_20, 256, 96, nullptr);
  lv_obj_align(send, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  auto refresh = [codeLbl, pvTop, pvSub, noteVal, send]() {
    lv_label_set_text(codeLbl, s_code.c_str());
    const int value = keypadValue(s_code);
    const ProblemCode* p = findProblemCode(value);
    if (p) {
      lv_label_set_text(pvTop, (std::string(p->jenis) + "  |  " + p->mesin).c_str());
      lv_label_set_text(pvSub, p->sub);
      lv_obj_set_style_text_color(pvSub, pal().fg, 0);
    } else {
      lv_label_set_text(pvTop, "");
      lv_label_set_text(pvSub, s_code.empty() ? "Masukkan kode 1-26" : "Kode tidak dikenal. Gunakan 1-26.");
      lv_obj_set_style_text_color(pvSub, pal().muted, 0);
    }
    lv_label_set_text(noteVal, s_catatan.empty() ? "Ketuk untuk menulis" : oneLineText(s_catatan).c_str());
    lv_obj_set_style_text_color(noteVal, s_catatan.empty() ? pal().muted : pal().fg, 0);
    setEnabled(send, p != nullptr);
  };
  refresh();

  onClick(send, []() {
    AppContext& cc = ctx();
    const int code = keypadValue(s_code);
    if (code < 0 || cc.counter.state() != CounterState::AwaitCode) return;
    DowntimeEvent e;
    e.mesin = cc.settings.machineCode;
    e.mo = cc.counter.activeMo();
    e.catatan = s_catatan;
    e.kodeProblem = code;
    e.startEpoch = cc.counter.startEpoch();
    e.stopEpoch = cc.counter.stopEpoch();
    e.totalDetik = cc.counter.elapsedSec(millis());
    e.estimated = cc.counter.estimated();
    e.prodDate = isClockValid(e.startEpoch) ? productionDate(e.startEpoch) : std::string();
    e.id = makeEventId(e.mesin, e.startEpoch, nextEventSeq());
    if (!cc.store.add(e)) {
      toast("Data belum tersimpan. Coba simpan lagi.");
      return;
    }
    cc.counter.complete();
    persistCounter();
    cc.outbox.kick();
    resetCodeEntry();
    setResultEventId(e.id);
    uiGo(Screen::Result);
  });

  lv_obj_t* kp = numericKeypad(root, 376, 344, [refresh](char k) {
    if (k == '\b') {
      if (!s_code.empty()) s_code.pop_back();
    } else if (k == 'c') {
      s_code.clear();
    } else {
      s_code = keypadAppend(s_code, k);
    }
    refresh();
  });
  lv_obj_align(kp, LV_ALIGN_TOP_LEFT, 408, 76);
}

// ---------- Hasil ----------

void setResultEventId(const std::string& id) { s_resultId = id; }

void buildResult(lv_obj_t* root) {
  const DowntimeEvent* e = ctx().store.find(s_resultId);
  lv_obj_t* icon = label(root, LV_SYMBOL_UPLOAD, &lv_font_montserrat_48, pal().muted);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_t* title = label(root, "Menyimpan...", &lv_font_montserrat_28, pal().fg);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 150);
  std::string summary;
  if (e) {
    const ProblemCode* p = findProblemCode(e->kodeProblem);
    summary = e->mo + "  |  Kode " + std::to_string(e->kodeProblem) + (p ? std::string(" ") + p->sub : "") +
              "  |  " + std::to_string(e->totalDetik) + " dtk";
  }
  lv_obj_t* sub = label(root, summary, &lv_font_montserrat_20, pal().muted);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 200);
  int32_t noteY = 250;
  if (e && !e->catatan.empty()) {
    lv_obj_t* cat = label(root, "Catatan: " + oneLineText(e->catatan), &lv_font_montserrat_16, pal().muted);
    lv_label_set_long_mode(cat, LV_LABEL_LONG_DOT);
    lv_obj_set_width(cat, 720);
    lv_obj_align(cat, LV_ALIGN_TOP_MID, 0, 232);
    noteY = 272;
  }
  lv_obj_t* note = label(root, "", &lv_font_montserrat_16, pal().muted);
  lv_obj_align(note, LV_ALIGN_TOP_MID, 0, noteY);

  const uint32_t shownAt = millis();
  struct State {
    bool decided = false;
    bool sticky = false;
    uint32_t decidedAt = 0;
  };
  auto state = std::make_shared<State>();

  auto decide = [=](const char* sym, lv_color_t color, const std::string& status, bool sticky) {
    lv_label_set_text(icon, sym);
    lv_obj_set_style_text_color(icon, color, 0);
    lv_label_set_text(title, "Data tersimpan di perangkat");
    if (sticky) {
      lv_label_set_text(note, status.c_str());
      lv_obj_t* back = button(root, "Kembali ke pencatatan", pal().accent, pal().onAccent, &lv_font_montserrat_20,
                              480, 72, []() { uiGo(Screen::Counting); });
      lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -24);
    } else {
      lv_label_set_text(note, (status + "  |  Kembali ke pencatatan...").c_str());
    }
    state->decided = true;
    state->sticky = sticky;
    state->decidedAt = millis();
  };

  // The event may already be settled (fast mock) before this screen was built.
  if (e && e->status == EventStatus::Sent) {
    decide(LV_SYMBOL_OK, pal().success, "Terkirim ke server", false);
  } else if (e && e->status == EventStatus::Rejected) {
    decide(LV_SYMBOL_CLOSE, pal().danger, "Server menolak data ini", true);
  }

  hooks().onNet = [=](const NetResult& r) {
    if (state->decided || r.kind != NetResult::SendDone || r.id != s_resultId) return;
    const SendOutcome o = classifyHttp(r.httpStatus);
    if (o == SendOutcome::Sent) {
      decide(LV_SYMBOL_OK, pal().success, "Terkirim ke server", false);
    } else if (o == SendOutcome::Rejected) {
      decide(LV_SYMBOL_CLOSE, pal().danger,
             "Server menolak data ini (kode " + std::to_string(r.httpStatus) + ")", true);
    } else {
      decide(LV_SYMBOL_SAVE, pal().warning, "Menunggu koneksi, dikirim ulang otomatis", false);
    }
  };
  hooks().tick = [=](uint32_t nowMs) {
    if (!state->decided && nowMs - shownAt > kResultWaitMs) {
      decide(LV_SYMBOL_SAVE, pal().warning, "Menunggu koneksi, dikirim ulang otomatis", false);
    }
    if (state->decided && !state->sticky && nowMs - state->decidedAt > kResultHoldMs) uiGo(Screen::Counting);
  };
}

}  // namespace ui
