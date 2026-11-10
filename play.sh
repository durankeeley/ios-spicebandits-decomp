#!/usr/bin/env bash
# Run Spice Bandits (iOS) under the host. Menu, world map, tech shop and career render; levels do not start yet
# (see docs/PORT_STATUS.md). Log to a file for bug reports:  ./play.sh 2> /tmp/sb.log
#   ./play.sh [--size WxH] [--trace]      Shift+Esc quits, F11 fullscreen
set -euo pipefail
cd "$(dirname "$0")"
[ -d work/std/Payload ] || tools/extract.sh
[ -x host/build/ra_host ] && [ -z "$(find host -name '*.[ch]' -newer host/build/ra_host)" ] || host/build.sh
APP="$PWD/work/std/Payload/Tower.app"
exec host/build/ra_host --app "$APP" "$@"
