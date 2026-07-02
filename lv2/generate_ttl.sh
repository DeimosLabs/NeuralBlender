#!/bin/bash
# generates lv2/*.ttl from lv2/*.ttl.in in source dir. Lines starting 
# with @@UI in the .ttl.in files are included only if LV2 UI
# is enabled (without the "@@UI" token) and stripped otherwise.

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
  echo "$file -> $destdir/lv2/${file%.in}"
  if [ "$include_ui" = 1 ]; then
    sed 's,^[[:space:]]*@@UI,,' "$file" > "$destdir/lv2/${file%.in}"
  else
    grep -v '^[[:space:]]*@@UI' "$file" > "$destdir/lv2/${file%.in}"
  fi
done
