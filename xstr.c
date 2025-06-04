#ifndef _AMALGAMATE_
#include "xstr.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#endif

struct xstr* xstr_create(size_t siz) {
  if (!siz) {
    siz = 16;
  }
  struct xstr *xstr;
  xstr = xmalloc(sizeof(*xstr));
  xstr->ptr = xmalloc(siz);
  xstr->size = 0;
  xstr->asize = siz;
  xstr->ptr[0] = '\0';
  return xstr;
}

struct  xstr* xstr_create_empty(void) {
  return xstr_create(0);
}

void xstr_cat2(struct xstr *xstr, const void *buf, size_t size) {
  size_t nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  memcpy(xstr->ptr + xstr->size, buf, size);
  xstr->size += size;
  xstr->ptr[xstr->size] = '\0';
}

void xstr_cat(struct xstr *xstr, const char *buf) {
  size_t size = strlen(buf);
  xstr_cat2(xstr, buf, size);
}

void xstr_shift(struct xstr *xstr, size_t shift_size) {
  if (shift_size == 0) {
    return;
  }
  if (shift_size > xstr->size) {
    shift_size = xstr->size;
  }
  if (xstr->size > shift_size) {
    memmove(xstr->ptr, xstr->ptr + shift_size, xstr->size - shift_size);
  }
  xstr->size -= shift_size;
  xstr->ptr[xstr->size] = '\0';
}

void xstr_unshift(struct xstr *xstr, const void *buf, size_t size) {
  unsigned nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  if (xstr->size) {
    // shift to right
    memmove(xstr->ptr + size, xstr->ptr, xstr->size);
  }
  memcpy(xstr->ptr, buf, size);
  xstr->size += size;
  xstr->ptr[xstr->size] = '\0';
}

void xstr_pop(struct xstr *xstr, size_t pop_size) {
  if (pop_size == 0) {
    return;
  }
  if (pop_size > xstr->size) {
    pop_size = xstr->size;
  }
  xstr->size -= pop_size;
  xstr->ptr[xstr->size] = '\0';
}

void xstr_insert(struct xstr *xstr, size_t pos, const void *buf) {
  if (pos > xstr->size) {
    return;
  }
  unsigned size = strlen(buf);
  if (size == 0) {
    return;
  }
  size_t nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  memmove(xstr->ptr + pos + size, xstr->ptr + pos, xstr->size - pos + 1 /* \0 */);
  memcpy(xstr->ptr + pos, buf, size);
  xstr->size += size;
}

int xstr_printf_va(struct xstr *xstr, const char *format, va_list va) {
  int rc = 0;
  char buf[1024];
  va_list cva;
  va_copy(cva, va);
  char *wp = buf;
  int len = vsnprintf(wp, sizeof(buf), format, va);
  if (len >= sizeof(buf)) {
    wp = xmalloc(len + 1);
    len = vsnprintf(wp, len + 1, format, cva);
    if (len < 0) {
      rc = EINVAL;
      goto finish;
    }
  }
  xstr_cat2(xstr, wp, len);

finish:
  va_end(cva);
  if (wp != buf) {
    free(wp);
  }
  return rc;
}

int xstr_printf(struct xstr *xstr, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int rc = xstr_printf_va(xstr, format, ap);
  va_end(ap);
  return rc;
}

struct xstr* xstr_clear(struct xstr *xstr) {
  xstr->size = 0;
  xstr->ptr[0] = '\0';
  return xstr;
}

char* xstr_ptr(struct xstr *xstr) {
  return xstr->ptr;
}

size_t xstr_size(struct xstr *xstr) {
  return xstr->size;
}

void xstr_destroy(struct xstr *xstr) {
  if (!xstr) {
    return;
  }
  free(xstr->ptr);
  free(xstr);
}

char* xstr_destroy_keep_ptr(struct xstr *xstr) {
  if (!xstr) {
    return 0;
  }
  char *ptr = xstr->ptr;
  free(xstr);
  return ptr;
}

void xstr_cat_repeat(struct xstr *xstr, char ch, int count) {
  for (int i = count; i > 0; --i) {
    char buf[] = { ch };
    xstr_cat2(xstr, buf, 1);
  }
}
