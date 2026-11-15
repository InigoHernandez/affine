# Breaks

AFFINE's breaks are Cyberworm's 60 royalty-free Amen loops (Dmitriy "Cyberworm" Vasilyev, rhythm-lab.com:
`huge/info.txt` says "All offered samples royalty free"): 20 in `cw/` (tempo in the name), 40 from the "Huge Amen
Break Loops Collection" in `huge/`. 16-bit 44.1 kHz WAV.

[`breaks.txt`](breaks.txt) lists every WAV with its tempo, bars and a short name, and marks with `*` the ones built
into the firmware. Everyone's own breaks go onto the FM-1 from the site's import page, after the built-in ones.

## How the bank is made

`./build.sh` turns the marked lines into `app/bank_data.h` (generated, git-ignored) with `tools/prep_breaks.py`:
mono, 22.05 kHz, IMA ADPCM, 16 slices a break with the markers on the drum hits (or on an even grid for a line marked
`*=`). Every break is brought to the same loudness as heard (-16 LUFS, ITU-R BS.1770) and a look-ahead limiter holds
its peaks under -0.2 dBFS. The firmware has 400 KB for breaks; `./build.sh bank` says whether the list fits.

The tool and the format come from CHOPPAH by Laurens Profittlich, through LOAF BREAKER by FLEXOR (MPL-2.0).
