// WIB (UTC+7) wall-clock helpers, production day and shift rules (spec §3.1).
// Pure arithmetic: never depends on the C library time zone.
#pragma once
#include <cstdint>
#include <string>

constexpr int64_t kMinValidEpoch = 1735689600;  // 2025-01-01T00:00:00Z
bool isClockValid(int64_t epochUtc);

struct WibTime {
  int year, month, day, hour, minute, second;
};
WibTime toWib(int64_t epochUtc);

// "YYYY-MM-DD" of the production day, which starts at 08:00 WIB.
std::string productionDate(int64_t epochUtc);
// 1: 08:00-17:00, 2: 17:00-23:00, 3: 23:00-08:00.
int shiftOf(int64_t epochUtc);
// True between 18:00 and 06:00 WIB (theme Auto).
bool isDarkHours(int64_t epochUtc);
// "2026-11-09T08:15:30+07:00"
std::string isoWib(int64_t epochUtc);
// "08:15:30"
std::string hhmmss(int64_t epochUtc);
