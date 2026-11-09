#include "downtime_counter.h"

#include "wib_time.h"

bool DowntimeCounter::setActiveMo(const std::string& mo) {
  if (state_ == CounterState::Running || state_ == CounterState::AwaitCode) return false;
  activeMo_ = mo;
  state_ = CounterState::Ready;
  return true;
}

bool DowntimeCounter::start(uint32_t nowMs, int64_t nowEpoch) {
  if (state_ != CounterState::Ready) return false;
  resetEvent();
  startMs_ = nowMs;
  startEpoch_ = isClockValid(nowEpoch) ? nowEpoch : 0;
  state_ = CounterState::Running;
  return true;
}

bool DowntimeCounter::stop(uint32_t nowMs, int64_t nowEpoch) {
  if (state_ != CounterState::Running) return false;
  totalSec_ = elapsedSec(nowMs);
  stopEpoch_ = isClockValid(nowEpoch) ? nowEpoch : 0;
  state_ = CounterState::AwaitCode;
  return true;
}

uint32_t DowntimeCounter::elapsedSec(uint32_t nowMs) const {
  if (state_ == CounterState::Running) return static_cast<uint32_t>(nowMs - startMs_) / 1000;
  if (state_ == CounterState::AwaitCode) return totalSec_;
  return 0;
}

bool DowntimeCounter::cancel() {
  if (state_ != CounterState::AwaitCode) return false;
  resetEvent();
  state_ = CounterState::Ready;
  return true;
}

bool DowntimeCounter::complete() { return cancel(); }

CounterSnapshot DowntimeCounter::snapshot() const {
  CounterSnapshot s;
  s.state = state_;
  s.activeMo = activeMo_;
  s.startEpoch = startEpoch_;
  s.stopEpoch = stopEpoch_;
  s.totalSec = totalSec_;
  s.estimated = estimated_;
  return s;
}

void DowntimeCounter::restore(const CounterSnapshot& s, uint32_t nowMs, int64_t nowEpoch) {
  activeMo_ = s.activeMo;
  state_ = s.state;
  resetEvent();
  switch (s.state) {
    case CounterState::Running:
      startEpoch_ = s.startEpoch;
      if (isClockValid(s.startEpoch) && isClockValid(nowEpoch) && nowEpoch >= s.startEpoch) {
        startMs_ = nowMs - static_cast<uint32_t>(nowEpoch - s.startEpoch) * 1000U;
        estimated_ = s.estimated;
      } else {
        startMs_ = nowMs;
        estimated_ = true;
      }
      break;
    case CounterState::AwaitCode:
      startEpoch_ = s.startEpoch;
      stopEpoch_ = s.stopEpoch;
      totalSec_ = s.totalSec;
      estimated_ = s.estimated;
      break;
    case CounterState::Ready:
    case CounterState::NoMo:
      break;
  }
}

void DowntimeCounter::resetEvent() {
  startMs_ = 0;
  totalSec_ = 0;
  startEpoch_ = 0;
  stopEpoch_ = 0;
  estimated_ = false;
}
