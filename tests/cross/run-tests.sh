#!/bin/sh
#
# End-to-end tests of the Autark cross compilation feature.
#
# Usage: sh run-tests.sh <autark-binary> <fixtures-dir> <system.sh> <work-dir>
#
set -eu

AUTARK=$1
FIXTURES=$2
SYSTEM_SH=$3
WORK=$4

# Nested builds must use the executable under test, not a bootstrap binary
# inherited from the outer build environment.
AUTARK=$(cd "$(dirname "$AUTARK")" && pwd)/$(basename "$AUTARK")
AUTARK_HOME=$(dirname "$AUTARK")
export AUTARK_HOME

PROJ=
CROSS_LOG=

fail() {
  echo "cross-tests: FAIL: $*" >&2
  exit 1
}

prepare_project() {
  PROJ=$1
  rm -rf "$PROJ"
  mkdir -p "$PROJ/.autark" "$PROJ/dep/.autark"
  cp -R "$FIXTURES/hello/." "$PROJ/"
  cp "$SYSTEM_SH" "$PROJ/.autark/system.sh"
  cp "$SYSTEM_SH" "$PROJ/dep/.autark/system.sh"
  CROSS_LOG="$PROJ/cross-cc.log"
  : > "$CROSS_LOG"
}

# build <log> [autark args...]
build() {
  log=$1
  shift
  (cd "$PROJ" && HOME="$WORK/home" CROSS_CC_LOG="$CROSS_LOG" AUTARK_HOME="$AUTARK_HOME" "$AUTARK" "$@") > "$log" 2>&1
}

expect() {
  grep -q -e "$1" "$2" || fail "$3"
}

expect_not() {
  if grep -q -e "$1" "$2"; then
    fail "$3"
  fi
}

tag_of() {
  test -f "$1.autark-tag" || fail "missing object tag: $1.autark-tag"
  cat "$1.autark-tag"
}

mkdir -p "$WORK/home"
rm -rf "$WORK"/*.log

# A C++ host tool build is tested only when a host C++ compiler is available
CXX_ARGS=
if printf 'int main() { return 0; }\n' > "$WORK/cxx-probe.cxx" \
  && "${CXX:-c++}" "$WORK/cxx-probe.cxx" -o "$WORK/cxx-probe" 2>/dev/null; then
  CXX_ARGS=-DBUILD_CXX_TOOL=1
fi

# ---------------------------------------------------------------------------
# 1. Native build must be unchanged (backward compatibility)
# ---------------------------------------------------------------------------
prepare_project "$WORK/native"
build "$WORK/native.log" || fail "native build failed: see $WORK/native.log"
expect 'CROSS= NAME=linux' "$WORK/native.log" "native build reports no target toolchain"
expect 'TARGET= SYSROOT=' "$WORK/native.log" "native build declares no target facts"
if [ -f "$PROJ/autark-cache/main.o.autark-tag" ]; then
  fail "native target object is tagged with a toolchain"
fi
test "$(tag_of "$PROJ/autark-cache/host-tools/gen.o")" = "host" || fail "native host object tag"
expect '^CC=' "$PROJ/autark-cache/host-env.txt" "native host tool environment has no C compiler"
expect '^CXX=' "$PROJ/autark-cache/host-env.txt" "native host tool environment has no C++ compiler"
expect_not '-DHOST_TOOL_' "$PROJ/autark-cache/host-env.txt" \
  "native host tool environment inherits descriptor flags"
expect_not 'host-tool-flags=' "$PROJ/autark-cache/generated.c" \
  "native host build tool inherits descriptor flags"
"$PROJ/autark-cache/app" > "$WORK/native-app.log" || fail "native app failed to run"
expect 'value=49' "$WORK/native-app.log" "native app output"

# ---------------------------------------------------------------------------
# 2. Cross build: declared facts, target compiler, host build tool
# ---------------------------------------------------------------------------
prepare_project "$WORK/cross"
build "$WORK/cross.log" --toolchain toolchain/fake-a.cfg $CXX_ARGS || fail "cross build failed: see $WORK/cross.log"
expect 'CROSS=1 NAME=android ARCH=aarch64 ARCH_ALT=arm64 BITS=64 BITS64=1 ENDIAN=little LE=1 BE= TARGET=fake-aarch64' "$WORK/cross.log" \
  "declared target facts are exposed to the build script"
expect 'TARGET=fake-aarch64' "$WORK/cross.log" "target id is exposed"
expect 'SYSROOT=/opt/fake/sysroot' "$WORK/cross.log" "custom toolchain keys are exposed"
expect 'CFLAGS_TARGET=-DFAKE_TARGET=1' "$WORK/cross.log" "target compiler flags are exposed"
expect "CC=$PROJ/toolchain/cross-cc.sh" "$WORK/cross.log" "target compiler comes from the descriptor"
expect 'HOST_CC=cc' "$WORK/cross.log" "host compiler comes from the descriptor"
expect 'Cross compiling for target: fake-aarch64' "$WORK/cross.log" "system.sh uses the cross branch"
expect '-DFAKE_TARGET=1' "$CROSS_LOG" "target compiler is invoked with declared target flags"
expect '-c ../main.c' "$CROSS_LOG" "target sources are cross compiled"

test -x "$PROJ/autark-cache/app" || fail "cross build produced no app"
"$PROJ/autark-cache/app" > "$WORK/cross-app.log" || fail "cross app failed to run"
expect 'value=49' "$WORK/cross-app.log" "cross app output"

test -d "$PROJ/autark-cache/host-tools" || fail "host build tool objects are not namespaced"
test "$(tag_of "$PROJ/autark-cache/main.o")" = "fake-aarch64" || fail "target object tag"
test "$(tag_of "$PROJ/autark-cache/host-tools/gen.o")" = "host" || fail "host object tag"
expect '^CFLAGS=-DHOST_TOOL_BUILD=1$' "$PROJ/autark-cache/host-env.txt" \
  "HOST_CFLAGS is not exported to host build tools"
expect '^CXXFLAGS=-DHOST_TOOL_CXX=1$' "$PROJ/autark-cache/host-env.txt" \
  "HOST_CXXFLAGS is not exported to host build tools"
expect '^CC=cc$' "$PROJ/autark-cache/host-env.txt" "host build tools must use HOST_CC"
expect 'host-tool-flags=host-cflags' "$PROJ/autark-cache/generated.c" \
  "host-cc does not use HOST_CFLAGS"
expect_not 'FAKE_TARGET' "$PROJ/autark-cache/generated.c" \
  "host build tool inherits target compiler flags"

# Nested build inherits the target context
expect 'DEP_CROSS=1 DEP_NAME=android DEP_ARCH=aarch64 DEP_TARGET=fake-aarch64' "$WORK/cross.log" \
  "nested build inherits the target context"
expect "DEP_CC=$PROJ/toolchain/cross-cc.sh" "$WORK/cross.log" "nested build uses the target compiler"
test "$(tag_of "$PROJ/autark-cache/nested/dep.o")" = "fake-aarch64" || fail "nested object tag"

if [ -n "$CXX_ARGS" ]; then
  test "$(tag_of "$PROJ/autark-cache/host-tools/genxx.o")" = "host" || fail "host C++ object tag"
  expect 'host-cxxflags=yes' "$PROJ/autark-cache/cxx-tool-info.txt" \
    "host-cxx does not use HOST_CXXFLAGS"
fi

# ---------------------------------------------------------------------------
# 3. Incremental build must not recompile anything
# ---------------------------------------------------------------------------
build "$WORK/cross-incremental.log" --toolchain toolchain/fake-a.cfg || fail "incremental cross build failed"
expect_not '-MMD' "$WORK/cross-incremental.log" "incremental cross build recompiled sources"

# Editing the toolchain descriptor invalidates cached check results
sleep 1
touch "$PROJ/toolchain/fake-a.cfg"
build "$WORK/cross-touched.log" --toolchain toolchain/fake-a.cfg || fail "build after toolchain edit failed"
expect 'Cross compiling for target: fake-aarch64' "$WORK/cross-touched.log" \
  "toolchain descriptor edit did not rerun the system check"

# ---------------------------------------------------------------------------
# 4. Switching the target in the same cache invalidates target artifacts
# ---------------------------------------------------------------------------
: > "$CROSS_LOG"
build "$WORK/cross-switch.log" --toolchain toolchain/fake-b.cfg || fail "target switch build failed"
expect 'CROSS=1 NAME=linux ARCH=x86_64' "$WORK/cross-switch.log" "switched target facts"
expect '-DFAKE_TARGET=2' "$CROSS_LOG" "target switch recompiled with the new target flags"
test "$(tag_of "$PROJ/autark-cache/main.o")" = "fake-x86-64" || fail "object tag after target switch"

# ---------------------------------------------------------------------------
# 5. Host and target objects must never be mixed
# ---------------------------------------------------------------------------
prepare_project "$WORK/mixed"
if build "$WORK/mixed-target.log" --toolchain toolchain/fake-a.cfg -DBAD_LINK=1; then
  fail "linking a host object into a target artifact was allowed"
fi
expect 'Refusing to link object' "$WORK/mixed-target.log" "no diagnostic for target/host object mix"

prepare_project "$WORK/mixed"
if build "$WORK/mixed-host.log" --toolchain toolchain/fake-a.cfg -DBAD_HOST_LINK=1; then
  fail "linking a target object into a host build tool was allowed"
fi
expect 'Refusing to link object' "$WORK/mixed-host.log" "no diagnostic for host/target object mix"

# ---------------------------------------------------------------------------
# 6. Toolchain can be selected by AUTARK_TOOLCHAIN only (nested build channel)
# ---------------------------------------------------------------------------
prepare_project "$WORK/env"
(cd "$PROJ" && HOME="$WORK/home" CROSS_CC_LOG="$CROSS_LOG" AUTARK_HOME="$AUTARK_HOME" \
  AUTARK_TOOLCHAIN="$PROJ/toolchain/fake-a.cfg" "$AUTARK") > "$WORK/env.log" 2>&1 \
  || fail "build with AUTARK_TOOLCHAIN failed: see $WORK/env.log"
expect 'CROSS=1 NAME=android ARCH=aarch64' "$WORK/env.log" "AUTARK_TOOLCHAIN is not honored"

# ---------------------------------------------------------------------------
# 6b. Target facts can be declared by SYSTEM_* environment variables only
# ---------------------------------------------------------------------------
prepare_project "$WORK/envonly"
(cd "$PROJ" && HOME="$WORK/home" CROSS_CC_LOG="$CROSS_LOG" AUTARK_HOME="$AUTARK_HOME" \
  SYSTEM_CROSS=1 SYSTEM_NAME=android SYSTEM_ARCH=aarch64 SYSTEM_ARCH_ALT=arm64 \
  SYSTEM_BITNESS=64 SYSTEM_ENDIAN=little CC="$PROJ/toolchain/cross-cc.sh" AR=ar \
  "$AUTARK") > "$WORK/envonly.log" 2>&1 \
  || fail "build with declared SYSTEM_* variables failed: see $WORK/envonly.log"
expect 'CROSS=1 NAME=android ARCH=aarch64 ARCH_ALT=arm64 BITS=64 BITS64=1 ENDIAN=little LE=1' \
  "$WORK/envonly.log" "SYSTEM_* environment declarations are not honored"
expect 'TARGET= SYSROOT=' "$WORK/envonly.log" "environment declared build has no descriptor"
expect '-c ../main.c' "$CROSS_LOG" "environment declared target compiler was not used"

# ---------------------------------------------------------------------------
# 7. Declared runner turns declared facts into measured ones
# ---------------------------------------------------------------------------
prepare_project "$WORK/runner"
build "$WORK/runner.log" --toolchain toolchain/runner.cfg || fail "runner build failed: see $WORK/runner.log"
expect 'TARGET=fake-measured' "$WORK/runner.log" "runner target id"
expect 'BITS=64 BITS64=1 ENDIAN=little LE=1 BE= TARGET=fake-measured' "$WORK/runner.log" \
  "target probe through the runner did not override declared facts"

# ---------------------------------------------------------------------------
# 8. Broken descriptors are reported
# ---------------------------------------------------------------------------
prepare_project "$WORK/broken"
if build "$WORK/missing.log" --toolchain toolchain/nonexistent.cfg; then
  fail "missing toolchain descriptor was accepted"
fi
expect 'Toolchain descriptor not found' "$WORK/missing.log" "no diagnostic for a missing descriptor"

printf 'not-a-toolchain { id { x } }\n' > "$PROJ/not-a-toolchain.cfg"
if build "$WORK/not-a-toolchain.log" --toolchain not-a-toolchain.cfg; then
  fail "descriptor without a toolchain rule was accepted"
fi
expect 'has no `toolchain { }` rule' "$WORK/not-a-toolchain.log" "no diagnostic for a descriptor without a toolchain rule"

echo "cross-tests: all checks passed"
