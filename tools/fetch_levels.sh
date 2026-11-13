#!/usr/bin/env bash
# Download the level packs the game normally fetches at run time (MapDownloadTracker) into levels/ (committed to the repo, so
# normally you do not need to run this; it refreshes them).
# The bundle only ships a handful of maps (training, 300, 609, 720, 722, ...); e.g. Airbase (map 721) comes from
# the on-5 level server, which is still online. host/main.c merges levels/maps into <docs>/downloads/maps.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=levels; ZIP=work/level-zips; mkdir -p "$OUT" "$ZIP"
python3 -I - <<'P' > "$ZIP/urls.txt"
import json
d = json.load(open('work/std/Payload/Tower.app/offline_data.dat'))
print("\n".join(d['userData']['tower_files']))
P
while read -r u; do
  z="$ZIP/$(basename "$u")"
  [ -s "$z" ] || curl -fsS --retry 2 -o "$z" "$u" || { echo "skip $u"; rm -f "$z"; continue; }
  unzip -o -q "$z" -x '__MACOSX/*' '*.DS_Store' -d "$OUT"
done < "$ZIP/urls.txt"
echo "levels: $(ls "$OUT"/maps/map_*.xml | wc -l) map xmls in $OUT/maps"

# Fitting-room clothing (server-hosted items: full image + preview per entry of userData.tower_dress), kept under levels/items/<url path without host>
python3 -I - <<'P'
import json, os, urllib.request
d = json.load(open('work/std/Payload/Tower.app/offline_data.dat'))
n = 0
for e in d['userData']['tower_dress']:
    for k in ('tURL', 'URL'):
        u = e.get(k)
        if not u: continue
        dst = os.path.join('levels/items', u.split('://', 1)[1].split('/', 1)[1])
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if not os.path.exists(dst):
            try: open(dst, 'wb').write(urllib.request.urlopen(u, timeout=20).read())
            except Exception as x: print('skip', u, x); continue
        n += 1
print('items:', n, 'files in levels/items')
P
