# AFFINE

A PS1-era jungle break chopper for the M-VAVE FM-1.

Amens cut into slices on the keys, performance effects under every key and knob, and the sound of a 1990s console's
sound chip: its reverbs, its sample grit, its CD audio.

**Play it in your browser:** <https://inigohernandez.github.io/affine/>. The page runs the firmware itself.

**Status:** test build 0.1, not yet released: its first run on an FM-1 comes next. The plan:
[docs/plan.md](docs/plan.md). What changed: [CHANGELOG.md](CHANGELOG.md).

## What it does

- **Breaks:** Cyberworm's royalty-free Amens built in, sixteen slices each on the white keys.
- **Perform:** eleven black keys held for an effect (rolls, reverse, half speed, vinyl break, freeze, skip, verb
  throw, crush, filter); HOME's four knobs FILTER, CRUSH, REDUX, VERB.
- **The chip:** after the break, five stages in the order of the era's console: CODEC (its ADPCM grit, or the CD's),
  VERB (nine reverbs at 22.05 kHz), CRUSH, REDUX and SKIP.
- **The look:** POLYGON, a floor of sixteen lanes in perspective and a low-poly gem; five skins.

## Build

```
./build.sh               # the firmware package: app/build/affine.fwsc
./build.sh --host        # the host simulator
./build.sh test          # the engine's and the chip's checks
sh web/build.sh          # the site: the firmware in WebAssembly, the screens, the sounds (web/)
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
