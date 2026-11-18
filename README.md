# AFFINE

A PS1-era jungle break chopper for the M-VAVE FM-1.

Amens cut into slices on the keys, performance effects under every key and knob, and the sound of a 1990s console's
sound chip: its reverbs, its sample grit, its CD audio.

**Play it in your browser:** <https://inigohernandez.github.io/affine/>. The page runs the firmware itself.

**Install:** <https://inigohernandez.github.io/affine/install/> (Chrome or Edge, over USB), or the `.fwsc` from the
[latest release](https://github.com/InigoHernandez/affine/releases/latest).

**Status:** 0.3, a beta, tested on an FM-1 (2026-11-18). The plan: [docs/plan.md](docs/plan.md).
What changed: [CHANGELOG.md](CHANGELOG.md).

## What it does

- **Breaks:** Cyberworm's royalty-free Amens built in, sixteen slices each on the white keys.
- **Perform:** eleven black keys held for an effect (rolls, reverse, half speed, vinyl break, freeze, skip, verb
  throw, crush, filter); HOME's four knobs FILTER, CRUSH, REDUX, VERB.
- **The chip:** after the break, five stages in the order of the era's console: CODEC (its ADPCM grit, or the CD's),
  VERB (nine reverbs at 22.05 kHz), CRUSH, REDUX and SKIP.
- **The look:** POLYGON, a floor of sixteen lanes in perspective and a low-poly gem; five skins.

## On the FM-1

- **Its identity:** `FM-1_2815000100` for 0.1 (`FM-1_2815` and the version), USB port "AFFINE FM-1".
- **What it keeps:** its settings, patterns and imported breaks in the FM-1's data area (`0x97000`–`0xDFFFF`, its settings at
  `0xFC000`: the places LOAF BREAKER and CHOPPAH use). It does not read what another firmware saved, but for the
  panel's calibration every fm1-base firmware shares: a store without AFFINE's mark is left unread and AFFINE starts
  fresh. Its first SAVE writes over that area, so back up what another firmware
  keeps, with that firmware, before installing AFFINE.
- **How to leave it:** install another firmware as usual, from its installer or the Firmware Hub; AFFINE answers the
  update and starts the new package's loader (Felucca's). The installer also puts M-VAVE's firmware back.
- **If it fails to start:** after two failed starts it comes up in SAFE MODE, silent, with USB on: reinstall from
  the installer (PLAY tries again).

## Build

```
./build.sh               # the firmware package: app/build/affine.fwsc
./build.sh --release X.Y # a release: app/build/affine-X.Y.fwsc
./build.sh --host        # the host simulator
./build.sh test          # the engine's and the chip's checks
sh web/build.sh          # the site: the firmware in WebAssembly, the screens, the sounds (web/)
RELEASE=X.Y sh web/build.sh   # and the installer with that release's package
```

`scripts/bootstrap.sh` sets up fm1-base and the toolchain once. Notes for working on it: [CLAUDE.md](CLAUDE.md).

## Credits

AFFINE is built on [LOAF BREAKER](https://github.com/FLEXOR-R/loaf-breaker) by FLEXOR, itself built on
[CHOPPAH](https://github.com/profikid/fm1-break-choppah) by Laurens Profittlich, on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp),
from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments). Forked from LOAF BREAKER
at commit 4f8722c.

The breaks are Dmitriy "Cyberworm" Vasilyev's royalty-free Amen loops (rhythm-lab.com).

The PS1 sound effects are AFFINE's own code, written from [psx-spx](https://psx-spx.consoledev.net/), the public
reference for the console's hardware.

The pixel font is the Elektron Pixel Font by savingaurora (CC BY-SA 3.0), adapted for MONO2K (CC BY-SA 4.0:
`web/fonts/`). Inter Tight is under the SIL Open Font License.

MPL-2.0 ([LICENSE](LICENSE)). M-VAVE and FM-1 are trademarks of their owners. AFFINE is not affiliated with M-VAVE,
FLEXOR, Laurens Profittlich or Sony.
