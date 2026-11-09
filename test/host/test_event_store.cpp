#include "core/event_store.h"
#include "mem_file_store.h"
#include "mini_test.h"

namespace {

DowntimeEvent ev(const std::string& id, const std::string& prodDate, uint32_t totalSec = 60) {
  DowntimeEvent e;
  e.id = id;
  e.mesin = "PALLETIZER-01";
  e.mo = "MO-1";
  e.prodDate = prodDate;
  e.kodeProblem = 1;
  e.totalDetik = totalSec;
  return e;
}

}  // namespace

TEST(store_persist_before_cache) {
  MemFileStore fs;
  EventStore s(fs);
  CHECK(s.add(ev("A", "2026-11-09")));
  CHECK_EQ(fs.list().size(), 1u);
  EventStore s2(fs);
  s2.load();
  CHECK_EQ(s2.pendingCount(), 1u);
  CHECK_EQ(s2.pendingIds()[0], "A");
}

TEST(store_order_and_status) {
  MemFileStore fs;
  EventStore s(fs);
  // Ids sort differently from insertion order on purpose.
  s.add(ev("Z", "2026-11-09"));
  s.add(ev("B", "2026-11-09"));
  s.add(ev("M", "2026-11-09"));
  CHECK(s.setStatus("B", EventStatus::Sent));
  CHECK(!s.setStatus("nope", EventStatus::Sent));
  EventStore r(fs);
  r.load();
  auto p = r.pendingIds();
  CHECK_EQ(p.size(), 2u);
  CHECK_EQ(p[0], "Z");
  CHECK_EQ(p[1], "M");
  auto h = r.history("2026-11-09");
  CHECK_EQ(h.size(), 3u);
  CHECK_EQ(h[0].id, "M");  // newest first
  CHECK_EQ(h[2].id, "Z");
  CHECK(r.find("B")->status == EventStatus::Sent);
}

TEST(store_order_continues_after_reload) {
  MemFileStore fs;
  {
    EventStore s(fs);
    s.add(ev("Z", "2026-11-09"));
  }
  EventStore r(fs);
  r.load();
  r.add(ev("A", "2026-11-09"));
  auto p = r.pendingIds();
  CHECK_EQ(p[0], "Z");
  CHECK_EQ(p[1], "A");
}

TEST(store_summary) {
  MemFileStore fs;
  EventStore s(fs);
  s.add(ev("A", "2026-11-09", 100));
  s.add(ev("B", "2026-11-09", 762));
  s.add(ev("C", "2026-11-08", 5));
  DaySummary d = s.summary("2026-11-09");
  CHECK_EQ(d.count, 2u);
  CHECK_EQ(d.totalSec, 862u);
}

TEST(store_unknown_clock_events_show_in_history) {
  MemFileStore fs;
  EventStore s(fs);
  s.add(ev("NC", "", 30));
  s.add(ev("A", "2026-11-09", 10));
  CHECK_EQ(s.history("2026-11-09").size(), 2u);
  CHECK_EQ(s.summary("2026-11-09").totalSec, 40u);
}

TEST(store_prune_keeps_pending) {
  MemFileStore fs;
  EventStore s(fs);
  s.add(ev("old_sent", "2026-11-08"));
  s.setStatus("old_sent", EventStatus::Sent);
  s.add(ev("old_pending", "2026-11-08"));
  s.add(ev("today", "2026-11-09"));
  s.prune("2026-11-09");
  CHECK(s.find("old_sent") == nullptr);
  CHECK(s.find("old_pending") != nullptr);
  CHECK(s.find("today") != nullptr);
  CHECK_EQ(fs.list().size(), 2u);
}

TEST(store_prune_caps_non_pending) {
  MemFileStore fs;
  EventStore s(fs);
  for (int i = 0; i < 6; ++i) {
    const std::string id = "S" + std::to_string(i);
    s.add(ev(id, "2026-11-09"));
    s.setStatus(id, EventStatus::Sent);
  }
  s.add(ev("P", "2026-11-09"));
  s.prune("2026-11-09", 3);
  CHECK(s.find("P") != nullptr);  // pending never pruned
  CHECK(s.find("S0") == nullptr);  // oldest go first
  CHECK(s.find("S5") != nullptr);
  CHECK_EQ(fs.list().size(), 3u);
}

TEST(store_skips_corrupt) {
  MemFileStore fs;
  fs.write("bad.json", "{");
  EventStore s(fs);
  s.load();
  CHECK_EQ(s.pendingCount(), 0u);
}
