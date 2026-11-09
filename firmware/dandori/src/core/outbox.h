// HTTP outcome classification and retry scheduling for queued events (spec §7.2, §8).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class SendOutcome { Sent, Retry, Rejected };

// status <= 0 means a transport error (no WiFi, timeout, refused).
SendOutcome classifyHttp(int status);
// attempt 1.. -> 5s, 10s, 20s, 40s, then capped at 60s.
uint32_t retryDelayMs(uint32_t attempt);

class OutboxScheduler {
 public:
  // A request with no result after this long is treated as a Retry.
  static constexpr uint32_t kInFlightTimeoutMs = 30000;

  // Id to send now, or "" (one request in flight at a time). pendingIds are oldest first.
  std::string next(const std::vector<std::string>& pendingIds, uint32_t nowMs);
  void onResult(const std::string& id, SendOutcome outcome, uint32_t nowMs);
  // Clears every backoff (manual "Kirim ulang", WiFi reconnect, new event).
  void kick();
  bool inFlight() const { return !inFlightId_.empty(); }

 private:
  struct Backoff {
    uint32_t attempts = 0;
    uint32_t notBeforeMs = 0;
  };

  std::map<std::string, Backoff> backoff_;
  std::string inFlightId_;
  uint32_t inFlightSinceMs_ = 0;
};
