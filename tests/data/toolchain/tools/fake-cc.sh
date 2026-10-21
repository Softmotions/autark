#!/bin/sh
# Fake cross compiler of the tests/data/toolchain descriptor.
# It just forwards the compilation to the host compiler.
exec "${FAKE_CC_REAL:-cc}" "$@"
