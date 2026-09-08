#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "pool.h"
#include "utils.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>
#endif

#define _UNIT_ALIGN_SIZE 8UL

static void _extend(struct pool *pool, size_t siz);

struct pool* pool_create_empty(void) {
  return xcalloc(1, sizeof(struct pool));
}

struct pool* pool_create_preallocated(size_t sz) {
  struct pool *pool = pool_create_empty();
  _extend(pool, sz);
  return pool;
}

struct pool* pool_create(void (*on_pool_destroy)(struct pool*)) {
  struct pool *pool = pool_create_empty();
  pool->on_pool_destroy = on_pool_destroy;
  return pool;
}

void pool_destroy(struct pool *pool) {
  if (!pool) {
    return;
  }
  if (pool->on_pool_destroy) {
    pool->on_pool_destroy(pool);
  }
  for (struct pool_unit *u = pool->unit, *next; u; u = next) {
    next = u->next;
    free(u->heap);
    free(u);
  }
  free(pool);
}

static void _extend(struct pool *pool, size_t siz) {
  struct pool_unit *nunit = xmalloc(sizeof(*nunit));
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  nunit->heap = xmalloc(siz);
  nunit->next = pool->unit;
  pool->heap = nunit->heap;
  pool->unit = nunit;
  pool->usiz = 0;
  pool->asiz = siz;
}

void* pool_alloc(struct pool *pool, size_t siz) {
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  size_t usiz = pool->usiz + siz;
  void *h = pool->heap;
  if (usiz > pool->asiz) {
    usiz = usiz + pool->asiz;
    _extend(pool, usiz);
    h = pool->heap;
  }
  pool->usiz += siz;
  pool->heap += siz;
  return h;
}

void* pool_calloc(struct pool *pool, size_t siz) {
  void *p = pool_alloc(pool, siz);
  memset(p, 0, siz);
  return p;
}

char* pool_strdup(struct pool *pool, const char *str) {
  if (str) {
    size_t len = strlen(str);
    char *ret = pool_alloc(pool, len + 1);
    memcpy(ret, str, len);
    ret[len] = '\0';
    return ret;
  } else {
    return 0;
  }
}

char* pool_strndup(struct pool *pool, const char *str, size_t len) {
  if (str) {
    len = utils_strnlen(str, len);
    char *ret = pool_alloc(pool, len + 1);
    memcpy(ret, str, len);
    ret[len] = '\0';
    return ret;
  } else {
    return 0;
  }
}

static inline int _printf_estimate_size(const char *format, va_list ap) {
  char buf[1];
  return vsnprintf(buf, sizeof(buf), format, ap) + 1;
}

static char* _printf_va(struct pool *pool, unsigned size, const char *format, va_list ap) {
  char *wbuf = pool_alloc(pool, size);
  if (!wbuf) {
    return 0;
  }
  vsnprintf(wbuf, size, format, ap);
  return wbuf;
}

char* pool_printf_va(struct pool *pool, const char *format, va_list va) {
  va_list cva;
  va_copy(cva, va);
  int size = _printf_estimate_size(format, va);
  va_end(va);
  char *res = _printf_va(pool, size, format, cva);
  va_end(cva);
  return res;
}

char* pool_printf(struct pool *pool, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int size = _printf_estimate_size(fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  char *res = _printf_va(pool, size, fmt, ap);
  va_end(ap);
  return res;
}

