#include "ulist.h"
#include "alloc.h"
#include "xstr.h"

#include <string.h>

#define _ALLOC_UNIT 32

struct ulist* ulist_create(unsigned initial_len, unsigned unit_size) {
  struct ulist *list = xmalloc(sizeof(*list));
  ulist_init(list, initial_len, unit_size);
  return list;
}

void ulist_init(struct ulist *list, unsigned ini_length, unsigned unit_size) {
  list->usize = unit_size;
  list->num = 0;
  list->start = 0;
  if (!ini_length) {
    ini_length = _ALLOC_UNIT;
  }
  list->anum = ini_length;
  list->array = xmalloc((size_t) unit_size * ini_length);
}

void ulist_destroy(struct ulist **lp) {
  if (lp && *lp) {
    ulist_destroy_keep(*lp);
    free(*lp);
    *lp = 0;
  }
}

void ulist_destroy_keep(struct ulist *list) {
  if (list) {
    free(list->array);
    memset(list, 0, sizeof(*list));
  }
}

void ulist_clear(struct ulist *list) {
  if (list) {
    free(list->array);
    ulist_init(list, _ALLOC_UNIT, list->usize);
  }
}

void ulist_reset(struct ulist *list) {
  if (list) {
    list->num = 0;
    list->start = 0;
  }
}

void* ulist_get(const struct ulist *list, unsigned idx) {
  if (idx >= list->num) {
    return 0;
  }
  idx += list->start;
  return list->array + (size_t) idx * list->usize;
}

void* ulist_peek(const struct ulist *list) {
  return list->num ? ulist_get(list, list->num - 1) : 0;
}

void ulist_insert(struct ulist *list, unsigned idx, const void *data) {
  if (idx > list->num) {
    return;
  }
  idx += list->start;
  if (list->start + list->num >= list->anum) {
    size_t anum = list->anum + list->num + 1;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  memmove(list->array + ((size_t) idx + 1) * list->usize,
          list->array + (size_t) idx * list->usize,
          (list->start + (size_t) list->num - idx) * list->usize);
  memcpy(list->array + (size_t) idx * list->usize, data, list->usize);
  ++list->num;
}

void ulist_set(struct ulist *list, unsigned idx, const void *data) {
  if (idx >= list->num) {
    return;
  }
  idx += list->start;
  memcpy(list->array + (size_t) idx * list->usize, data, list->usize);
}

void ulist_remove(struct ulist *list, unsigned idx) {
  if (idx >= list->num) {
    return;
  }
  idx += list->start;
  --list->num;
  memmove(list->array + (size_t) idx * list->usize, list->array + ((size_t) idx + 1) * list->usize,
          (list->start + (size_t) list->num - idx) * list->usize);
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= list->num * 2)) {
    if (list->start) {
      memmove(list->array, list->array + (size_t) list->start * list->usize, (size_t) list->num * list->usize);
      list->start = 0;
    }
    size_t anum = list->num > _ALLOC_UNIT ? list->num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
}

void ulist_push(struct ulist *list, const void *data) {
  size_t idx = list->start + list->num;
  if (idx >= list->anum) {
    size_t anum = list->anum + list->num + 1;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  memcpy(list->array + idx * list->usize, data, list->usize);
  ++list->num;
}

void ulist_pop(struct ulist *list) {
  if (!list->num) {
    return;
  }
  size_t num = list->num - 1;
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= num * 2)) {
    if (list->start) {
      memmove(list->array, list->array + (size_t) list->start * list->usize, num * list->usize);
      list->start = 0;
    }
    size_t anum = num > _ALLOC_UNIT ? num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  list->num = num;
}

void ulist_unshift(struct ulist *list, const void *data) {
  if (!list->start) {
    if (list->num >= list->anum) {
      size_t anum = list->anum + list->num + 1;
      void *nptr = xrealloc(list->array, anum * list->usize);
      list->anum = anum;
      list->array = nptr;
    }
    list->start = list->anum - list->num;
    memmove(list->array + (size_t) list->start * list->usize, list->array, (size_t) list->num * list->usize);
  }
  memcpy(list->array + ((size_t) list->start - 1) * list->usize, data, list->usize);
  --list->start;
  ++list->num;
}

void ulist_shift(struct ulist *list) {
  if (!list->num) {
    return;
  }
  size_t num = list->num - 1;
  size_t start = list->start + 1;
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= num * 2)) {
    if (start) {
      memmove(list->array, list->array + start * list->usize, num * list->usize);
      start = 0;
    }
    size_t anum = num > _ALLOC_UNIT ? num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  list->start = start;
  list->num = num;
}

struct ulist* ulist_clone(const struct ulist *list) {
  if (!list->num) {
    return ulist_create(list->anum, list->usize);
  }
  struct ulist *nlist = xmalloc(sizeof(*nlist));
  unsigned anum = list->num > _ALLOC_UNIT ? list->num : _ALLOC_UNIT;
  nlist->array = xmalloc(anum * list->usize);
  memcpy(nlist->array, list->array + list->start, list->num * list->usize);
  nlist->usize = list->usize;
  nlist->num = list->num;
  nlist->anum = anum;
  nlist->start = 0;
  return nlist;
}

int ulist_find(const struct ulist *list, const void *data) {
  for (unsigned i = list->start; i < list->start + list->num; ++i) {
    void *ptr = list->array + i * list->usize;
    if (memcmp(data, ptr, list->usize) == 0) {
      return i - list->start;
    }
  }
  return -1;
}

int ulist_remove_by(struct ulist *list, const void *data) {
  for (unsigned i = list->start; i < list->start + list->num; ++i) {
    void *ptr = list->array + i * list->usize;
    if (memcmp(data, ptr, list->usize) == 0) {
      ulist_remove(list, i - list->start);
      return i;
    }
  }
  return -1;
}

char* ulist_to_vlist(const struct ulist *list) {
  akassert(list && list->usize == sizeof(char*));
  struct xstr *xstr = xstr_create_empty();
  for (int i = 0; i < list->num; ++i) {
    const char *c = *(const char**) ulist_get(list, i);
    xstr_cat(xstr, "\1");
    xstr_cat(xstr, c);
  }
  return xstr_destroy_keep_ptr(xstr);
}
