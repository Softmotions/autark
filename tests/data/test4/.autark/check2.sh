#!/bin/sh
set -e

CF=./check_file
[ ! -e $CF ] && touch $CF

SIZE=$(ls -s $CF | awk '{print $1}')
echo "$CF size $SIZE"

#autark set CF_SIZE=${SIZE}