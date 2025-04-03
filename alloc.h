#ifndef ALLOC_H
#define ALLOC_H

#include "basedefs.h"
#include "log.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

AK_ALLOC static inline char* xstrdup(const char *str) {
  char *ret = strdup(str);
  if (!ret) {
    akfatal2("Allocation failed");
  }
  return ret;
}

AK_ALLOC static inline void* xmalloc(size_t size) {
  void *ptr = malloc(size);
  if (!ptr) {
    akfatal2("Allocation failed");
  }
  return ptr;
}

AK_ALLOC static inline void* xrealloc(void *p, size_t size) {
  void *ptr = realloc(p, size);
  if (!ptr) {
    akfatal2("Allocation failed");
  }
  return ptr;
}

AK_ALLOC static inline void* xcalloc(size_t nmemb, size_t size) {
  void *ptr = calloc(nmemb, size);
  if (!ptr) {
     akfatal2("Allocation failed");
  }
  return ptr;
}

#endif
