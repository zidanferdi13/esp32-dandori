#include "core/downtime_counter.h"
#include "mini_test.h"
#include "test_helpers.h"

TEST(no_mo_blocks_start) {
  DowntimeCounter c;
  CHECK(c.state() == CounterState::NoMo);
  CHECK(!c.start(0, 0));
}

TEST(basic_cycle) {
  DowntimeCounter c;
  c.setActiveMo("MO-1");
  CHECK(c.state() == CounterState::Ready);
  CHECK(c.start(1000, T0));
  CHECK(!c.start(1500, T0));  // double tap ignored
  CHECK(!c.setActiveMo("MO-2"));  // locked while running
  CHECK_EQ(c.elapsedSec(1000 + 5500), 5u);
  CHECK(c.stop(1000 + 335000, T0 + 335));
  CHECK_EQ(c.elapsedSec(999999), 335u);
  CHECK(c.state() == CounterState::AwaitCode);
  CHECK(!c.setActiveMo("MO-2"));  // locked while awaiting code
  CHECK_EQ(c.startEpoch(), T0);
  CHECK_EQ(c.stopEpoch(), T0 + 335);
  CHECK(c.complete());
  CHECK(c.state() == CounterState::Ready);
  CHECK_EQ(c.activeMo(), "MO-1");
  CHECK(c.setActiveMo("MO-2"));
  CHECK_EQ(c.activeMo(), "MO-2");
}

TEST(stop_requires_running) {
  DowntimeCounter c;
  c.setActiveMo("M");
  CHECK(!c.stop(10, T0));
  CHECK(!c.cancel());
  CHECK(!c.complete());
}

TEST(millis_wrap) {
  DowntimeCounter c;
  c.setActiveMo("M");
  c.start(0xFFFFF000u, 0);
  c.stop(0x00001000u, 0);
  CHECK_EQ(c.elapsedSec(0), 8u);  // 8192 ms across the wrap
}

TEST(total_ignores_clock_jump) {
  DowntimeCounter c;
  c.setActiveMo("M");
  c.start(0, 0);  // no clock at start
  c.stop(60000, T0);
  CHECK_EQ(c.elapsedSec(0), 60u);
  CHECK_EQ(c.startEpoch(), 0);
}

TEST(cancel) {
  DowntimeCounter c;
  c.setActiveMo("M");
  c.start(0, T0);
  c.stop(1000, T0 + 1);
  CHECK(c.cancel());
  CHECK(c.state() == CounterState::Ready);
  CHECK_EQ(c.elapsedSec(5000), 0u);
}

TEST(resume_with_clock) {
  DowntimeCounter a;
  a.setActiveMo("M");
  a.start(5000, T0);
  DowntimeCounter b;
  b.restore(a.snapshot(), 200, T0 + 120);
  CHECK(b.state() == CounterState::Running);
  CHECK_EQ(b.elapsedSec(200), 120u);
  CHECK(!b.estimated());
  CHECK_EQ(b.elapsedSec(200 + 10000), 130u);
  CHECK_EQ(b.startEpoch(), T0);
}

TEST(resume_without_clock) {
  DowntimeCounter a;
  a.setActiveMo("M");
  a.start(5000, T0);
  DowntimeCounter b;
  b.restore(a.snapshot(), 200, 0);
  CHECK(b.state() == CounterState::Running);
  CHECK_EQ(b.elapsedSec(200), 0u);
  CHECK(b.estimated());
}

TEST(resume_await_code) {
  DowntimeCounter a;
  a.setActiveMo("M");
  a.start(0, T0);
  a.stop(42000, T0 + 42);
  DowntimeCounter b;
  b.restore(a.snapshot(), 7, 0);
  CHECK(b.state() == CounterState::AwaitCode);
  CHECK_EQ(b.elapsedSec(7), 42u);
  CHECK_EQ(b.activeMo(), "M");
  CHECK_EQ(b.stopEpoch(), T0 + 42);
}

TEST(resume_ready_keeps_mo) {
  DowntimeCounter a;
  a.setActiveMo("MO-9");
  DowntimeCounter b;
  b.restore(a.snapshot(), 0, 0);
  CHECK(b.state() == CounterState::Ready);
  CHECK_EQ(b.activeMo(), "MO-9");
}
