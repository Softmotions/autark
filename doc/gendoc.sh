#!/bin/sh
cd "$(cd "$(dirname "$0")"; pwd -P)"
mkdir -p ../dist/docs
pandoc --from gfm --standalone --highlight-style=monochrome --metadata title=Autark --include-in-header=./autark-css.html -o ../dist/docs/index.html  ../README.md