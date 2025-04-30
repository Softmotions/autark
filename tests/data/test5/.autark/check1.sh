#!/bin/sh

set -e
set -x

autark set KEY1=VAL1
autark set KEY2=VAL2
touch ./test-file.txt
autark dep ./test-file.txt