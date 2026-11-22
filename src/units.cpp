#include "units.h"
#include <math.h>
float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
float msToMph(float ms) { return ms * 2.2369363f; }
float mmToIn(float mm) { return mm / 25.4f; }
float mbToInHg(float mb) { return mb * 0.0295300f; }
float kmToMi(float km) { return km * 0.6213712f; }
const char* degToCompass(float deg) {
  static const char* n[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
  if (isnan(deg)) return "--";
  float d = fmodf(deg, 360.0f);
  if (d < 0) d += 360.0f;
  int i = (int)((d + 11.25f) / 22.5f) % 16;
  return n[i];
}
