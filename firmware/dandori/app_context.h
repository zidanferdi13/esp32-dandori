// References to every long-lived service, shared by the UI screens.
#pragma once
#include <string>
#include <vector>

#include "src/core/downtime_counter.h"
#include "src/core/event_store.h"
#include "src/core/mo_list.h"
#include "src/core/outbox.h"
#include "src/core/settings.h"

struct AppContext {
  Settings& settings;
  DowntimeCounter& counter;
  EventStore& store;
  OutboxScheduler& outbox;
  std::vector<MoItem>& lastMoList;
  bool& lastDark;
};
