#include <stdlib.h>

static unsigned int seed=1;

int random(void);
void srandom(unsigned int i);

int rand(void) {
  return rand_r(&seed);
}

void srand(unsigned int i) { seed=i?i:23; }

int random(void) {
  return rand();
}

void srandom(unsigned int i) {
  srand(i);
}
