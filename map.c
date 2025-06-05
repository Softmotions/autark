#ifndef _AMALGAMATE_
#include "map.h"
#include "log.h"
#include "alloc.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#endif

#define MIN_BUCKETS 64
#define STEPS       4

struct _map_entry {
  void    *key;
  void    *val;
  uint32_t hash;
};

struct _map_bucket {
  struct _map_entry *entries;
  uint32_t      used;
  uint32_t      total;
};

struct map {
  uint32_t       count;
  uint32_t       buckets_mask;
  struct _map_bucket *buckets;

  int      (*cmp_fn)(const void*, const void*);
  uint32_t (*hash_key_fn)(const void*);
  void     (*kv_free_fn)(void*, void*);

  int int_key_as_pointer_value;
};

static void _map_noop_kv_free(void *key, void *val) {
}

static void _map_noop_uint64_kv_free(void *key, void *val) {
  if (key) {
    free(key);
  }
}

static inline uint32_t _map_n_buckets(struct map *hm) {
  return hm->buckets_mask + 1;
}

static int _map_ptr_cmp(const void *v1, const void *v2) {
  return v1 > v2 ? 1 : v1 < v2 ? -1 : 0;
}

static int _map_uint32_cmp(const void *v1, const void *v2) {
  intptr_t p1 = (intptr_t) v1;
  intptr_t p2 = (intptr_t) v2;
  return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
}

static int _map_uint64_cmp(const void *v1, const void *v2) {
  if (sizeof(uintptr_t) >= sizeof(uint64_t)) {
    intptr_t p1 = (intptr_t) v1;
    intptr_t p2 = (intptr_t) v2;
    return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
  } else {
    uint64_t l1, l2;
    memcpy(&l1, v1, sizeof(l1));
    memcpy(&l2, v2, sizeof(l2));
    return l1 > l2 ? 1 : l1 < l2 ? -1 : 0;
  }
}

static inline uint32_t _map_hash_uint32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x85ebca6bU;
  x ^= x >> 13;
  x *= 0xc2b2ae35U;
  x ^= x >> 16;
  return x;
}

static inline uint32_t _map_hash_uint64(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

static inline uint32_t _map_hash_uint64_key(const void *key) {
  uint64_t lv = 0;
  memcpy(&lv, key, sizeof(void*));
  return _map_hash_uint64(lv);
}

static inline uint32_t _map_hash_uint32_key(const void *key) {
  return _map_hash_uint32((uintptr_t) key);
}

static inline uint32_t _map_hash_buf_key(const void *key) {
  const char *str = key;
  uint32_t hash = 2166136261U;
  while (*str) {
    hash ^= (uint8_t) (*str);
    hash *= 16777619U;
    str++;
  }
  return hash;
}

static struct _map_entry* _map_entry_add(struct map *hm, void *key, uint32_t hash) {
  struct _map_entry *entry;
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);

  if (bucket->used + 1 >= bucket->total) {
    if (UINT32_MAX - bucket->total < STEPS) {
      akfatal(AK_ERROR_OVERFLOW, 0, 0, 0);
    }
    uint32_t new_total = bucket->total + STEPS;
    struct _map_entry *new_entries = xrealloc(bucket->entries, new_total * sizeof(new_entries[0]));
    bucket->entries = new_entries;
    bucket->total = new_total;
  }
  entry = bucket->entries;
  for (struct _map_entry *end = entry + bucket->used; entry < end; ++entry) {
    if ((hash == entry->hash) && (hm->cmp_fn(key, entry->key) == 0)) {
      return entry;
    }
  }
  ++bucket->used;
  ++hm->count;

  entry->hash = hash;
  entry->key = 0;
  entry->val = 0;

  return entry;
}

static struct _map_entry* _map_entry_find(struct map *hm, const void *key, uint32_t hash) {
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);
  struct _map_entry *entry = bucket->entries;
  for (struct _map_entry *end = entry + bucket->used; entry < end; ++entry) {
    if (hash == entry->hash && hm->cmp_fn(key, entry->key) == 0) {
      return entry;
    }
  }
  return 0;
}

static void _map_rehash(struct map *hm, uint32_t num_buckets) {
  struct _map_bucket *buckets = xcalloc(num_buckets, sizeof(*buckets));
  struct _map_bucket *bucket,
                *bucket_end = hm->buckets + _map_n_buckets(hm);

  struct map hm_copy = *hm;
  hm_copy.count = 0;
  hm_copy.buckets_mask = num_buckets - 1;
  hm_copy.buckets = buckets;

  for (bucket = hm->buckets; bucket < bucket_end; ++bucket) {
    struct _map_entry *entry_old = bucket->entries;
    if (entry_old) {
      struct _map_entry *entry_old_end = entry_old + bucket->used;
      for ( ; entry_old < entry_old_end; ++entry_old) {
        struct _map_entry *entry_new = _map_entry_add(&hm_copy, entry_old->key, entry_old->hash);
        entry_new->key = entry_old->key;
        entry_new->val = entry_old->val;
      }
    }
  }

  for (bucket = hm->buckets; bucket < bucket_end; ++bucket) {
    free(bucket->entries);
  }
  free(hm->buckets);

  hm->buckets = buckets;
  hm->buckets_mask = num_buckets - 1;
  return;
}

static void _map_entry_remove(struct map *hm, struct _map_bucket *bucket, struct _map_entry *entry) {
  hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : entry->key, entry->val);
  if (bucket->used > 1) {
    struct _map_entry *entry_last = bucket->entries + bucket->used - 1;
    if (entry != entry_last) {
      memcpy(entry, entry_last, sizeof(*entry));
    }
  }
  --bucket->used;
  --hm->count;

  if ((hm->buckets_mask > MIN_BUCKETS - 1) && (hm->count < hm->buckets_mask / 2)) {
    _map_rehash(hm, _map_n_buckets(hm) / 2);
  } else {
    uint32_t steps_used = bucket->used / STEPS;
    uint32_t steps_total = bucket->total / STEPS;
    if (steps_used + 1 < steps_total) {
      struct _map_entry *entries_new = realloc(bucket->entries, ((size_t) steps_used + 1) * STEPS * sizeof(entries_new[0]));
      if (entries_new) {
        bucket->entries = entries_new;
        bucket->total = (steps_used + 1) * STEPS;
      }
    }
  }
}

void map_kv_free(void *key, void *val) {
  free(key);
  free(val);
}

void map_k_free(void *key, void *val) {
  free(key);
}

struct map* map_create(
  int (*cmp_fn)(const void*, const void*),
  uint32_t (*hash_key_fn)(const void*),
  void (*kv_free_fn)(void*, void*)) {
  if (!hash_key_fn) {
    return 0;
  }
  if (!cmp_fn) {
    cmp_fn = _map_ptr_cmp;
  }
  if (!kv_free_fn) {
    kv_free_fn = _map_noop_kv_free;
  }

  struct map *hm = xmalloc(sizeof(*hm));
  hm->buckets = xcalloc(MIN_BUCKETS, sizeof(hm->buckets[0]));
  hm->cmp_fn = cmp_fn;
  hm->hash_key_fn = hash_key_fn;
  hm->kv_free_fn = kv_free_fn;
  hm->buckets_mask = MIN_BUCKETS - 1;
  hm->count = 0;
  hm->int_key_as_pointer_value = 0;
  return hm;
}

void map_destroy(struct map *hm) {
  if (hm) {
    for (struct _map_bucket *b = hm->buckets, *be = hm->buckets + _map_n_buckets(hm); b < be; ++b) {
      if (b->entries) {
        for (struct _map_entry *e = b->entries, *ee = b->entries + b->used; e < ee; ++e) {
          hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : e->key, e->val);
        }
        free(b->entries);
      }
    }
    free(hm->buckets);
    free(hm);
  }
}

struct map* map_create_u64(void (*kv_free_fn)(void*, void*)) {
  if (!kv_free_fn) {
    kv_free_fn = _map_noop_uint64_kv_free;
  }
  struct map *hm = map_create(_map_uint64_cmp, _map_hash_uint64_key, kv_free_fn);
  if (hm) {
    if (sizeof(uintptr_t) >= sizeof(uint64_t)) {
      hm->int_key_as_pointer_value = 1;
    }
  }
  return hm;
}

struct map* map_create_u32(void (*kv_free_fn)(void*, void*)) {
  struct map *hm = map_create(_map_uint32_cmp, _map_hash_uint32_key, kv_free_fn);
  if (hm) {
    hm->int_key_as_pointer_value = 1;
  }
  return hm;
}

struct map* map_create_str(void (*kv_free_fn)(void*, void*)) {
  return map_create((int (*)(const void*, const void*)) strcmp, _map_hash_buf_key, kv_free_fn);
}

void map_put(struct map *hm, void *key, void *val) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_entry *entry = _map_entry_add(hm, key, hash);
  hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : entry->key, entry->val);
  entry->key = key;
  entry->val = val;
  if (hm->count > hm->buckets_mask) {
    _map_rehash(hm, _map_n_buckets(hm) * 2);
  }
}

void map_put_u32(struct map *hm, uint32_t key, void *val) {
  map_put(hm, (void*) (uintptr_t) key, val);
}

void map_put_u64(struct map *hm, uint64_t key, void *val) {
  if (hm->int_key_as_pointer_value) {
    map_put(hm, (void*) (uintptr_t) key, val);
  } else {
    uint64_t *kv = xmalloc(sizeof(*kv));
    memcpy(kv, &key, sizeof(*kv));
    map_put(hm, kv, val);
  }
}

void map_put_str(struct map *hm, const char *key_, void *val) {
  char *key = xstrdup(key_);
  map_put(hm, key, val);
}

void map_put_str_no_copy(struct map *hm, const char *key, void *val) {
  map_put(hm, (void*) key, val);
}

int map_remove(struct map *hm, const void *key) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);
  struct _map_entry *entry = _map_entry_find(hm, key, hash);
  if (entry) {
    _map_entry_remove(hm, bucket, entry);
    return 1;
  } else {
    return 0;
  }
}

int map_remove_u64(struct map *hm, uint64_t key) {
  if (hm->int_key_as_pointer_value) {
    return map_remove(hm, (void*) (uintptr_t) key);
  } else {
    return map_remove(hm, &key);
  }
}

int map_remove_u32(struct map *hm, uint32_t key) {
  return map_remove(hm, (void*) (uintptr_t) key);
}

void* map_get(struct map *hm, const void *key) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_entry *entry = _map_entry_find(hm, key, hash);
  if (entry) {
    return entry->val;
  } else {
    return 0;
  }
}

void* map_get_u64(struct map *hm, uint64_t key) {
  if (hm->int_key_as_pointer_value) {
    return map_get(hm, (void*) (intptr_t) key);
  } else {
    return map_get(hm, &key);
  }
}

void* map_get_u32(struct map *hm, uint32_t key) {
  return map_get(hm, (void*) (intptr_t) key);
}

uint32_t map_count(const struct map *hm) {
  return hm->count;
}

void map_clear(struct map *hm) {
  if (!hm) {
    return;
  }
  for (struct _map_bucket *b = hm->buckets, *be = hm->buckets + _map_n_buckets(hm); b < be; ++b) {
    for (struct _map_entry *e = b->entries, *ee = b->entries + b->used; e < ee; ++e) {
      hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : e->key, e->val);
    }
    free(b->entries);
    b->used = 0;
    b->total = 0;
    b->entries = 0;
  }
  if (_map_n_buckets(hm) > MIN_BUCKETS) {
    struct _map_bucket *buckets_new = realloc(hm->buckets, sizeof(buckets_new[0]) * MIN_BUCKETS);
    if (buckets_new) {
      memset(buckets_new, 0, sizeof(buckets_new[0]) * MIN_BUCKETS);
      hm->buckets = buckets_new;
      hm->buckets_mask = MIN_BUCKETS - 1;
    }
  }
  hm->count = 0;
}

void map_iter_init(struct map *hm, struct map_iter *iter) {
  iter->hm = hm;
  iter->entry = -1;
  iter->bucket = 0;
  iter->key = 0;
  iter->val = 0;
}

int map_iter_next(struct map_iter *iter) {
  if (!iter->hm) {
    return 0;
  }
  struct _map_entry *entry;
  struct _map_bucket *bucket = iter->hm->buckets + iter->bucket;

  ++iter->entry;
  if ((uint32_t) iter->entry >= bucket->used) {
    uint32_t n = _map_n_buckets(iter->hm);
    iter->entry = 0;
    for (++iter->bucket; iter->bucket < n; ++iter->bucket) {
      bucket = iter->hm->buckets + iter->bucket;
      if (bucket->used > 0) {
        break;
      }
    }
    if (iter->bucket >= n) {
      return 0;
    }
  }
  entry = bucket->entries + iter->entry;
  iter->key = entry->key;
  iter->val = entry->val;
  return 1;
}
