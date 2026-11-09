// Shared helpers for host tests. Independent of the code under test.
#pragma once
#include <cstdint>

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil).
inline int64_t testDaysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

inline int64_t utcEpochOf(int y, int mo, int d, int h, int mi, int s) {
  return testDaysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

// Epoch (UTC seconds) of a WIB (UTC+7) wall-clock time.
inline int64_t wib(int y, int mo, int d, int h, int mi, int s) {
  return utcEpochOf(y, mo, d, h, mi, s) - 7 * 3600;
}

constexpr int64_t T0 = 1794200000;  // a valid (post-2025) epoch
