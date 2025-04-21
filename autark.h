#ifndef AUTARK_H
#define AUTARK_H

#include <stdbool.h>

void autark_init(void);

void autark_run(int argc, const char **argv);

void autark_dispose(void);

void autark_build_prepare(const char *script_path);

#endif
