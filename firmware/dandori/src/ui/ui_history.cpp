// Riwayat Hari Ini (spec §5.3): read-only list of this production day's events, newest first.
#include "../core/problem_codes.h"
#include "../core/wib_time.h"
#include "ui_internal.h"

namespace ui {
namespace {

constexpr size_t kMaxRows = 60;  // keep the LVGL object count bounded

struct StatusText {
  const char* symbol;
  const char* text;
  lv_color_t color;
};

StatusText statusOf(EventStatus s) {
  switch (s) {
    case EventStatus::Sent: return {LV_SYMBOL_OK, "Terkirim", pal().success};
    case EventStatus::Rejected: return {LV_SYMBOL_CLOSE, "Ditolak", pal().danger};
    case EventStatus::Pending: break;
  }
  return {LV_SYMBOL_UPLOAD, "Antri", pal().warning};
}

void fillList(lv_obj_t* list, lv_obj_t* header, lv_obj_t* note) {
  lv_obj_clean(list);
  AppContext& c = ctx();
  const std::string day = prodDateNow();
  const std::vector<DowntimeEvent> events = c.store.history(day);
  const DaySummary d = c.store.summary(day);
  const std::string title =
      (day.empty() ? std::string("Hari ini (jam belum sinkron)") : "Hari produksi " + day) + "   |   " +
      std::to_string(d.count) + " kejadian, " + std::to_string(d.totalSec / 60) + " mnt " +
      std::to_string(d.totalSec % 60) + " dtk";
  lv_label_set_text(header, title.c_str());
  lv_label_set_text(note, events.size() > kMaxRows ? "Menampilkan 60 kejadian terbaru. Data lain tetap tersimpan."
                                                   : "");

  if (events.empty()) {
    lv_obj_t* l = label(list, "Belum ada downtime tercatat hari ini", &lv_font_montserrat_20, pal().muted);
    lv_obj_center(l);
    return;
  }
  for (size_t i = 0; i < events.size() && i < kMaxRows; ++i) {
    const DowntimeEvent& e = events[i];
    lv_obj_t* row = card(list);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_ver(row, 6, 0);

    const std::string time = isClockValid(e.startEpoch) ? hhmmss(e.startEpoch).substr(0, 5) : std::string("--:--");
    lv_obj_t* t = label(row, time, &lv_font_montserrat_20, pal().fg);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    const ProblemCode* p = findProblemCode(e.kodeProblem);
    lv_obj_t* w = label(row, e.mo, &lv_font_montserrat_20, pal().fg);
    lv_obj_set_width(w, 340);
    lv_label_set_long_mode(w, LV_LABEL_LONG_DOT);
    lv_obj_align(w, LV_ALIGN_TOP_LEFT, 68, 0);
    lv_obj_t* sub = label(row, std::to_string(e.kodeProblem) + "  " + (p ? p->sub : "?"), &lv_font_montserrat_16,
                          pal().muted);
    lv_obj_set_width(sub, 340);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 68, 0);

    const std::string dur = (e.estimated ? "~" : "") + std::to_string(e.totalDetik / 60) + "m " +
                            std::to_string(e.totalDetik % 60) + "s";
    lv_obj_t* du = label(row, dur, &lv_font_montserrat_20, pal().fg);
    lv_obj_set_width(du, 100);
    lv_obj_set_style_text_align(du, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(du, LV_ALIGN_RIGHT_MID, -196, 0);

    const StatusText st = statusOf(e.status);
    lv_obj_t* icon = label(row, st.symbol, &lv_font_montserrat_20, st.color);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -140, 0);
    lv_obj_t* word = label(row, st.text, &lv_font_montserrat_16, st.color);
    lv_obj_set_width(word, 124);
    lv_obj_set_style_text_align(word, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(word, LV_ALIGN_RIGHT_MID, 0, 0);
  }
}

}  // namespace

void buildHistory(lv_obj_t* root) {
  lv_obj_t* back = button(root, LV_SYMBOL_LEFT " Kembali", pal().card2, pal().fg, &lv_font_montserrat_16, 140, 48,
                          []() { uiGo(Screen::Home); });
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 16, 12);
  lv_obj_t* caption = label(root, "RIWAYAT HARI INI", &lv_font_montserrat_20, pal().fg);
  lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 172, 22);
  lv_obj_t* header = label(root, "", &lv_font_montserrat_16, pal().muted);
  lv_obj_set_width(header, 768);
  lv_label_set_long_mode(header, LV_LABEL_LONG_DOT);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 16, 66);
  lv_obj_t* note = label(root, "", &lv_font_montserrat_16, pal().muted);
  lv_obj_set_width(note, 768);
  lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_obj_t* list = lv_obj_create(root);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, 768, 284);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 96);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

  fillList(list, header, note);
  hooks().onNet = [list, header, note](const NetResult& r) {
    if (r.kind == NetResult::SendDone) fillList(list, header, note);
  };
}

}  // namespace ui
