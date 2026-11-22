#include "alert.h"
#include <stdio.h>
#include <math.h>

namespace {
int strikes3h(const WeatherData& d) {
  if (d.have & HAVE_OBS) return d.cur.strikes3h;
  // no station observation: fall back to the newest three history hours
  int s = 0;
  if (d.have & HAVE_HISTORY) for (int i = HIST_BUCKETS - 3; i < HIST_BUCKETS; i++) s += d.hist[i].strikes;
  return s;
}
}  // namespace

Alert pickAlert(const WeatherData& d) {
  Alert a;

  // 1. lightning in the last 3 h
  int n = strikes3h(d);
  if (n > 0) {
    a.kind = AlertKind::Lightning;
    snprintf(a.headline, sizeof a.headline, "%d lightning strike%s", n, n == 1 ? "" : "s");
    if (isfinite(d.cur.lastStrikeMi) && d.cur.lastStrikeMi >= 0)
      snprintf(a.sub, sizeof a.sub, "in the last 3 hours \xC2\xB7 nearest %d mi", (int)lroundf(d.cur.lastStrikeMi));
    else
      snprintf(a.sub, sizeof a.sub, "in the last 3 hours");
    return a;
  }

  // 2. raining now
  float rate = d.cur.rainRateInHr;
  if (isfinite(rate) && rate > 0) {
    a.kind = AlertKind::Raining;
    snprintf(a.headline, sizeof a.headline, "Raining %.2f in/hr", rate);
    float today = isfinite(d.cur.rainTodayIn) ? d.cur.rainTodayIn : (d.today.valid ? d.today.rainIn : NAN);
    if (isfinite(today)) snprintf(a.sub, sizeof a.sub, "%.2f in so far today", today);
    return a;
  }

  // 3. high UV
  float uv = d.cur.uv;
  if (isfinite(uv) && uv >= ALERT_UV_MIN) {
    a.kind = AlertKind::UV;
    int u = (int)lroundf(uv);
    snprintf(a.headline, sizeof a.headline, "UV %d \xC2\xB7 %s", u, u >= 11 ? "extreme" : u >= 8 ? "very high" : "high");
    snprintf(a.sub, sizeof a.sub, "limit time in the sun");
    return a;
  }
  return a;   // None
}
