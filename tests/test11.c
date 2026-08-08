#include "test_utils.h"
#include "script.h"

int main(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct pool *pool = pool_create_empty();
  test_init(true);

  const char *cache_dir = path_normalize_cwd_pool("../../tests/data/test11/autark-cache", 0, pool);
  const char *install_dir = pool_printf(pool, "%s", cache_dir);
  const char *overlay_dir = pool_printf(pool, "%s" AUTARK_CACHE_OVERLAY_DIR, cache_dir);

  g_env.install.prefix_dir = install_dir;
  g_env.project.cache_overlay_dir = overlay_dir;
  g_env.install.enabled = true;
  g_env.install.flags |= INSTALL_FLG_SRC_WITH_DEPS;

  struct sctx *sctx;
  int rc = script_open("../../tests/data/test11/Autark", &sctx);
  akassert(rc == 0);

  script_build(sctx);
  script_close(&sctx);
  chdir(cwd_prev);

#define _PCACHE "../../tests/data/test11/autark-cache"

  akassert(path_is_exist(_PCACHE "/include/libext/libext.h"));
  akassert(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/Autark"));
  akassert(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/extern_libext/Autark"));
  akassert(path_is_exist(
             _PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/extern_libext/.autark/env.sh"));
  akassert(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/" AUTARK_FETCHED_REG_DIST));

  fprintf(stderr, "\n\n");
  test_reinit(false);
  g_env.install.enabled = false;
  g_env.install.flags = 0;

  rc = script_open(_PCACHE "/shared/test11/test11-source-1.0.0/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);

  chdir(cwd_prev);
  akassert(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/main"));

  fprintf(stderr, "\n\n");
  test_reinit(false);

  rc = script_open(_PCACHE "/shared/test11/test11-source-1.0.0/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);

  pool_destroy(pool);
  return 0;
}
