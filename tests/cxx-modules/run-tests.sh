#!/bin/sh
#
# End-to-end tests of the C++20 modules compilation support.
#
# Usage: sh run-tests.sh <autark-binary> <work-dir>
#
# Any compiler from the CXX_MODULES_TEST_COMPILERS list (default: clang++ g++)
# that supports modules is tested. The script succeeds when no compiler supports
# modules, so it can run on systems with older toolchains.
#
set -eu

AUTARK=$1
WORK=$2

fail() {
  echo "cxx-modules-tests: FAIL: $*" >&2
  exit 1
}

expect() {
  grep -q -E -e "$1" "$2" || fail "$3"
}

expect_not() {
  if grep -q -E -e "$1" "$2"; then
    fail "$3"
  fi
}

AUTARK=$(cd "$(dirname "$AUTARK")" && pwd)/$(basename "$AUTARK")
[ -x "$AUTARK" ] || fail "autark binary not found: $AUTARK"

mkdir -p "$WORK"
rm -rf "$WORK"/*

TEST_CC=
TEST_STYLE=
PROJ=

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# probe <style> <compiler>
probe() {
  style=$1
  cc=$2
  d="$WORK/probe-$style"
  rm -rf "$d"
  mkdir -p "$d"
  (
    cd "$d" || exit 1
    printf 'export module m;\nexport int f();\n' > m.cppm
    printf 'module m;\nint f() { return 1; }\n' > m_impl.cpp
    printf 'import m;\nint main() { return f() - 1; }\n' > main.cpp
    case $style in
      clang)
        "$cc" -std=c++20 -x c++-module -fmodule-output=m.pcm -c m.cppm -o m.o &&
        "$cc" -std=c++20 -fprebuilt-module-path=. -x c++ -c m_impl.cpp -o m_impl.o &&
        "$cc" -std=c++20 -fprebuilt-module-path=. -c main.cpp -o main.o &&
        "$cc" main.o m.o m_impl.o -o app && ./app
        ;;
      gcc)
        printf 'm m.gcm\n' > modules.map
        "$cc" -std=c++20 -fmodules-ts -fmodule-mapper=modules.map -x c++ -c m.cppm -o m.o &&
        "$cc" -std=c++20 -fmodules-ts -fmodule-mapper=modules.map -x c++ -c m_impl.cpp -o m_impl.o &&
        "$cc" -std=c++20 -fmodules-ts -fmodule-mapper=modules.map -x c++ -c main.cpp -o main.o &&
        "$cc" main.o m.o m_impl.o -o app && ./app
        ;;
    esac
  ) > "$WORK/probe-$style.log" 2>&1
}

# build <log> [autark args...]
build() {
  log=$1
  shift
  ( cd "$PROJ" && CXX="$TEST_CC" \
      CXX_MODULE_STYLE="$TEST_STYLE" \
      MOD_CXX_REAL="$TEST_CC" \
      MOD_CXX_LOG="$PROJ/mod-cxx.log" \
      "$AUTARK" "$@" ) > "$log" 2>&1
}

# prepare_project <dir>
prepare_project() {
  proj=$1
  rm -rf "$proj"
  mkdir -p "$proj/toolchain"

  cat > "$proj/Autark" <<'EOF'
set { CXXFLAGS -std=c++20 ..${CXXFLAGS} }
set { CXX ${CXX c++} }
set { HOST_CXX ${HOST_CXX ${CXX}} }
set { SOURCES base.cppm base_impl.cppm derived.cppm main.cpp }

cxx {
  ${SOURCES}
  ${CXXFLAGS}
  ${CXX}
}

run {
  exec { ${CXX} -o app ${CXX_OBJS} }
  consumes { ${CXX_OBJS} }
  produces { app }
}

set { TOOL_SOURCES toolmod.cppm toolimpl.cpp toolmain.cpp }
host-cxx {
  ${TOOL_SOURCES}
  ${CXXFLAGS}
  ${HOST_CXX}
  objects { TOOL_OBJS }
}

run {
  host
  exec { ${HOST_CXX} -o tool ${TOOL_OBJS} }
  consumes { ${TOOL_OBJS} }
  produces { tool }
}
EOF

  cat > "$proj/base.cppm" <<'EOF'
export module base;
export int b();
EOF
  cat > "$proj/base_impl.cppm" <<'EOF'
module base;
int b() { return 10; }
EOF
  cat > "$proj/derived.cppm" <<'EOF'
export module derived;
export import base;
export int d() { return b() + 5; }
EOF
  cat > "$proj/main.cpp" <<'EOF'
import derived;
#include <cstdio>
int main() { std::printf("%d\n", d()); }
EOF
  cat > "$proj/toolmod.cppm" <<'EOF'
export module toolmod;
export int t();
EOF
  cat > "$proj/toolimpl.cpp" <<'EOF'
module toolmod;
int t() { return 1; }
EOF
  cat > "$proj/toolmain.cpp" <<'EOF'
import toolmod;
int main() { return t() - 1; }
EOF

  cat > "$proj/toolchain/cxx.sh" <<'EOF'
#!/bin/sh
[ -n "${MOD_CXX_LOG:-}" ] && echo "$*" >> "$MOD_CXX_LOG"
exec "${MOD_CXX_REAL:?}" "$@"
EOF
  chmod +x "$proj/toolchain/cxx.sh"
  cat > "$proj/toolchain/mod-a.cfg" <<'EOF'
toolchain {
  id { modtarget-a }
  system { linux }
  arch { x86_64 }
  bits { 64 }
  endian { little }
  cxx { SS{cxx.sh} }
  host-cxx { SS{cxx.sh} }
  ar { ar }
}
EOF
  sed 's/modtarget-a/modtarget-b/' "$proj/toolchain/mod-a.cfg" > "$proj/toolchain/mod-b.cfg"
}

# expect_app <project> <expected output>
expect_app() {
  "$1/autark-cache/app" > "$WORK/app.out" 2>&1 || fail "app failed: see $WORK/app.out"
  expect "$2" "$WORK/app.out" "unexpected app output"
}

# ---------------------------------------------------------------------------
# Native build tests
# ---------------------------------------------------------------------------
native_tests() {
  case $TEST_STYLE in
    clang) bmi_ext=pcm ;;
    gcc)   bmi_ext=gcm ;;
  esac
  tag=$TEST_STYLE
  PROJ="$WORK/native-$tag"
  prepare_project "$PROJ"

  # 1. Clean build of a module chain, an implementation unit and a host tool
  build "$WORK/native-$tag.log" || fail "native build failed: see $WORK/native-$tag.log"
  expect 'Build successful' "$WORK/native-$tag.log" "native build failed"
  expect_app "$PROJ" 15
  "$PROJ/autark-cache/tool" || fail "host module build tool failed"

  test -f "$PROJ/autark-cache/.cxx-modules/base.$bmi_ext" || fail "no base BMI"
  test -f "$PROJ/autark-cache/.cxx-modules/derived.$bmi_ext" || fail "no derived BMI"
  test -f "$PROJ/autark-cache/.cxx-modules/host/toolmod.$bmi_ext" || fail "host BMI is not separated"
  test ! -f "$PROJ/autark-cache/base.o.autark-tag" || fail "native target object is tagged"
  test "$(cat "$PROJ/autark-cache/host-tools/toolmain.o.autark-tag")" = "host" \
    || fail "host tool object tag"

  # 2. Incremental build must not recompile anything
  build "$WORK/native-$tag-incremental.log"
  expect_not '-MMD' "$WORK/native-$tag-incremental.log" "incremental build recompiled sources"

  # 3. Editing an importer compiles only that source
  sleep 1
  touch "$PROJ/main.cpp"
  build "$WORK/native-$tag-importer.log"
  expect '\.\./main\.cpp' "$WORK/native-$tag-importer.log" "edited importer was not recompiled"
  expect_not '\.\./base\.cppm' "$WORK/native-$tag-importer.log" "interface unit was recompiled needlessly"
  expect_not '\.\./derived\.cppm' "$WORK/native-$tag-importer.log" "interface unit was recompiled needlessly"

  # 4. Editing an interface rebuilds the whole module group in one build
  sleep 1
  touch "$PROJ/base.cppm"
  build "$WORK/native-$tag-interface.log"
  expect '\.\./base\.cppm' "$WORK/native-$tag-interface.log" "edited interface was not recompiled"
  expect '\.\./derived\.cppm' "$WORK/native-$tag-interface.log" "dependent interface was not recompiled"
  expect '\.\./main\.cpp' "$WORK/native-$tag-interface.log" "importer was not recompiled"
  expect_app "$PROJ" 15
  build "$WORK/native-$tag-converged.log"
  expect_not '-MMD' "$WORK/native-$tag-converged.log" "build after an interface edit did not converge"

  # 5. A missing BMI invalidates the whole group
  rm -f "$PROJ/autark-cache/.cxx-modules/base.$bmi_ext"
  build "$WORK/native-$tag-bmi.log"
  expect '\.\./base\.cppm' "$WORK/native-$tag-bmi.log" "missing BMI was not rebuilt"
  expect_app "$PROJ" 15

  # 6. `modules { }` marks a non standard extension as module interface unit
  PROJ="$WORK/bag-$tag"
  prepare_project "$PROJ"
  cat > "$PROJ/Autark" <<'EOF'
set { CXXFLAGS -std=c++20 ..${CXXFLAGS} }
set { CXX ${CXX c++} }
set { SOURCES impl.cpp bagmain.cpp }
cxx {
  ${SOURCES}
  ${CXXFLAGS}
  ${CXX}
  modules { impl.cpp }
}
run {
  exec { ${CXX} -o app ${CXX_OBJS} }
  consumes { ${CXX_OBJS} }
  produces { app }
}
EOF
  printf 'export module bagmod;\nexport int v() { return 7; }\n' > "$PROJ/impl.cpp"
  printf 'import bagmod;\n#include <cstdio>\nint main() { std::printf("%%d\\n", v()); }\n' > "$PROJ/bagmain.cpp"
  build "$WORK/bag-$tag.log" || fail "`modules { }` build failed: see $WORK/bag-$tag.log"
  expect_app "$PROJ" 7
  test -f "$PROJ/autark-cache/.cxx-modules/bagmod.$bmi_ext" || fail "no BMI for `modules { }` unit"

  # 7. Module partitions are reported as unsupported
  PROJ="$WORK/partition-$tag"
  prepare_project "$PROJ"
  printf 'export module part:sub;\nexport int p() { return 1; }\n' > "$PROJ/base.cppm"
  if build "$WORK/partition-$tag.log"; then
    fail "module partition unit was accepted"
  fi
  expect 'Module partition units are not supported' "$WORK/partition-$tag.log" \
    "no diagnostic for a module partition"

  # 8. Module units are rejected in C rules
  PROJ="$WORK/ccrule-$tag"
  prepare_project "$PROJ"
  cat > "$PROJ/Autark" <<'EOF'
set { CFLAGS -std=c99 ..${CFLAGS} }
set { CC ${CC cc} }
cc {
  base.cppm
  ${CFLAGS}
  ${CC}
}
EOF
  if build "$WORK/ccrule-$tag.log"; then
    fail "module unit in a cc rule was accepted"
  fi
  expect 'C\+\+ module units require a cxx rule' "$WORK/ccrule-$tag.log" \
    "no diagnostic for a module unit in a cc rule"

  # 9. A broken interface unit must not compile importers and must be retried
  PROJ="$WORK/retry-$tag"
  prepare_project "$PROJ"
  printf 'export module derived;\nexport import base;\nexport int d() { return b() + ; }\n' > "$PROJ/derived.cppm"
  if build "$WORK/retry-$tag-broken.log"; then
    fail "broken module interface was accepted"
  fi
  test ! -f "$PROJ/autark-cache/main.o" || fail "importer was compiled with a broken interface"
  printf 'export module derived;\nexport import base;\nexport int d() { return b() + 5; }\n' > "$PROJ/derived.cppm"
  build "$WORK/retry-$tag-fixed.log" || fail "build after fixing an interface failed"
  expect_app "$PROJ" 15
  build "$WORK/retry-$tag-converged.log"
  expect_not '-MMD' "$WORK/retry-$tag-converged.log" "build after a fixed interface did not converge"

  # 10. The compilation database exposes module flags (implies a clean rebuild)
  PROJ="$WORK/native-$tag"
  build "$WORK/native-$tag-cdb.log" --compile-commands \
    || fail "compile commands build failed: see $WORK/native-$tag-cdb.log"
  case $TEST_STYLE in
    clang) expect 'fprebuilt-module-path' "$PROJ/autark-cache/compile_commands.json" "no module path in CDB" ;;
    gcc)   expect 'fmodule-mapper' "$PROJ/autark-cache/compile_commands.json" "no module mapper in CDB" ;;
  esac
  expect '"-x"' "$PROJ/autark-cache/compile_commands.json" "module unit language flag is missing in CDB"
  expect_app "$PROJ" 15
}

# ---------------------------------------------------------------------------
# Toolchain (cross compilation) tests
# ---------------------------------------------------------------------------
toolchain_tests() {
  case $TEST_STYLE in
    clang) bmi_ext=pcm ;;
    gcc)   bmi_ext=gcm ;;
  esac
  tag=$TEST_STYLE
  PROJ="$WORK/toolchain-$tag"
  prepare_project "$PROJ"

  # 1. BMIs and objects are tag separated
  build "$WORK/toolchain-$tag-a.log" --toolchain toolchain/mod-a.cfg \
    || fail "toolchain build failed: see $WORK/toolchain-$tag-a.log"
  expect 'Build successful' "$WORK/toolchain-$tag-a.log" "toolchain build failed"
  expect_app "$PROJ" 15
  "$PROJ/autark-cache/tool" || fail "toolchain host module build tool failed"
  test -f "$PROJ/autark-cache/.cxx-modules/modtarget-a/base.$bmi_ext" || fail "no target BMI"
  test -f "$PROJ/autark-cache/.cxx-modules/host/toolmod.$bmi_ext" || fail "no host BMI"
  test ! -f "$PROJ/autark-cache/.cxx-modules/base.$bmi_ext" \
    || fail "target BMI is not separated from the native target"
  test "$(cat "$PROJ/autark-cache/main.o.autark-tag")" = "modtarget-a" || fail "target object tag"
  test "$(cat "$PROJ/autark-cache/host-tools/toolmain.o.autark-tag")" = "host" || fail "host object tag"
  expect 'fmodules-ts|fprebuilt-module-path' "$PROJ/mod-cxx.log" "no module compiler flags"

  # 2. Incremental toolchain build must not recompile anything
  build "$WORK/toolchain-$tag-incremental.log" --toolchain toolchain/mod-a.cfg
  expect_not '-MMD' "$WORK/toolchain-$tag-incremental.log" "incremental toolchain build recompiled sources"

  # 3. Switching the target keeps BMIs of both targets separated
  build "$WORK/toolchain-$tag-b.log" --toolchain toolchain/mod-b.cfg \
    || fail "target switch build failed: see $WORK/toolchain-$tag-b.log"
  expect_app "$PROJ" 15
  test -f "$PROJ/autark-cache/.cxx-modules/modtarget-b/base.$bmi_ext" || fail "no BMI of the switched target"
  test -f "$PROJ/autark-cache/.cxx-modules/modtarget-a/base.$bmi_ext" || fail "BMI of the previous target is lost"
  test "$(cat "$PROJ/autark-cache/main.o.autark-tag")" = "modtarget-b" || fail "object tag after target switch"
}

# ---------------------------------------------------------------------------
# Run tests for every supported compiler
# ---------------------------------------------------------------------------
COMPILERS=${CXX_MODULES_TEST_COMPILERS:-clang++ g++}
tested=0

for pair in $COMPILERS; do
  case $pair in
    *:*)
      style=${pair%%:*}
      cc=${pair#*:}
      ;;
    clang*)
      style=clang
      cc=$pair
      ;;
    *)
      style=gcc
      cc=$pair
      ;;
  esac
  command -v "$cc" > /dev/null 2>&1 || continue
  if ! probe "$style" "$cc"; then
    echo "cxx-modules-tests: $cc does not support modules, skipped"
    continue
  fi
  echo "cxx-modules-tests: testing $cc ($style style)"
  TEST_CC=$cc
  TEST_STYLE=$style
  native_tests
  toolchain_tests
  tested=$((tested + 1))
done

if [ "$tested" -eq 0 ]; then
  echo "cxx-modules-tests: no compiler with C++20 modules support found, skipped"
  exit 0
fi

echo "cxx-modules-tests: all checks passed"
