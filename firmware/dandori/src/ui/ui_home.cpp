// Home launcher: MO aktif + Downtime sebagai tugas utama, Riwayat/Pengaturan sebagai tujuan sekunder.
#include "ui_internal.h"

namespace ui {
namespace {

void tile(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* symbol, const char* title,
          const char* sub, Screen target, bool primary) {
  lv_obj_t* t = lv_obj_create(parent);
  lv_obj_set_size(t, w, h);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_style_bg_color(t, pal().card, 0);
  lv_obj_set_style_border_color(t, pal().card2, 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_pad_all(t, primary ? 20 : 12, 0);
  lv_obj_set_style_pad_row(t, primary ? 10 : 4, 0);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(t, pal().card2, LV_STATE_PRESSED);

  lv_obj_t* plate = lv_obj_create(t);
  lv_obj_remove_style_all(plate);
  lv_obj_set_size(plate, primary ? 56 : 32, primary ? 56 : 32);
  lv_obj_set_style_radius(plate, 8, 0);
  lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(plate, primary ? pal().accentSoft : pal().card2, 0);
  lv_obj_remove_flag(plate, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* icon = label(plate, symbol, primary ? &lv_font_montserrat_28 : &lv_font_montserrat_20,
                         primary ? pal().accent : pal().muted);
  lv_obj_center(icon);

  lv_obj_t* l = label(t, title, primary ? &lv_font_montserrat_28 : &lv_font_montserrat_20, pal().fg);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* s = label(t, sub, &lv_font_montserrat_16, pal().muted);
  lv_obj_set_width(s, w - (primary ? 72 : 56));
  lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* arrow = label(t, LV_SYMBOL_RIGHT, &lv_font_montserrat_20, pal().muted);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);

  onClick(t, [target]() { uiGo(target); });
}

}  // namespace

void buildHome(lv_obj_t* root) {
  const std::string& mo = ctx().counter.activeMo();
  lv_obj_t* moCard = card(root);
  lv_obj_set_size(moCard, 768, 72);
  lv_obj_align(moCard, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_remove_flag(moCard, LV_OBJ_FLAG_SCROLLABLE);
  label(moCard, "MO AKTIF", &lv_font_montserrat_16, pal().muted);
  lv_obj_t* moVal = label(moCard, mo.empty() ? std::string("Belum ada MO aktif") : mo,
                          mo.empty() ? &lv_font_montserrat_20 : &lv_font_montserrat_28,
                          mo.empty() ? pal().muted : pal().fg);
  lv_obj_set_width(moVal, 700);
  lv_label_set_long_mode(moVal, LV_LABEL_LONG_DOT);
  lv_obj_align(moVal, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  if (!mo.empty()) {
    lv_obj_t* dot = label(moCard, LV_SYMBOL_OK, &lv_font_montserrat_20, pal().success);
    lv_obj_align(dot, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  tile(root, 16, 104, 472, 248, LV_SYMBOL_PLAY, "Downtime", "Catat mesin berhenti", Screen::Counting, true);
  tile(root, 504, 104, 280, 116, LV_SYMBOL_LIST, "Riwayat hari ini", "Kejadian tercatat", Screen::History, false);
  tile(root, 504, 236, 280, 116, LV_SYMBOL_SETTINGS, "Pengaturan", "Jaringan, mesin, tema", Screen::Settings, false);

  lv_obj_t* hint = label(root, "Pencatatan downtime, bukan kontrol mesin", &lv_font_montserrat_16, pal().muted);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 20, -18);
}

}  // namespace ui
