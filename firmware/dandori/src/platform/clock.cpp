#include "clock.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>

#include "../core/wib_time.h"

namespace {

std::string s_server;
volatile int64_t s_lastSync = 0;
volatile bool s_restartPending = false;

void onSync(struct timeval* tv) { s_lastSync = tv ? tv->tv_sec : 0; }

}  // namespace

void clockBegin(const std::string& ntpServer) {
  s_server = ntpServer.empty() ? "pool.ntp.org" : ntpServer;
  sntp_set_time_sync_notification_cb(onSync);
  configTime(0, 0, s_server.c_str());  // keep the system clock in UTC
}

// configTime() starts SNTP once, possibly before WiFi has an IP/DNS. Polling picks up
// the network later, but a failed first round is silent and retries are slow (~64 s).
// Re-arming it on every WiFi-up and while the clock is unset makes sync prompt.
void clockRestart() {
  if (esp_sntp_enabled()) {
    sntp_restart();
  } else {
    configTime(0, 0, s_server.empty() ? "pool.ntp.org" : s_server.c_str());
  }
}

void clockRestartDeferred() { s_restartPending = true; }

bool clockConsumeRestartRequest() {
  if (!s_restartPending) return false;
  s_restartPending = false;
  return true;
}

int64_t nowEpoch() { return static_cast<int64_t>(time(nullptr)); }

bool clockSynced() { return isClockValid(nowEpoch()); }

int64_t lastSyncEpoch() { return s_lastSync; }

int clockSyncState() { return static_cast<int>(esp_sntp_get_sync_status()); }

const char* clockServer() { return s_server.c_str(); }
