#ifndef _AMALGAMATE_
#include "map.h"
#include "log.h"
#include "alloc.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#endif

/// Initial number of entry slots.
#define INITIAL_CAPACITY 64u

/// Maximal number of entry slots: keeps slot indices within the `int32_t` range
/// of `struct map_iter::entry` and prevents capacity arithmetic overflows.
#define MAX_CAPACITY (1u << 30)

/// The entries table is grown once the number of entries reaches `capacity * 3 / 4`.
#define LOAD_PERCENT 3u

/// The entries table is shrunk once the number of entries falls below `capacity / 8`.
#define SHRINK_DIVISOR 8u

/// Hash map entry, also used as an entries table slot: an empty slot is
/// marked by `hash == 0` and terminates the probe sequence.
///
/// `hash` keeps a normalized key hash (see `_map_nhash()`), which allows to
/// reject most candidates without calling `strcmp()`.
struct _map_entry {
  void    *key;
  void    *val;
  uint32_t hash;
};

/// Entries are stored in a single flat, power of two sized table and are
/// resolved with linear probing. Removals shift the rest of the probe cluster
/// backwards, so the table holds no tombstones and any empty slot is a valid
/// probe terminator.
struct map {
  struct _map_entry *entries;
  uint32_t capacity;       ///< Number of slots: a power of two.
  uint32_t capacity_mask;  ///< `capacity - 1`
  uint32_t count;          ///< Number of entries stored.

  void (*kv_free_fn)(void*, void*);
};

static void _map_noop_kv_free(void *key, void *val) {
}

void map_kv_free(void *key, void *val) {
  free(key);
  free(val);
}

void map_k_free(void *key, void *val) {
  free(key);
}

/// Maximal number of entries the table of given capacity can hold.
static inline uint32_t _map_max_load(uint32_t capacity) {
  return capacity - capacity / (LOAD_PERCENT + 1);
}

/// Normalizes the key hash so that `0` is reserved to mark an empty slot.
static inline uint32_t _map_nhash(uint32_t hash) {
  hash += 1;
  return hash ? hash : 1;
}

/// Hash of a zero terminated string key (FNV-1a).
static inline uint32_t _map_hash_buf_key(const char *key) {
  const char *str = key;
  uint32_t hash = 2166136261U;
  while (*str) {
    hash ^= (uint8_t) (*str);
    hash *= 16777619U;
    str++;
  }
  return hash;
}

static struct _map_entry* _map_entries_alloc(uint32_t capacity) {
  return xcalloc(capacity, sizeof(struct _map_entry));
}

/// Looks up the entry with the given key.
/// @param nhash Normalized key hash, see `_map_nhash()`.
/// @return Entry pointer or zero if there is no such key.
static struct _map_entry* _map_entry_find(struct map *hm, const char *key, uint32_t nhash) {
  struct _map_entry *entries = hm->entries;
  uint32_t mask = hm->capacity_mask;
  for (uint32_t i = nhash & mask; ; i = (i + 1) & mask) {
    struct _map_entry *entry = entries + i;
    // An empty slot terminates the probe sequence: removals never leave holes.
    if (!entry->hash) {
      return 0;
    }
    if ((entry->hash == nhash) && (strcmp(key, entry->key) == 0)) {
      return entry;
    }
  }
}

/// Returns an entry for the given key: either an existing one, or a newly
/// claimed (zeroed) slot. Increments `hm->count` when a new slot is claimed.
///
/// The caller is responsible for keeping at least one empty slot in the table.
/// @param nhash Normalized key hash, see `_map_nhash()`.
/// @param created Set to `true` if a new slot has been claimed.
static struct _map_entry* _map_entry_put(struct map *hm, const char *key, uint32_t nhash, bool *created) {
  struct _map_entry *entries = hm->entries;
  uint32_t mask = hm->capacity_mask;
  for (uint32_t i = nhash & mask; ; i = (i + 1) & mask) {
    struct _map_entry *entry = entries + i;
    if (!entry->hash) {
      entry->hash = nhash;
      entry->key = 0;
      entry->val = 0;
      ++hm->count;
      *created = true;
      return entry;
    }
    if ((entry->hash == nhash) && (strcmp(key, entry->key) == 0)) {
      *created = false;
      return entry;
    }
  }
}

/// Reallocates the entries table to the given capacity and re-places all entries.
/// Existing entries are moved as is: `kv_free_fn` is not called.
static void _map_resize(struct map *hm, uint32_t capacity) {
  akassert(!(capacity & (capacity - 1)));
  akassert(capacity != hm->capacity);
  akassert((capacity >= INITIAL_CAPACITY) && (capacity <= MAX_CAPACITY));

  struct _map_entry *entries_new = _map_entries_alloc(capacity);
  struct _map_entry *entries_old = hm->entries;
  uint32_t mask_new = capacity - 1;
  uint32_t count = 0;

  for (struct _map_entry *entry = entries_old, *end = entries_old + hm->capacity; entry < end; ++entry) {
    if (!entry->hash) {
      continue;
    }
    uint32_t i = entry->hash & mask_new;
    while (entries_new[i].hash) {
      i = (i + 1) & mask_new;
    }
    entries_new[i] = *entry;
    ++count;
  }

  akassert(count == hm->count);
  (void) count;
  hm->entries = entries_new;
  hm->capacity = capacity;
  hm->capacity_mask = mask_new;
  free(entries_old);
}

/// Makes sure the table can hold one more entry.
static void _map_reserve_entry(struct map *hm) {
  if (hm->count + 1 <= _map_max_load(hm->capacity)) {
    return;
  }
  if (hm->capacity >= MAX_CAPACITY) {
    akfatal(AK_ERROR_OVERFLOW, 0, 0);
  }
  _map_resize(hm, hm->capacity * 2);
}

/// Removes the entry from the table and shifts the rest of its probe cluster
/// backwards, so the table keeps no tombstones.
/// @note Invalidates all entry pointers, including the given one.
static void _map_entry_remove(struct map *hm, struct _map_entry *entry) {
  hm->kv_free_fn(entry->key, entry->val);
  --hm->count;

  struct _map_entry *entries = hm->entries;
  uint32_t mask = hm->capacity_mask;
  uint32_t hole = (uint32_t) (entry - entries);

  for (uint32_t i = (hole + 1) & mask; entries[i].hash; i = (i + 1) & mask) {
    uint32_t home = entries[i].hash & mask;
    // The entry can be moved back only if the hole is located on its probe path,
    // otherwise moving it would make the entry unreachable for lookups.
    if (((hole - home) & mask) <= ((i - home) & mask)) {
      entries[hole] = entries[i];
      hole = i;
    }
  }
  entry = entries + hole;
  entry->hash = 0;
  entry->key = 0;
  entry->val = 0;

  if ((hm->capacity > INITIAL_CAPACITY) && (hm->count < hm->capacity / SHRINK_DIVISOR)) {
    _map_resize(hm, hm->capacity / 2);
  }
}

static void _map_put(struct map *hm, const char *key, void *val) {
  uint32_t nhash = _map_nhash(_map_hash_buf_key(key));
  // Reserve space before the lookup: a `put` of an already existing key may grow
  // the table one step earlier than strictly needed, but it keeps the insertion
  // path to a single probe sequence.
  _map_reserve_entry(hm);
  bool created;
  struct _map_entry *entry = _map_entry_put(hm, key, nhash, &created);
  if (!created) {
    hm->kv_free_fn(entry->key, entry->val);
  }
  entry->key = (void*) key;
  entry->val = val;
}

struct map* map_create_str(void (*kv_free_fn)(void*, void*)) {
  if (!kv_free_fn) {
    kv_free_fn = _map_noop_kv_free;
  }
  struct map *hm = xmalloc(sizeof(*hm));
  hm->entries = _map_entries_alloc(INITIAL_CAPACITY);
  hm->capacity = INITIAL_CAPACITY;
  hm->capacity_mask = INITIAL_CAPACITY - 1;
  hm->count = 0;
  hm->kv_free_fn = kv_free_fn;
  return hm;
}

void map_destroy(struct map *hm) {
  if (!hm) {
    return;
  }
  for (struct _map_entry *entry = hm->entries, *end = hm->entries + hm->capacity; entry < end; ++entry) {
    if (entry->hash) {
      hm->kv_free_fn(entry->key, entry->val);
    }
  }
  free(hm->entries);
  free(hm);
}

void map_put_str(struct map *hm, const char *key, void *val) {
  _map_put(hm, xstrdup(key), val);
}

void map_put_str_no_copy(struct map *hm, const char *key, void *val) {
  _map_put(hm, key, val);
}

int map_remove(struct map *hm, const char *key) {
  struct _map_entry *entry = _map_entry_find(hm, key, _map_nhash(_map_hash_buf_key(key)));
  if (!entry) {
    return 0;
  }
  _map_entry_remove(hm, entry);
  return 1;
}

void* map_get(struct map *hm, const char *key) {
  struct _map_entry *entry = _map_entry_find(hm, key, _map_nhash(_map_hash_buf_key(key)));
  return entry ? entry->val : 0;
}

void map_iter_init(struct map *hm, struct map_iter *iter) {
  iter->hm = hm;
  iter->entry = -1;
  iter->bucket = 0;
  iter->key = 0;
  iter->val = 0;
}

int map_iter_next(struct map_iter *iter) {
  struct map *hm = iter->hm;
  if (!hm) {
    return 0;
  }
  struct _map_entry *entries = hm->entries;
  uint32_t capacity = hm->capacity;

  if ((iter->entry >= 0) && ((uint32_t) iter->entry < capacity)) {
    // Removals shift the rest of the probe cluster backwards, so the slot
    // returned by the previous call may hold another entry now (this is the case
    // when the caller removes the entry it has just received). Return that entry
    // as well, otherwise it would be silently skipped by the iteration.
    struct _map_entry *entry = entries + iter->entry;
    if (entry->hash && ((entry->hash != iter->bucket) || (entry->key != iter->key))) {
      iter->bucket = entry->hash;
      iter->key = entry->key;
      iter->val = entry->val;
      return 1;
    }
  }

  uint32_t i = (iter->entry < 0) ? 0 : (uint32_t) iter->entry + 1;
  for ( ; (i < capacity) && !entries[i].hash; ++i) {}
  if (i >= capacity) {
    iter->entry = (int32_t) capacity;
    return 0;
  }
  struct _map_entry *entry = entries + i;
  iter->entry = (int32_t) i;
  iter->bucket = entry->hash;
  iter->key = entry->key;
  iter->val = entry->val;
  return 1;
}
