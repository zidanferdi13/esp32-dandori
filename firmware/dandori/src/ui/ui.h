// Public UI entry points used by dandori.ino.
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../platform/net_service.h"

enum class Screen { Home, MoSelect, Counting, CodeInput, Result, History, PinLock, Settings };

void uiBegin(AppContext& ctx);  // restores the counter snapshot and shows the first screen
void uiGo(Screen s);
void uiTick(uint32_t nowMs);    // status bar, timers, Auto theme, deferred counter restore
void uiOnNetResult(const NetResult& r);
void uiApplyTheme();            // re-resolves Light/Dark/Auto and rebuilds the current screen
