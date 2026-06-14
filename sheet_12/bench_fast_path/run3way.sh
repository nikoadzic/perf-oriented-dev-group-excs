#!/bin/bash

set -u
export LC_ALL=C
RUNS="${1:-${RUNS:-5}}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHEETDIR="$(dirname "$SCRIPT_DIR")"
LUADIR="$SHEETDIR/lua-5.5.0"
SRC="$LUADIR/src"
LUA="$SRC/lua"
TMP="$SCRIPT_DIR/_tmp"
RAW="$SCRIPT_DIR/raw"
CSV="$SCRIPT_DIR/results3way.csv"
VANILLA="$SCRIPT_DIR/lvm_vanilla.c"
PATCHED="$SCRIPT_DIR/lvm_patched.c"
HOSTLBL="$(hostname -s 2>/dev/null || echo node)"
mkdir -p "$TMP" "$RAW"

for f in "$VANILLA" "$PATCHED" "$SHEETDIR/fib.lua"; do
  [ -f "$f" ] || { echo "ERROR: missing $f"; exit 1; }
done
grep -q "fast path: call to a plain Lua closure" "$PATCHED" \
  || { echo "ERROR: $PATCHED has no OP_CALL fast-path (wrong file?)"; exit 1; }
grep -q "fast path: call to a plain Lua closure" "$VANILLA" \
  && { echo "ERROR: $VANILLA unexpectedly contains the patch"; exit 1; }

cleanprof(){ rm -f "$SRC"/*.gcda "$SRC"/*.gcno; }

build_o2(){
  cp "$1" "$SRC/lvm.c"; cleanprof
  make -C "$LUADIR" clean >/dev/null 2>&1
  make -C "$LUADIR"          >/dev/null 2>&1 || { echo "BUILD FAILED ($2)"; exit 1; }
  cp "$LUA" "$2"; chmod +x "$2"
}
build_pgo(){
  cp "$1" "$SRC/lvm.c"; cleanprof
  make -C "$LUADIR" clean >/dev/null 2>&1
  make -C "$LUADIR" MYCFLAGS="-O3 -fprofile-generate" MYLDFLAGS="-fprofile-generate" >/dev/null 2>&1 \
    || { echo "PGO-GEN FAILED ($2)"; exit 1; }
  ( cd "$SHEETDIR" && "$LUA" fib.lua ) >/dev/null 2>&1
  ls "$SRC"/*.gcda >/dev/null 2>&1 || { echo "ERROR: PGO training produced no .gcda -- could not run $LUA."; \
    echo "  This filesystem is likely mounted 'noexec'. Run from an exec-allowed dir (e.g. \$HOME)."; exit 1; }
  make -C "$LUADIR" clean >/dev/null 2>&1
  make -C "$LUADIR" MYCFLAGS="-O3 -fprofile-use -fprofile-correction" MYLDFLAGS="-fprofile-use" >/dev/null 2>&1 \
    || { echo "PGO-USE FAILED ($2)"; exit 1; }
  cp "$LUA" "$2"; chmod +x "$2"
}

echo "=== node: $(hostname) | gcc: $(gcc -dumpversion) | RUNS=$RUNS ==="
echo "[1/3] building baseline (-O2, vanilla)";    build_o2  "$VANILLA" "$TMP/lua_base"
echo "[2/3] building PGO-only (O3+PGO, vanilla)"; build_pgo "$VANILLA" "$TMP/lua_pgo"
echo "[3/3] building patched (O3+PGO, OP_CALL)";  build_pgo "$PATCHED" "$TMP/lua_patched"

cp "$PATCHED" "$SRC/lvm.c"; cleanprof
make -C "$LUADIR" clean >/dev/null 2>&1; make -C "$LUADIR" >/dev/null 2>&1

for b in lua_base lua_pgo lua_patched; do
  o="$("$TMP/$b" "$SHEETDIR/fib.lua" 2>&1)" || true
  if printf '%s' "$o" | grep -qi 'permission denied'; then
    echo "ERROR: cannot execute $TMP/$b (Permission denied)."
    echo "  This filesystem is likely mounted 'noexec'. Run the project from an"
    echo "  exec-allowed location such as your \$HOME, not $SHEETDIR."
    exit 1
  fi
  printf '%s' "$o" | grep -q -- "-- *832040" \
    || { echo "ERROR: $b produced a wrong result:"; printf '%s\n' "$o" | head; exit 1; }
done

field(){ echo "$1" | awk -v k="$2" '$0~k{for(i=1;i<=NF;i++)if($i=="time:")print $(i+1)}'; }
med(){ printf '%s\n' "$@" | sort -n | awk '{v[NR]=$1}END{print v[int((NR+1)/2)]}'; }

bn=(); bt=(); bi=(); gn=(); gt=(); gi=(); pn=(); pt=(); pi=()
echo "host,config,run,naive_s,tail_s,iter_s,avg_s" > "$CSV"
rm -f "$RAW"/base.log "$RAW"/pgo.log "$RAW"/patched.log
echo "=== running fib.lua x$RUNS per config (interleaved) -> $CSV ==="
for r in $(seq 1 "$RUNS"); do
  for cfg in base pgo patched; do
    case $cfg in base) bin=lua_base;; pgo) bin=lua_pgo;; patched) bin=lua_patched;; esac
    out="$(cd "$SHEETDIR" && "$TMP/$bin" fib.lua)"
    n="$(field "$out" naive)"; t="$(field "$out" tail)"; i="$(field "$out" iter)"
    a="$(awk -v n="$n" -v t="$t" -v i="$i" 'BEGIN{printf "%.4f",(n+t+i)/3}')"
    echo "$HOSTLBL,$cfg,$r,$n,$t,$i,$a" >> "$CSV"
    printf '### run %d\n%s\n' "$r" "$out" >> "$RAW/$cfg.log"
    case $cfg in
      base)    bn+=("$n"); bt+=("$t"); bi+=("$i");;
      pgo)     gn+=("$n"); gt+=("$t"); gi+=("$i");;
      patched) pn+=("$n"); pt+=("$t"); pi+=("$i");;
    esac
  done
  echo "  run $r/$RUNS done"
done

awk -v R="$RUNS" \
    -v bn="$(med "${bn[@]}")" -v bt="$(med "${bt[@]}")" -v bi="$(med "${bi[@]}")" \
    -v gn="$(med "${gn[@]}")" -v gt="$(med "${gt[@]}")" -v gi="$(med "${gi[@]}")" \
    -v pn="$(med "${pn[@]}")" -v pt="$(med "${pt[@]}")" -v pi="$(med "${pi[@]}")" 'BEGIN{
  ba=(bn+bt+bi)/3; ga=(gn+gt+gi)/3; pa=(pn+pt+pi)/3
  print ""
  printf "============== THREE-WAY RESULT (median of %d runs, seconds) ==============\n", R
  printf "%-24s %9s %9s %9s %9s\n","config","naive","tail","iter","AVG"
  printf "%-24s %9.4f %9.4f %9.4f %9.4f\n","baseline (-O2)",bn,bt,bi,ba
  printf "%-24s %9.4f %9.4f %9.4f %9.4f\n","PGO only (O3+PGO)",gn,gt,gi,ga
  printf "%-24s %9.4f %9.4f %9.4f %9.4f\n","patched (OP_CALL+PGO)",pn,pt,pi,pa
  print ""
  printf "%-24s %8.1f%% %8.1f%% %8.1f%% %8.1f%%\n","PGO vs baseline",100*(gn-bn)/bn,100*(gt-bt)/bt,100*(gi-bi)/bi,100*(ga-ba)/ba
  printf "%-24s %8.1f%% %8.1f%% %8.1f%% %8.1f%%\n","patched vs baseline",100*(pn-bn)/bn,100*(pt-bt)/bt,100*(pi-bi)/bi,100*(pa-ba)/ba
  printf "%-24s %8.1f%% %8.1f%% %8.1f%% %8.1f%%\n","patched vs PGO-only",100*(pn-gn)/gn,100*(pt-gt)/gt,100*(pi-gi)/gi,100*(pa-ga)/ga
  print "(all %% above: negative = faster / less time)"
  print ""
  printf "Headline (AVG, negative = faster): patched vs baseline %+.1f%%, patched vs PGO-only %+.1f%%\n",100*(pa-ba)/ba,100*(pa-ga)/ga
}'
echo ""
echo "Per-run data (CSV):  $CSV"
echo "Raw fib.lua output:  $RAW/{base,pgo,patched}.log"
echo "=== done; per-config binaries kept in $TMP ==="
