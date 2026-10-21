#include <stdio.h>

int generated_value(void);
int dep_value(void);

int main(void) {
  int value = generated_value() + dep_value();
  printf("value=%d\n", value);
  return value == 49 ? 0 : 1;
}
