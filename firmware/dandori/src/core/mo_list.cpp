#include "mo_list.h"

#include <ArduinoJson.h>

#include "wib_time.h"

std::string moListPath(int64_t nowEpoch) {
  const std::string path = "/kanban/getMOPalletizer";
  if (!isClockValid(nowEpoch)) return path;
  return path + "?tgl=" + productionDate(nowEpoch);
}

bool parseMoList(const std::string& body, std::vector<MoItem>& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;

  JsonVariantConst code = doc["respone_code"];
  if (!code.isNull()) {
    const int c = code | 0;
    if (c < 200 || c >= 300) return false;
  }

  JsonArrayConst data = doc["data"].as<JsonArrayConst>();
  if (data.isNull()) return false;

  std::vector<MoItem> items;
  for (JsonObjectConst item : data) {
    if ((item["isactive"] | 1) == 0) continue;  // Palletizer service marks closed MOs with isactive=0
    MoItem m;
    m.mo = item["mo"] | "";
    if (m.mo.empty()) m.mo = item["nomor_mo"] | "";
    if (m.mo.empty()) continue;
    m.produk = item["produk"] | "";
    if (m.produk.empty()) m.produk = item["product_name"] | "";
    items.push_back(m);
  }
  out = items;
  return true;
}
