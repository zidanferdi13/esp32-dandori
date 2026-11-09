// In-memory FileStore for host tests.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/event_store.h"

class MemFileStore : public FileStore {
 public:
  bool write(const std::string& name, const std::string& data) override {
    files_[name] = data;
    return true;
  }
  bool read(const std::string& name, std::string& out) override {
    auto it = files_.find(name);
    if (it == files_.end()) return false;
    out = it->second;
    return true;
  }
  bool remove(const std::string& name) override { return files_.erase(name) > 0; }
  std::vector<std::string> list() override {
    std::vector<std::string> names;
    for (const auto& f : files_) names.push_back(f.first);
    return names;
  }

 private:
  std::map<std::string, std::string> files_;
};
