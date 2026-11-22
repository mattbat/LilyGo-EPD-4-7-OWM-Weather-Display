// Native preview: will render every screen from test/fixtures to out/*.png (task 2+).
// Excluded when building unit tests (they bring their own main).
#ifndef PIO_UNIT_TESTING
#include <stdio.h>
int main() {
  printf("native preview scaffold\n");
  return 0;
}
#endif
