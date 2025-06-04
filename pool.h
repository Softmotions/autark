#ifndef POOL_H
#define POOL_H

#ifndef _AMALGAMATE_
#include <stddef.h>
#include <stdarg.h>
#endif

struct  pool_unit {
  void *heap;
  struct pool_unit *next;
};

struct pool {
  size_t usiz;                            /// Used size
  size_t asiz;                            /// Allocated size
  struct pool_unit *unit;                 /// Current heap unit
  void *user_data;                        /// Associated user data
  char *heap;                             /// Current pool heap ptr
  void  (*on_pool_destroy)(struct pool*); /// Called when pool destroyed
};

struct pool* pool_create_empty(void);

struct pool* pool_create(void (*)(struct pool*));

void pool_destroy(struct pool*);

void* pool_alloc(struct pool*, size_t siz);

void* pool_calloc(struct pool*, size_t siz);

char* pool_strdup(struct pool*, const char*);

char* pool_strndup(struct pool*, const char*, size_t len);

char* pool_printf_va(struct pool*, const char *format, va_list va);

char* pool_printf(struct pool*, const char*, ...) __attribute__((format(__printf__, 2, 3)));

const char** pool_split_string(
  struct pool *pool,
  const char  *haystack,
  const char  *split_chars,
  int          ignore_whitespace);

const char** pool_split(
  struct pool*, const char *split_chars, int ignore_ws, const char *fmt,
  ...) __attribute__((format(__printf__, 4, 5)));

#endif
