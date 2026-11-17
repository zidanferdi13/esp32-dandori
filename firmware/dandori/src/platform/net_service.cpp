#include "net_service.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>

#include <algorithm>

#include "../core/settings.h"
#include "clock.h"

namespace {

constexpr uint32_t kHttpTimeoutMs = 8000;
constexpr int kStatusNoServer = -2;

struct NetRequest {
  enum Kind { MoList, Send, TestConn, WifiScan, NtpChange } kind;
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
volatile bool s_connectPending = false;  // set by the UI task, applied by the worker
volatile bool s_hasCredentials = false;
volatile bool s_reconnected = false;
volatile bool s_wasConnected = false;

Config readConfig() {
  xSemaphoreTake(s_cfgMutex, portMAX_DELAY);
  Config c = s_cfg;
  xSemaphoreGive(s_cfgMutex);
  return c;
}

// The IDF default country is "01" (world safe) = channels 1-11, so APs on channel 12/13 never
// appear in a scan. Indonesia allows 1-13. MANUAL policy keeps this even while associated to an
// AP that advertises a different country via 802.11d.
void applyScanCountry() {
  wifi_country_t id = {};
  id.cc[0] = 'I';
  id.cc[1] = 'D';
  id.schan = 1;
  id.nchan = 13;
  id.max_tx_power = 20;
  id.policy = WIFI_COUNTRY_POLICY_MANUAL;
  const esp_err_t err = esp_wifi_set_country(&id);
  Serial.printf("wifi: country ID 1-13 -> %s\n", esp_err_to_name(err));
}

// A drop logged only as status=6 hides the cause, and every reason has a different fix
// (wrong password, AP out of range, DHCP, or a rejected reconnect).
void logDisconnect(arduino_event_id_t event, arduino_event_info_t info) {
  if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) return;
  const uint8_t reason = info.wifi_sta_disconnected.reason;
  Serial.printf("wifi: disconnected reason=%u %s\n", reason,
                WiFi.STA.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
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
      res->httpStatus = httpCall(cfg, true, "/kanban/postDownTime", req.body, response);
      res->ok = res->httpStatus >= 200 && res->httpStatus < 300;
      break;
    case NetRequest::WifiScan: {
      res->kind = NetResult::WifiScan;
      // A disconnected STA may still be connecting. Retrying scans alone never cancels it.
      // Keep an established link; pause reconnect only while scanning an offline STA.
      const bool offline = WiFi.status() != WL_CONNECTED;
      const bool autoReconnect = WiFi.getAutoReconnect();
      const bool resume = offline && s_hasCredentials;
      if (offline) {
        WiFi.setAutoReconnect(false);
        const esp_err_t err = esp_wifi_disconnect();
        Serial.printf("wifi: pause connect -> %s\n", esp_err_to_name(err));
      }
      const uint32_t started = millis();
      int n = WIFI_SCAN_FAILED;
      for (int attempt = 0; attempt < 2; ++attempt) {
        n = WiFi.scanNetworks();
        if (n != WIFI_SCAN_FAILED && n != WIFI_SCAN_RUNNING) break;
        if (!offline) break;
        vTaskDelay(pdMS_TO_TICKS(800));
      }
      Serial.printf("wifi: scan -> %d (status=%d elapsed=%lu heap=%u)\n", n,
                    static_cast<int>(WiFi.status()), static_cast<unsigned long>(millis() - started),
                    static_cast<unsigned>(ESP.getFreeHeap()));
      if (n < 0) {
        const esp_err_t err = esp_wifi_scan_stop();
        Serial.printf("wifi: scan stop -> %s\n", esp_err_to_name(err));
      }
      std::vector<std::pair<int, std::string>> found;
      for (int i = 0; i < n; ++i) {
        const std::string ssid = WiFi.SSID(i).c_str();
        if (ssid.empty()) continue;
        const bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        found.push_back({WiFi.RSSI(i), ssid + "\t" + std::to_string(WiFi.RSSI(i)) + "\t" + (secured ? "1" : "0")});
      }
      WiFi.scanDelete();
      if (offline) {
        WiFi.setAutoReconnect(autoReconnect);
        if (resume) {
          const bool ok = WiFi.reconnect();
          Serial.printf("wifi: resume connect -> %d\n", ok ? 1 : 0);
        }
      }
      std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
      for (const auto& f : found) res->ssids.push_back(f.second);
      res->ok = n >= 0;
      if (n == WIFI_SCAN_RUNNING) res->error = "pemindaian masih berjalan";
      else if (n == WIFI_SCAN_FAILED) res->error = "pemindaian gagal";
      break;
    }
    case NetRequest::NtpChange:
      clockBegin(req.path);  // SNTP stays on this task; the loop only queues the change
      delete res;
      return;
  }
  publish(res);
}

void connectWifi(const Settings& s);

void worker(void*) {
  for (;;) {
    NetRequest* req = nullptr;
    if (xQueueReceive(s_requests, &req, pdMS_TO_TICKS(500)) == pdTRUE && req) {
      handle(*req);
      delete req;
    }
    // Only this task touches the WiFi driver, so a settings change cannot race with a scan.
    if (clockConsumeRestartRequest()) clockRestart();
    if (s_connectPending) {
      s_connectPending = false;
      Settings s;
      xSemaphoreTake(s_cfgMutex, portMAX_DELAY);
      s = s_applied;
      s_hasCredentials = !s.wifiSsid.empty();
      xSemaphoreGive(s_cfgMutex);
      WiFi.setAutoReconnect(true);
      connectWifi(s);
    }
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !s_wasConnected) {
      s_reconnected = true;
      clockRestart();  // SNTP may have started before there was an IP/DNS
    }
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
  applyScanCountry();  // must follow mode(): the driver has to be initialised first
  WiFi.onEvent(logDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.setAutoReconnect(true);
  // scanNetworks() blocks until the scan-done event. The 60 s default can exceed the outbox
  // in-flight timeout, which would resend a POST that is still running, so cap it.
  WiFi.setScanTimeout(8000);
  s_applied = s;
  s_hasCredentials = !s.wifiSsid.empty();
  s_cfg = {s.baseUrl, s.token};
  connectWifi(s);
  clockBegin(s.ntpServer);

  xTaskCreatePinnedToCore(worker, "net", 16384, nullptr, 1, nullptr, 0);
}

void netApplySettings(const Settings& s) {
  bool ntpChanged = false;
  xSemaphoreTake(s_cfgMutex, portMAX_DELAY);
  s_cfg = {s.baseUrl, s.token};
  const bool wifiChanged = s.wifiSsid != s_applied.wifiSsid || s.wifiPass != s_applied.wifiPass;
  ntpChanged = s.ntpServer != s_applied.ntpServer;
  s_applied = s;
  xSemaphoreGive(s_cfgMutex);

  if (wifiChanged) s_connectPending = true;  // worker reconnects after any scan in flight
  if (ntpChanged) post(new NetRequest{NetRequest::NtpChange, s.ntpServer, "", ""});
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
