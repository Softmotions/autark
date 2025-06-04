#!/bin/sh
set -e

cd "$(cd "$(dirname "$0")" pwd -P)"

VERSION=0.98
F=./autark.c

REV="$(git rev-parse --short HEAD)"
echo "#define VERSION \"${VERSION} (${REV})\"" > ${F}

cat <<EOF >> ${F}
#define _AMALGAMATE_
#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
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
cat ./autark_core.c >> ${F}
cat ./main.c >> ${F}

cat ./script_impl.h >> ${F}
leg -P ./scriptx.leg >> ${F}
cat ./script.c >> ${F}
