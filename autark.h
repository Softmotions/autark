#ifndef AUTARK_H
#define AUTARK_H

#ifndef _AMALGAMATE_
#include <stdbool.h>
#endif

/// Exit code of `autark fetch-plan` telling that a resource fetch is required.
#define AUTARK_FETCH_PENDING_RC 3

/// Exit code of `autark fetch-plan` telling that the fetch dir is unusable.
#define AUTARK_FETCH_REJECTED_RC 4

void autark_init(void);

void autark_run(int argc, const char **argv);

void autark_dispose(void);

void autark_build_prepare(const char *script_path);

#endif
