#!/bin/sh
set -e

cd "$(cd "$(dirname "$0")"; pwd -P)"

mkdir -p ./dist
rm -f ./dist/autark.c
rm -f ./dist/build.sh

F=./dist/autark.c
B=./dist/build.sh

if [ -f ./autark-cache/config.h -a -f ./autark-cache/version.sh ]; then
  . ./autark-cache/version.sh
  cat ./autark-cache/config.h > ${F}
else
  META_VERSION="${META_VERSION:-dev}"
  META_REVISION="$(git rev-parse --short HEAD)"
  echo "#define META_VERSION \"${META_VERSION}\"" > ${F}
  echo "#define META_REVISION \"${META_REVISION}\"" >> ${F}
  echo "#define _POSIX_C_SOURCE 200809L" >> ${F}
fi

cat <<'EOF' >> ${F}
#define _AMALGAMATE_
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>
#include <glob.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
EOF

cat ./basedefs.h >> ${F}
cat ./log.h >> ${F}
cat ./alloc.h >> ${F}
cat ./pool.h >> ${F}
cat ./xstr.h >> ${F}
cat ./ulist.h >> ${F}
cat ./map.h >> ${F}
cat ./utils.h >> ${F}
cat ./spawn.h >> ${F}
cat ./paths.h >> ${F}
cat ./env.h >> ${F}
cat ./deps.h >> ${F}
cat ./nodes.h >> ${F}
cat ./autark.h >> ${F}
cat ./script.h >> ${F}

cat ./xstr.c >> ${F}
cat ./ulist.c >> ${F}
cat ./pool.c >> ${F}
cat ./log.c >> ${F}
cat ./map.c >> ${F}
cat ./utils.c >> ${F}
cat ./paths.c >> ${F}
cat ./spawn.c >> ${F}
cat ./deps.c >> ${F}
cat ./node_script.c >> ${F}
cat ./node_check.c >> ${F}
cat ./node_set.c >> ${F}
cat ./node_subst.c >> ${F}
cat ./node_if.c >> ${F}
cat ./node_join.c >> ${F}
cat ./node_meta.c >> ${F}
cat ./node_include.c >> ${F}
cat ./node_run.c >> ${F}
cat ./node_configure.c >> ${F}
cat ./node_cc.c >> ${F}
cat ./node_basename.c >> ${F}
cat ./node_foreach.c >> ${F}
cat ./node_in_sources.c >> ${F}
cat ./node_dir.c >> ${F}
cat ./node_option.c >> ${F}
cat ./node_error.c >> ${F}
cat ./node_echo.c >> ${F}
cat ./node_install.c >> ${F}
cat ./node_find.c >> ${F}
cat ./autark_core.c >> ${F}
cat ./main.c >> ${F}

cat ./script_impl.h >> ${F}
leg -P ./scriptx.leg >> ${F}
cat ./script.c >> ${F}

# Make a build script

cat<<EOF > ${B}
#!/bin/sh
# MIT License
# Autark (https://autark.dev) build system script wrapper.
# Copyright (c) 2012-2025 Softmotions Ltd <info@softmotions.com>
#
# https://github.com/Softmotions/autark

META_VERSION=${META_VERSION}
META_REVISION=${META_REVISION}
EOF

cat <<'EOF' >> ${B}
cd "$(cd "$(dirname "$0")"; pwd -P)"

prev_arg=""
for arg in "$@"; do
  case "$prev_arg" in
    -H)
      AUTARK_HOME="${arg}/.autark-dist"
      prev_arg="" # сброс
      continue
      ;;
  esac

  case "$arg" in
    -H)
      prev_arg="-H"
      ;;
    -H*)
      AUTARK_HOME="${arg#-H}/.autark-dist"
      ;;
    --cache=*)
      AUTARK_HOME="${arg#--cache=}/.autark-dist"
      ;;
    *)
      prev_arg=""
      ;;
  esac
done

export AUTARK_HOME=${AUTARK_HOME:-autark-cache/.autark-dist}
AUTARK=${AUTARK_HOME}/autark

if [ "${META_VERSION}.${META_REVISION}" = "$(${AUTARK} -v)" ]; then
  ${AUTARK} "$@"
  exit $?
fi

set -e
echo "Building Autark executable in ${AUTARK_HOME}"

if [ -n "$CC" ] && command -v "$CC" >/dev/null 2>&1; then
  COMPILER="$CC"
elif command -v clang >/dev/null 2>&1; then
  COMPILER=clang
elif command -v gcc >/dev/null 2>&1; then
  COMPILER=gcc
else
  echo "No suitable C compiler found" >&2
  exit 1
fi

mkdir -p ${AUTARK_HOME}
cat <<'a292effa503b' > ${AUTARK_HOME}/autark.c
EOF

cat ${F} | sed -r '/^\s*$/d' >> ${B}
echo 'a292effa503b' >> ${B}

cat <<'EOF' >> ${B}
if grep -E '^ID(_LIKE)?=.*debian' /etc/os-release >/dev/null; then
  CFLAGS="-DDEBIAN_MULTIARCH"
fi

(set -x; ${COMPILER} ${AUTARK_HOME}/autark.c --std=c99 -O1 -march=native ${CFLAGS} -o ${AUTARK_HOME}/autark)
cp $0 ${AUTARK_HOME}/build.sh
echo "Done"

set +e
${AUTARK} "$@"
exit $?
EOF

chmod u+x ${B}
ln -sf ${B} ./build.sh