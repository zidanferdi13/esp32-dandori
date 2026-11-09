#include "clock.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>

#include "../core/wib_time.h"

namespace {

std::string s_server;
volatile int64_t s_lastSync = 0;

void onSync(struct timeval* tv) { s_lastSync = tv ? tv->tv_sec : 0; }

}  // namespace

void clockBegin(const std::string& ntpServer) {
  s_server = ntpServer.empty() ? "pool.ntp.org" : ntpServer;
  sntp_set_time_sync_notification_cb(onSync);
  configTime(0, 0, s_server.c_str());  // keep the system clock in UTC
}

int64_t nowEpoch() { return static_cast<int64_t>(time(nullptr)); }

bool clockSynced() { return isClockValid(nowEpoch()); }

int64_t lastSyncEpoch() { return s_lastSync; }
