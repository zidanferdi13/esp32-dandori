#include "downtime_event.h"

#include <ArduinoJson.h>

#include <cstdio>

#include "wib_time.h"

std::string makeEventId(const std::string& mesin, int64_t startEpoch, uint32_t seq) {
  char buf[48];
  if (isClockValid(startEpoch)) {
    const WibTime t = toWib(startEpoch);
    std::snprintf(buf, sizeof(buf), "-%04d%02d%02d-%02d%02d%02d-%04u", t.year, t.month, t.day,
                  t.hour, t.minute, t.second, static_cast<unsigned>(seq % 10000));
  } else {
    std::snprintf(buf, sizeof(buf), "-NC-%06u", static_cast<unsigned>(seq % 1000000));
  }
  return mesin + buf;
}

std::string toApiJson(const DowntimeEvent& e) {
  JsonDocument doc;
  doc["id"] = e.id;
  doc["mesin"] = e.mesin;
  doc["mo"] = e.mo;
  doc["kode_problem"] = e.kodeProblem;
  doc["catatan"] = e.catatan.substr(0, kMaxCatatanLen);
  if (isClockValid(e.startEpoch)) {
    doc["start"] = isoWib(e.startEpoch);
  } else {
    doc["start"] = nullptr;
  }
  if (isClockValid(e.stopEpoch)) {
    doc["stop"] = isoWib(e.stopEpoch);
  } else {
    doc["stop"] = nullptr;
  }
  doc["total_detik"] = e.totalDetik;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string toStorageJson(const DowntimeEvent& e) {
  JsonDocument doc;
  doc["id"] = e.id;
  doc["mesin"] = e.mesin;
  doc["mo"] = e.mo;
  doc["cat"] = e.catatan;
  doc["pd"] = e.prodDate;
  doc["kode"] = e.kodeProblem;
  doc["s"] = e.startEpoch;
  doc["e"] = e.stopEpoch;
  doc["t"] = e.totalDetik;
  doc["est"] = e.estimated;
  doc["st"] = static_cast<int>(e.status);
  doc["n"] = e.order;
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool fromStorageJson(const std::string& s, DowntimeEvent& out) {
  JsonDocument doc;
  if (deserializeJson(doc, s) != DeserializationError::Ok) return false;
  if (!doc["id"].is<const char*>()) return false;
  DowntimeEvent e;
  e.id = doc["id"].as<std::string>();
  e.mesin = doc["mesin"] | "";
  e.mo = doc["mo"] | "";
  e.catatan = doc["cat"] | "";
  e.prodDate = doc["pd"] | "";
  e.kodeProblem = doc["kode"] | 0;
  e.startEpoch = doc["s"] | static_cast<int64_t>(0);
  e.stopEpoch = doc["e"] | static_cast<int64_t>(0);
  e.totalDetik = doc["t"] | 0u;
  e.estimated = doc["est"] | false;
  const int st = doc["st"] | 0;
  if (st < 0 || st > static_cast<int>(EventStatus::Rejected)) return false;
  e.status = static_cast<EventStatus>(st);
  e.order = doc["n"] | 0u;
  out = e;
  return true;
}
