#include "core/settings.h"
#include "mini_test.h"
#include "test_helpers.h"

TEST(settings_defaults) {
  Settings s;
  CHECK_EQ(s.pin, "1234");
  CHECK_EQ(s.machineCode, "PALLETIZER-01");
  CHECK_EQ(s.ntpServer, "pool.ntp.org");
  CHECK(s.theme == ThemeMode::Dark);
  CHECK_EQ(s.screenOffMin, 5);
}

TEST(settings_validation) {
  CHECK(isValidPin("1234"));
  CHECK(!isValidPin("123"));
  CHECK(!isValidPin("12345"));
  CHECK(!isValidPin("12a4"));
  CHECK(isValidBaseUrl("http://192.168.1.10:4500"));
  CHECK(isValidBaseUrl("https://api.example.co.id"));
  CHECK(!isValidBaseUrl("192.168.1.10"));
  CHECK(!isValidBaseUrl("http://"));
  CHECK(!isValidBaseUrl("ftp://x"));
  CHECK(!isValidBaseUrl("http://a b"));
  CHECK_EQ(joinUrl("http://h:4500/", "/api/mo"), "http://h:4500/api/mo");
  CHECK_EQ(joinUrl("http://h:4500", "/api/mo"), "http://h:4500/api/mo");
}

TEST(theme_resolve) {
  CHECK(!resolveDark(ThemeMode::Light, wib(2026, 11, 9, 22, 0, 0), true));
  CHECK(resolveDark(ThemeMode::Dark, wib(2026, 11, 9, 10, 0, 0), false));
  CHECK(resolveDark(ThemeMode::Auto, wib(2026, 11, 9, 22, 0, 0), false));
  CHECK(!resolveDark(ThemeMode::Auto, wib(2026, 11, 9, 10, 0, 0), true));
  CHECK(resolveDark(ThemeMode::Auto, 0, true));
  CHECK(!resolveDark(ThemeMode::Auto, 0, false));
}
