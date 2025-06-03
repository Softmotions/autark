#!/bin/sh

set -e

which ar || { echo "'ar' not found in PATH"; exit 1; }

autark set AR=ar
autark set BUILD_TYPE=Debug

