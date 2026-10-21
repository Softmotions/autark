#include "test_utils.h"
#include "toolchain.h"
#include "script.h"
#include "env.h"

static char cwd_start[PATH_MAX];

static int suite_init(void) {
  return getcwd(cwd_start, sizeof(cwd_start)) ? 0 : 1;
}

static const char* _var_name(struct toolchain *tc, const char *key) {
  for (int i = 0; i < tc->vars.num; ++i) {
    struct toolchain_var *v = *(struct toolchain_var**) ulist_get(&tc->vars, i);
    if (strcmp(v->key, key) == 0) {
      return v->var;
    }
  }
  return 0;
}

static struct toolchain* load_basic(void) {
  chdir(cwd_start);
  test_reinit(true);
  autark_build_prepare("../../tests/data/toolchain/Autark");
  unsetenv("TEST_TOOLCHAIN_EXTRA");
  struct toolchain *tc = toolchain_load(g_env.pool, "basic.autark");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(tc);
  return tc;
}

static void test_descriptor(void) {
  struct toolchain *tc = load_basic();

  // Well known keys define the target contract
  IW_TEST_ASSERT_STR_EQUAL(tc->id, "test-arm64");
  IW_TEST_ASSERT_STR_EQUAL(tc->system, "android"); // Normalized to lower case
  IW_TEST_ASSERT_STR_EQUAL(tc->arch, "aarch64");
  IW_TEST_ASSERT_STR_EQUAL(tc->arch_alt, "arm64"); // `arch-alt` is the same key as `arch_alt`
  IW_TEST_ASSERT_STR_EQUAL(tc->bits, "64");
  IW_TEST_ASSERT_STR_EQUAL(tc->endian, "little");
  IW_TEST_ASSERT_STR_EQUAL(tc->ar, "ar");
  IW_TEST_ASSERT_STR_EQUAL(tc->host_cc, "cc");
  IW_TEST_ASSERT_STR_EQUAL(tc->cflags, "-DONE -DTWO=2");

  // `SS{}` in a descriptor is resolved against the descriptor directory
  char ccbuf[PATH_MAX];
  IW_TEST_ASSERT_STR_EQUAL(tc->cc, path_normalize_cwd("tools/fake-cc.sh", g_env.cwd, ccbuf));
  IW_TEST_ASSERT_TRUE(path_is_absolute(tc->path));
  IW_TEST_ASSERT_TRUE(path_is_file(tc->path));

  // Arbitrary keys are supported and published as TOOLCHAIN_<KEY>
  IW_TEST_ASSERT_STR_EQUAL(toolchain_var_get(tc, "sysroot"), "/nonexistent/ndk/sysroot");
  IW_TEST_ASSERT_STR_EQUAL(toolchain_var_get(tc, "SYSROOT"), "/nonexistent/ndk/sysroot");
  IW_TEST_ASSERT_STR_EQUAL(toolchain_var_get(tc, "ARCH-ALT"), "arm64");
  IW_TEST_ASSERT_STR_EQUAL(_var_name(tc, "sysroot"), "TOOLCHAIN_SYSROOT");
  IW_TEST_ASSERT_PTR_NULL(toolchain_var_get(tc, "extra"));
  IW_TEST_ASSERT_PTR_NULL(toolchain_var_get(tc, "unknown-key"));
}

static void test_conditional_keys(void) {
  load_basic();

  // Conditional descriptor keys
  setenv("TEST_TOOLCHAIN_EXTRA", "1", 1);
  struct toolchain *tc2 = toolchain_load(g_env.pool, "basic.autark");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(tc2);
  IW_TEST_ASSERT_STR_EQUAL(toolchain_var_get(tc2, "extra"), "extra-value");
  unsetenv("TEST_TOOLCHAIN_EXTRA");
}

static void test_apply(void) {
  struct toolchain *tc = load_basic();

  toolchain_apply(tc);
  IW_TEST_ASSERT_STR_EQUAL(toolchain_id(), "test-arm64");

  struct unit *root = unit_root();
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(root);
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_CROSS"), "1");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_TARGET"), "test-arm64");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_NAME"), "android");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_ANDROID"), "1");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_ARCH"), "aarch64");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_ARCH_ALT"), "arm64");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_BITNESS_64"), "1");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "SYSTEM_LITTLE_ENDIAN"), "1");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "CFLAGS_TARGET"), "-DONE -DTWO=2");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "TOOLCHAIN_SYSROOT"), "/nonexistent/ndk/sysroot");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "HOST_CC"), "cc");
  IW_TEST_ASSERT_STR_EQUAL(unit_env_get_raw(root, "AUTARK_TARGET"), "test-arm64");
  IW_TEST_ASSERT_STR_EQUAL(getenv(AUTARK_TARGET_ENV), "test-arm64");
  IW_TEST_ASSERT_STR_EQUAL(getenv("SYSTEM_CROSS"), "1");
  IW_TEST_ASSERT_STR_EQUAL(getenv("TOOLCHAIN_SYSROOT"), "/nonexistent/ndk/sysroot");
}

static void test_object_tags(void) {
  struct toolchain *tc = load_basic();
  toolchain_apply(tc);

  char tag[128];
  IW_TEST_ASSERT_EQUAL_FATAL(toolchain_object_tag_write("cross-test.o", toolchain_object_tag(false)), 0);
  IW_TEST_ASSERT_PTR_NULL(toolchain_object_check("cross-test.o", false, tag, sizeof(tag)));
  IW_TEST_ASSERT_PTR_NOT_NULL(toolchain_object_check("cross-test.o", true, tag, sizeof(tag)));
  IW_TEST_ASSERT_STR_EQUAL(tag, "test-arm64");

  IW_TEST_ASSERT_EQUAL_FATAL(toolchain_object_tag_write("cross-test.o", toolchain_object_tag(true)), 0);
  IW_TEST_ASSERT_PTR_NULL(toolchain_object_check("cross-test.o", true, tag, sizeof(tag)));
  IW_TEST_ASSERT_PTR_NOT_NULL(toolchain_object_check("cross-test.o", false, tag, sizeof(tag)));
  IW_TEST_ASSERT_STR_EQUAL(tag, TOOLCHAIN_TAG_HOST);

  IW_TEST_ASSERT_EQUAL_FATAL(toolchain_object_tag_write("cross-test.o", 0), 0);
  IW_TEST_ASSERT_PTR_NULL(toolchain_object_check("cross-test.o", true, tag, sizeof(tag)));
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_descriptor),
  IW_TEST_CASE(test_conditional_keys),
  IW_TEST_CASE(test_apply),
  IW_TEST_CASE(test_object_tags),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test13", cases, suite_init, 0));
}
