// Dandori downtime recorder — Waveshare ESP32-S3-Touch-LCD-7.
// Spec: docs/superpowers/specs/2026-11-09-dandori-downtime-design.md
#include <lvgl.h>

#include "app_context.h"
#include "src/core/wib_time.h"
#include "src/platform/clock.h"
#include "src/platform/display.h"
#include "src/platform/net_service.h"
#include "src/platform/storage.h"
#include "src/ui/ui.h"

static Settings g_settings;
static DowntimeCounter g_counter;
static LittleFsFileStore g_files;
static EventStore g_store(g_files);
static OutboxScheduler g_outbox;
static std::vector<MoItem> g_moList;
static bool g_lastDark = true;
static AppContext g_ctx{g_settings, g_counter, g_store, g_outbox, g_moList, g_lastDark};

static std::string g_lastPrunedDay;

static void handleNetResult(const NetResult& r) {
  if (r.kind == NetResult::SendDone) {
    const SendOutcome o = classifyHttp(r.httpStatus);
    if (o == SendOutcome::Sent) g_store.setStatus(r.id, EventStatus::Sent);
    if (o == SendOutcome::Rejected) g_store.setStatus(r.id, EventStatus::Rejected);
    g_outbox.onResult(r.id, o, millis());
    Serial.printf("send %s -> %d\n", r.id.c_str(), r.httpStatus);
  }
  uiOnNetResult(r);
}

static void pumpOutbox() {
  if (netConsumeReconnected()) g_outbox.kick();
  const std::string id = g_outbox.next(g_store.pendingIds(), millis());
  if (id.empty()) return;
  const DowntimeEvent* e = g_store.find(id);
  if (e) netRequestSend(id, toApiJson(*e));
}

static void pruneOnNewDay() {
  if (!clockSynced()) return;
  const std::string day = productionDate(nowEpoch());
  if (day == g_lastPrunedDay) return;
  g_lastPrunedDay = day;
  g_store.prune(day);
}

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) delay(10);  // let a USB host attach to see boot logs
  if (!storageBegin()) Serial.println("storage init failed");
  g_settings = settingsLoad();
  g_store.load();
  Serial.printf("dandori: %u events, %u pending\n", (unsigned)g_files.list().size(), (unsigned)g_store.pendingCount());

  if (!displayBegin()) Serial.println("display init failed");
  displaySetScreenOffMinutes(g_settings.screenOffMin);
  netBegin(g_settings);
  uiBegin(g_ctx);
}

void loop() {
  displayTick();
  NetResult r;
  while (netPoll(r)) handleNetResult(r);
  pumpOutbox();
  uiTick(millis());
  pruneOnNewDay();
  delay(5);
}
