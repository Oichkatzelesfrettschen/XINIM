#include <math.h>

int __finite(double d);

int finite(double d) {
  return isinf(d)==0 && isnan(d)==0;
}

int __finite(double d) {
  return finite(d);
}
