#!/bin/sh
cd "$(cd "$(dirname "$0")"; pwd -P)"
mkdir -p ../dist/docs


cat ../README.md \
  | sed 's/```cfg/```autark/' \
  | pandoc --from gfm --standalone \
       --syntax-definition=./autark.xml \
       --highlight-style=./autark.theme \
       --metadata title=Autark \
       --metadata 'pagetitle=Autark - Self-contained C/C++ Build System' \
       --metadata lang=en \
       --include-in-header=./autark-css.html \
       -o ../dist/docs/index.html