#!/bin/sh
set -e

[ -z "$1" ] && exit 1
[ -z "$2" ] && exit 1

[ "$1" = "$(cat ./run1-product1.txt)" ] || exit 2
[ "$2" = "$(cat ./run1-product2.txt)" ] || exit 2

echo "$1" > run2-product1.txt
echo "$2" > run2-product2.txt