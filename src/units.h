// Portable unit conversions.
#pragma once
float cToF(float c);
float msToMph(float ms);
float mmToIn(float mm);
float mbToInHg(float mb);
float kmToMi(float km);
// 16-point compass name for degrees (any real value, wraps; NAN -> "--").
const char* degToCompass(float deg);
