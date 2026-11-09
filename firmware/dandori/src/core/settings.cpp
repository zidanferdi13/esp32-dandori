#include "settings.h"

#include "wib_time.h"

bool isValidPin(const std::string& pin) {
  if (pin.size() != 4) return false;
  for (char c : pin) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

bool isValidBaseUrl(const std::string& url) {
  std::string rest;
  if (url.rfind("http://", 0) == 0) {
    rest = url.substr(7);
  } else if (url.rfind("https://", 0) == 0) {
    rest = url.substr(8);
  } else {
    return false;
  }
  if (rest.empty() || rest[0] == '/' || rest[0] == ':') return false;
  return url.find_first_of(" \t\r\n") == std::string::npos;
}

std::string joinUrl(const std::string& base, const std::string& path) {
  std::string b = base;
  while (!b.empty() && b.back() == '/') b.pop_back();
  if (path.empty() || path[0] != '/') return b + "/" + path;
  return b + path;
}

bool resolveDark(ThemeMode mode, int64_t nowEpoch, bool lastDark) {
  switch (mode) {
    case ThemeMode::Light:
      return false;
    case ThemeMode::Dark:
      return true;
    case ThemeMode::Auto:
      return isClockValid(nowEpoch) ? isDarkHours(nowEpoch) : lastDark;
  }
  return lastDark;
}
