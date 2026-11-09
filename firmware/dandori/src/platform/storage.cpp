#include "storage.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>

namespace {

constexpr const char* kEventDir = "/ev";
constexpr const char* kNvsNamespace = "dandori";

Preferences s_prefs;

std::string eventPath(const std::string& name) { return std::string(kEventDir) + "/" + name; }

std::string getStr(const char* key, const std::string& fallback) {
  return std::string(s_prefs.getString(key, fallback.c_str()).c_str());
}

}  // namespace

bool storageBegin() {
  if (!LittleFS.begin(true)) {
    Serial.println("storage: LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(kEventDir)) LittleFS.mkdir(kEventDir);
  return s_prefs.begin(kNvsNamespace, false);
}

bool LittleFsFileStore::write(const std::string& name, const std::string& data) {
  // Write to a temp file then rename, so a power cut never leaves a half-written event.
  const std::string path = eventPath(name);
  const std::string tmp = path + ".tmp";
  File f = LittleFS.open(tmp.c_str(), "w");
  if (!f) return false;
  const size_t written = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  f.close();
  if (written != data.size()) {
    LittleFS.remove(tmp.c_str());
    return false;
  }
  LittleFS.remove(path.c_str());
  return LittleFS.rename(tmp.c_str(), path.c_str());
}

bool LittleFsFileStore::read(const std::string& name, std::string& out) {
  File f = LittleFS.open(eventPath(name).c_str(), "r");
  if (!f) return false;
  out.clear();
  out.reserve(f.size());
  while (f.available()) out.push_back(static_cast<char>(f.read()));
  f.close();
  return true;
}

bool LittleFsFileStore::remove(const std::string& name) { return LittleFS.remove(eventPath(name).c_str()); }

std::vector<std::string> LittleFsFileStore::list() {
  std::vector<std::string> names;
  File dir = LittleFS.open(kEventDir);
  if (!dir || !dir.isDirectory()) return names;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    std::string name = f.name();
    const size_t slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    f.close();
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0) {
      LittleFS.remove(eventPath(name).c_str());  // leftover from an interrupted write
      continue;
    }
    names.push_back(name);
  }
  return names;
}

Settings settingsLoad() {
  Settings d;
  Settings s;
  s.wifiSsid = getStr("wifiSsid", d.wifiSsid);
  s.wifiPass = getStr("wifiPass", d.wifiPass);
  s.baseUrl = getStr("baseUrl", d.baseUrl);
  s.token = getStr("token", d.token);
  s.machineCode = getStr("machine", d.machineCode);
  s.ntpServer = getStr("ntp", d.ntpServer);
  s.pin = getStr("pin", d.pin);
  s.theme = static_cast<ThemeMode>(s_prefs.getUChar("theme", static_cast<uint8_t>(d.theme)));
  s.screenOffMin = s_prefs.getUShort("screenOff", d.screenOffMin);
  s.beep = s_prefs.getBool("beep", d.beep);
  return s;
}

void settingsSave(const Settings& s) {
  s_prefs.putString("wifiSsid", s.wifiSsid.c_str());
  s_prefs.putString("wifiPass", s.wifiPass.c_str());
  s_prefs.putString("baseUrl", s.baseUrl.c_str());
  s_prefs.putString("token", s.token.c_str());
  s_prefs.putString("machine", s.machineCode.c_str());
  s_prefs.putString("ntp", s.ntpServer.c_str());
  s_prefs.putString("pin", s.pin.c_str());
  s_prefs.putUChar("theme", static_cast<uint8_t>(s.theme));
  s_prefs.putUShort("screenOff", s.screenOffMin);
  s_prefs.putBool("beep", s.beep);
}

bool counterLoad(CounterSnapshot& out) {
  if (!s_prefs.isKey("cState")) return false;
  out.state = static_cast<CounterState>(s_prefs.getUChar("cState", 0));
  out.activeMo = getStr("cMo", "");
  out.startEpoch = s_prefs.getLong64("cStart", 0);
  out.stopEpoch = s_prefs.getLong64("cStop", 0);
  out.totalSec = s_prefs.getUInt("cTotal", 0);
  out.estimated = s_prefs.getBool("cEst", false);
  return true;
}

void counterSave(const CounterSnapshot& s) {
  s_prefs.putString("cMo", s.activeMo.c_str());
  s_prefs.putLong64("cStart", s.startEpoch);
  s_prefs.putLong64("cStop", s.stopEpoch);
  s_prefs.putUInt("cTotal", s.totalSec);
  s_prefs.putBool("cEst", s.estimated);
  s_prefs.putUChar("cState", static_cast<uint8_t>(s.state));  // last: marks the snapshot complete
}

uint32_t nextEventSeq() {
  const uint32_t seq = s_prefs.getUInt("seq", 0) + 1;
  s_prefs.putUInt("seq", seq);
  return seq;
}

StorageUsage storageUsage() { return {LittleFS.usedBytes(), LittleFS.totalBytes()}; }
