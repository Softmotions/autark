#ifndef AUTARK_H
#define AUTARK_H

#include <stdbool.h>

void autark_init(void);

void autark_run(int argc, char **argv);

void autark_dispose(void);

void autark_build_prepare(bool clean_cache, const char *script_path);

#endif
