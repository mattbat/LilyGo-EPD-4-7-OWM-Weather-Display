// Test/debug hooks of screens.cpp. Every string the screens draw goes through one wrapper that
// refuses "nan"/"inf" tokens (draws "--" instead) and counts them here.
#pragma once
int screenTextAnomalies();          // number of nan/inf strings intercepted since the last reset
const char* screenLastAnomaly();    // the most recent offending string
void screenTextAnomaliesReset();
// x pixel of an epoch on the live chart whose "now" (the current reading) is nowT: true time over [now - 6 h, now + 18 h]
#include <stdint.h>
int liveChartX(int64_t epoch, int64_t nowT);
