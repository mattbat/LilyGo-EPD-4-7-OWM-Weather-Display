#include "units.h"
float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
float msToMph(float ms) { return ms * 2.2369363f; }
float mmToIn(float mm) { return mm / 25.4f; }
float mbToInHg(float mb) { return mb * 0.0295300f; }
const char* degToCompass(float deg) {
  static const char* n[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
  int i = (int)((deg + 11.25f) / 22.5f) % 16;
  if (i < 0) i += 16;
  return n[i];
}
