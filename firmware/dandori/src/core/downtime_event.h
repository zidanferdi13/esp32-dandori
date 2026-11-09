// One recorded downtime event: id, API payload (spec §7.2) and on-device storage form.
#pragma once
#include <cstdint>
#include <string>

enum class EventStatus { Pending, Sent, Rejected };

struct DowntimeEvent {
  std::string id;
  std::string mesin;
  std::string mo;
  std::string prodDate;  // production day of startEpoch, "" when the clock was unknown
  int kodeProblem = 0;
  int64_t startEpoch = 0;  // 0 = unknown clock
  int64_t stopEpoch = 0;
  uint32_t totalDetik = 0;
  bool estimated = false;
  EventStatus status = EventStatus::Pending;
  uint32_t order = 0;  // insertion order inside EventStore
};

// "<mesin>-YYYYMMDD-HHMMSS-<seq4>" with a valid clock, "<mesin>-NC-<seq6>" otherwise.
std::string makeEventId(const std::string& mesin, int64_t startEpoch, uint32_t seq);
// Exactly the keys id, mesin, mo, kode_problem, start, stop, total_detik.
std::string toApiJson(const DowntimeEvent& e);
std::string toStorageJson(const DowntimeEvent& e);
bool fromStorageJson(const std::string& s, DowntimeEvent& out);
