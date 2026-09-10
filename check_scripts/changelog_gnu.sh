#!/bin/sh

# GNU style changelog parser.
#
# Example:
# 2026-09-09	Anton Adamansky  <adamansky@gmail.com>  [v1.3.2-dev]
#	* Autark: Project versions now track Changelog.
#	* src/Autark: macOS poller and shared-library builds - #11
# ...

set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 CHANGELOG" >&2
  exit 2
}

f=$1

[ -r "$f" ] || {
  echo "$0: cannot read: $f" >&2
  exit 1
}

autark dep "$f"

version=$(awk '
  /^[[:space:]]*[0-9]/ &&
  match($0, /\[[vV]?[0-9][^]]*\]/) {
    s = substr($0, RSTART + 1, RLENGTH - 2)
    sub(/^[vV]/, "", s)
    print s
    exit
  }
' "$f")

[ -n "$version" ] || {
  echo "$0: no changelog version found in $f" >&2
  exit 1
}

autark set "CHANGELOG_VERSION=$version"

base=${version%%-*}

case $version in
  *-*)
    flavor=${version#*-}
    [ -n "$flavor" ] &&
      autark set "CHANGELOG_VERSION_FLAVOR=$flavor"
    ;;
esac

set_num() {
  case $2 in
    ''|*[!0-9]*) ;;
    *) autark set "$1=$2" ;;
  esac
}

major=${base%%.*}
rest=${base#*.}
set_num CHANGELOG_VERSION_MAJOR "$major"

if [ "$rest" != "$base" ]; then
  minor=${rest%%.*}
  rest2=${rest#*.}
  set_num CHANGELOG_VERSION_MINOR "$minor"

  if [ "$rest2" != "$rest" ]; then
    patch=${rest2%%.*}
    set_num CHANGELOG_VERSION_PATCH "$patch"
  fi
fi

changelog=$(awk '
  function header() {
    return $0 ~ /^[[:space:]]*[0-9]/ &&
           match($0, /\[[vV]?[0-9][^]]*\]/)
  }

  header() {
    if (seen)
      exit
    seen = 1
    next
  }

  seen {
    s = $0
    sub(/\r$/, "", s)

    if (s ~ /^[[:space:]]*$/) {
      if (out)
        blanks++
      next
    }

    while (blanks > 0) {
      printf "\\n"
      blanks--
    }

    if (out)
      printf "\\n"

    for (i = 1; i <= length(s); i++) {
      c = substr(s, i, 1)
      if (c == "\\")
        printf "\\\\"
      else
        printf "%s", c
    }

    out = 1
  }
' "$f")

autark set "CHANGELOG=$changelog"