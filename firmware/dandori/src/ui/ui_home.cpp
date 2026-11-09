// Home launcher: MO aktif row + Downtime / Riwayat Hari Ini / Pengaturan tiles.
#include "ui_internal.h"

namespace ui {
namespace {

void tile(lv_obj_t* parent, const char* symbol, uint32_t iconColor, const char* text, Screen target) {
  lv_obj_t* t = card(parent);
  lv_obj_set_size(t, 236, 250);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(t, 18, 0);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* icon = lv_obj_create(t);
  lv_obj_remove_style_all(icon);
  lv_obj_set_size(icon, 96, 96);
  lv_obj_set_style_radius(icon, 24, 0);
  lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(icon, lv_color_hex(iconColor), 0);
  lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* sym = label(icon, symbol, &lv_font_montserrat_48, lv_color_hex(0xFFFFFF));
  lv_obj_center(sym);

  lv_obj_t* l = label(t, text, &lv_font_montserrat_20, pal().fg);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  onClick(t, [target]() { uiGo(target); });
}

}  // namespace

void buildHome(lv_obj_t* root) {
  const std::string& mo = ctx().counter.activeMo();
  lv_obj_t* moRow = label(root, mo.empty() ? std::string("Belum ada MO aktif") : "MO aktif:  " + mo,
                          &lv_font_montserrat_20, mo.empty() ? pal().muted : pal().fg);
  lv_obj_align(moRow, LV_ALIGN_TOP_LEFT, 28, 22);

  lv_obj_t* row = lv_obj_create(root);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), 270);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  tile(row, LV_SYMBOL_PLAY, kRed, "Downtime", Screen::Counting);
  tile(row, LV_SYMBOL_LIST, 0x2D6CDF, "Riwayat Hari Ini", Screen::History);
  tile(row, LV_SYMBOL_SETTINGS, 0x6B7785, "Pengaturan", Screen::Settings);
}

}  // namespace ui
