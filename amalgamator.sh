#!/bin/sh
set -e

cd "$(cd "$(dirname "$0")"; pwd -P)"

rm -rf ./dist
mkdir -p ./dist

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
fi

cat <<'EOF' >> ${F}
#define _AMALGAMATE_
#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
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
cat ./autark_core.c >> ${F}
cat ./main.c >> ${F}

cat ./script_impl.h >> ${F}
leg -P ./scriptx.leg >> ${F}
cat ./script.c >> ${F}

# Make a build script

cat<<EOF > ${B}
#!/bin/sh
# Autark build system script wrapper.

META_VERSION=${META_VERSION}
META_REVISION=${META_REVISION}
EOF

cat <<'EOF' >> ${B}
cd "$(cd "$(dirname "$0")"; pwd -P)"

export AUTARK_HOME=${AUTARK_HOME:-${HOME}/.autark}
AUTARK=${AUTARK_HOME}/autark

if [ "${META_VERSION}" = "$(${AUTARK} -v)" ]; then
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

(set -x; ${COMPILER} ${AUTARK_HOME}/autark.c --std=c99 -O1 -o ${AUTARK_HOME}/autark)
echo "Done"

set +e
${AUTARK} "$@"
exit $?
EOF

chmod u+x ${B}
ln -sf ${B} ./build.sh