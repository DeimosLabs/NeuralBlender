#!/bin/bash

lv2dir="`dirname $0`"
lv2dir="`readlink -f "$lv2dir"`"
destdir="`pwd -P`"
destdir="`readlink -f "$destdir"`"
srcdir="`readlink -f "$lv2dir/.."`"
include_ui="$1"

#echo "$0: got args $*"
echo "$0: srcdir='$srcdir'"
echo "$0: destdir='$destdir'"

cd "$srcdir/lv2" || {
  echo "can't find '$srcdir/lv2'"
  exit 1
}

mkdir -p "$destdir/lv2"
echo -n "$0 cwd:"; pwd -P
for file in *.ttl.in; do
  if [ "$include_ui" = 1 ]; then
    cat "$file" | sed 's,^@@UI,,' > "$destdir/lv2/${file%.in}"
  else
    cat "$file" | grep -v '^@@UI' > "$destdir/lv2/${file%.in}"
  fi
done
