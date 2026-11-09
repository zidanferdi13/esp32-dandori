// Device settings (spec §5.4) and their validation.
#pragma once
#include <cstdint>
#include <string>

enum class ThemeMode { Light, Dark, Auto };

struct Settings {
  std::string wifiSsid;
  std::string wifiPass;
  std::string baseUrl;
  std::string token;
  std::string machineCode = "PALLETIZER-01";
  std::string ntpServer = "pool.ntp.org";
  std::string pin = "1234";
  ThemeMode theme = ThemeMode::Dark;
  uint16_t screenOffMin = 5;
  bool beep = true;
};

bool isValidPin(const std::string& pin);  // exactly 4 digits
bool isValidBaseUrl(const std::string& url);  // http:// or https:// followed by a host, no spaces
// Joins base and path with exactly one '/'.
std::string joinUrl(const std::string& base, const std::string& path);
// Light/Dark are fixed; Auto is dark during isDarkHours() when the clock is valid, else lastDark.
bool resolveDark(ThemeMode mode, int64_t nowEpoch, bool lastDark);
