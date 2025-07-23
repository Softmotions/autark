#!/bin/sh
cd "$(cd "$(dirname "$0")"; pwd -P)"
pandoc --from gfm --standalone --highlight-style=monochrome --metadata title=Autark --include-in-header=./autark-css.html -o ./index.html  ../README.md