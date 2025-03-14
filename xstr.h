#ifndef XSTR_H
#define XSTR_H

#include <stdarg.h>
#include <stddef.h>

struct xstr {
  char  *ptr;
  size_t size;
  size_t asize;
};

struct xstr* xstr_create(size_t siz);

struct  xstr* xstr_create_empty(void);

void xstr_cat2(struct xstr*, const void *buf, size_t size);

void xstr_cat(struct xstr*, const char *buf);

void xstr_shift(struct xstr*, size_t shift_size);

void xstr_unshift(struct xstr*, const void *buf, size_t size);

void xstr_pop(struct xstr*, size_t pop_size);

void xstr_insert(struct xstr*, size_t pos, const void *buf);

int xstr_printf_va(struct xstr*, const char *format, va_list va);

int xstr_printf(struct xstr*, const char *format, ...) __attribute__((format(__printf__, 2, 3)));

void xstr_clear(struct xstr*);

char* xstr_ptr(struct xstr*);

size_t xstr_size(struct xstr*);

void xstr_destroy(struct xstr*);

char* xstr_destroy_keep_ptr(struct xstr*);

#endif
