# AFFINE on the FM-1: test 1

The package: `build/review/affine-0.1-test1.fwsc` (621 KB, identity FM-1_281500, a test build, shows "DEV").

## Install

1. Plug the FM-1 in by USB, open the installer in Chrome or Edge: https://charlesvestal.github.io/fm1-base/install/
2. Pick the `.fwsc`, install, wait for the restart. (From a terminal: `python3 base/tools/fm1_install.py FILE`.)

If anything goes wrong: the same installer puts M-VAVE's firmware back, and an FM-1 that fails to start twice comes
up in a safe mode the installer still reaches. Both flash chips (the stock P25Q80 and the newer UC25HQ80) are known
to the platform.

The first start asks BEGINNER or EXPERT: pick **EXPERT** (the CPU meter needs it).

## Check

| | What | Expect |
|---|---|---|
| 1 | Power on | the splash: streaks, the gem flying in, the stripes and AFFINE; PRESS PLAY |
| 2 | PLAY | the break; HOME's floor lit on the slice playing, the gem turning with it |
| 3 | HOME K1-K4 | FILTER, CRUSH, REDUX, VERB heard and drawn |
| 4 | Black keys 8, 9, 10 | SKIP, VERB THROW, CRUSH while held |
| 5 | FX, SELECT past slot 4 | the PSX page: VERB's types, CODEC GRIT and XA |
| 6 | EDIT | BOOM: 808, JUNGLE, DUB, TIGHT |
| 7 | SAVE, power off and on | the pattern and settings kept |

## Read and send back

- **CPU:** SYSTEM (GLO) > MIDI, K3 METER on. Play with VERB up, CODEC GRIT on (PSX page) and CRUSH up: the highest
  CPU % you see.
- **Sound:** any click, crackle or dropout, and when.
- **Screen:** does HOME move smoothly, or step?
- Anything that looks or sounds wrong.
