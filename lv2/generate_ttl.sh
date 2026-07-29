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

startport=99
mode_hipass=1
mode_lowshelf=2
mode_curve=3
mode_hishelf=4
mode_lowpass=5

generate_one_eq_port() {
  local eq="$1"
  local band="$2"
  local param="$3"
  local mode
  local freq
  local default
  local minimum
  local maximum
  local property=""

  case "$band" in
    A)
      mode="$mode_hipass"
      freq=50
    ;;
    
    B)
      mode="$mode_lowshelf"
      freq=100
    ;;
    
    C)
      mode="$mode_curve"
      freq=250
    ;;
    
    D)
      mode="$mode_curve"
      freq=500
    ;;
    
    E)
      mode="$mode_curve"
      freq=1000
    ;;
    
    F)
      mode="$mode_curve"
      freq=2000
    ;;
    
    G)
      mode="$mode_hishelf"
      freq=4000
    ;;
    
    H)
      mode="$mode_lowpass"
      freq=8000
    ;;
  esac

  case "$param" in
    "enabled")
      property="        lv2:portProperty lv2:toggled ;"
      default=0
      minimum=0
      maximum=1
    ;;
    
    "mode")
      property="        lv2:portProperty lv2:integer ;"
      default="$mode"
      minimum=1
      maximum=5
    ;;
    
    "freq")
      default="$freq"
      minimum=20
      maximum=20000
    ;;
    
    "gain")
      default=0
      minimum=-36
      maximum=36
    ;;
    
    "q")
      default=1
      minimum=0.01
      maximum=100
    ;;
  esac

  cat << EOF
    [
        a lv2:InputPort ,
          lv2:ControlPort ;
        lv2:index ${portnum} ;
        lv2:symbol "${eq}_${band}_${param}" ;
        lv2:name "$eq $band $param" ;
${property}
        lv2:default ${default} ;
        lv2:minimum ${minimum} ;
        lv2:maximum ${maximum}
      ] ,
EOF

  portnum=$((portnum + 1))
}

generate_eq_ports() {
  for eq in eqpre eqpost; do
    for band in A B C D E F G H; do
      generate_one_eq_port "$eq" "$band" enabled
      generate_one_eq_port "$eq" "$band" mode
      generate_one_eq_port "$eq" "$band" freq
      generate_one_eq_port "$eq" "$band" gain
      generate_one_eq_port "$eq" "$band" q
    done
    cat << EOF
    [
        a lv2:InputPort ,
          lv2:ControlPort ;
        lv2:index ${portnum} ;
        lv2:symbol "${eq}_master_gain" ;
        lv2:name "$eq master gain" ;
        lv2:default 0 ;
        lv2:minimum -36 ;
        lv2:maximum 36
      ] ,
EOF
    portnum=$((portnum + 1))
  done
}

insert_eq() {
  while IFS="" read -r line; do
    case "$line" in
      "@@INSERT_EQ_PORTS_HERE")
        generate_eq_ports
      ;;

      *@@portnum*)
        echo "${line//@@portnum/$portnum}"
        portnum=$((portnum + 1))
      ;;
      
      *)
        echo "$line"
      ;;
    esac
  done
}

mkdir -p "$destdir/lv2"
echo -n "$0 cwd:"; pwd -P
for file in *.ttl.in; do
  portnum="$startport"
  echo "$file -> $destdir/lv2/${file%.in}"
  if [ "$include_ui" = 1 ]; then
    sed 's,^[[:space:]]*@@UI,,' "$file" | insert_eq > "$destdir/lv2/${file%.in}"
  else
    grep -v '^[[:space:]]*@@UI' "$file" | insert_eq > "$destdir/lv2/${file%.in}"
  fi
done
