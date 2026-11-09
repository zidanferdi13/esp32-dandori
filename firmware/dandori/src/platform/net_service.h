// WiFi + HTTP on a worker task (core 0). The loop posts requests and polls results;
// the worker never touches LVGL or EventStore.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "../core/mo_list.h"
#include "../core/settings.h"

struct NetResult {
  enum Kind { MoList, SendDone, TestConn, WifiScan } kind = MoList;
  bool ok = false;
  int httpStatus = 0;  // <= 0: transport error (no WiFi, timeout, refused); -2: server URL not set
  std::string id;      // SendDone: event id
  std::vector<MoItem> mos;
  std::vector<std::string> ssids;  // WifiScan, strongest first: "<ssid>\t<rssi>\t<1 if secured>"
  std::string error;
};

void netBegin(const Settings& s);
void netApplySettings(const Settings& s);

void netRequestMoList(int64_t nowEpoch);
void netRequestSend(const std::string& id, const std::string& apiJson);
void netRequestTestConnection(int64_t nowEpoch);
void netRequestWifiScan();

bool netPoll(NetResult& out);  // non-blocking
// True once after WiFi goes from disconnected to connected (the outbox uses it to retry now).
bool netConsumeReconnected();

bool netWifiConnected();
std::string netIp();
std::string netMac();
