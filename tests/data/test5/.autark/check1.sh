#!/bin/sh

set -e
set -X

autark set VAR1=VAL1
autark dep ./test-file.txt