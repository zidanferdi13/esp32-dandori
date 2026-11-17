// SNTP time source. All formatting is done by core/wib_time.
#pragma once
#include <stdint.h>

#include <string>

// Starts (or restarts, when the server changes) SNTP. UTC epoch; WIB is applied in core.
void clockBegin(const std::string& ntpServer);
// Re-arms SNTP without changing the server. Call when WiFi comes up and while the clock is unset.
void clockRestart();
// Requests a restart from the main loop; the network task performs it, so only one task
// ever touches the SNTP timer.
void clockRestartDeferred();
bool clockConsumeRestartRequest();
int64_t nowEpoch();
bool clockSynced();
int64_t lastSyncEpoch();  // 0 = never
int clockSyncState();  // SNTP_SYNC_STATUS_RESET / _COMPLETED / _IN_PROGRESS, for diagnostics
const char* clockServer();  // server in use, after the empty -> pool.ntp.org fallback
