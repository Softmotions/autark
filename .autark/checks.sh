#!/bin/sh

#--- Leg

LEG=$(command -v leg) || { echo "leg not found"; exit 1; }
VERSION=$("$LEG" -V 2>/dev/null | awk '{ print $3 }')

case "$VERSION" in
  0.1.*) echo "leg version $VERSION is acceptable" ;;
  *) echo "leg version $VERSION is not in the 0.1.x range"; exit 1 ;;
esac

set -e

if grep -E '^ID(_LIKE)?=.*debian' /etc/os-release >/dev/null; then
  autark set "DEBIAN_MULTIARCH=1"
fi

autark set "LEG=${LEG}"
autark set "AR=ar"