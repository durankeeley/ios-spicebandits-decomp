#!/usr/bin/env bash
# Run the game headless to frame N, save a PNG and print what is still missing.
#   tools/shot.sh [frame=600] [click-list e.g. 400:512:384] [out-prefix]
cd "$(dirname "$0")/.."; ulimit -c 0
F=${1:-600}; C=${2:-}; O=${3:-/tmp/sbshot}
RA_UIDUMP=1 RA_CLICKS="$C" RA_NOVSYNC=1 RA_EXIT_AFTER_SHOT=1 RA_SHOT=$O.ppm RA_SHOT_FRAME=$F SDL_VIDEODRIVER=offscreen \
  timeout ${TIMEOUT:-280} host/build/ra_host --app "$PWD/work/std/Payload/Tower.app" --trace 2>&1 | head -c ${LOGCAP:-60000000} > $O.log; echo "rc=${PIPESTATUS[0]}"   # log capped: a runaway guest loop must not fill the disk
[ -f $O.ppm ] && python3 tools/ppm2png.py $O.ppm $O.png && python3 tests/imgcheck.py stats $O.ppm
grep -E 'UNIMPLEMENTED|emu error|memory fault|too deep' $O.log | sort -u | head -${N:-12} | cut -c1-150
