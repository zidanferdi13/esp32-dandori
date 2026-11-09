#include "net_service.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "../core/settings.h"
#include "clock.h"

namespace {

constexpr uint32_t kHttpTimeoutMs = 8000;
constexpr int kStatusNoServer = -2;

struct NetRequest {
  enum Kind { MoList, Send, TestConn, WifiScan } kind;
  std::string path;
  std::string id;
  std::string body;
};

struct Config {
  std::string baseUrl;
  std::string token;
};

QueueHandle_t s_requests = nullptr;
QueueHandle_t s_results = nullptr;
SemaphoreHandle_t s_cfgMutex = nullptr;
Config s_cfg;
Settings s_applied;
volatile bool s_reconnected = false;
volatile bool s_wasConnected = false;

Config readConfig() {
  xSemaphoreTake(s_cfgMutex, portMAX_DELAY);
  Config c = s_cfg;
  xSemaphoreGive(s_cfgMutex);
  return c;
}

void post(NetRequest* r) {
  if (xQueueSend(s_requests, &r, 0) != pdTRUE) delete r;  // queue full: the caller retries later
}

void publish(NetResult* r) {
  if (xQueueSend(s_results, &r, pdMS_TO_TICKS(100)) != pdTRUE) delete r;
}

// Returns the HTTP status, or <= 0 on a transport error.
int httpCall(const Config& cfg, bool isPost, const std::string& path, const std::string& body,
             std::string& response) {
  if (cfg.baseUrl.empty()) return kStatusNoServer;
  if (WiFi.status() != WL_CONNECTED) return -1;

  const std::string url = joinUrl(cfg.baseUrl, path);
  const bool https = url.rfind("https://", 0) == 0;
  WiFiClient plain;
  WiFiClientSecure secure;
  // TODO(cert): certificate checking for the production API is still open (spec §9).
  if (https) secure.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  const bool started = https ? http.begin(secure, url.c_str()) : http.begin(plain, url.c_str());
  if (!started) return -1;
  if (!cfg.token.empty()) http.addHeader("Authorization", ("Bearer " + cfg.token).c_str());

  int status;
  if (isPost) {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())), body.size());
  } else {
    status = http.GET();
  }
  if (status > 0) response = http.getString().c_str();
  http.end();
  return status;
}

void handle(const NetRequest& req) {
  NetResult* res = new NetResult();
  const Config cfg = readConfig();
  std::string response;

  switch (req.kind) {
    case NetRequest::MoList:
    case NetRequest::TestConn: {
      res->kind = req.kind == NetRequest::MoList ? NetResult::MoList : NetResult::TestConn;
      res->httpStatus = httpCall(cfg, false, req.path, "", response);
      if (res->httpStatus >= 200 && res->httpStatus < 300) {
        res->ok = req.kind == NetRequest::TestConn || parseMoList(response, res->mos);
        if (!res->ok) res->error = "respons tidak valid";
      } else {
        res->error = res->httpStatus == kStatusNoServer ? "server belum diatur" : "gagal";
      }
      break;
    }
    case NetRequest::Send:
      res->kind = NetResult::SendDone;
      res->id = req.id;
      res->httpStatus = httpCall(cfg, true, "/api/downtime", req.body, response);
      res->ok = res->httpStatus >= 200 && res->httpStatus < 300;
      break;
    case NetRequest::WifiScan: {
      res->kind = NetResult::WifiScan;
      const int n = WiFi.scanNetworks();
      std::vector<std::pair<int, std::string>> found;
      for (int i = 0; i < n; ++i) {
        const std::string ssid = WiFi.SSID(i).c_str();
        if (ssid.empty()) continue;
        const bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        found.push_back({WiFi.RSSI(i), ssid + "\t" + std::to_string(WiFi.RSSI(i)) + "\t" + (secured ? "1" : "0")});
      }
      WiFi.scanDelete();
      std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
      for (const auto& f : found) res->ssids.push_back(f.second);
      res->ok = n >= 0;
      break;
    }
  }
  publish(res);
}

void worker(void*) {
  for (;;) {
    NetRequest* req = nullptr;
    if (xQueueReceive(s_requests, &req, pdMS_TO_TICKS(500)) == pdTRUE && req) {
      handle(*req);
      delete req;
    }
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !s_wasConnected) s_reconnected = true;
    s_wasConnected = connected;
  }
}

void connectWifi(const Settings& s) {
  WiFi.disconnect();
  if (!s.wifiSsid.empty()) WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
}

}  // namespace

void netBegin(const Settings& s) {
  s_requests = xQueueCreate(8, sizeof(NetRequest*));
  s_results = xQueueCreate(8, sizeof(NetResult*));
  s_cfgMutex = xSemaphoreCreateMutex();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  s_applied = s;
  s_cfg = {s.baseUrl, s.token};
  connectWifi(s);
  clockBegin(s.ntpServer);

  xTaskCreatePinnedToCore(worker, "net", 16384, nullptr, 1, nullptr, 0);
}

void netApplySettings(const Settings& s) {
  xSemaphoreTake(s_cfgMutex, portMAX_DELAY);
  s_cfg = {s.baseUrl, s.token};
  xSemaphoreGive(s_cfgMutex);

  if (s.wifiSsid != s_applied.wifiSsid || s.wifiPass != s_applied.wifiPass) connectWifi(s);
  if (s.ntpServer != s_applied.ntpServer) clockBegin(s.ntpServer);
  s_applied = s;
}

void netRequestMoList(int64_t nowEpoch) { post(new NetRequest{NetRequest::MoList, moListPath(nowEpoch), "", ""}); }

void netRequestSend(const std::string& id, const std::string& apiJson) {
  post(new NetRequest{NetRequest::Send, "", id, apiJson});
}

void netRequestTestConnection(int64_t nowEpoch) {
  post(new NetRequest{NetRequest::TestConn, moListPath(nowEpoch), "", ""});
}

void netRequestWifiScan() { post(new NetRequest{NetRequest::WifiScan, "", "", ""}); }

bool netPoll(NetResult& out) {
  NetResult* r = nullptr;
  if (xQueueReceive(s_results, &r, 0) != pdTRUE || !r) return false;
  out = *r;
  delete r;
  return true;
}

bool netConsumeReconnected() {
  if (!s_reconnected) return false;
  s_reconnected = false;
  return true;
}

bool netWifiConnected() { return WiFi.status() == WL_CONNECTED; }

std::string netIp() { return netWifiConnected() ? WiFi.localIP().toString().c_str() : ""; }

std::string netMac() { return WiFi.macAddress().c_str(); }
