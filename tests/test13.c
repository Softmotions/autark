#include "test_utils.h"
#include "toolchain.h"
#include "script.h"
#include "env.h"

static const char* _var_name(struct toolchain *tc, const char *key) {
  for (int i = 0; i < tc->vars.num; ++i) {
    struct toolchain_var *v = *(struct toolchain_var**) ulist_get(&tc->vars, i);
    if (strcmp(v->key, key) == 0) {
      return v->var;
    }
  }
  return 0;
}

static bool _eq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

int main(void) {
  int ret = 1;
  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  test_init(true);
  autark_build_prepare("../../tests/data/toolchain/Autark");

  unsetenv("TEST_TOOLCHAIN_EXTRA");
  struct toolchain *tc = toolchain_load(g_env.pool, "basic.cfg");

  // Well known keys define the target contract
  ASSERT(finish, _eq(tc->id, "test-arm64"));
  ASSERT(finish, _eq(tc->system, "android")); // Normalized to lower case
  ASSERT(finish, _eq(tc->arch, "aarch64"));
  ASSERT(finish, _eq(tc->arch_alt, "arm64")); // `arch-alt` is the same key as `arch_alt`
  ASSERT(finish, _eq(tc->bits, "64"));
  ASSERT(finish, _eq(tc->endian, "little"));
  ASSERT(finish, _eq(tc->ar, "ar"));
  ASSERT(finish, _eq(tc->host_cc, "cc"));
  ASSERT(finish, _eq(tc->cflags, "-DONE -DTWO=2"));

  // `SS{}` in a descriptor is resolved against the descriptor directory
  char ccbuf[PATH_MAX];
  ASSERT(finish, _eq(tc->cc, path_normalize_cwd("tools/fake-cc.sh", g_env.cwd, ccbuf)));
  ASSERT(finish, path_is_absolute(tc->path));
  ASSERT(finish, path_is_file(tc->path));

  // Arbitrary keys are supported and published as TOOLCHAIN_<KEY>
  ASSERT(finish, _eq(toolchain_var_get(tc, "sysroot"), "/nonexistent/ndk/sysroot"));
  ASSERT(finish, _eq(toolchain_var_get(tc, "SYSROOT"), "/nonexistent/ndk/sysroot"));
  ASSERT(finish, _eq(toolchain_var_get(tc, "ARCH-ALT"), "arm64"));
  ASSERT(finish, _eq(_var_name(tc, "sysroot"), "TOOLCHAIN_SYSROOT"));
  ASSERT(finish, toolchain_var_get(tc, "extra") == 0);
  ASSERT(finish, toolchain_var_get(tc, "unknown-key") == 0);

  // Conditional descriptor keys
  setenv("TEST_TOOLCHAIN_EXTRA", "1", 1);
  struct toolchain *tc2 = toolchain_load(g_env.pool, "basic.cfg");
  ASSERT(finish, _eq(toolchain_var_get(tc2, "extra"), "extra-value"));
  unsetenv("TEST_TOOLCHAIN_EXTRA");

  toolchain_apply(tc);
  ASSERT(finish, _eq(toolchain_id(), "test-arm64"));

  struct unit *root = unit_root();
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_CROSS"), "1"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_TARGET"), "test-arm64"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_NAME"), "android"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_ANDROID"), "1"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_ARCH"), "aarch64"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_ARCH_ALT"), "arm64"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_BITNESS_64"), "1"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "SYSTEM_LITTLE_ENDIAN"), "1"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "CFLAGS_TARGET"), "-DONE -DTWO=2"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "TOOLCHAIN_SYSROOT"), "/nonexistent/ndk/sysroot"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "HOST_CC"), "cc"));
  ASSERT(finish, _eq(unit_env_get_raw(root, "AUTARK_TARGET"), "test-arm64"));
  ASSERT(finish, _eq(getenv(AUTARK_TARGET_ENV), "test-arm64"));
  ASSERT(finish, _eq(getenv("SYSTEM_CROSS"), "1"));
  ASSERT(finish, _eq(getenv("TOOLCHAIN_SYSROOT"), "/nonexistent/ndk/sysroot"));

  // Object toolchain tags
  char tag[128];
  akassert(toolchain_object_tag_write("cross-test.o", toolchain_object_tag(false)) == 0);
  ASSERT(finish, toolchain_object_check("cross-test.o", false, tag, sizeof(tag)) == 0);
  ASSERT(finish, toolchain_object_check("cross-test.o", true, tag, sizeof(tag)) != 0);
  ASSERT(finish, _eq(tag, "test-arm64"));

  akassert(toolchain_object_tag_write("cross-test.o", toolchain_object_tag(true)) == 0);
  ASSERT(finish, toolchain_object_check("cross-test.o", true, tag, sizeof(tag)) == 0);
  ASSERT(finish, toolchain_object_check("cross-test.o", false, tag, sizeof(tag)) != 0);
  ASSERT(finish, _eq(tag, TOOLCHAIN_TAG_HOST));

  akassert(toolchain_object_tag_write("cross-test.o", 0) == 0);
  ASSERT(finish, toolchain_object_check("cross-test.o", true, tag, sizeof(tag)) == 0);

  ret = 0;
finish:
  chdir(cwd_prev);
  return ret;
}
