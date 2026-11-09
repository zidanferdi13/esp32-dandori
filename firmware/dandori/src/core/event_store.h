// Persistent queue + history of downtime events (spec §5.3, §8).
// Every event is one file, written before it is ever sent.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "downtime_event.h"

class FileStore {
 public:
  virtual ~FileStore() = default;
  virtual bool write(const std::string& name, const std::string& data) = 0;
  virtual bool read(const std::string& name, std::string& out) = 0;
  virtual bool remove(const std::string& name) = 0;
  virtual std::vector<std::string> list() = 0;  // names only
};

struct DaySummary {
  uint32_t count = 0;
  uint32_t totalSec = 0;
};

class EventStore {
 public:
  explicit EventStore(FileStore& fs) : fs_(fs) {}

  void load();  // reads every file, skips unparsable ones
  bool add(const DowntimeEvent& e);  // assigns the insertion order, writes the file, then caches
  bool setStatus(const std::string& id, EventStatus s);
  const DowntimeEvent* find(const std::string& id) const;

  std::vector<std::string> pendingIds() const;  // oldest first
  size_t pendingCount() const;
  // Events of that production day plus those recorded without a clock (prodDate ""), newest first.
  std::vector<DowntimeEvent> history(const std::string& prodDate) const;
  DaySummary summary(const std::string& prodDate) const;  // same set as history()
  // Drops non-pending events of other days, then the oldest non-pending beyond maxKept files.
  void prune(const std::string& currentProdDate, size_t maxKept = 500);

 private:
  static std::string fileNameOf(const DowntimeEvent& e);
  bool persist(const DowntimeEvent& e);
  bool inDay(const DowntimeEvent& e, const std::string& prodDate) const;

  FileStore& fs_;
  std::vector<DowntimeEvent> events_;  // ordered by insertion
  uint32_t nextOrder_ = 1;
};
