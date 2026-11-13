#include <ArduinoJson.h>

#include "core/downtime_event.h"
#include "mini_test.h"
#include "test_helpers.h"

namespace {

DowntimeEvent sample() {
  DowntimeEvent e;
  e.mesin = "PALLETIZER-01";
  e.mo = "MO-2611-0012";
  e.kodeProblem = 13;
  e.startEpoch = wib(2026, 11, 9, 8, 15, 30);
  e.stopEpoch = wib(2026, 11, 9, 8, 21, 5);
  e.totalDetik = 335;
  e.catatan = "bearing aus";
  e.id = makeEventId(e.mesin, e.startEpoch, 7);
  e.prodDate = "2026-11-09";
  return e;
}

JsonDocument parse(const std::string& s) {
  JsonDocument doc;
  deserializeJson(doc, s);
  return doc;
}

}  // namespace

TEST(event_id) {
  CHECK_EQ(makeEventId("PALLETIZER-01", wib(2026, 11, 9, 8, 15, 30), 7),
           "PALLETIZER-01-20261109-081530-0007");
  CHECK_EQ(makeEventId("PALLETIZER-01", 0, 12), "PALLETIZER-01-NC-000012");
}

TEST(api_json_with_clock) {
  JsonDocument j = parse(toApiJson(sample()));
  CHECK_EQ(j["id"].as<std::string>(), "PALLETIZER-01-20261109-081530-0007");
  CHECK_EQ(j["mo"].as<std::string>(), "MO-2611-0012");
  CHECK_EQ(j["start"].as<std::string>(), "2026-11-09T08:15:30+07:00");
  CHECK_EQ(j["stop"].as<std::string>(), "2026-11-09T08:21:05+07:00");
  CHECK_EQ(j["total_detik"].as<int>(), 335);
  CHECK_EQ(j["kode_problem"].as<int>(), 13);
  CHECK_EQ(j["mesin"].as<std::string>(), "PALLETIZER-01");
  CHECK_EQ(j["catatan"].as<std::string>(), "bearing aus");
  CHECK_EQ(j.as<JsonObject>().size(), 8u);
}

TEST(api_json_empty_catatan) {
  DowntimeEvent e = sample();
  e.catatan.clear();
  const std::string raw = toApiJson(e);
  JsonDocument j = parse(raw);
  CHECK(j["catatan"].is<const char*>());  // always present, never null
  CHECK_EQ(j["catatan"].as<std::string>(), "");
  CHECK_EQ(j.as<JsonObject>().size(), 8u);
}

TEST(api_json_catatan_truncated) {
  DowntimeEvent e = sample();
  e.catatan.assign(300, 'x');
  JsonDocument j = parse(toApiJson(e));
  CHECK_EQ(j["catatan"].as<std::string>().size(), kMaxCatatanLen);
}

TEST(api_json_no_clock) {
  DowntimeEvent e = sample();
  e.startEpoch = 0;
  e.stopEpoch = 0;
  const std::string raw = toApiJson(e);
  JsonDocument j = parse(raw);
  CHECK(j["start"].isNull());
  CHECK(j["stop"].isNull());
  CHECK(raw.find("\"start\":null") != std::string::npos);
  CHECK(raw.find("\"stop\":null") != std::string::npos);
  CHECK_EQ(j["total_detik"].as<int>(), 335);
}

TEST(storage_roundtrip) {
  DowntimeEvent e = sample();
  e.status = EventStatus::Rejected;
  e.estimated = true;
  DowntimeEvent b;
  CHECK(fromStorageJson(toStorageJson(e), b));
  CHECK_EQ(b.id, e.id);
  CHECK_EQ(b.mesin, e.mesin);
  CHECK_EQ(b.mo, e.mo);
  CHECK_EQ(b.prodDate, e.prodDate);
  CHECK_EQ(b.kodeProblem, 13);
  CHECK_EQ(b.startEpoch, e.startEpoch);
  CHECK_EQ(b.stopEpoch, e.stopEpoch);
  CHECK_EQ(b.totalDetik, 335u);
  CHECK_EQ(b.catatan, "bearing aus");
  CHECK(b.status == EventStatus::Rejected);
  CHECK(b.estimated);
  CHECK(!fromStorageJson("{garbage", b));
}
