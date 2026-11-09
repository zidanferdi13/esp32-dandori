#include "mo_list.h"

#include <ArduinoJson.h>

#include "wib_time.h"

std::string moListPath(int64_t nowEpoch) {
  if (!isClockValid(nowEpoch)) return "/api/mo";
  return "/api/mo?tgl=" + productionDate(nowEpoch);
}

bool parseMoList(const std::string& body, std::vector<MoItem>& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  JsonArrayConst data = doc["data"].as<JsonArrayConst>();
  if (data.isNull()) return false;

  std::vector<MoItem> items;
  for (JsonObjectConst item : data) {
    MoItem m;
    m.mo = item["mo"] | "";
    if (m.mo.empty()) continue;
    m.produk = item["produk"] | "";
    items.push_back(m);
  }
  out = items;
  return true;
}
