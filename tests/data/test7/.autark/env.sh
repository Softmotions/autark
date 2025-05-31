#!/bin/sh

set -e

which ar || { echo "'ar' not found in PATH"; exit 1; }
which ld || { echo "'ld' not dound in PATH"; exit 1; }

autark set AR=ar
autark set LD=ld
autark set BUILD_TYPE=Debug


