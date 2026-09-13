#!/bin/sh

set -e

if grep -E '^ID(_LIKE)?=.*debian' /etc/os-release >/dev/null; then
  autark set "DEBIAN_MULTIARCH=1"
fi

autark set "AR=ar"