#ifndef FETCHREG_H
#define FETCHREG_H

#ifndef _AMALGAMATE_
#include <stdbool.h>
#endif

struct fetchreg;
struct fetcherg_entry {
  const char *url;
  const char *target;
};

int fetchreg_open(const char *path, struct fetchreg **out);

bool fetchreg_find(struct fetchreg*, const char *url, void *user_data, void (*cb)(const struct fetcherg_entry*, void*));

int fetchreg_register(struct fetchreg*, const struct fetcherg_entry*);

void fetchreg_close(struct fetchreg*);

#endif
