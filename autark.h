#ifndef AUTARK_H
#define AUTARK_H

#ifndef _AMALGAMATE_
#include <stdbool.h>
#endif

void autark_init(void);

void autark_run(int argc, const char **argv);

void autark_dispose(void);

void autark_build_prepare(const char *script_path);

#endif
