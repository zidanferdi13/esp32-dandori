#include "event_store.h"

#include <algorithm>
#include <cstdio>

std::string EventStore::fileNameOf(const DowntimeEvent& e) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%08u.json", static_cast<unsigned>(e.order));
  return buf;
}

void EventStore::load() {
  events_.clear();
  nextOrder_ = 1;
  for (const std::string& name : fs_.list()) {
    std::string data;
    DowntimeEvent e;
    if (!fs_.read(name, data) || !fromStorageJson(data, e)) continue;
    events_.push_back(e);
    nextOrder_ = std::max(nextOrder_, e.order + 1);
  }
  std::sort(events_.begin(), events_.end(),
            [](const DowntimeEvent& a, const DowntimeEvent& b) { return a.order < b.order; });
}

bool EventStore::persist(const DowntimeEvent& e) { return fs_.write(fileNameOf(e), toStorageJson(e)); }

bool EventStore::add(const DowntimeEvent& e) {
  DowntimeEvent copy = e;
  copy.order = nextOrder_;
  if (!persist(copy)) return false;
  ++nextOrder_;
  events_.push_back(copy);
  return true;
}

bool EventStore::setStatus(const std::string& id, EventStatus s) {
  for (DowntimeEvent& e : events_) {
    if (e.id != id) continue;
    DowntimeEvent updated = e;
    updated.status = s;
    if (!persist(updated)) return false;
    e = updated;
    return true;
  }
  return false;
}

const DowntimeEvent* EventStore::find(const std::string& id) const {
  for (const DowntimeEvent& e : events_) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

std::vector<std::string> EventStore::pendingIds() const {
  std::vector<std::string> ids;
  for (const DowntimeEvent& e : events_) {
    if (e.status == EventStatus::Pending) ids.push_back(e.id);
  }
  return ids;
}

size_t EventStore::pendingCount() const {
  return static_cast<size_t>(std::count_if(events_.begin(), events_.end(), [](const DowntimeEvent& e) {
    return e.status == EventStatus::Pending;
  }));
}

bool EventStore::inDay(const DowntimeEvent& e, const std::string& prodDate) const {
  return e.prodDate.empty() || e.prodDate == prodDate;
}

std::vector<DowntimeEvent> EventStore::history(const std::string& prodDate) const {
  std::vector<DowntimeEvent> out;
  for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
    if (inDay(*it, prodDate)) out.push_back(*it);
  }
  return out;
}

DaySummary EventStore::summary(const std::string& prodDate) const {
  DaySummary d;
  for (const DowntimeEvent& e : events_) {
    if (!inDay(e, prodDate)) continue;
    ++d.count;
    d.totalSec += e.totalDetik;
  }
  return d;
}

void EventStore::prune(const std::string& currentProdDate, size_t maxKept) {
  auto drop = [this](size_t index) {
    fs_.remove(fileNameOf(events_[index]));
    events_.erase(events_.begin() + static_cast<std::ptrdiff_t>(index));
  };
  for (size_t i = 0; i < events_.size();) {
    const DowntimeEvent& e = events_[i];
    if (e.status != EventStatus::Pending && !e.prodDate.empty() && e.prodDate != currentProdDate) {
      drop(i);
    } else {
      ++i;
    }
  }
  for (size_t i = 0; i < events_.size() && events_.size() > maxKept;) {
    if (events_[i].status != EventStatus::Pending) {
      drop(i);
    } else {
      ++i;
    }
  }
}
