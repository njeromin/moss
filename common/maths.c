#include <common/maths.h>

size_t math_min_s(size_t a, size_t b) {
  if (b < a) {
    return b;
  }

  return a;
}
