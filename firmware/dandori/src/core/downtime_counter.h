// Start/stop state machine for one downtime event (spec §4).
// Durations come from the monotonic ms counter; epochs are only labels (0 = unknown clock).
#pragma once
#include <cstdint>
#include <string>

enum class CounterState { NoMo, Ready, Running, AwaitCode };

// Persisted on every state change so a reboot can resume (spec §4.3).
struct CounterSnapshot {
  CounterState state = CounterState::NoMo;
  std::string activeMo;
  int64_t startEpoch = 0;
  int64_t stopEpoch = 0;
  uint32_t totalSec = 0;  // valid in AwaitCode
  bool estimated = false;
};

class DowntimeCounter {
 public:
  // False while Running/AwaitCode. NoMo/Ready -> Ready.
  bool setActiveMo(const std::string& mo);
  const std::string& activeMo() const { return activeMo_; }
  CounterState state() const { return state_; }

  bool start(uint32_t nowMs, int64_t nowEpoch);  // Ready -> Running
  bool stop(uint32_t nowMs, int64_t nowEpoch);   // Running -> AwaitCode, freezes the total
  // Running: live; AwaitCode: frozen; otherwise 0.
  uint32_t elapsedSec(uint32_t nowMs) const;
  bool cancel();    // AwaitCode -> Ready, event discarded
  bool complete();  // AwaitCode -> Ready, event recorded

  int64_t startEpoch() const { return startEpoch_; }
  int64_t stopEpoch() const { return stopEpoch_; }
  // True when the timer was resumed after a reboot without a valid clock.
  bool estimated() const { return estimated_; }

  CounterSnapshot snapshot() const;
  void restore(const CounterSnapshot& s, uint32_t nowMs, int64_t nowEpoch);

 private:
  void resetEvent();

  CounterState state_ = CounterState::NoMo;
  std::string activeMo_;
  uint32_t startMs_ = 0;
  uint32_t totalSec_ = 0;
  int64_t startEpoch_ = 0;
  int64_t stopEpoch_ = 0;
  bool estimated_ = false;
};
