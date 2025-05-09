#!/bin/sh
set -e

[ -z "$1" ] && exit 1
[ -z "$2" ] && exit 1

echo "$1" > run1-product1.txt
echo "$2" > run1-product2.txt