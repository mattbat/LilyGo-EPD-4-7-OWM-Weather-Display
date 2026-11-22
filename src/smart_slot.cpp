#include "smart_slot.h"
#include "timefmt.h"
#include "charts.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

namespace {
void set(char* dst, size_t n, const char* s) { snprintf(dst, n, "%s", s); }

int strikes3h(const WeatherData& d) {
  if (d.have & HAVE_OBS) return d.cur.strikes3h;
  // no station observation: fall back to the newest three history hours
  int s = 0;
  if (d.have & HAVE_HISTORY) for (int i = HIST_BUCKETS - 3; i < HIST_BUCKETS; i++) s += d.hist[i].strikes;
  return s;
}
}  // namespace

SmartSlot pickSmartSlot(const WeatherData& d, int64_t now) {
  SmartSlot s;
  char b[32];

  // 1. lightning in the last 3 h
  int n = strikes3h(d);
  if (n > 0) {
    s.kind = SlotKind::Lightning; s.icon = SlotIcon::Bolt;
    set(s.label, sizeof s.label, "LIGHTNING");
    snprintf(s.big, sizeof s.big, "%d", n);
    set(s.sub1, sizeof s.sub1, "strikes \xC2\xB7 3 h");
    if (isfinite(d.cur.lastStrikeMi) && d.cur.lastStrikeMi >= 0) {
      snprintf(s.sub2, sizeof s.sub2, "nearest %d mi", (int)lroundf(d.cur.lastStrikeMi));
    }
    return s;
  }

  // 2. raining now
  float rate = d.cur.rainRateInHr;
  if (isfinite(rate) && rate > 0) {
    s.kind = SlotKind::Raining; s.icon = SlotIcon::Rain;
    set(s.label, sizeof s.label, "RAINING");
    snprintf(s.big, sizeof s.big, "%.2f", rate);
    set(s.sub1, sizeof s.sub1, "in / hr");
    set(s.sub2, sizeof s.sub2, rate < 0.1f ? "light" : rate < 0.4f ? "moderate" : "heavy");
    return s;
  }

  // 3. high UV
  float uv = d.cur.uv;
  if (isfinite(uv) && uv >= SLOT_UV_MIN) {
    s.kind = SlotKind::UV; s.icon = SlotIcon::Sun;
    set(s.label, sizeof s.label, "UV");
    snprintf(s.big, sizeof s.big, "%d", (int)lroundf(uv));
    set(s.sub1, sizeof s.sub1, uv >= 11 ? "extreme" : uv >= 8 ? "very high" : "high");
    set(s.sub2, sizeof s.sub2, "sun protection");
    return s;
  }

  // 4. next rain from the hourly forecast
  set(s.label, sizeof s.label, "NEXT RAIN");
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    int64_t t = (int64_t)h.time;
    if (t + 3600 <= now) continue;                                   // hour already over
    if (t - now > (int64_t)SLOT_NEXT_RAIN_HOURS * 3600) break;
    if (h.precipProbPct < SLOT_NEXT_RAIN_PCT) continue;
    s.kind = SlotKind::NextRain; s.icon = SlotIcon::Rain;
    struct tm lt;
    if (sameLocalDate(t, now)) set(s.big, sizeof s.big, "Today");
    else if (localTm(t, &lt)) set(s.big, sizeof s.big, dowName(lt.tm_wday));
    formatHourLabel(b, sizeof b, localHourOfEpoch((uint32_t)t));
    set(s.sub1, sizeof s.sub1, b);
    snprintf(s.sub2, sizeof s.sub2, "%d%% chance", (int)h.precipProbPct);
    return s;
  }

  // 5. nothing
  s.kind = SlotKind::NoRain; s.icon = SlotIcon::None;
  set(s.big, sizeof s.big, "None");
  set(s.sub1, sizeof s.sub1, "No rain in");
  set(s.sub2, sizeof s.sub2, "3 days");
  return s;
}
