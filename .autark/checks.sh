#!/bin/sh

#--- Leg

LEG=$(command -v leg) || { echo "leg not found"; exit 1; }
VERSION=$("$LEG" -V 2>/dev/null | awk '{ print $3 }')

case "$VERSION" in
  0.1.*) echo "leg version $VERSION is acceptable" ;;
  *) echo "leg version $VERSION is not in the 0.1.x range"; exit 1 ;;
esac

set -e

autark set LEG="$LEG"
autark set CC="${CC:=cc}"
autark env CC