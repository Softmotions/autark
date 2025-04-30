#!/bin/sh

set -e
set -x

autark set KEY1=VAL1
autark set KEY2=VAL2
autark dep ./test-file.txt
touch ./test-file.txt