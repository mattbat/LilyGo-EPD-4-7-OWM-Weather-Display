#include "screens.h"
// TODO(task 5): implement per docs/mockups.html.
void drawLive(Gfx& g, const WeatherData&, bool, int64_t) { g.clear(); }
void drawOvernight(Gfx& g, const WeatherData&, bool) { g.clear(); }
void drawChargeMe(Gfx& g, float, int64_t) { g.clear(); }
void drawError(Gfx& g, const char*, int64_t) { g.clear(); }
