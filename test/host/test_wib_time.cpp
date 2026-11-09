#include "core/wib_time.h"
#include "mini_test.h"
#include "test_helpers.h"

TEST(iso) {
  CHECK_EQ(isoWib(wib(2026, 11, 9, 8, 15, 30)), "2026-11-09T08:15:30+07:00");
  CHECK_EQ(hhmmss(wib(2026, 11, 9, 8, 5, 3)), "08:05:03");
}

TEST(to_wib_fields) {
  WibTime t = toWib(wib(2026, 12, 31, 23, 59, 58));
  CHECK_EQ(t.year, 2026);
  CHECK_EQ(t.month, 12);
  CHECK_EQ(t.day, 31);
  CHECK_EQ(t.hour, 23);
  CHECK_EQ(t.minute, 59);
  CHECK_EQ(t.second, 58);
}

TEST(prod_day_boundary) {
  CHECK_EQ(productionDate(wib(2026, 11, 10, 7, 59, 59)), "2026-11-09");
  CHECK_EQ(productionDate(wib(2026, 11, 10, 8, 0, 0)), "2026-11-10");
  CHECK_EQ(productionDate(wib(2026, 11, 10, 2, 0, 0)), "2026-11-09");
  CHECK_EQ(productionDate(wib(2027, 1, 1, 7, 0, 0)), "2026-12-31");
  CHECK_EQ(productionDate(wib(2028, 3, 1, 6, 0, 0)), "2028-02-29");
}

TEST(shift) {
  CHECK_EQ(shiftOf(wib(2026, 11, 9, 8, 0, 0)), 1);
  CHECK_EQ(shiftOf(wib(2026, 11, 9, 16, 59, 59)), 1);
  CHECK_EQ(shiftOf(wib(2026, 11, 9, 17, 0, 0)), 2);
  CHECK_EQ(shiftOf(wib(2026, 11, 9, 23, 0, 0)), 3);
  CHECK_EQ(shiftOf(wib(2026, 11, 10, 7, 59, 59)), 3);
}

TEST(dark) {
  CHECK(isDarkHours(wib(2026, 11, 9, 18, 0, 0)));
  CHECK(isDarkHours(wib(2026, 11, 9, 5, 59, 0)));
  CHECK(!isDarkHours(wib(2026, 11, 9, 6, 0, 0)));
  CHECK(!isDarkHours(wib(2026, 11, 9, 17, 59, 59)));
}

TEST(clock_valid) {
  CHECK(!isClockValid(0));
  CHECK(!isClockValid(1735689599));
  CHECK(isClockValid(1735689600));
}
