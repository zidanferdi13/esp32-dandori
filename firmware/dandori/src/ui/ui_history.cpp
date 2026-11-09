// Riwayat Hari Ini (spec §5.3): read-only list of this production day's events, newest first.
#include "../core/problem_codes.h"
#include "../core/wib_time.h"
#include "ui_internal.h"

namespace ui {
namespace {

constexpr size_t kMaxRows = 60;  // keep the LVGL object count bounded

const char* statusSymbol(EventStatus s) {
  switch (s) {
    case EventStatus::Sent: return LV_SYMBOL_OK;
    case EventStatus::Rejected: return LV_SYMBOL_CLOSE;
    case EventStatus::Pending: break;
  }
  return LV_SYMBOL_UPLOAD;
}

uint32_t statusColor(EventStatus s) {
  switch (s) {
    case EventStatus::Sent: return kGreen;
    case EventStatus::Rejected: return kRed;
    case EventStatus::Pending: break;
  }
  return kAmber;
}

void fillList(lv_obj_t* list, lv_obj_t* header) {
  lv_obj_clean(list);
  AppContext& c = ctx();
  const std::string day = prodDateNow();
  const std::vector<DowntimeEvent> events = c.store.history(day);
  const DaySummary d = c.store.summary(day);
  const std::string title = (day.empty() ? std::string("Hari ini") : "Hari produksi " + day) + "   |   " +
                            std::to_string(d.count) + " kejadian, " + std::to_string(d.totalSec / 60) + " mnt " +
                            std::to_string(d.totalSec % 60) + " dtk";
  lv_label_set_text(header, title.c_str());

  if (events.empty()) {
    lv_obj_t* l = label(list, "Belum ada downtime tercatat hari ini", &lv_font_montserrat_20, pal().muted);
    lv_obj_center(l);
    return;
  }
  for (size_t i = 0; i < events.size() && i < kMaxRows; ++i) {
    const DowntimeEvent& e = events[i];
    lv_obj_t* row = card(list);
    lv_obj_set_size(row, LV_PCT(100), 58);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_ver(row, 6, 0);

    const std::string time = isClockValid(e.startEpoch) ? hhmmss(e.startEpoch).substr(0, 5) : std::string("--:--");
    lv_obj_t* t = label(row, time, &lv_font_montserrat_20, pal().fg);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);

    const ProblemCode* p = findProblemCode(e.kodeProblem);
    const std::string what = e.mo + "\n" + std::to_string(e.kodeProblem) + "  " + (p ? p->sub : "?");
    lv_obj_t* w = label(row, what, &lv_font_montserrat_16, pal().fg);
    lv_obj_align(w, LV_ALIGN_LEFT_MID, 90, 0);

    const std::string dur = (e.estimated ? "~" : "") + std::to_string(e.totalDetik / 60) + "m " +
                            std::to_string(e.totalDetik % 60) + "s";
    lv_obj_t* du = label(row, dur, &lv_font_montserrat_20, pal().fg);
    lv_obj_align(du, LV_ALIGN_RIGHT_MID, -60, 0);

    lv_obj_t* st = label(row, statusSymbol(e.status), &lv_font_montserrat_28, lv_color_hex(statusColor(e.status)));
    lv_obj_align(st, LV_ALIGN_RIGHT_MID, 0, 0);
  }
}

}  // namespace

void buildHistory(lv_obj_t* root) {
  lv_obj_t* back = button(root, LV_SYMBOL_LEFT " Kembali", pal().card2, pal().fg, &lv_font_montserrat_16, 140, 44,
                          []() { uiGo(Screen::Home); });
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 16, 12);
  lv_obj_t* header = label(root, "", &lv_font_montserrat_20, pal().fg);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 176, 22);
  lv_obj_t* legend = label(root, LV_SYMBOL_OK " terkirim   " LV_SYMBOL_UPLOAD " antri   " LV_SYMBOL_CLOSE
                                 " ditolak   ~ perkiraan",
                           &lv_font_montserrat_14, pal().muted);
  lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, -6);

  lv_obj_t* list = lv_obj_create(root);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, 768, 330);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

  fillList(list, header);
  hooks().onNet = [list, header](const NetResult& r) {
    if (r.kind == NetResult::SendDone) fillList(list, header);
  };
}

}  // namespace ui
