// LittleFS event files and NVS (Preferences) for settings, counter snapshot and the event sequence.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "../core/downtime_counter.h"
#include "../core/event_store.h"
#include "../core/settings.h"

// Mounts LittleFS (formats it on the first failed mount) and opens NVS.
bool storageBegin();

class LittleFsFileStore : public FileStore {
 public:
  bool write(const std::string& name, const std::string& data) override;
  bool read(const std::string& name, std::string& out) override;
  bool remove(const std::string& name) override;
  std::vector<std::string> list() override;
};

Settings settingsLoad();
void settingsSave(const Settings& s);

bool counterLoad(CounterSnapshot& out);
void counterSave(const CounterSnapshot& s);

// Monotonic, persisted before it is returned, so event ids stay unique across reboots.
uint32_t nextEventSeq();

struct StorageUsage {
  size_t usedBytes;
  size_t totalBytes;
};
StorageUsage storageUsage();
