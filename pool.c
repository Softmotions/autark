#include "basedefs.h"
#include "pool.h"
#include "utils.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>

#define _UNIT_ALIGN_SIZE 8UL

struct pool* pool_create_empty(void) {
  struct pool *p = xcalloc(1, sizeof(*p));
  return p;
}

void pool_destroy(struct pool *pool) {
  if (!pool) {
    return;
  }
  for (struct pool_unit *u = pool->unit, *next; u; u = next) {
    next = u->next;
    free(u->heap);
    free(u);
  }
  free(pool);
}

static int _extend(struct pool *pool, size_t siz) {
  struct pool_unit *nunit = xmalloc(sizeof(*nunit));
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  nunit->heap = xmalloc(siz);
  nunit->next = pool->unit;
  pool->heap = nunit->heap;
  pool->unit = nunit;
  pool->usiz = 0;
  pool->asiz = siz;
  return 1;
}

void* pool_alloc(struct pool *pool, size_t siz) {
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  size_t usiz = pool->usiz + siz;
  void *h = pool->heap;
  if (usiz > pool->asiz) {
    usiz = usiz + pool->asiz;
    if (!_extend(pool, usiz)) {
      return 0;
    }
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

const char* pool_strdup(struct pool *pool, const char *str) {
  size_t len = strlen(str);
  char *ret = pool_alloc(pool, len + 1);
  memcpy(ret, str, len);
  ret[len] = '\0';
  return ret;
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

const char** pool_split_string(
  struct pool *pool,
  const char  *haystack,
  const char  *split_chars,
  int          ignore_whitespace) {
  size_t hsz = strlen(haystack);
  const char **ret = (const char**) pool_alloc(pool, (hsz + 1) * sizeof(char*));
  const char *sp = haystack;
  const char *ep = sp;
  int j = 0;
  for (int i = 0; *ep; ++i, ++ep) {
    const char ch = haystack[i];
    const char *sch = strchr(split_chars, ch);
    if ((ep >= sp) && (sch || (*(ep + 1) == '\0'))) {
      if (!sch && (*(ep + 1) == '\0')) {
        ++ep;
      }
      if (ignore_whitespace) {
        while (utils_char_is_space(*sp)) ++sp;
        while (utils_char_is_space(*(ep - 1))) --ep;
      }
      if (ep >= sp) {
        char *s = pool_alloc(pool, ep - sp + 1);
        memcpy(s, sp, ep - sp);
        s[ep - sp] = '\0';
        ret[j++] = s;
        ep = haystack + i;
      }
      sp = haystack + i + 1;
    }
  }
  ret[j] = 0;
  return ret;
}
