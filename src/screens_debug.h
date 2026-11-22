// Test/debug hooks of screens.cpp. Every string the screens draw goes through one wrapper that
// refuses "nan"/"inf" tokens (draws "--" instead) and counts them here.
#pragma once
int screenTextAnomalies();          // number of nan/inf strings intercepted since the last reset
const char* screenLastAnomaly();    // the most recent offending string
void screenTextAnomaliesReset();
