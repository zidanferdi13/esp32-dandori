// SNTP time source. All formatting is done by core/wib_time.
#pragma once
#include <stdint.h>

#include <string>

// Starts (or restarts, when the server changes) SNTP. UTC epoch; WIB is applied in core.
void clockBegin(const std::string& ntpServer);
int64_t nowEpoch();
bool clockSynced();
int64_t lastSyncEpoch();  // 0 = never
