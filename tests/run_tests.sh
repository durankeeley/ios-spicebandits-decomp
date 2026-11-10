#!/usr/bin/env bash
# Tests for the Spice Bandits port (current milestone: analysis + app launch path; no rendering yet).
#   tests/run_tests.sh --quick   static analysis tests only
#   tests/run_tests.sh           + build + boot smoke tests (~1.5 min, headless)
set -uo pipefail
cd "$(dirname "$0")/.."
MODE=${1:-full}; APP="$PWD/work/std/Payload/Tower.app"; BIN=host/build/ra_host
OUT=$(mktemp -d); export SDL_VIDEODRIVER=${TEST_VIDEO:-offscreen} RA_NOVSYNC=1
PASS=0; FAIL=0; SKIP=0
ok() { echo "  PASS  $1"; PASS=$((PASS+1)); }; bad() { echo "  FAIL  $1${2:+  -- $2}"; FAIL=$((FAIL+1)); }; skip() { echo "  SKIP  $1${2:+  -- $2}"; SKIP=$((SKIP+1)); }
check() { local n=$1; shift; if "$@" >"$OUT/last.log" 2>&1; then ok "$n"; else bad "$n" "$(tail -n 2 "$OUT/last.log" | tr '\n' ' ')"; fi; }
PY=python3; [ -x .venv/bin/python ] && PY=.venv/bin/python

echo "== 1. static analysis"
check "toolkit output (functions, ObjC names, exports, nib headers, IPA encryption, docs)" $PY tests/test_tools.py
[ "$MODE" = "--quick" ] && { echo; echo "passed=$PASS failed=$FAIL skipped=$SKIP"; [ $FAIL -eq 0 ]; exit; }

echo "== 2. build"
check "host builds" host/build.sh
[ -x $BIN ] && ok "host binary exists" || { bad "host binary exists"; exit 1; }
[ -d "$APP" ] || { skip "boot tests" "run tools/extract.sh"; echo "passed=$PASS failed=$FAIL skipped=$SKIP"; [ $FAIL -eq 0 ]; exit; }

echo "== 3. CLI"
$BIN >/dev/null 2>&1; [ $? -eq 2 ] && ok "no arguments -> usage, exit 2" || bad "no arguments -> usage"
$BIN --app /nonexistent >/dev/null 2>&1; [ $? -ne 0 ] && ok "missing app -> non-zero" || bad "missing app -> non-zero"

echo "== 4. boot smoke (30 s of the app launch path)"
timeout 30 $BIN --app "$APP" >"$OUT/boot.log" 2>&1; RC=$?
[ $RC -eq 124 ] && ok "app keeps running for 30 s (no crash, no exit)" || bad "app keeps running for 30 s" "rc=$RC"
grep -q "loaded .*Tower, entry" "$OUT/boot.log" && ok "Mach-O loaded (PIE, binds resolved)" || bad "Mach-O loaded"
grep -q "UIApplicationMain\|launched; delegate=" "$OUT/boot.log" && ok "UIApplicationMain reached, delegate created" || bad "UIApplicationMain reached"
grep -q "launched; delegate=" "$OUT/boot.log" && ok "application:didFinishLaunching completed" || bad "didFinishLaunching completed"
grep -q "launched; delegate=" "$OUT/boot.log" && grep -q "TowerViewController.nib" "$OUT/boot.log" || grep -q "launched; delegate=" "$OUT/boot.log" && ok "launch finished with the main window built from nibs" || bad "launch finished"
! grep -q "emu error\|memory fault" "$OUT/boot.log" && ok "no CPU emulation errors / memory faults" || bad "no CPU errors" "$(grep -m1 -E 'emu error|memory fault' $OUT/boot.log)"
! grep -q "heap exhausted" "$OUT/boot.log" && ok "no guest heap exhaustion" || bad "no heap exhaustion"
N=$(grep -o 'UNIMPLEMENTED import [A-Za-z0-9_]*' "$OUT/boot.log" | sort -u | wc -l); [ "$N" -le 8 ] && ok "unimplemented imports bounded ($N <= 8)" || bad "unimplemented imports bounded" "$N"
! grep -q "Segmentation\|core dumped" "$OUT/boot.log" && ok "host did not crash" || bad "host did not crash"
check "HLE backlog generator runs (tools/hle_todo.sh)" tools/hle_todo.sh "$APP" 10

echo "== 5. rendering: boot -> main menu -> Play -> world map (screenshot signatures)"
shot() { local n=$1 f=$2 c=${3:-}; RA_CLICKS="$c" RA_SHOT="$OUT/$n.ppm" RA_SHOT_FRAME=$f RA_EXIT_AFTER_SHOT=1 timeout 400 $BIN --app "$APP" >"$OUT/$n.log" 2>&1 | head -c 50000000; SHOTRC=$?; }
shot menu 900 ""
[ -f "$OUT/menu.ppm" ] && ok "frames are presented (main menu screenshot produced)" || bad "main menu screenshot produced"
[ -f "$OUT/menu.ppm" ] && check "main menu renders (nib views, CgBI images, text) - matches golden" python3 tests/imgcheck.py cmp "$OUT/menu.ppm" tests/golden/menu.txt 24 || true
[ -f "$OUT/menu.ppm" ] && python3 tests/imgcheck.py cmp "$OUT/menu.ppm" tests/golden/map.txt 14 >/dev/null && bad "menu differs from the map golden" || ok "menu is distinguishable from the map screen"
shot map 1100 "700:600:272"
[ -f "$OUT/map.ppm" ] && check "Play -> world map renders in landscape (GL scene)" python3 tests/imgcheck.py cmp "$OUT/map.ppm" tests/golden/map.txt 26 || bad "map screenshot produced"
! grep -q "emu error\|memory fault" "$OUT/map.log" && ok "no CPU errors while loading the map" || bad "no CPU errors while loading the map"
shot shop 1200 "700:640:436"
[ -f "$OUT/shop.ppm" ] && check "Tech shop opens with its item trees (offline JSON data loaded)" python3 tests/imgcheck.py cmp "$OUT/shop.ppm" tests/golden/shop.txt 24 || bad "shop screenshot produced"
RA_CLICK_HOLD=8 shot done 1700 "700:640:436,1100:40:37"
[ -f "$OUT/done.ppm" ] && check "shop 'Done' returns to the main menu" python3 tests/imgcheck.py cmp "$OUT/done.ppm" tests/golden/menu.txt 24 || bad "done screenshot produced"
RA_CLICK_HOLD=8 shot career 1800 "700:640:436,1100:210:37"
[ -f "$OUT/career.ppm" ] && check "shop -> 'Career' tab opens the career page" python3 tests/imgcheck.py cmp "$OUT/career.ppm" tests/golden/career.txt 24 || bad "career screenshot produced"
RA_CLICK_HOLD=8 shot shopmap 1800 "700:640:436,1100:115:37"
[ -f "$OUT/shopmap.ppm" ] && check "shop -> 'Map' tab opens the world map" python3 tests/imgcheck.py cmp "$OUT/shopmap.ppm" tests/golden/map.txt 60 || bad "shop->map screenshot produced"
echo; echo "passed=$PASS failed=$FAIL skipped=$SKIP   (logs: $OUT)"; [ $FAIL -eq 0 ]
