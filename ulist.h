#ifndef ULIST_H
#define ULIST_H

#ifndef _AMALGAMATE_
#include "basedefs.h"
#endif

struct ulist {
  char    *array;
  unsigned usize;
  unsigned num;
  unsigned anum;
  unsigned start;
};

struct ulist* ulist_create(unsigned initial_len, unsigned unit_size);

void ulist_init(struct ulist*, unsigned ini_length, unsigned unit_size);

void ulist_destroy(struct ulist**);

void ulist_destroy_keep(struct ulist*);

void ulist_clear(struct ulist*);

void ulist_reset(struct ulist*);

void* ulist_get(const struct ulist*, unsigned idx);

void* ulist_peek(const struct ulist*);

void ulist_insert(struct ulist*, unsigned idx, const void *data);

void ulist_set(struct ulist*, unsigned idx, const void *data);

void ulist_remove(struct ulist*, unsigned idx);

void ulist_push(struct ulist*, const void *data);

void ulist_pop(struct ulist*);

void ulist_unshift(struct ulist*, const void *data);

void ulist_shift(struct ulist*);

struct ulist* ulist_clone(const struct ulist*);

int ulist_find(const struct ulist*, const void *data);

int ulist_remove_by(struct ulist*, const void *data);

AK_ALLOC char* ulist_to_vlist(const struct ulist *list);

#endif
