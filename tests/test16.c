/*
 * Tests of the external resources store used by the `.autark/fetch_resource.sh`
 * check script.
 *
 * A fetched external project is identified by its URL. Autark keeps the build
 * wide registry `autark-cache/.autark-externs` mapping an URL to the dir it was
 * fetched into and shares that store with nested builds (AUTARK_EXTERNS_DIR).
 *
 * Covered situations (see tests/data/test16):
 *  - same URL requested twice into the same dir: fetched once, reused, and the
 *    next build does not refetch it (marker mtime and fetched content survive);
 *  - same URL requested into different dirs: fetched once into the dir owned by
 *    the first request, the second request is redirected to it;
 *  - different URLs requested into the same dir by one build: rejected before
 *    the download, so the data fetched from the first URL is kept intact;
 *  - changed URL of an already fetched resource (version bump): the resource of
 *    the previous build is replaced in the same dir;
 *  - shared dependency of two nested external projects: fetched once into the
 *    store of the top level build, not into the caches of the nested projects.
 */

#include "test_utils.h"
#include "script.h"

#include <sys/wait.h>

#define _TDATA "../../tests/data/test16"

static void _build(const char *script) {
  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open(script, &sctx), 0);
  script_build(sctx);
  script_close(&sctx);
}

/// Reads a whole file into an allocated buffer.
static char* _read_file(const char *path) {
  struct value v = utils_file_as_buf(path, 65536);
  if (v.error || !v.buf) {
    value_destroy(&v);
    return 0;
  }
  return v.buf;
}

static bool _str_contains_str(const char *path, const char *needle) {
  char *buf = _read_file(path);
  if (!buf) {
    return false;
  }
  bool ret = strstr(buf, needle) != 0;
  free(buf);
  return ret;
}

/// Asserts that the file contains exactly `num` newline separated lines.
static void _assert_lines(const char *path, int num) {
  char *buf = _read_file(path);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(buf);
  int cnt = 0;
  for (char *p = buf; *p; ++p) {
    if (*p == '\n') {
      ++cnt;
    }
  }
  free(buf);
  IW_TEST_ASSERT_EQUAL(cnt, num);
}

/// Asserts that `dirs.txt` contains two equal paths ending with `suffix`.
static void _assert_dirs_txt(const char *path, const char *suffix) {
  char *buf = _read_file(path);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(buf);
  char *nl = strchr(buf, '\n');
  if (nl) {
    *nl = '\0';
  }
  char *sep = strchr(buf, ' ');
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(sep);
  *sep++ = '\0';
  IW_TEST_ASSERT_STR_EQUAL(buf, sep);
  IW_TEST_ASSERT_TRUE(utils_endswith(sep, suffix));
  free(buf);
}

static void test_same_url_same_dir(void) {
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));
  test_reinit(true);

  _build(_TDATA "/same_url_same_dir/Autark");
  _assert_dirs_txt("autark-cache/dirs.txt", "/autark-cache/extern_lib");

  IW_TEST_ASSERT_TRUE(path_is_file("autark-cache/extern_lib/libext.c"));
  IW_TEST_ASSERT_TRUE(_str_contains_str("autark-cache/extern_lib/.autark-fetch-dep", "fixtures/libext_v1"));
  // Single resource: single registry entry
  _assert_lines("autark-cache/.autark-externs", 1);

  // The second build must not refetch the resource
  IW_TEST_ASSERT_EQUAL(utils_file_write_buf("autark-cache/extern_lib/.sentinel", "1", 1, false), 0);
  struct akpath_stat st0;
  IW_TEST_ASSERT_EQUAL(path_stat("autark-cache/extern_lib/.autark-fetch-dep", &st0), 0);

  chdir(cwd_prev);
  test_reinit(false);
  _build(_TDATA "/same_url_same_dir/Autark");

  IW_TEST_ASSERT_TRUE(path_is_file("autark-cache/extern_lib/.sentinel"));
  struct akpath_stat st1;
  IW_TEST_ASSERT_EQUAL(path_stat("autark-cache/extern_lib/.autark-fetch-dep", &st1), 0);
  IW_TEST_ASSERT_EQUAL(st0.mtime, st1.mtime);
  chdir(cwd_prev);
}

static void test_same_url_diff_dir(void) {
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));
  test_reinit(true);

  _build(_TDATA "/same_url_diff_dir/Autark");
  _assert_dirs_txt("autark-cache/dirs.txt", "/autark-cache/extern_lib_a");

  // The dir requested by the second caller is never created
  IW_TEST_ASSERT_TRUE(path_is_dir("autark-cache/extern_lib_a"));
  IW_TEST_ASSERT_TRUE(!path_is_exist("autark-cache/extern_lib_b"));
  _assert_lines("autark-cache/.autark-externs", 1);
  chdir(cwd_prev);
}

static void test_diff_url_same_dir(void) {
  pid_t pid = fork();
  IW_TEST_ASSERT_TRUE(pid >= 0);
  if (pid == 0) { // Child: the build must fail
    test_reinit(true);
    struct sctx *sctx = 0;
    if (script_open(_TDATA "/diff_url_same_dir/Autark", &sctx)) {
      _exit(1);
    }
    script_build(sctx);
    script_close(&sctx);
    _exit(0);
  }

  int status = 0;
  IW_TEST_ASSERT_EQUAL(waitpid(pid, &status, 0), pid);
  IW_TEST_ASSERT_TRUE(WIFEXITED(status));
  IW_TEST_ASSERT_EQUAL(WEXITSTATUS(status), 254);

  // Rejected before the download: the first version is kept intact
  IW_TEST_ASSERT_TRUE(_str_contains_str(_TDATA "/diff_url_same_dir/autark-cache/extern_lib/.autark-fetch-dep",
                                        "fixtures/libext_v1"));
  IW_TEST_ASSERT_TRUE(_str_contains_str(_TDATA "/diff_url_same_dir/autark-cache/extern_lib/libext.c", "\"v1\""));
}

/// Builds a script in a child process, so the build really starts from scratch.
static int _fork_build(const char *script, bool cleanup) {
  pid_t pid = fork();
  IW_TEST_ASSERT_TRUE(pid >= 0);
  if (pid == 0) {
    test_reinit(cleanup);
    struct sctx *sctx = 0;
    if (script_open(script, &sctx)) {
      _exit(1);
    }
    script_build(sctx);
    script_close(&sctx);
    _exit(0);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}

static void test_url_bump(void) {
  struct pool *pool = pool_create_empty();
  const char *v1 = path_normalize_cwd_pool(_TDATA "/fixtures/libext_v1", 0, pool);
  const char *v2 = path_normalize_cwd_pool(_TDATA "/fixtures/libext_v2", 0, pool);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v1);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v2);
  const char *url_v1 = pool_printf(pool, "dir://%s", v1);
  const char *url_v2 = pool_printf(pool, "dir://%s", v2);

  IW_TEST_ASSERT_EQUAL(setenv("LIB_URL", url_v1, 1), 0);
  IW_TEST_ASSERT_EQUAL(_fork_build(_TDATA "/url_bump/Autark", true), 0);
  IW_TEST_ASSERT_TRUE(_str_contains_str(_TDATA "/url_bump/autark-cache/extern_lib/libext.c", "\"v1\""));

  // The new version takes over the same dir of the previous build
  IW_TEST_ASSERT_EQUAL(setenv("LIB_URL", url_v2, 1), 0);
  IW_TEST_ASSERT_EQUAL(_fork_build(_TDATA "/url_bump/Autark", false), 0);
  IW_TEST_ASSERT_TRUE(_str_contains_str(_TDATA "/url_bump/autark-cache/extern_lib/libext.c", "\"v2\""));
  IW_TEST_ASSERT_TRUE(_str_contains_str(_TDATA "/url_bump/autark-cache/extern_lib/.autark-fetch-dep", "libext_v2"));

  unsetenv("LIB_URL");
  pool_destroy(pool);
}

static void test_nested_shared_store(void) {
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));
  struct pool *pool = pool_create_empty();

  const char *libd_path = path_normalize_cwd_pool(_TDATA "/fixtures/libd", 0, pool);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(libd_path);
  const char *libd_url = pool_printf(pool, "dir://%s", libd_path);
  IW_TEST_ASSERT_EQUAL(setenv("LIBD_URL", libd_url, 1), 0);
  test_reinit(true);
  _build(_TDATA "/nested/Autark");

  // Both external projects were fetched and built by nested autark invocations
  IW_TEST_ASSERT_TRUE(path_is_dir("autark-cache/extern_a/autark-cache"));
  IW_TEST_ASSERT_TRUE(path_is_dir("autark-cache/extern_b/autark-cache"));

  // The shared resource lives in the store of the top level build only
  IW_TEST_ASSERT_TRUE(path_is_file("autark-cache/libd/.autark-fetch-dep"));
  IW_TEST_ASSERT_TRUE(!path_is_exist("autark-cache/extern_a/autark-cache/libd"));
  IW_TEST_ASSERT_TRUE(!path_is_exist("autark-cache/extern_b/autark-cache/libd"));
  IW_TEST_ASSERT_TRUE(_str_contains_str("autark-cache/.autark-externs", libd_url));

  unsetenv("LIBD_URL");
  chdir(cwd_prev);
  pool_destroy(pool);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_same_url_same_dir),
  IW_TEST_CASE(test_same_url_diff_dir),
  IW_TEST_CASE(test_diff_url_same_dir),
  IW_TEST_CASE(test_url_bump),
  IW_TEST_CASE(test_nested_shared_store),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test16", cases, 0, 0));
}
