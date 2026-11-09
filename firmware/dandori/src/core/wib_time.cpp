#include "wib_time.h"

#include <cstdio>

namespace {

constexpr int64_t kWibOffset = 7 * 3600;
constexpr int64_t kDayStartSec = 8 * 3600;  // production day starts 08:00

int64_t floorDiv(int64_t a, int64_t b) { return (a >= 0 ? a : a - b + 1) / b; }

// civil_from_days (Howard Hinnant), days since 1970-01-01.
void civilFromDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y = static_cast<int>(yy + (m <= 2));
}

std::string formatDate(int64_t localDays) {
  int y, m, d;
  civilFromDays(localDays, y, m, d);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
  return buf;
}

int secondOfDay(int64_t epochUtc) {
  const int64_t local = epochUtc + kWibOffset;
  return static_cast<int>(local - floorDiv(local, 86400) * 86400);
}

}  // namespace

bool isClockValid(int64_t epochUtc) { return epochUtc >= kMinValidEpoch; }

WibTime toWib(int64_t epochUtc) {
  const int64_t local = epochUtc + kWibOffset;
  const int64_t days = floorDiv(local, 86400);
  const int sod = static_cast<int>(local - days * 86400);
  WibTime t{};
  civilFromDays(days, t.year, t.month, t.day);
  t.hour = sod / 3600;
  t.minute = (sod % 3600) / 60;
  t.second = sod % 60;
  return t;
}

std::string productionDate(int64_t epochUtc) {
  return formatDate(floorDiv(epochUtc + kWibOffset - kDayStartSec, 86400));
}

int shiftOf(int64_t epochUtc) {
  const int hour = secondOfDay(epochUtc) / 3600;
  if (hour >= 8 && hour < 17) return 1;
  if (hour >= 17 && hour < 23) return 2;
  return 3;
}

bool isDarkHours(int64_t epochUtc) {
  const int hour = secondOfDay(epochUtc) / 3600;
  return hour >= 18 || hour < 6;
}

std::string isoWib(int64_t epochUtc) {
  const WibTime t = toWib(epochUtc);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d+07:00", t.year, t.month, t.day,
                t.hour, t.minute, t.second);
  return buf;
}

std::string hhmmss(int64_t epochUtc) {
  const WibTime t = toWib(epochUtc);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.hour, t.minute, t.second);
  return buf;
}
