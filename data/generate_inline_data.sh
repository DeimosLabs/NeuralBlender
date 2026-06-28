#!/bin/bash
# hackish way to generate data.c containing inline resources (icon files etc.)
# ADDED: also manages compiling .po files to gzipped .mo format

shopt -s lastpipe

# to test if this actually gets run by build system
#echo "$0: gratuitously wasting 10 seconds of your time..."
#sleep 10

do_language_files=0
#do_debug=1

case "$(basename "$(pwd)")" in
  data)
  ;;
  
  *)
    mkdir -p data 2>/dev/null
    cd data || {
      echo "can't create data directory"
      exit 1
    }
  ;;
esac

startwd=`pwd -P`
cname="data.cpp"
hname="data.h"
datadir="`dirname $0`"
datadir="`readlink -f "$datadir"`"
destdir="`pwd -P`"
destdir="`readlink -f "$destdir"`"
srcdir="`readlink -f "$datadir/.."`"
timestamp_script="${srcdir}/timestamp.sh"
data_c="$destdir/$cname"
data_h="$destdir/$hname"
listfile_in="`basename "$0"`"
listfile_in="${datadir}/${listfile_in%.sh}.list"
listfile="${destdir}/inline_list"
me="`readlink -f "$0"`"
baseme="`basename "$me"`"
langlistfile_in="$srcdir/po/LINGUAS" 
langlistfile="$destdir/language_list"
make_new_data_c="no"

echo
echo "$me: START"
echo "  Reading from directory: $datadir"
echo "  Generating data file:   $data_c"
echo "  Generating header file: $data_h"
#echo "in/out directories:"
#echo "  srcdir=$srcdir"
#echo "  datadir=$datadir"
#echo "  destdir=$destdir"
#echo "  data_c=$data_c"
#echo "  data_h=$data_h"
echo

# TODO: maybe include all files except $0, build files, etc?
[ -f "$listfile_in" ] || {
  echo "$0: $listfile_in found. exiting..."
  exit 1
}

debug () {
  [ "${do_debug%0}" != "" ] && echo "$@" >&2
}

remove_leading_prefix () {
  ##echo "remove_leading_prefix: got '$1'" > /dev/stderr
  #if [ "$1" != "" ]; then
  #  expr=`echo $1 | tr '/\?*\-%\$' "_"`
  #  #echo "expr=$expr" > /dev/stderr
  #  sed "s/$expr//g"
  #else
  #  cat
  #fi
  sed 's,_.*_data_,data_,g'
  #sed 's,.*data/,data_,g'
}

# this function expects an absolute path, all path checking/parsing arleady done
pack_one_file () {
  packfile="$1"
  
  [ -f "$packfile" ] || {
    echo "$packfile not found."
    return 1
  }
  
  if [ "$2" = "" ]; then
    echo "$baseme: adding file $packfile"
  else
    echo "$baseme: adding file $packfile with conditional compile '$2'"
  fi
  
  (
    #echo
    echo "/* auto-generated with xxd from $packfile */"
    #echo
    [ "$2" != "" ] && echo "$2"
    echo
    xxd -i "$packfile" | remove_leading_prefix "${packfile%$1}" || {
      echo "xxd FAILED to process '$packfile'" >&2
      exit 1
    }
    echo
    [ "$2" != "" ] && echo '#endif'
    echo
  ) >> "$data_c"
}

remove_comments () {   # NOTE: and empty lines, redundant space etc.
  #sed 's,#.*,,g' | grep -v '^$'
  sed -e 's,#.*,,g' -e 's,^[[:space:]]$,,g' | grep -v '^$' | tr ' \t' '\n'
}

cd "$datadir/.."

# language files
rm "$langlistfile" 2>/dev/null # from dest (build) dir
if [ "$do_language_files" = "1" ] && [ -r "$langlistfile_in" ]; then
  echo "Generating language translation catalogs:"
  cat "$langlistfile_in" | remove_comments | while read -r lang; do
    # generate catalog for this language. TODO: fix cases when we have more
    # files than just 'lang.po' and 'wxstd-$lang.po' - should be (sort of) straightforward
    if ! [ -r "$destdir/$lang.mo.gz" ] || [ "$destdir/$lang.mo.gz" -ot "po/${lang}.po" ] || \
       [ "$destdir/$lang.mo.gz" -ot "po/wxstd-${lang}.po" ]; then
      echo "  $lang..."
      (
        [ -f "po/${lang}.po" ] && {
          echo
          echo "# NOTE: This file is regenerated at each build by:"
          echo "#       $me"
          echo "#       Any changes might get discarded at next build."
          echo "#       (depending which language files are modified)"
          echo
          cat "po/${lang}.po"
        }
        [ -f "po/wxstd-${lang}.po" ] && { 
          echo
          echo "### Following is content from 'po/wxstd-${lang}.po'"
          echo
          cat "po/wxstd-${lang}.po"
        }
      ) > "$destdir/${lang}-all.po"
      make_new_data_c="yes"
    else
      echo "  $lang: up to date"
    fi
    
    if [ -f "$langlistfile" ]; then
      #echo -n ", \"$lang\"" >> "$langlistfile"
      (
        echo ","
        echo -n "  { data_${lang}_mo_gz, data_${lang}_mo_gz_len, \"$lang\" }"
      ) >> "$langlistfile"
    else
      #echo -n "const char *g_lang_list [] = { \"$lang\"" >> "$langlistfile"
      (
        # kind of hackish, but should work on all OS where bash works.
        echo "__t_language_data g_language_data [] = { "
        echo -n "  { data_${lang}_mo_gz, data_${lang}_mo_gz_len, \"$lang\" }"
      ) >> "$langlistfile"
    fi
    
    # next, compile into .mo file (gzipped) to be inlined directly.
    if [ "$make_new_data_c" = "yes" ]; then
      if msgfmt -c -o "$destdir/$lang.mo" "$destdir/$lang-all.po" && \
      cat "$destdir/$lang.mo" | gzip -9 > "$destdir/$lang.mo.gz" && \
      rm "$destdir/$lang.mo"; then
       echo "generated $destdir/$lang.mo.gz"
      else
       echo "FAILED to generate $destdir/$lang.mo.gz"
       exit 1
      fi
    fi
  done
  
  [ -f "$langlistfile" ] && (
    echo ","
    echo "  { 0, 0, 0 }"
    echo "};"
  ) >> "$langlistfile"
  
  echo "done."
else
  if [ "$do_language_files" = "1" ]; then
    (
    echo "const char *g_lang_list [] = {"
    echo "  { 0, 0, 0 }"
    echo "};"
    ) > "$langlistfile"
  fi
fi

# generate build-time list of files to inline, including .mo language files
# FIXME: any lingering .mo files in build dir we're no longer maintaining will
# get included.
ls "$destdir"/*.mo.gz > "$listfile" 2>/dev/null
cat "$listfile_in" | remove_comments | sed "s,$^,$datadir/," | while read -r file; do
  if [ "${file#/}" != "$file" ]; then
    # abs. path
    echo "$file"
  elif [ -f "${destdir}/$file" ]; then
    # relative path, file exists in dest dir
    echo "${destdir}/$file"
  else
    # default to file in src dir
    echo "${datadir}/$file"
  fi
done >> "$listfile"

#echo
#echo "contents of $listfile:"
#cat "$listfile"
#echo

# check timestamps, generate $listfile (incl. language .mo files) in the process
if [ "$make_new_data_c" != "yes" ]; then
  debug "checking if we need to regenerate $cname and $hname..."
  if ! [ -f "$data_c" ]; then
    echo "$cname doesn't exit in output dir, creating new"
    make_new_data_c="yes"
  elif ! [ -f "$data_h" ]; then
    echo "$hname doesn't exist in output dir, creating new"
    make_new_data_c="yes"
  elif [ "$data_h" -ot "$data_c" ] || [ "$data_c" -ot "$data_h" ]; then
    # both files SHOULD have the same timestamp, see 'touch' command below
    echo "$cname and $hname timestamp mismatch, creating new"
    make_new_data_c="yes"
  else
    cat "$listfile" | while read -r file; do
      if [ ! -f "$file" ]; then
        echo "$0: ERROR: can't find '$file', aborting."
        exit 1
      fi
      
      #echo "data_c='$data_c', file='$file'"
      if [ "$data_c" -ot "$file" ]; then
        echo "$file is newer than $data_c, re-generating"
        make_new_data_c="yes"
        break
      #else
      #  echo "$file is older than $data_c"
      fi
    done
  fi
else
  debug "make_new_data_c already is set to 'yes'"
fi

# if so, or data.{c,h} don't exist, regenerate it
if [ "$make_new_data_c" = "yes" ]; then

  # this goes at the top of the file
  (
    echo
    echo "/* `basename $data_c` auto-generated by $baseme */"
    echo
    echo "#include \"$hname\""
    echo
    [ "$do_language_files" = 1 ] && cat "$langlistfile"
    echo
  ) > "$data_c"

  # THIS IS WHERE WE ADD FILES TO OUR INLINE DATA
  # Usage: pack_one_file <path to file> <ifdef statement>
  # Path to file must be from toplevel of source tree.
  #
  # ADDED: this will check for a matching file in the
  # build tree and use that instead if found
  #
  # UPDATE 09/mar/2019: we're now using an external
  # list file instead of hard-coding filenames in this
  # script.
  #
  # UPDATE 03/jan/2021: same with language/translation
  # files, for each language listen in po/LINGUAS
  
  debug "current dir: `pwd`"
  echo
  cat "$listfile" | remove_comments | while read -r file; do
    pack_one_file "$file"
  done
  
  # finally generate the matching header file
  (
    echo
    echo "/* `basename $data_h` auto-generated by $baseme */" 
    echo "//#include \"../src/main.h\""
    echo
    echo "typedef struct {"
    echo "  unsigned char *data;"
    echo "  unsigned int data_len;"
    echo "  char *name;"
    echo "} __t_language_data;"
    echo
    cat "$data_c" | grep = | cut -d = -f 1 | sed -e 's/ $/;/' -e 's/^/extern /'
    #echo
    #echo "extern __t_language_data *g_language_data;" # done for us in the process just above
    echo
  ) > "$data_h"
  
  # ensure both files have the same timestamp
  touch "$data_c" "$data_h"
  
else
  echo "$baseme: inline data up to date, no need to regenerate"
fi

cd "$startwd/.."
rm "timestamp.h" 2>/dev/null
echo
if [ -r "$timestamp_script" ]; then
  pwd
  echo "$baseme: generating build timestamp..."
  bash "$timestamp_script" >/dev/null
else
  echo "$baseme: `basename "$timestamp_script"` not generating timestamp"
fi

echo "$me: DONE."
echo
