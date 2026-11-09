#include "outbox.h"

SendOutcome classifyHttp(int status) {
  if (status >= 200 && status < 300) return SendOutcome::Sent;
  if (status <= 0 || status == 408 || status == 429 || status >= 500) return SendOutcome::Retry;
  return SendOutcome::Rejected;
}

uint32_t retryDelayMs(uint32_t attempt) {
  constexpr uint32_t kBase = 5000;
  constexpr uint32_t kCap = 60000;
  if (attempt == 0) return 0;
  if (attempt > 4) return kCap;
  const uint32_t delay = kBase << (attempt - 1);
  return delay > kCap ? kCap : delay;
}

std::string OutboxScheduler::next(const std::vector<std::string>& pendingIds, uint32_t nowMs) {
  if (inFlight()) {
    if (static_cast<uint32_t>(nowMs - inFlightSinceMs_) < kInFlightTimeoutMs) return "";
    const std::string lost = inFlightId_;  // copy: onResult clears inFlightId_
    onResult(lost, SendOutcome::Retry, nowMs);
  }
  for (const std::string& id : pendingIds) {
    auto it = backoff_.find(id);
    if (it != backoff_.end() && static_cast<int32_t>(nowMs - it->second.notBeforeMs) < 0) continue;
    inFlightId_ = id;
    inFlightSinceMs_ = nowMs;
    return id;
  }
  return "";
}

void OutboxScheduler::onResult(const std::string& id, SendOutcome outcome, uint32_t nowMs) {
  if (id != inFlightId_) return;
  inFlightId_.clear();
  if (outcome == SendOutcome::Retry) {
    Backoff& b = backoff_[id];
    ++b.attempts;
    b.notBeforeMs = nowMs + retryDelayMs(b.attempts);
  } else {
    backoff_.erase(id);
  }
}

void OutboxScheduler::kick() {
  for (auto& entry : backoff_) entry.second.notBeforeMs = 0;
}
