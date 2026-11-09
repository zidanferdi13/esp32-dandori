# Dandori Downtime Recorder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Arduino firmware for the Waveshare ESP32-S3-Touch-LCD-7. Operators pick an active MO, time each machine stop with Start/Stop, enter a problem code (1–26), and send the event to a REST API. A local Postman mock stands in for the backend.

**Architecture:** All business logic lives in `firmware/dandori/src/core/`: plain C++17 with no `Arduino.h`. It is unit-tested on the PC with g++. Hardware code lives in `src/platform/`: display, touch, CH422G expander, LittleFS/NVS, WiFi/NTP/HTTP. UI screens live in `src/ui/` (LVGL 9). Only the main loop touches LVGL and `EventStore`. HTTP runs in a FreeRTOS worker task on core 0 and talks to the loop through queues.

**Tech Stack:** Arduino IDE with esp32 core 3.3.7, LVGL 9.4.0, GFX Library for Arduino 1.6.2 (`Arduino_ESP32RGBPanel`), SensorLib 0.3.3 (`TouchDrvGT911`), ArduinoJson 7.4.3. All of these are already installed. Host tests use g++ (WinLibs). The mock is Postman CLI 1.73 (`postman mock`).

**Spec:** `docs/superpowers/specs/2026-11-09-dandori-downtime-design.md`. Read it before any task. The section numbers below (§) refer to it.

## Global Constraints

- Board: Waveshare ESP32-S3-Touch-LCD-7, 800×480 landscape (§3). Before Task 2, confirm the board is not the 7B variant (1024×600).
- Toolchain: Arduino IDE. The sketch must open and compile in the IDE as-is. CLI builds use the IDE's bundled `arduino-cli.exe` with the same libraries.
- Sketchbook/libraries dir: `%USERPROFILE%\OneDrive\Documents\Arduino` (libraries in `...\libraries`).
- `src/core/**` must not include `Arduino.h`, FreeRTOS, or LVGL. It may include ArduinoJson and the std library only.
- Time zone: WIB, UTC+7, fixed offset. Core computes it arithmetically and never uses the C library TZ.
- Production day starts 08:00 WIB. Shift 1 08:00–17:00, Shift 2 17:00–23:00, Shift 3 23:00–08:00 (§3.1).
- Clock is "valid" only if epoch ≥ `1735689600` (2025-01-01 UTC).
- `total_detik` comes from the monotonic ms counter, never from an NTP difference (§4.3).
- Problem codes: exactly the 26-row table in §6, numeric 1–26.
- API: `GET {baseUrl}/api/mo[?tgl=YYYY-MM-DD]` and `POST {baseUrl}/api/downtime`. Header `Authorization: Bearer <token>` only when the token is non-empty (§7).
- POST body keys exactly: `id, mesin, mo, kode_problem, start, stop, total_detik`. `start`/`stop` are ISO-8601 with `+07:00`, or JSON `null` when the clock is invalid.
- HTTP outcome: 2xx → sent. Transport error, 408, 429, or 5xx → retry. Any other 4xx → rejected, no further retries (§7.2).
- Settings defaults: PIN `1234`, machine code `PALLETIZER-01`, NTP server `pool.ntp.org`, theme Dark, screen-off 5 min. PIN, WiFi, Server API, Mesin, Jam, and Keamanan are PIN-locked. Tampilan and Tentang are not.
- Theme Auto: dark from 18:00 to 06:00 WIB when the clock is valid. Otherwise keep the last applied theme.
- Downtime red (`#D63C3C`) is used in both themes while the timer runs.
- UI copy is in Bahasa Indonesia, using the strings from the spec/mockups (`START`, `STOP`, `KIRIM`, `Batal`, `Ganti MO`, `MESIN BERHENTI`, `Data terkirim`, `Disimpan, akan dikirim ulang otomatis`, `Pilih MO dulu`).
- Mockups (layout reference): `.superpowers/brainstorm/*/content/alur-layar.html`, `launcher-setting.html`.

## Review Focus

1. **Power loss or reboot while the timer runs.** After boot the timer resumes from the persisted start (clock valid), or restarts from boot marked *estimated* (clock invalid). Downtime is never silently dropped. Tests are in Task 5.
2. **WiFi or API down when sending.** The event is persisted before the send and retried with backoff across reboots. A retried event keeps the same `id`, so it is never duplicated. A 4xx stops the retries. Tests are in Tasks 6 and 7.
3. **NTP never syncs** (LAN without internet). Get MO omits `tgl`, `start`/`stop` are `null`, `total_detik` stays correct, and the event `id` stays unique. Tests are in Tasks 4 and 6.
4. **Production-day boundary.** 07:59:59 WIB belongs to the previous date and 08:00:00 to today, including across month and year ends. History and summary follow the production day. Tests are in Task 4.
5. **Bad Get MO response.** An empty list, malformed JSON, a missing `produk`, or HTTP 500 shows a message and a Refresh button. It never crashes and never clears the current active MO. Tests are in Task 6, with the device check in Task 12.

---

## File Structure

```
esp32-dandori/
  .gitignore                       build/, *.exe   (.superpowers/ mockups stay committed)
  README.md                        setup, build, flash, run mock
  tools/build.sh                   compile/upload via bundled arduino-cli; copies lv_conf.h
  tools/run_host_tests.sh          g++ host test runner
  api/openapi.yaml                 API contract (also the hand-off doc for the backend team)
  postman/mocks/dandori/           generated local mock
  firmware/lv_conf.h               LVGL 9 config (copied to libraries/ by build.sh)
  firmware/dandori/
    dandori.ino                    setup()/loop(): wires AppContext, ticks LVGL, outbox, net results
    app_context.h                  struct AppContext (references to all services)
    board_config.h                 pins/timings/I2C addresses for the 7" board
    src/core/  (host-tested)
      problem_codes.h/.cpp         table + lookup + keypad parsing
      wib_time.h/.cpp              WIB conversion, production date, shift, dark hours, ISO string
      downtime_counter.h/.cpp      state machine + snapshot/restore
      downtime_event.h/.cpp        event struct, id, API/storage JSON
      mo_list.h/.cpp               GET /api/mo path + response parsing
      outbox.h/.cpp                retry classification + send scheduling
      event_store.h/.cpp           FileStore interface, queue/history/prune/summary
      settings.h/.cpp              Settings struct, defaults, validation, URL join, theme resolve
    src/platform/
      display.h/.cpp               RGB panel + LVGL display/indev + GT911 + CH422G backlight
      storage.h/.cpp               LittleFsFileStore, SettingsRepo, CounterRepo, sequence counter
      clock.h/.cpp                 NTP config + nowEpoch()/clockValid()
      net_service.h/.cpp           WiFi, HTTP worker task, request/result queues
    src/ui/
      ui.h/.cpp                    navigation, status bar, theme application
      ui_home.cpp  ui_downtime.cpp  ui_history.cpp  ui_settings.cpp  ui_widgets.h/.cpp (keypad, PIN pad)
  test/host/
    mini_test.h  test_main.cpp  test_helpers.h  test_<module>.cpp
```

---

### Task 1: Repository, host test harness, build script

**Files:**
- Create: `.gitignore`, `tools/run_host_tests.sh`, `tools/build.sh`, `test/host/mini_test.h`, `test/host/test_main.cpp`, `test/host/test_smoke.cpp`, `firmware/dandori/dandori.ino` (empty `setup`/`loop`), `firmware/dandori/src/core/.keep`

**Interfaces:**
- Produces: `mini_test.h` macros `TEST(name)`, `CHECK(cond)`, `CHECK_EQ(a, b)`. These print file:line plus both values on failure. `test_main.cpp` runs all registered tests and exits non-zero if any fail.
- Produces: `tools/run_host_tests.sh` compiles `test/host/*.cpp firmware/dandori/src/core/*.cpp` with `g++ -std=c++17 -Wall -Wextra -I firmware/dandori/src -I "$USERPROFILE/OneDrive/Documents/Arduino/libraries/ArduinoJson/src"` into `build/host_tests.exe`, then runs it.
- Produces: `tools/build.sh [--upload PORT]` runs `CLI="$LOCALAPPDATA/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"` with `ARDUINO_DIRECTORIES_USER="$USERPROFILE/OneDrive/Documents/Arduino"` and `compile --fqbn "$FQBN" firmware/dandori`. Before compiling, it copies `firmware/lv_conf.h` to `$ARDUINO_DIRECTORIES_USER/libraries/lv_conf.h` (once `lv_conf.h` exists, from Task 2).

- [ ] **Step 1:** `git init`, then add `.gitignore` (`build/`, `*.exe`).
- [ ] **Step 2:** Install a host compiler: `winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT`. Verify with `g++ --version` in a new shell. If it is not on PATH, hard-code the WinLibs `bin` path in `run_host_tests.sh`.
- [ ] **Step 3:** Write `mini_test.h`, `test_main.cpp`, and `test_smoke.cpp`, with `TEST(smoke) { CHECK_EQ(1 + 1, 2); }` and a deliberately failing `TEST(smoke_fail) { CHECK_EQ(1, 2); }`.
- [ ] **Step 4:** Run `bash tools/run_host_tests.sh`. Expected: the failure report names `smoke_fail` with values `1` and `2`, and the exit code is non-zero. Delete `smoke_fail` and re-run. Expected: `1 passed, 0 failed`, exit 0.
- [ ] **Step 5:** Pick the FQBN. Run `"$CLI" board details -b esp32:esp32:esp32s3` and choose `PSRAM=opi`, `FlashSize=16M`, a 16 MB partition scheme that includes a `spiffs` partition ≥ 1 MB (needed by LittleFS), `USBMode=hwcdc`, and `CDCOnBoot=cdc`. Write the chosen FQBN into `build.sh` and `README.md`.
- [ ] **Step 6:** Run `bash tools/build.sh`. Expected: the empty sketch compiles and prints a "Sketch uses … bytes" line.
- [ ] **Step 7:** Commit with `chore: repo scaffold, host test harness, build script`, adding the spec, the plan, and the mockups too.

### Task 2: Hardware bring-up (display, touch, backlight, LVGL)

**Files:**
- Create: `firmware/lv_conf.h`, `firmware/dandori/board_config.h`, `src/platform/display.h/.cpp`
- Modify: `dandori.ino`

**Interfaces:**
- Produces: `bool displayBegin();` initializes I2C, CH422G, panel, GT911, and LVGL (display plus pointer indev), then returns false on failure. `void displaySetBacklight(bool on);` `bool displayBacklightOn();` `void displayTick();` calls `lv_timer_handler()`. Call `displayTick()` from `loop()`.
- Produces: `void displaySetScreenOffMinutes(uint16_t m);`. `0` means never. After `m` minutes of `lv_display_get_inactive_time()`, the backlight turns off. While the backlight is off, a touch only turns it on. The touch read callback reports `LV_INDEV_STATE_RELEASED` until that finger lifts, so the wake-up touch never clicks a widget (§5.4).

**board_config.h values.** These come from memory and are **not verified**. Check them against Waveshare's official ESP32-S3-Touch-LCD-7 Arduino demo before relying on them:
- I2C: SDA 8, SCL 9. CH422G: write `0x01` to address `0x24` (outputs enabled), then output bits to address `0x38`. Bits: EXIO1 = touch reset, EXIO2 = backlight, EXIO3 = LCD reset, EXIO4 = SD CS.
- GT911: INT 4. Address 0x5D, or 0x14 depending on the INT level during reset.
- RGB: DE 5, VSYNC 3, HSYNC 46, PCLK 7. R0–R4 = 1, 2, 42, 41, 40. G0–G5 = 39, 0, 45, 48, 47, 21. B0–B4 = 14, 38, 18, 17, 10.
- Timing: pclk 16 MHz, `pclk_active_neg = 1`, hsync pulse 4 / back porch 8 / front porch 8, vsync pulse 4 / back porch 8 / front porch 8. If the image is shifted or rolls, use the demo's values.

**Notes.**
- `lv_conf.h`: `LV_COLOR_DEPTH 16`. Malloc via the C library, since large allocations land in PSRAM. Tick via `lv_tick_set_cb(millis)`. Montserrat 14, 16, 20, 28, 48. Enable `LV_USE_KEYBOARD`, `LV_USE_TEXTAREA`, `LV_USE_LIST`, `LV_USE_SWITCH`, `LV_USE_MSGBOX`, `LV_USE_SPINNER`, `LV_USE_BUTTONMATRIX`, `LV_USE_TABVIEW`.
- Display uses two partial draw buffers (800×48 px each) in PSRAM. Flush via `gfx->draw16bitRGBBitmap`.
- If the picture jitters while WiFi is active (a known ESP32-S3 RGB issue), enable the RGB panel bounce buffer (≈ 800×10 px).
- Verify in Waveshare docs whether the backlight supports PWM. If it does not (expected), there is no brightness API (§5.4).
- Check whether the board has a buzzer. If not, the "Bunyi tombol" setting is dropped in Task 14 (§5.4).

- [ ] **Step 1:** Write `board_config.h`, `lv_conf.h`, `display.*`, plus a test screen in `dandori.ino`: a label "Halo Dandori" and a button that counts taps, with touch x/y logged to Serial.
- [ ] **Step 2:** Run `bash tools/build.sh`. Expected: compile OK.
- [ ] **Step 3: Device check (needs the user).** Flash with `bash tools/build.sh --upload COMx`. Expected: the label is centered, the colors are correct (a red test rectangle looks red), the button counter increments on tap, and the Serial coordinates match the corners (0,0 to 799,479). Set screen-off to 1 min temporarily and confirm that the backlight turns off, and that the first touch wakes it without incrementing the counter.
- [ ] **Step 4:** Commit with `feat: board bring-up (RGB panel, GT911, CH422G backlight, LVGL 9)`.

### Task 3: Problem codes and keypad input

**Files:**
- Create: `src/core/problem_codes.h/.cpp`, `test/host/test_problem_codes.cpp`

**Interfaces:**
- Produces: `struct ProblemCode { uint8_t code; const char* jenis; const char* mesin; const char* sub; };`
- Produces: `const ProblemCode* findProblemCode(int code);` returns nullptr outside 1–26. `size_t problemCodeCount();` returns 26.
- Produces: `std::string keypadAppend(const std::string& cur, char digit);` caps the input at 2 digits and ignores a leading `'0'` on empty input. `int keypadValue(const std::string& digits);` returns 1–26, or `-1` if empty or out of range.

- [ ] **Step 1: Failing tests.**
```cpp
TEST(codes_table) { CHECK_EQ(problemCodeCount(), 26u);
  CHECK_EQ(std::string(findProblemCode(1)->sub), "Conveyor Infeed");
  CHECK_EQ(std::string(findProblemCode(13)->jenis), "Loss Time");
  CHECK_EQ(std::string(findProblemCode(13)->sub), "Dandory Produk");
  CHECK_EQ(std::string(findProblemCode(22)->mesin), "Utility");
  CHECK_EQ(std::string(findProblemCode(26)->jenis), "Performance Loss");
  CHECK(findProblemCode(0) == nullptr); CHECK(findProblemCode(27) == nullptr); }
TEST(keypad) { CHECK_EQ(keypadAppend("", '0'), "");  CHECK_EQ(keypadAppend("1", '3'), "13");
  CHECK_EQ(keypadAppend("13", '5'), "13"); CHECK_EQ(keypadValue("13"), 13);
  CHECK_EQ(keypadValue(""), -1); CHECK_EQ(keypadValue("27"), -1); CHECK_EQ(keypadValue("9"), 9); }
```
- [ ] **Step 2:** Run `bash tools/run_host_tests.sh`. Expected: FAIL to compile (undefined symbols).
- [ ] **Step 3:** Implement. Copy all 26 rows verbatim from §6, including spelling (`Dandory Produk`, `Pressing Plat BCD`, `Lain-Lain`).
- [ ] **Step 4:** Run the tests. Expected: all pass.
- [ ] **Step 5:** Commit with `feat(core): problem code table and keypad parsing`.

### Task 4: WIB time, production day, shift

**Files:**
- Create: `src/core/wib_time.h/.cpp`, `test/host/test_wib_time.cpp`

**Interfaces:**
- Produces: `constexpr int64_t kMinValidEpoch = 1735689600;` and `bool isClockValid(int64_t epochUtc);`
- Produces: `struct WibTime { int year, month, day, hour, minute, second; };` and `WibTime toWib(int64_t epochUtc);`. Implement with a days-from-civil algorithm, no `<ctime>` TZ.
- Produces: `std::string productionDate(int64_t epochUtc);` returns `"YYYY-MM-DD"`, where the day starts at 08:00 WIB. `int shiftOf(int64_t epochUtc);` returns 1, 2, or 3. `bool isDarkHours(int64_t epochUtc);` is true for 18:00 ≤ t < 06:00. `std::string isoWib(int64_t epochUtc);` returns `"2026-11-09T08:15:30+07:00"`. `std::string hhmmss(int64_t epochUtc);` returns `"08:15:30"`.

- [ ] **Step 1: Failing tests.** Create `test/host/test_helpers.h` with `int64_t wib(y, mo, d, h, mi, s)` (= `utcEpochOf(...) - 7*3600`, where `utcEpochOf` is its own days-from-civil, independent of the code under test) and `constexpr int64_t T0 = 1794200000;`. Tasks 5–7 include it too.
```cpp
TEST(iso) { CHECK_EQ(isoWib(wib(2026,11,9,8,15,30)), "2026-11-09T08:15:30+07:00"); }
TEST(prod_day_boundary) {
  CHECK_EQ(productionDate(wib(2026,11,10,7,59,59)), "2026-11-09");
  CHECK_EQ(productionDate(wib(2026,11,10,8,0,0)),  "2026-11-10");
  CHECK_EQ(productionDate(wib(2026,11,10,2,0,0)),  "2026-11-09");
  CHECK_EQ(productionDate(wib(2027,1,1,7,0,0)),    "2026-12-31");
  CHECK_EQ(productionDate(wib(2028,3,1,6,0,0)),    "2028-02-29"); }
TEST(shift) { CHECK_EQ(shiftOf(wib(2026,11,9,8,0,0)),1);  CHECK_EQ(shiftOf(wib(2026,11,9,16,59,59)),1);
  CHECK_EQ(shiftOf(wib(2026,11,9,17,0,0)),2); CHECK_EQ(shiftOf(wib(2026,11,9,23,0,0)),3);
  CHECK_EQ(shiftOf(wib(2026,11,10,7,59,59)),3); }
TEST(dark) { CHECK(isDarkHours(wib(2026,11,9,18,0,0))); CHECK(isDarkHours(wib(2026,11,9,5,59,0)));
  CHECK(!isDarkHours(wib(2026,11,9,6,0,0))); CHECK(!isDarkHours(wib(2026,11,9,17,59,59))); }
TEST(clock_valid) { CHECK(!isClockValid(0)); CHECK(!isClockValid(1735689599)); CHECK(isClockValid(1735689600)); }
```
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run. Expected: all pass.
- [ ] **Step 5:** Commit with `feat(core): WIB time, production day, shifts`.

### Task 5: Downtime counter state machine with persistence

**Files:**
- Create: `src/core/downtime_counter.h/.cpp`, `test/host/test_downtime_counter.cpp`

**Interfaces:**
- Consumes: `isClockValid` (Task 4).
- Produces: `enum class CounterState { NoMo, Ready, Running, AwaitCode };`
- Produces:
```cpp
struct CounterSnapshot {            // persisted on every state change
  CounterState state; std::string activeMo;
  int64_t startEpoch;  int64_t stopEpoch;   // 0 when unknown
  uint32_t totalSec;                          // valid in AwaitCode
};
class DowntimeCounter {
 public:
  bool setActiveMo(const std::string& mo);    // false while Running/AwaitCode; NoMo/Ready -> Ready
  const std::string& activeMo() const;
  CounterState state() const;
  bool start(uint32_t nowMs, int64_t nowEpoch);   // Ready -> Running
  bool stop(uint32_t nowMs, int64_t nowEpoch);    // Running -> AwaitCode, freezes totalSec
  uint32_t elapsedSec(uint32_t nowMs) const;      // Running: live; AwaitCode: frozen; else 0
  bool cancel();                                   // AwaitCode -> Ready (discard)
  bool complete();                                 // AwaitCode -> Ready (after event created)
  int64_t startEpoch() const; int64_t stopEpoch() const;
  bool estimated() const;                          // true if resumed without a valid clock
  CounterSnapshot snapshot() const;
  void restore(const CounterSnapshot& s, uint32_t nowMs, int64_t nowEpoch);
};
```
- Restore rules (§4.3):
  - `Running` with a valid persisted `startEpoch` and a valid `nowEpoch`: continue with elapsed `= nowEpoch - startEpoch`, `estimated() == false`.
  - `Running` otherwise: elapsed restarts at 0 from `nowMs`, `estimated() == true`.
  - `AwaitCode`: restore the frozen `totalSec`, epochs, and the estimated flag.
  - `Ready`/`NoMo`: restore the MO only.

- [ ] **Step 1: Failing tests.**
```cpp
TEST(no_mo_blocks_start) { DowntimeCounter c; CHECK(c.state()==CounterState::NoMo); CHECK(!c.start(0, 0)); }
TEST(basic_cycle) { DowntimeCounter c; c.setActiveMo("MO-1");
  CHECK(c.start(1000, T0)); CHECK(!c.start(1500, T0));            // double tap ignored
  CHECK(!c.setActiveMo("MO-2"));                                  // locked while running
  CHECK_EQ(c.elapsedSec(1000 + 5500), 5u);
  CHECK(c.stop(1000 + 335000, T0 + 335)); CHECK_EQ(c.elapsedSec(999999), 335u);
  CHECK(c.state()==CounterState::AwaitCode); CHECK(c.complete()); CHECK(c.state()==CounterState::Ready);
  CHECK_EQ(c.activeMo(), "MO-1"); }
TEST(millis_wrap) { DowntimeCounter c; c.setActiveMo("M"); c.start(0xFFFFF000u, 0); c.stop(0x00001000u, 0);
  CHECK_EQ(c.elapsedSec(0), 8u); }                                  // 8192 ms across wrap
TEST(total_ignores_clock_jump) { DowntimeCounter c; c.setActiveMo("M"); c.start(0, 0);       // no clock at start
  c.stop(60000, T0); CHECK_EQ(c.elapsedSec(0), 60u); CHECK_EQ(c.startEpoch(), 0); }
TEST(cancel) { DowntimeCounter c; c.setActiveMo("M"); c.start(0,T0); c.stop(1000,T0+1); CHECK(c.cancel());
  CHECK(c.state()==CounterState::Ready); }
TEST(resume_with_clock) { DowntimeCounter a; a.setActiveMo("M"); a.start(5000, T0);
  DowntimeCounter b; b.restore(a.snapshot(), 200, T0 + 120);
  CHECK(b.state()==CounterState::Running); CHECK_EQ(b.elapsedSec(200), 120u); CHECK(!b.estimated());
  CHECK_EQ(b.elapsedSec(200 + 10000), 130u); }
TEST(resume_without_clock) { DowntimeCounter a; a.setActiveMo("M"); a.start(5000, T0);
  DowntimeCounter b; b.restore(a.snapshot(), 200, 0);
  CHECK(b.state()==CounterState::Running); CHECK_EQ(b.elapsedSec(200), 0u); CHECK(b.estimated()); }
TEST(resume_await_code) { DowntimeCounter a; a.setActiveMo("M"); a.start(0,T0); a.stop(42000,T0+42);
  DowntimeCounter b; b.restore(a.snapshot(), 7, 0); CHECK(b.state()==CounterState::AwaitCode);
  CHECK_EQ(b.elapsedSec(7), 42u); CHECK_EQ(b.activeMo(), "M"); }
```
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement. Store `startMs` and compute `(uint32_t)(nowMs - startMs) / 1000` to be wrap-safe. On a clock-valid resume, set `startMs = nowMs - (nowEpoch - startEpoch) * 1000`.
- [ ] **Step 4:** Run. Expected: all pass.
- [ ] **Step 5:** Commit with `feat(core): downtime counter with snapshot/restore`.

### Task 6: Event, MO list parsing, outbox scheduling

**Files:**
- Create: `src/core/downtime_event.h/.cpp`, `src/core/mo_list.h/.cpp`, `src/core/outbox.h/.cpp`, `test/host/test_event.cpp`, `test/host/test_mo_list.cpp`, `test/host/test_outbox.cpp`

**Interfaces:**
- Consumes: `isoWib`, `productionDate`, `isClockValid` (Task 4).
- Produces:
```cpp
enum class EventStatus { Pending, Sent, Rejected };
struct DowntimeEvent { std::string id, mesin, mo, prodDate; int kodeProblem; int64_t startEpoch, stopEpoch;
                       uint32_t totalDetik; bool estimated; EventStatus status; };
std::string makeEventId(const std::string& mesin, int64_t startEpoch, uint32_t seq);
std::string toApiJson(const DowntimeEvent& e);        // §7.2 keys only
std::string toStorageJson(const DowntimeEvent& e);    // all fields
bool fromStorageJson(const std::string& s, DowntimeEvent& out);

struct MoItem { std::string mo, produk; };
std::string moListPath(int64_t nowEpoch);             // "/api/mo?tgl=<productionDate>" or "/api/mo"
bool parseMoList(const std::string& body, std::vector<MoItem>& out);   // false on malformed / no "data" array

enum class SendOutcome { Sent, Retry, Rejected };
SendOutcome classifyHttp(int status);                 // status <= 0 means transport error
uint32_t retryDelayMs(uint32_t attempt);              // attempt 1..: 5s,10s,20s,40s, then 60s cap
class OutboxScheduler {                               // one request in flight at a time
 public:
  // returns id to send now, or "" ; pendingIds in oldest-first order
  std::string next(const std::vector<std::string>& pendingIds, uint32_t nowMs);
  void onResult(const std::string& id, SendOutcome o, uint32_t nowMs);
  void kick();                                        // clear backoff (manual "Kirim ulang"/WiFi reconnect)
  bool inFlight() const;
};
```
- Event id: with a valid clock, `"<mesin>-YYYYMMDD-HHMMSS-<seq 4 digits>"` from the WIB start time. Without one, `"<mesin>-NC-<seq 6 digits>"`. `seq` is a persisted monotonic counter (Task 9), so ids stay unique across reboots.
- `prodDate` is `productionDate(startEpoch)` when the clock is valid, `""` otherwise.

- [ ] **Step 1: Failing tests.**
```cpp
TEST(event_id) { CHECK_EQ(makeEventId("PALLETIZER-01", wib(2026,11,9,8,15,30), 7), "PALLETIZER-01-20261109-081530-0007");
  CHECK_EQ(makeEventId("PALLETIZER-01", 0, 12), "PALLETIZER-01-NC-000012"); }
TEST(api_json_with_clock) { auto e = sample(); /* mo MO-2611-0012, kode 13, 08:15:30→08:21:05, 335 s */
  auto j = parse(toApiJson(e));
  CHECK_EQ(j["start"].as<std::string>(), "2026-11-09T08:15:30+07:00");
  CHECK_EQ(j["stop"].as<std::string>(),  "2026-11-09T08:21:05+07:00");
  CHECK_EQ(j["total_detik"].as<int>(), 335); CHECK_EQ(j["kode_problem"].as<int>(), 13);
  CHECK_EQ(j["mesin"].as<std::string>(), "PALLETIZER-01"); CHECK_EQ(j.size(), 7u); }
TEST(api_json_no_clock) { auto e = sample(); e.startEpoch = 0; e.stopEpoch = 0;
  auto j = parse(toApiJson(e)); CHECK(j["start"].isNull()); CHECK(j["stop"].isNull()); CHECK_EQ(j["total_detik"].as<int>(), 335); }
TEST(storage_roundtrip) { auto e = sample(); e.status = EventStatus::Rejected; e.estimated = true;
  DowntimeEvent b; CHECK(fromStorageJson(toStorageJson(e), b)); CHECK_EQ(b.id, e.id); CHECK(b.status==EventStatus::Rejected); CHECK(b.estimated);
  CHECK(!fromStorageJson("{garbage", b)); }
TEST(mo_path) { CHECK_EQ(moListPath(wib(2026,11,10,2,0,0)), "/api/mo?tgl=2026-11-09"); CHECK_EQ(moListPath(0), "/api/mo"); }
TEST(mo_parse) { std::vector<MoItem> v;
  CHECK(parseMoList(R"({"tgl":"2026-11-09","data":[{"mo":"MO-1","produk":"Bata"},{"mo":"MO-2"}]})", v));
  CHECK_EQ(v.size(), 2u); CHECK_EQ(v[1].produk, ""); 
  CHECK(parseMoList(R"({"data":[]})", v)); CHECK_EQ(v.size(), 0u);
  CHECK(!parseMoList("<html>500</html>", v)); CHECK(!parseMoList(R"({"x":1})", v)); }
TEST(classify) { CHECK(classifyHttp(200)==SendOutcome::Sent); CHECK(classifyHttp(201)==SendOutcome::Sent);
  CHECK(classifyHttp(-1)==SendOutcome::Retry); CHECK(classifyHttp(500)==SendOutcome::Retry);
  CHECK(classifyHttp(408)==SendOutcome::Retry); CHECK(classifyHttp(429)==SendOutcome::Retry);
  CHECK(classifyHttp(400)==SendOutcome::Rejected); CHECK(classifyHttp(401)==SendOutcome::Rejected); }
TEST(outbox_backoff) { OutboxScheduler o; std::vector<std::string> p{"a","b"};
  CHECK_EQ(o.next(p, 0), "a"); CHECK_EQ(o.next(p, 0), "");          // in flight
  o.onResult("a", SendOutcome::Retry, 0);
  CHECK_EQ(o.next(p, 100), "b");                                     // a is backing off, b goes
  o.onResult("b", SendOutcome::Sent, 100); p = {"a"};
  CHECK_EQ(o.next(p, 4999), ""); CHECK_EQ(o.next(p, 5000), "a");
  o.onResult("a", SendOutcome::Retry, 5000); CHECK_EQ(o.next(p, 14999), ""); o.kick(); CHECK_EQ(o.next(p, 15000 - 1), "a"); }
TEST(retry_delay_cap) { CHECK_EQ(retryDelayMs(1), 5000u); CHECK_EQ(retryDelayMs(4), 40000u); CHECK_EQ(retryDelayMs(9), 60000u); }
```
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement with ArduinoJson 7 (`JsonDocument`). For null epochs, assign `nullptr`.
- [ ] **Step 4:** Run. Expected: all pass.
- [ ] **Step 5:** Commit with `feat(core): event JSON, MO list parsing, outbox scheduler`.

### Task 7: Event store and settings logic

**Files:**
- Create: `src/core/event_store.h/.cpp`, `src/core/settings.h/.cpp`, `test/host/test_event_store.cpp`, `test/host/test_settings.cpp`, `test/host/mem_file_store.h` (in-memory `FileStore` for tests)

**Interfaces:**
- Consumes: `DowntimeEvent`, `toStorageJson`/`fromStorageJson` (Task 6).
- Produces:
```cpp
class FileStore { public: virtual ~FileStore() = default;
  virtual bool write(const std::string& name, const std::string& data) = 0;
  virtual bool read(const std::string& name, std::string& out) = 0;
  virtual bool remove(const std::string& name) = 0;
  virtual std::vector<std::string> list() = 0; };            // names only
struct DaySummary { uint32_t count; uint32_t totalSec; };
class EventStore {
 public:
  explicit EventStore(FileStore& fs);
  void load();                                               // read all files; skip unparsable
  bool add(const DowntimeEvent& e);                          // write file first, then cache
  bool setStatus(const std::string& id, EventStatus s);
  const DowntimeEvent* find(const std::string& id) const;
  std::vector<std::string> pendingIds() const;               // oldest first (by insertion seq)
  size_t pendingCount() const;
  std::vector<DowntimeEvent> history(const std::string& prodDate) const;  // newest first; includes prodDate=="" events
  DaySummary summary(const std::string& prodDate) const;     // same set as history()
  void prune(const std::string& currentProdDate, size_t maxKept = 500);   // never deletes Pending
};

enum class ThemeMode { Light, Dark, Auto };
struct Settings { std::string wifiSsid, wifiPass, baseUrl, token, machineCode = "PALLETIZER-01",
                  ntpServer = "pool.ntp.org", pin = "1234";
                  ThemeMode theme = ThemeMode::Dark; uint16_t screenOffMin = 5; bool beep = true; };
bool isValidPin(const std::string& p);                       // exactly 4 digits
bool isValidBaseUrl(const std::string& u);                   // http:// or https:// + host
std::string joinUrl(const std::string& base, const std::string& path);   // strips trailing '/' of base
bool resolveDark(ThemeMode m, int64_t nowEpoch, bool lastDark);          // Auto uses isDarkHours if clock valid
```
- File name per event: `<id>.json`. Insertion order: persist the store's own `seq` inside the stored JSON (add an `n` field via `toStorageJson`'s caller or a wrapper; decide inside the task). Order must survive a reload.

- [ ] **Step 1: Failing tests.**
```cpp
TEST(store_persist_before_cache) { MemFileStore fs; EventStore s(fs); auto e = ev("A", "2026-11-09");
  CHECK(s.add(e)); CHECK_EQ(fs.list().size(), 1u);
  EventStore s2(fs); s2.load(); CHECK_EQ(s2.pendingCount(), 1u); CHECK_EQ(s2.pendingIds()[0], "A"); }
TEST(store_order_and_status) { MemFileStore fs; EventStore s(fs);
  s.add(ev("A","2026-11-09")); s.add(ev("B","2026-11-09")); s.add(ev("C","2026-11-09"));
  s.setStatus("B", EventStatus::Sent);
  EventStore r(fs); r.load(); auto p = r.pendingIds(); CHECK_EQ(p.size(), 2u); CHECK_EQ(p[0], "A"); CHECK_EQ(p[1], "C");
  CHECK_EQ(r.history("2026-11-09")[0].id, "C"); }
TEST(store_summary) { MemFileStore fs; EventStore s(fs);
  s.add(ev("A","2026-11-09", 100)); s.add(ev("B","2026-11-09", 762)); s.add(ev("C","2026-11-08", 5));
  auto d = s.summary("2026-11-09"); CHECK_EQ(d.count, 2u); CHECK_EQ(d.totalSec, 862u); }
TEST(store_prune_keeps_pending) { MemFileStore fs; EventStore s(fs);
  s.add(ev("old_sent","2026-11-08")); s.setStatus("old_sent", EventStatus::Sent);
  s.add(ev("old_pending","2026-11-08")); s.add(ev("today","2026-11-09"));
  s.prune("2026-11-09"); CHECK(s.find("old_sent") == nullptr); CHECK(s.find("old_pending") != nullptr);
  CHECK(s.find("today") != nullptr); CHECK_EQ(fs.list().size(), 2u); }
TEST(store_skips_corrupt) { MemFileStore fs; fs.write("bad.json", "{"); EventStore s(fs); s.load(); CHECK_EQ(s.pendingCount(), 0u); }
TEST(settings_validation) { CHECK(isValidPin("1234")); CHECK(!isValidPin("123")); CHECK(!isValidPin("12a4"));
  CHECK(isValidBaseUrl("http://192.168.1.10:4500")); CHECK(!isValidBaseUrl("192.168.1.10")); CHECK(!isValidBaseUrl("http://"));
  CHECK_EQ(joinUrl("http://h:4500/", "/api/mo"), "http://h:4500/api/mo"); CHECK_EQ(Settings{}.pin, "1234"); }
TEST(theme_resolve) { CHECK(!resolveDark(ThemeMode::Light, wib(2026,11,9,22,0,0), true));
  CHECK(resolveDark(ThemeMode::Auto, wib(2026,11,9,22,0,0), false)); CHECK(!resolveDark(ThemeMode::Auto, wib(2026,11,9,10,0,0), true));
  CHECK(resolveDark(ThemeMode::Auto, 0, true)); CHECK(!resolveDark(ThemeMode::Auto, 0, false)); }
```
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run. Expected: all pass, and all earlier tests still pass.
- [ ] **Step 5:** Commit with `feat(core): event store and settings logic`.

### Task 8: API contract and local Postman mock

Load the `postman:api-mocking` skill before this task.

**Files:**
- Create: `api/openapi.yaml`, `postman/mocks/dandori/` (generated), and a README section on running the mock.

**Interfaces:**
- Produces: `GET /api/mo` (optional `tgl` query) → 200 with the body from §7.1, containing 3 MOs, one of them without `produk`. `POST /api/downtime` → 201 `{"status":"ok","id":"<echo>"}`. Error scenarios selectable at request time: `GET /api/mo` → empty `data` and → 500; `POST /api/downtime` → 500 and → 400. Use the mock's own scenario or status override mechanism, as documented by the skill.

- [ ] **Step 1:** Write `api/openapi.yaml` (OpenAPI 3.0) for both endpoints, with the request and response examples above, `bearerAuth` security (optional), and the field descriptions from §7.2. This file is also the hand-off to the backend team.
- [ ] **Step 2:** Run `postman mock generate api/openapi.yaml --output postman/mocks/dandori --port 4500`.
- [ ] **Step 3:** Run `postman mock run postman/mocks/dandori --port 4500` in the background. Check with `curl -s http://localhost:4500/api/mo?tgl=2026-11-09`: expected JSON with 3 MOs. Check with `curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: application/json" -d @<sample> http://localhost:4500/api/downtime`: expected `201`. Exercise each error scenario and confirm its status.
- [ ] **Step 4:** Check LAN reachability with the same curl against `http://<PC LAN IP>:4500/api/mo`. If the mock only binds localhost, document a port-forward workaround (`netsh interface portproxy`) in the README. Note in the README that Windows Firewall must allow inbound TCP 4500. Adding that firewall rule is the user's action.
- [ ] **Step 5:** Commit with `feat(api): OpenAPI contract and local Postman mock`.

### Task 9: Platform storage and clock

**Files:**
- Create: `src/platform/storage.h/.cpp`, `src/platform/clock.h/.cpp`
- Modify: `dandori.ino` (temporary Serial self-check)

**Interfaces:**
- Consumes: `FileStore`, `Settings`, `CounterSnapshot` (Tasks 5 and 7).
- Produces: `class LittleFsFileStore : public FileStore` (dir `/ev`, mounts LittleFS, formats on first mount failure). `bool storageBegin();`
- Produces: `Settings settingsLoad(); void settingsSave(const Settings&);` (Preferences namespace `"dandori"`). `bool counterLoad(CounterSnapshot&); void counterSave(const CounterSnapshot&);` `uint32_t nextEventSeq();` persists before returning.
- Produces: `void clockBegin(const std::string& ntpServer);` calls `configTime(7*3600, 0, ntpServer)` and is re-callable when the server changes. `int64_t nowEpoch();` `bool clockSynced();` is `isClockValid(nowEpoch())`. `int64_t lastSyncEpoch();` uses the SNTP sync callback.

- [ ] **Step 1:** Implement. Add a temporary self-check in `setup()` under `#ifdef DANDORI_SELFTEST`. It writes a dummy event via `EventStore`, reloads it, bumps `nextEventSeq()`, saves and reloads a `CounterSnapshot`, and prints `SELFTEST storage OK seq=<n>`.
- [ ] **Step 2:** Run `bash tools/build.sh` with `-DDANDORI_SELFTEST`. Expected: compile OK.
- [ ] **Step 3: Device check.** Flash, open Serial, and reset twice. Expected: `SELFTEST storage OK`, with `seq` increasing across resets, and the event count from the previous boot visible after reload.
- [ ] **Step 4:** Remove the self-check flag from the build, then commit with `feat(platform): LittleFS/NVS storage and NTP clock`.

### Task 10: Network service

**Files:**
- Create: `src/platform/net_service.h/.cpp`

**Interfaces:**
- Consumes: `Settings`, `joinUrl`, `moListPath`, `parseMoList`, `toApiJson`, `classifyHttp`, `clockBegin`.
- Produces:
```cpp
struct NetResult {
  enum Kind { MoList, SendDone, TestConn, WifiScan } kind;
  bool ok; int httpStatus; std::string id; std::vector<MoItem> mos; std::vector<std::string> ssids; std::string error;
};
void netBegin(const Settings& s);            // WiFi STA + auto-reconnect, clockBegin, start worker task (core 0)
void netApplySettings(const Settings& s);    // reconnect if SSID/pass changed; update baseUrl/token/ntp
void netRequestMoList(int64_t nowEpoch);
void netRequestSend(const std::string& id, const std::string& apiJson);
void netRequestTestConnection();             // GET /api/mo, reports status
void netRequestWifiScan();
bool netPoll(NetResult& out);                // non-blocking; call from loop()
bool netWifiConnected(); std::string netIp();
```
- Rules:
  - The worker owns all HTTP calls: `HTTPClient`, 8 s timeout. `https://` uses `WiFiClientSecure::setInsecure()` for now (§9 open item; leave a `// TODO(cert)` comment and a README note).
  - `Authorization: Bearer <token>` is sent only when the token is non-empty. `Content-Type: application/json` is set on POST.
  - Requests and results pass through FreeRTOS queues. Settings are copied into the worker under a mutex. The worker never touches LVGL or `EventStore`.
  - When WiFi goes from disconnected to connected, emit an internal flag that `loop()` uses to call `OutboxScheduler::kick()`.

- [ ] **Step 1:** Implement. In `dandori.ino`, add a temporary `DANDORI_NETTEST` path: with hard-coded WiFi and the mock URL, request the MO list and print it, then send one sample event and print the outcome.
- [ ] **Step 2:** Run `bash tools/build.sh` with `-DDANDORI_NETTEST`. Expected: compile OK.
- [ ] **Step 3: Device check, with the mock running (Task 8).** Expected on Serial: three MOs, then `SendDone ok=1 status=201`. Stop the mock and send again. Expected: `ok=0 status=-1` (or similar), and the loop keeps running.
- [ ] **Step 4:** Remove the test path and commit with `feat(platform): WiFi/HTTP worker with request/result queues`.

### Task 11: UI shell (theme, status bar, Home, navigation)

**Files:**
- Create: `src/ui/ui.h/.cpp`, `src/ui/ui_home.cpp`, `firmware/dandori/app_context.h`
- Modify: `dandori.ino` (real `setup`/`loop`)

**Interfaces:**
- Produces:
```cpp
struct AppContext { Settings& settings; DowntimeCounter& counter; EventStore& store; OutboxScheduler& outbox;
                    std::vector<MoItem>& lastMoList; bool& lastDark; };
enum class Screen { Home, MoSelect, Counting, CodeInput, Result, History, PinLock, Settings };
void uiBegin(AppContext& ctx);     // builds screens lazily
void uiGo(Screen s);
void uiTick(uint32_t nowMs);       // 1 Hz: status bar clock/WiFi/shift/queue badge, timer label, Auto theme check
void uiOnNetResult(const NetResult& r);   // dispatch to the active screen
void uiApplyTheme();               // resolveDark(...) → LVGL theme + palette tokens
```
- `loop()` order:
  1. `displayTick()`
  2. drain `netPoll` → `uiOnNetResult`. For `SendDone`: `store.setStatus` and `outbox.onResult`.
  3. `outbox.next(store.pendingIds(), millis())` → `netRequestSend`
  4. `uiTick`
  5. once per production-day change: `store.prune(...)`.
- Every `counter` state change is followed by `counterSave(counter.snapshot())`. Put that in one helper in `ui.cpp`, not in each screen.
- Status bar, all screens: machine code, MO aktif, `Shift N`, WiFi icon, clock `HH:MM` (or `--:--` if not synced), and a badge `⏳ N` when the pending count is > 0.
- Home: three tiles, Downtime / Riwayat Hari Ini / Pengaturan (mockup `launcher-setting.html`). The MO aktif row shows "Belum ada MO aktif" when the state is NoMo.
- On boot, `counterLoad` → `counter.restore(...)`. If the restored state is Running or AwaitCode, go straight to Counting or CodeInput instead of Home. If the resume was clock-valid, show a toast `Downtime dilanjutkan sejak HH:MM:SS`.
- Colors: dark palette `bg #11161c, card #1b222b, fg #e8edf2, muted #8496a8`. Light palette `bg #f3f5f8, card #ffffff, fg #18202a, muted #6a7787`. Accent `#2d6cdf`, start green `#1f9d55`, downtime red `#D63C3C`.

- [ ] **Step 1:** Implement the shell with stub screens: each non-Home screen is a title plus a back button.
- [ ] **Step 2:** Run `bash tools/build.sh`. Expected: compile OK. Run `bash tools/run_host_tests.sh`. Expected: still green.
- [ ] **Step 3: Device check.** Home renders in both themes (temporarily toggle `settings.theme`). Tiles navigate. The status bar clock appears after NTP sync, or `--:--` without internet.
- [ ] **Step 4:** Commit with `feat(ui): app shell, theme, status bar, home`.

### Task 12: Downtime flow screens

**Files:**
- Create: `src/ui/ui_downtime.cpp`, `src/ui/ui_widgets.h/.cpp`

**Interfaces:**
- Consumes: everything from Tasks 3–11.
- Produces: `lv_obj_t* makeNumericKeypad(lv_obj_t* parent, std::function<void(char)> onKey);`. Keys `1–9, ⌫, 0, HAPUS`. `⌫` maps to `'\b'` and `HAPUS` to `'c'`. This is reused by the PIN pad in Task 14.
- Screens and behavior (§4, §5.2, mockup `alur-layar.html`):
  - **MoSelect:** a spinner while loading, then a list of `mo` (plus `produk` if present), with Refresh and "Pilih MO ini".
    - Empty list: "Tidak ada MO untuk hari ini" plus Refresh.
    - Failure (parse error or HTTP ≠ 2xx): "Gagal ambil daftar MO (kode N)" plus Refresh.
    - A failure never changes the current active MO.
    - Confirming → `counter.setActiveMo` → Counting.
  - **Counting:** Ready/NoMo/Running in one screen.
    - NoMo: the big button reads `Pilih MO dulu` and opens MoSelect.
    - Ready: green `▶ START`, grey `00:00:00`, a summary line `Hari ini: N kejadian · X mnt Y dtk` from `store.summary(productionDate(now))` (or the `""` bucket when the clock is invalid), a `Ganti MO` link, and Home.
    - Running: the whole screen uses the red treatment with `⚠ MESIN BERHENTI`, a live `HH:MM:SS`, `Mulai HH:MM:SS` (or `Mulai —` without a clock), and a red `■ STOP`. `Ganti MO` and Home are hidden.
  - **CodeInput:**
    - Header: `Durasi: X mnt Y dtk (N dtk)` and the start→stop times.
    - A big code box and the keypad.
    - Problem preview `<jenis> · <mesin>` / **`<sub>`**, or "Kode tidak dikenal" in muted text.
    - `✓ KIRIM` is enabled only when `keypadValue(...) != -1`.
    - `Batal` → confirm dialog "Yakin dibuang?" → `counter.cancel()`.
    - KIRIM: build the `DowntimeEvent` (`id = makeEventId(machine, startEpoch, nextEventSeq())`, `prodDate`, `estimated`), `store.add`, `counter.complete()`, `outbox.kick()`, then go to Result.
  - **Result:** waits up to 8 s for that id's `SendDone`.
    - Sent: `✓ Data terkirim` plus a summary line.
    - Retry, timeout, or no WiFi: `Disimpan, akan dikirim ulang otomatis`.
    - Rejected: `✗ Ditolak server (kode N)`.
    - Auto-return to Counting after 3 s.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Run `bash tools/build.sh`. Expected: compile OK.
- [ ] **Step 3: Device check with the mock.**
  - Full cycle: Get MO → pick → START → wait ~10 s → STOP → type `13` (the preview shows "Loss Time · Palletizer / Dandory Produk") → KIRIM → `Data terkirim`.
  - The mock log shows the POST body with the 7 keys and correct ISO times.
  - Type `27`: KIRIM is disabled. Type `0` on empty input: nothing appears.
  - Set the mock to an empty MO list, then to MO 500: the messages and Refresh work, and the active MO is unchanged.
- [ ] **Step 4:** Commit with `feat(ui): MO select, counting, code input, result screens`.

### Task 13: Riwayat Hari Ini

**Files:**
- Create: `src/ui/ui_history.cpp`

**Interfaces:**
- Consumes: `store.history(productionDate(now))`, `findProblemCode`.
- Behavior (§5.3): rows show `HH:MM` (start, or `—`), MO, `kode · sub`, duration, and status `✓` / `⏳` / `✗`, plus `≈` if estimated. Newest first. Read-only. The header shows the production date and the summary. The view refreshes when a `SendDone` arrives while it is open.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Run `bash tools/build.sh`. Expected: compile OK.
- [ ] **Step 3: Device check.** After Task 12's cycles, the history lists them. Stop the mock, record an event, and it shows `⏳`. Restart the mock: within ≤ 60 s it flips to `✓` without user action.
- [ ] **Step 4:** Commit with `feat(ui): today's history`.

### Task 14: Pengaturan (settings, PIN lock, screen-off)

**Files:**
- Create: `src/ui/ui_settings.cpp`

**Interfaces:**
- Consumes: `settingsSave`, `netApplySettings`, `netRequestWifiScan`, `netRequestTestConnection`, `clockBegin`, `displaySetScreenOffMinutes`, `uiApplyTheme`, `makeNumericKeypad`, `isValidPin`, `isValidBaseUrl`.
- Behavior (§5.4 table, mockup `launcher-setting.html`):
  - Left menu: Tampilan, WiFi, Server API, Mesin, Jam, Keamanan, Tentang. The right pane shows the selected item.
  - Opening a locked item for the first time in this visit to Pengaturan shows the PIN pad. A correct PIN unlocks all locked items until the user leaves Pengaturan. A wrong PIN shows "PIN salah" and clears the input.
  - **Tampilan:**
    - Tema segmented control (Light/Dark/Auto), applied immediately.
    - "Layar mati setelah" dropdown with Tidak pernah / 1 / 5 / 10 / 30 menit.
    - "Bunyi tombol" switch, shown only if Task 2 found a buzzer.
    - No brightness slider, unless Task 2 found PWM backlight.
  - **WiFi:** a scan list (SSID, signal, lock icon) and the current connection with IP. Tap → password textarea with `lv_keyboard` → Simpan → `netApplySettings`.
  - **Server API:**
    - Base URL textarea, validated by `isValidBaseUrl` (otherwise "Alamat tidak valid").
    - Token textarea, masked.
    - `Tes koneksi` → shows `OK (200)` or `Gagal (kode N)`.
    - The pending count and `Kirim ulang` → `outbox.kick()`.
  - **Mesin:** machine code textarea, non-empty, uppercase.
  - **Jam:** sync status, last sync `HH:MM:SS`, NTP server textarea → `clockBegin`.
  - **Keamanan:** new PIN twice, validated by `isValidPin`, and both entries must match.
  - **Tentang:** firmware version `const char* kFwVersion = "0.1.0"`, MAC address, free heap and PSRAM, LittleFS used/total.
  - Every change goes through `settingsSave`.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Run `bash tools/build.sh`. Expected: compile OK.
- [ ] **Step 3: Device check.**
  - The wrong PIN is rejected and `1234` opens.
  - Change the PIN to `4321` and reboot: `1234` now fails and `4321` works.
  - Switching the theme recolors all screens, including the status bar.
  - Point the base URL to the mock, run Tes koneksi, and it shows OK.
  - Change WiFi via scan and keyboard: it reconnects.
  - Screen-off at 1 min: the backlight turns off, and the first tap only wakes the screen.
- [ ] **Step 4:** Commit with `feat(ui): settings with PIN lock`.

### Task 15: Acceptance on device and README

**Files:**
- Create or modify: `README.md` (setup: libraries + lv_conf copy, board options, build/flash, mock run, firewall note, first-boot defaults, HTTPS TODO), `docs/acceptance-checklist.md`

- [ ] **Step 1:** Write `docs/acceptance-checklist.md` containing exactly the scenarios below, each with an expected result. Then run them with the user on the device and the mock:
  1. **Reboot mid-timer, clock synced:** START, wait 30 s, press reset. The device returns to Counting/Running with ≈ 30 s+ and the toast `Downtime dilanjutkan sejak …`. STOP and KIRIM: `total_detik` covers the whole span.
  2. **Reboot mid-timer, no clock:** set the NTP server to an invalid host, unplug internet (keep the LAN), and reboot. Counting resumes from 0 and the history row shows `≈`.
  3. **Reboot during CodeInput:** the device returns to CodeInput with the same duration.
  4. **Offline send:** stop the mock and record 3 events: 3 × `⏳` and the badge reads `⏳ 3`. Reboot, then start the mock: all 3 arrive once each. Check the mock log for unique ids and no duplicates.
  5. **Mock returns 400 on POST:** the event shows `✗` and is not retried. The mock log shows exactly one request.
  6. **No NTP:** Get MO calls `/api/mo` without `tgl`, and the POST has `start: null`, `stop: null`, and a correct `total_detik`.
  7. **Production day:** the device clock cannot be set by hand, so this is covered by the host tests (Task 4) and recorded as "host-verified".
  8. **Accidental Start:** START → STOP → Batal → confirm. Nothing is stored or sent.
  9. **Ganti MO locked while running;** after STOP + KIRIM, Ganti MO works and the next event carries the new MO (the dandori code 13 flow from §4.2).
  10. **Theme Auto:** after 18:00 WIB the next minute tick switches to dark.
- [ ] **Step 2:** Record pass or fail per item in the checklist. Fix failures under the task that owns the code, then re-run that item.
- [ ] **Step 3:** Run `bash tools/run_host_tests.sh` and `bash tools/build.sh`. Expected: all green and compile OK.
- [ ] **Step 4:** Commit with `docs: README and acceptance checklist results`.

---

## Notes for the executor

- Tasks 2, 9–15 need the physical board. The agent can compile and (if the board is connected) upload and read Serial, but **someone has to look at the screen and tap it**, so pause at each "Device check" step and ask the user.
- Ask the user for the board's COM port, and WiFi credentials for the test network. **Never put real credentials or tokens in committed files.** Hard-coded values in the temporary `DANDORI_NETTEST` path must be removed before commit (Task 10, Step 4).
- `tools/build.sh` writes `lv_conf.h` into the user's global Arduino `libraries/` folder. It currently has none. Tell the user, because other LVGL sketches on this PC will use it too.
