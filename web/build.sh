#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# AFFINE's site (web/): what it takes from the build, gathered beside the pages.
#   sh web/build.sh          the browser emulator (./build.sh --web, then emu/), the screens, the sounds
#   sh web/build.sh --copy   the same without building the emulator again (app/build/emu as it is)
#   RELEASE=X.Y [UNTESTED=1] sh web/build.sh [--copy]
#                            also the installer: install/ (fm1-base's tools/make_site.py, then tools/install_theme.py) and
#                            firmware/ (app/build/affine-X.Y.fwsc from ./build.sh --release X.Y, m-bootloader and the
#                            licences beside it), the emulator built as X.Y; UNTESTED=1 says on the installer that the
#                            package has not run on an FM-1
# The screens are the host simulator's (sh tests/scenarios.sh first); the sounds are tests/engine/psx_demo.c's renders
# (build/review/sounds), cut down for the page with macOS's afconvert. Serve web/ (.claude/launch.json: affine-web).
set -e
cd "$(dirname "$0")/.."
WEB=web
if [ "$1" != "--copy" ]; then
    if [ -n "$RELEASE" ]; then ./build.sh --web --release "$RELEASE"; else ./build.sh --web; fi
fi
[ -f app/build/emu/app.wasm ] || { echo "web/build.sh: no app/build/emu/app.wasm (./build.sh --web)"; exit 1; }
rm -rf "$WEB/emu"
mkdir -p "$WEB/emu/parts" "$WEB/screens" "$WEB/sounds"
cp app/build/emu/app.wasm app/build/emu/worklet.js app/build/emu/app.js app/build/emu/app_worklet.js "$WEB/emu/"
cp app/build/emu/parts/*.js "$WEB/emu/parts/"

# the simulator's screens: site name, scenario shot
S=app/build/scenarios
while read -r name shot; do
    [ -n "$name" ] || continue
    if [ -f "$S/$shot.png" ]; then cp "$S/$shot.png" "$WEB/screens/$name.png"; else echo "web/build.sh: no $S/$shot.png"; fi
done <<EOF
home ah_home
h-filter ah_filter
h-crush ah_crush
h-gestures ah_gestures
k-roll8 p_roll8
k-roll16 p_roll16
k-roll32 roll
k-reverse p_rev
k-half p_half
k-vinyl p_stop
k-freeze p_freeze
k-skip ak_skip
k-verb ak_verb
k-crush ak_crush
k-filter p_filter
psx-verb pp_verb
psx-codec pp_codec
psx-lofi pp_lofi
psx-skip pp_skip
boom b_boom
keys a_blackkeys
settings hset
seq n_seq
EOF

# (as palette PNGs: a screen has a few dozen colours; the simulator writes them as 170 KB of RGB)
"${PYTHON:-python3}" - "$WEB/screens" <<'EOF'
import sys, glob
from PIL import Image
for f in glob.glob(sys.argv[1] + "/*.png"):
    im = Image.open(f).convert("RGB")
    if len(im.getcolors(1 << 16) or []) <= 256:
        im = im.quantize(colors=256, method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE)
    im.save(f, optimize=True)
EOF

# the sounds: AAC at 96 kb/s (the demo's renders are 250)
R=build/review/sounds
for n in verbs grit xa crush redux skip; do
    if [ -f "$WEB/sounds/$n.m4a" ] && [ ! "$R/affine-$n.m4a" -nt "$WEB/sounds/$n.m4a" ]; then
        :                                 # (as it is: encoding again makes other bytes for the same sound)
    elif [ -f "$R/affine-$n.m4a" ]; then
        afconvert -f m4af -d aac -b 96000 "$R/affine-$n.m4a" "$WEB/sounds/$n.m4a"
    else
        echo "web/build.sh: no $R/affine-$n.m4a (tests/engine/psx_demo.c renders them)"
    fi
done
# the installer, for a release
if [ -n "$RELEASE" ]; then
    PKG="app/build/affine-$RELEASE.fwsc"
    [ -f "$PKG" ] || { echo "web/build.sh: no $PKG (./build.sh --release $RELEASE)"; exit 1; }
    TMP=$(mktemp -d)
    "${PYTHON:-python3}" base/tools/make_site.py app "$PKG" "$RELEASE" "$TMP/site" >/dev/null
    rm -rf "$WEB/install" "$WEB/firmware"
    mkdir -p "$WEB/install"
    cp -R "$TMP/site/firmware" "$WEB/firmware"
    "${PYTHON:-python3}" tools/install_theme.py "$TMP/site/install/index.html" "$WEB/install/index.html" ${UNTESTED:+--untested}
    rm -rf "$TMP"
    shasum -a 256 "$PKG" "$WEB/firmware/affine-$RELEASE.fwsc"
fi
du -sh "$WEB/emu" "$WEB/screens" "$WEB/sounds"
