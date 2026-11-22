// Small portable local-time formatting helpers (no strftime / locale). Local time comes from the C
// library's localtime(), so the TZ environment variable must be set (main.cpp does; native tools and
// tests set it to TZ_POSIX).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

inline bool localTm(int64_t epoch, struct tm* out) {
  time_t t = (time_t)epoch;
  struct tm* p = localtime(&t);
  if (!p) { memset(out, 0, sizeof *out); return false; }
  *out = *p;
  return true;
}
inline const char* dowName(int wday) {
  static const char* n[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return n[((wday % 7) + 7) % 7];
}
inline const char* monName(int mon) {
  static const char* n[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return n[((mon % 12) + 12) % 12];
}
// "2:05 PM". epoch <= 0 -> "--".
inline void fmtClock(char* buf, size_t n, int64_t epoch) {
  struct tm t;
  if (epoch <= 0 || !localTm(epoch, &t)) { snprintf(buf, n, "--"); return; }
  int h = t.tm_hour % 12; if (h == 0) h = 12;
  snprintf(buf, n, "%d:%02d %s", h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
}
// "6:58" (no AM/PM, for sunrise/sunset). epoch <= 0 -> "--".
inline void fmtClockShort(char* buf, size_t n, int64_t epoch) {
  struct tm t;
  if (epoch <= 0 || !localTm(epoch, &t)) { snprintf(buf, n, "--"); return; }
  int h = t.tm_hour % 12; if (h == 0) h = 12;
  snprintf(buf, n, "%d:%02d", h, t.tm_min);
}
// "Sat, Nov 21". epoch <= 0 -> "--".
inline void fmtDate(char* buf, size_t n, int64_t epoch) {
  struct tm t;
  if (epoch <= 0 || !localTm(epoch, &t)) { snprintf(buf, n, "--"); return; }
  snprintf(buf, n, "%s, %s %d", dowName(t.tm_wday), monName(t.tm_mon), t.tm_mday);
}
inline bool sameLocalDate(int64_t a, int64_t b) {
  struct tm x, y;
  if (!localTm(a, &x) || !localTm(b, &y)) return false;
  return x.tm_year == y.tm_year && x.tm_yday == y.tm_yday;
}
inline void upperAscii(char* s) {
  for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}
