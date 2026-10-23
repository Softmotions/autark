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

/// Key/Value free callback which uses standard `free()` deallocation.
/// @param key Pointer to key or zero.
/// @param val Pointer to value or zero.
void map_kv_free(void *key, void *val);

/// Key free callback which uses standard `free()` deallocation.
/// @param key Pointer to key or zero.
/// @param val Value, ignored.
void map_k_free(void *key, void *val);

/// Creates a map with `strcmp()` keys.
/// @param kv_free_fn Callback used to release keys and values on removal,
///        zero means that the map does not own stored keys and values.
struct map* map_create_str(void (*kv_free_fn)(void*, void*));

void map_destroy(struct map*);

/// Makes a copy of key (`strdup()`) then puts key/value pair into the map.
/// @note Key memory is expected to be released by `kv_free_fn` given to `map_create_str()`.
void map_put_str(struct map*, const char *key, void *val);

/// Puts key/value pair into the map without copying the key.
/// @note Key memory is expected to be released by `kv_free_fn` given to `map_create_str()`.
void map_put_str_no_copy(struct map*, const char *key, void *val);

/// Removes the given key from the map.
/// @return `1` if the key has been found and removed, `0` otherwise.
int map_remove(struct map*, const char *key);

/// @return Value associated with the given key or zero.
void* map_get(struct map*, const char *key);

void map_iter_init(struct map*, struct map_iter*);

/// Advances the iterator to the next entry.
/// @return `1` if the iterator points to an entry, `0` at the end of the map.
int map_iter_next(struct map_iter*);

#endif
