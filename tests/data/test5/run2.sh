#!/bin/sh
set -e
set -x

echo "!!! Run2 $1 $2"

[ -z "$1" ] && exit 1
[ -z "$2" ] && exit 1

echo "$1" > run2-product1.txt
echo "$2" > run2-product2.txt