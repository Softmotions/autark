#!/bin/sh
set -e

FILE=./check_file
[ ! -e $FILE ] && touch $FILE

SIZE=$(ls -s $FILE | awk '{print $1}')
echo "$FILE size ${SIZE}"

#autark set SIZE=${SIZE}