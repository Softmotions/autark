#ifndef MAP_H
#define MAP_H

#ifndef _AMALGAMATE_
#include <stdint.h>
#endif

struct map;
struct map_iter;

struct map_iter {
  struct map *hm;
  const void *key;
  const void *val;
  uint32_t    bucket;
  int32_t     entry;
};

void map_kv_free(void *key, void *val);
void map_k_free(void *key, void *val);

struct map* map_create(
  int (*cmp_fn)(const void*, const void*),
  uint32_t (*hash_fn)(const void*),
  void (*kv_free_fn)(void*, void*));

void map_destroy(struct map*);

struct map* map_create_u64(void (*kv_free_fn)(void*, void*));

struct map* map_create_u32(void (*kv_free_fn)(void*, void*));

struct map* map_create_str(void (*kv_free_fn)(void*, void*));

void map_put(struct map*, void *key, void *val);

void map_put_u32(struct map*, uint32_t key, void *val);

void map_put_u64(struct map*, uint64_t key, void *val);

void map_put_str(struct map*, const char *key, void *val);

void map_put_str_no_copy(struct map*, const char *key, void *val);

int map_remove(struct map*, const void *key);

int map_remove_u64(struct map*, uint64_t key);

int map_remove_u32(struct map*, uint32_t key);

void* map_get(struct map*, const void *key);

void* map_get_u64(struct map*, uint64_t key);

void* map_get_u32(struct map*, uint32_t key);

uint32_t map_count(const struct map*);

void map_clear(struct map*);

void map_iter_init(struct map*, struct map_iter*);

int map_iter_next(struct map_iter*);

#endif
