#include "test_utils.h"
#include "alloc.h"
#include "map.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_GROW_N 2000
#define TEST_ITER_N 30

static void test_put_get_remove(void) {
  struct map *m = map_create_str(map_k_free);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(m);

  IW_TEST_ASSERT_PTR_NULL(map_get(m, "a"));

  map_put_str(m, "a", (void*) (intptr_t) 1);
  map_put_str(m, "b", (void*) (intptr_t) 2);
  IW_TEST_ASSERT_EQUAL((intptr_t) map_get(m, "a"), 1);
  IW_TEST_ASSERT_EQUAL((intptr_t) map_get(m, "b"), 2);
  IW_TEST_ASSERT_PTR_NULL(map_get(m, "c"));

  // Overwriting an existing key releases the old key and value
  map_put_str(m, "a", (void*) (intptr_t) 3);
  IW_TEST_ASSERT_EQUAL((intptr_t) map_get(m, "a"), 3);

  IW_TEST_ASSERT_EQUAL(map_remove(m, "a"), 1);
  IW_TEST_ASSERT_EQUAL(map_remove(m, "a"), 0);
  IW_TEST_ASSERT_PTR_NULL(map_get(m, "a"));
  IW_TEST_ASSERT_EQUAL((intptr_t) map_get(m, "b"), 2);

  map_destroy(m);
}

static void test_put_str_no_copy(void) {
  struct map *m = map_create_str(0);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(m);

  char key[] = "borrowed";
  map_put_str_no_copy(m, key, (void*) (intptr_t) 42);
  IW_TEST_ASSERT_EQUAL((intptr_t) map_get(m, "borrowed"), 42);
  IW_TEST_ASSERT_EQUAL(map_remove(m, "borrowed"), 1);
  IW_TEST_ASSERT_PTR_NULL(map_get(m, "borrowed"));

  map_destroy(m);
}

static void test_grow_shrink_iter(void) {
  struct map *m = map_create_str(map_kv_free);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(m);

  char key[64];
  for (int i = 0; i < TEST_GROW_N; ++i) {
    snprintf(key, sizeof(key), "path/to/source/file-%d.c", i);
    map_put_str(m, key, xstrdup(key));
  }

  for (int i = 0; i < TEST_GROW_N; ++i) {
    snprintf(key, sizeof(key), "path/to/source/file-%d.c", i);
    const char *val = map_get(m, key);
    IW_TEST_ASSERT_PTR_NOT_NULL(val);
    IW_TEST_ASSERT_STR_EQUAL(val, key);
  }

  int visited = 0;
  struct map_iter it;
  map_iter_init(m, &it);
  while (map_iter_next(&it)) {
    IW_TEST_ASSERT_PTR_NOT_NULL(it.key);
    IW_TEST_ASSERT_PTR_NOT_NULL(it.val);
    ++visited;
  }
  IW_TEST_ASSERT_EQUAL(visited, TEST_GROW_N);

  // Removing half of the keys triggers the table shrink
  for (int i = 1; i < TEST_GROW_N; i += 2) {
    snprintf(key, sizeof(key), "path/to/source/file-%d.c", i);
    IW_TEST_ASSERT_EQUAL(map_remove(m, key), 1);
    IW_TEST_ASSERT_PTR_NULL(map_get(m, key));
  }

  for (int i = 0; i < TEST_GROW_N; ++i) {
    snprintf(key, sizeof(key), "path/to/source/file-%d.c", i);
    const char *val = map_get(m, key);
    if (i & 1) {
      IW_TEST_ASSERT_PTR_NULL(val);
    } else {
      IW_TEST_ASSERT_PTR_NOT_NULL(val);
      IW_TEST_ASSERT_STR_EQUAL(val, key);
    }
  }

  visited = 0;
  map_iter_init(m, &it);
  while (map_iter_next(&it)) {
    ++visited;
  }
  IW_TEST_ASSERT_EQUAL(visited, TEST_GROW_N / 2);

  map_destroy(m);
}

/// Removals shift the rest of the probe cluster backwards: the entry returned
/// by the iterator must not be silently skipped if the caller removes it.
static void test_iter_remove_current(void) {
  struct map *m = map_create_str(map_k_free);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(m);

  // Capacity stays at the initial one, so the table is not rehashed during the iteration
  char key[32];
  uint32_t seen[TEST_ITER_N] = {0};
  for (int i = 0; i < TEST_ITER_N; ++i) {
    snprintf(key, sizeof(key), "key-%d", i);
    map_put_str(m, key, (void*) (intptr_t) (i + 1));
  }

  int visited = 0;
  struct map_iter it;
  map_iter_init(m, &it);
  while (map_iter_next(&it)) {
    intptr_t idx = (intptr_t) it.val - 1;
    IW_TEST_ASSERT_TRUE_FATAL((idx >= 0) && (idx < TEST_ITER_N));
    ++seen[idx];
    ++visited;
    IW_TEST_ASSERT_EQUAL_FATAL(map_remove(m, (const char*) it.key), 1);
  }
  IW_TEST_ASSERT_EQUAL(visited, TEST_ITER_N);
  for (int i = 0; i < TEST_ITER_N; ++i) {
    IW_TEST_ASSERT_EQUAL(seen[i], 1);
  }

  map_iter_init(m, &it);
  IW_TEST_ASSERT_EQUAL(map_iter_next(&it), 0);
  for (int i = 0; i < TEST_ITER_N; ++i) {
    snprintf(key, sizeof(key), "key-%d", i);
    IW_TEST_ASSERT_PTR_NULL(map_get(m, key));
  }

  map_destroy(m);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_put_get_remove),
  IW_TEST_CASE(test_put_str_no_copy),
  IW_TEST_CASE(test_grow_shrink_iter),
  IW_TEST_CASE(test_iter_remove_current),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test14", cases, 0, 0));
}
