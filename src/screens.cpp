#include "screens.h"
// TODO(task 5): implement per docs/mockups.html.
void drawLive(Gfx& g, const WeatherData&, const ScreenStatus&) { g.clear(); }
void drawOvernight(Gfx& g, const WeatherData&, const ScreenStatus&, OvernightTitles) { g.clear(); }
void drawChargeMe(Gfx& g, const ScreenStatus&) { g.clear(); }
void drawError(Gfx& g, const char*, const ScreenStatus&) { g.clear(); }
