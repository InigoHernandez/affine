# AFFINE: notes for Claude sessions

Firmware for the M-VAVE FM-1 (JieLi AC791N): a PS1-era jungle break chopper. Plan and decisions: `docs/plan.md` (keep it
current). Forked on 2026-11-15 from LOAF BREAKER (FLEXOR, https://github.com/FLEXOR-R/loaf-breaker, 4f8722c), itself
CHOPPAH's fork (Laurens Profittlich). LOAF's own notes, PRD and README are in `docs/upstream/`: read them as a map of
the code inherited, never as rules for AFFINE (their release, push and changelog rules are FLEXOR's).
The owner's lessons from MONO2K and Demake: `../FM1-FIRMWARE-HANDBOOK.md`. Add to it after every AFFINE update.

## The owner's rules

- **Never push to FLEXOR's or Laurens' repositories.** The `upstream` remote is fetch-only on purpose; leave it so.
- AFFINE's repo is local until the owner says it goes to GitHub. Push every finished, checked change once it does,
  with a changelog line. Ask before tags, releases, the installer, or the Firmware Hub.
- Downloads need the owner's yes each time.
- English Markdown files. Little text on the site and the screen (a title and one line, then the thing).
- Breaks: royalty free only (Cyberworm's Amens, `breaks/README.md`) plus the player's own through the import page.
  Never add breaks from commercial records to the repo.
- No PlayStation or Sony marks in product text, art or the name. The PS1 is a reference ("PS1-era").
- The owner hears and sees before anything is kept: renders of 30-40 s with numbers, screenshots, phone shots.

## Clean room (the PSX effects)

- AFFINE's code is its own, written from public descriptions of the hardware (psx-spx). Only documented numbers come
  from the hardware (the reverb presets' registers, the resampler's taps), each credited where it is used.
- Never ROM, BIOS, SDK or emulator code, never audio from games.

## Hard rules (the platform, from CHOPPAH and LOAF BREAKER)

- `app/dsp/` stays platform-free: no `plat_*`, no fm1-base headers, no libc beyond `<stdint.h>`. It builds with plain
  `cc` (`./build.sh test`) and as fm1-base `o2` units.
- **No double precision** anywhere in `app/` (the device build rejects it): `f` on every float literal, no `math.h` in
  dsp (use `brk_math.h`). The tests build with `-Wdouble-promotion -Wfloat-conversion -Werror`.
- No 64-bit division on the device. Keep sample maths in 32 bits.
- The render (`app_render`, `brk_render`) runs in the audio interrupt: no flash writes, no blocking, no allocation.
  Main loop to render through setters (single words) and the key queue.
- The FX chain's per-sample loop (`fx_samples`) is `BRK_RAM` (.ram_text): nothing it reaches may be a call
  (`BRK_INL`); the build fails on "ram_text contains calls". Block-rate code is `BRK_COLD`.
- The render must stay under its 5.8 ms block (256 frames at 44.1 kHz), or audio glitches and the watchdog resets.
- Flash writes stall audio: only on SAVE or while stopped.
- Settings (`settings_t`) only grow at the end; a store layout change needs a version and a conversion on load.
  A saved setting never changes at a restart.
- The canvas is 240x60: draw in strips of 60 rows at most.
- Budgets (FM-1, 2026-11-15 import): app slot 581,564 B (image 568,332); RAM .data+.bss 97,372 of 98,304; pool
  340,876 of 344,064. `./build.sh` prints them: read the `ok image` line after every change.

## Build

Local tools: the JieLi toolchain in `~/.jieli/toolchain`, the AC79 files in `~/fw-AC79_AIoT_SDK`, Docker through Colima.
This Mac's Pillow has no libraqm; until AFFINE has its pixel font, build with the scratch shim that drops the
`tnum` feature, and Python from `../mono2k/.venv`:

```
PYTHON=../mono2k/.venv/bin/python PYTHONPATH=<shim dir> ./build.sh          # app/build/affine.fwsc
PYTHON=../mono2k/.venv/bin/python PYTHONPATH=<shim dir> ./build.sh --host   # app/build/host/affine_host
./build.sh test                                                             # the engine's tests, plain cc
```

## Layout

`app/app.c` is the core (settings, patterns, MIDI, the render); it ends by including `app/ui/*.c` in order: one
unity build. `app/dsp/*.c` are separate `o2` units (brk, brk_fx, kick, perc, ovl; AFFINE's psx).

Platform: 44.1 kHz stereo, 256-frame blocks; 96 KB RAM + 336 KB pool; 240x240 screen; 16 white and 11 black keys,
14 buttons, 7 encoders, the MASTER pot. Store objects of at most 3840 B; data area 0x97000-0xDFFFF.
