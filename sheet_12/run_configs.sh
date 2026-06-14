#!/bin/bash

set -u

RUNS="${1:-3}"
HOSTLBL="${HOSTLBL:-local}"

SHEETDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LUADIR="$SHEETDIR/lua-5.5.0"
LUA="$LUADIR/src/lua"
RESDIR="$SHEETDIR/results"
RAWDIR="$RESDIR/raw"
CSV="$RESDIR/results.csv"

CONFIGS=(
  "baseline|-O2||"
  "O3|-O3||"
  "O3_lto|-O3 -flto|-flto|"
  "O3_native|-O3 -march=native||"
  "O3_nocross|-O3 -fno-gcse -fno-crossjumping||"
  "O3_pgo|-O3||pgo"
  "O3_pgo_nocross|-O3 -fno-gcse -fno-crossjumping||pgo"
  "O3_native_pgo|-O3 -march=native||pgo"
)

mkdir -p "$RAWDIR"
echo "host,config,run,naive_s,tail_s,iter_s,avg_s" > "$CSV"

build_normal() {
  make -C "$LUADIR" clean >/dev/null 2>&1
  make -C "$LUADIR" MYCFLAGS="$1" MYLDFLAGS="$2" >/dev/null 2>"$RAWDIR/build_err.log"
}

build_pgo() {
  make -C "$LUADIR" clean >/dev/null 2>&1
  rm -f "$LUADIR"/src/*.gcda "$LUADIR"/src/*.gcno
  make -C "$LUADIR" MYCFLAGS="$1 -fprofile-generate" MYLDFLAGS="$2 -fprofile-generate" \
       >/dev/null 2>"$RAWDIR/build_err.log" || return 1
  ( cd "$SHEETDIR" && "$LUA" fib.lua ) >/dev/null 2>&1
  make -C "$LUADIR" clean >/dev/null 2>&1
  make -C "$LUADIR" MYCFLAGS="$1 -fprofile-use -fprofile-correction" \
       MYLDFLAGS="$2 -fprofile-use" >/dev/null 2>"$RAWDIR/build_err.log" || return 1
}

parse_run() {
  awk '
    /fibonacci_naive/ { for(i=1;i<=NF;i++) if($i=="time:") n=$(i+1);  if($NF!=832040) bad=1 }
    /fibonacci_tail/  { for(i=1;i<=NF;i++) if($i=="time:") t=$(i+1);  if($NF!=832040) bad=1 }
    /fibonacci_iter/  { for(i=1;i<=NF;i++) if($i=="time:") it=$(i+1); if($NF!=832040) bad=1 }
    /^AVG:/ { a=$2 }
    END { printf "%s %s %s %s %d\n", n, t, it, a, (bad?0:1) }
  '
}

for entry in "${CONFIGS[@]}"; do
  IFS='|' read -r name cflags ldflags mode <<< "$entry"
  printf '\n===== %-14s (%s) =====\n' "$name" "$cflags${ldflags:+ / ld:$ldflags}"

  echo "  building..."
  if [ "$mode" = "pgo" ]; then
    build_pgo "$cflags" "$ldflags"
  else
    build_normal "$cflags" "$ldflags"
  fi
  if [ ! -x "$LUA" ]; then
    echo "  BUILD FAILED -- skipping. Last errors:"
    tail -5 "$RAWDIR/build_err.log" | sed 's/^/    /'
    continue
  fi

  raw="$RAWDIR/$name.log"
  : > "$raw"
  for r in $(seq 1 "$RUNS"); do
    out="$( cd "$SHEETDIR" && "$LUA" fib.lua )"
    printf '### run %d\n%s\n' "$r" "$out" >> "$raw"
    read -r n t it a ok <<< "$(printf '%s\n' "$out" | parse_run)"
    [ "$ok" != "1" ] && echo "  run $r: WRONG RESULT (not 832040) -- build is INCORRECT!"
    printf '  run %d: naive=%s tail=%s iter=%s avg=%s\n' "$r" "$n" "$t" "$it" "$a"
    echo "$HOSTLBL,$name,$r,$n,$t,$it,$a" >> "$CSV"
  done
done

med_min() {
  awk -F, -v c="$1" -v col="$2" 'NR>1 && $2==c {print $col}' "$CSV" | sort -n | awk '
    { v[NR]=$1 } END {
      if (NR==0){print "NA NA"; exit}
      m = (NR%2 ? v[(NR+1)/2] : (v[NR/2]+v[NR/2+1])/2)
      printf "%.4f %.4f\n", m, v[1]
    }'
}

printf '\n\n=========== SUMMARY (median/best over %s runs, host=%s) ===========\n' "$RUNS" "$HOSTLBL"
printf '%-15s %15s %15s %15s %9s\n' "config" "naive med/best" "tail med/best" "iter med/best" "avg med"
for entry in "${CONFIGS[@]}"; do
  IFS='|' read -r name _ _ _ <<< "$entry"
  grep -q ",$name," "$CSV" || continue
  read -r nmed nbest <<< "$(med_min "$name" 4)"
  read -r tmed tbest <<< "$(med_min "$name" 5)"
  read -r imed ibest <<< "$(med_min "$name" 6)"
  read -r amed _     <<< "$(med_min "$name" 7)"
  printf '%-15s %7s/%-7s %7s/%-7s %7s/%-7s %9s\n' \
    "$name" "$nmed" "$nbest" "$tmed" "$tbest" "$imed" "$ibest" "$amed"
done

echo
echo "CSV:  $CSV"
echo "Raw:  $RAWDIR/<config>.log"
