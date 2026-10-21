#!/bin/sh
# Fake cross compiler used by tests/cross/run-tests.sh.
# It records every invocation and forwards the compilation to the host compiler.
if [ -n "${CROSS_CC_LOG:-}" ]; then
  echo "$*" >> "$CROSS_CC_LOG"
fi
exec "${CROSS_CC_REAL:-cc}" "$@"
