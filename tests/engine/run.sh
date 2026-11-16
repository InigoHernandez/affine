#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# CHOPPAH's engine tests: app/dsp/ built with the computer's own C compiler (no FM-1 toolchain, no fm1-base).
#   tests/engine/run.sh [OUTDIR]     WAV renders go to OUTDIR (default build/engine)
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-build/engine}"
mkdir -p "$OUT"
CC="${CC:-cc}"
# the device build rejects double precision: warn here on any float that gets promoted
"$CC" -std=c99 -O2 -Wall -Wextra -Werror -Wdouble-promotion -Wfloat-conversion -Wno-unused-parameter \
    -Iapp/dsp -c app/dsp/brk.c -o "$OUT/brk.o"
for f in brk_fx brk_synth adpcm kick perc ovl brk_math psx; do
    "$CC" -std=c99 -O2 -Wall -Wextra -Werror -Wdouble-promotion -Wfloat-conversion -Iapp/dsp -c app/dsp/$f.c -o "$OUT/$f.o"
done
"$CC" -std=c99 -O2 -Wall -Iapp/dsp tests/engine/test_engine.c "$OUT"/brk.o "$OUT"/brk_fx.o "$OUT"/brk_synth.o \
    "$OUT"/adpcm.o "$OUT"/kick.o "$OUT"/perc.o "$OUT"/ovl.o "$OUT"/brk_math.o "$OUT"/psx.o -lm -o "$OUT/test_engine"
"$OUT/test_engine" "$OUT"
# AFFINE: the PSX effects over a Cyberworm Amen (checks, then WAVs in OUTDIR: crush, redux, grit, xa, skip, verbs, tour)
"$CC" -std=c99 -O2 -Wall -Iapp/dsp tests/engine/psx_demo.c "$OUT"/psx.o -lm -o "$OUT/psx_demo"
"$OUT/psx_demo" breaks/huge/cw_amen_jungle.wav 175 "$OUT"
# AFFINE: the colour stage (GRIT's path: rate, RAW, CRUNCH, SMOOTH): checks; PSX_COLOR_RENDERS=1 also writes the A/B
# renders to build/review/psx_color/ (index.txt says what each is)
"$CC" -std=c99 -O2 -Wall -Iapp/dsp tests/engine/psx_color_demo.c "$OUT"/psx.o -lm -o "$OUT/psx_color_demo"
if [ -n "$PSX_COLOR_RENDERS" ]; then
    mkdir -p build/review/psx_color
    "$OUT/psx_color_demo" breaks/huge/cw_amen_jungle.wav build/review/psx_color
else
    "$OUT/psx_color_demo" breaks/huge/cw_amen_jungle.wav
fi
# AFFINE: the PSX colour, phases 3 and 4 (SPU VERB's INPUT, XA's clock, ...): checks; PSX_COLOR2_RENDERS=1 also writes
# the owner's renders to build/review/psx_color2/
"$CC" -std=c99 -O2 -Wall -Iapp/dsp -Itests/engine tests/engine/psx_color2_demo.c "$OUT"/psx.o -lm -o "$OUT/psx_color2_demo"
if [ -n "$PSX_COLOR2_RENDERS" ]; then
    mkdir -p build/review/psx_color2
    "$OUT/psx_color2_demo" breaks/huge/cw_amen_jungle.wav build/review/psx_color2
else
    "$OUT/psx_color2_demo" breaks/huge/cw_amen_jungle.wav
fi
"$CC" -std=c99 -O2 -Wall -Iapp/dsp tests/engine/psx_bench.c "$OUT"/psx.o "$OUT"/brk_fx.o "$OUT"/brk.o "$OUT"/brk_synth.o "$OUT"/adpcm.o "$OUT"/brk_math.o -lm -o "$OUT/psx_bench"
