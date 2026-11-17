# AFFINE: notes for Claude sessions

Firmware for the M-VAVE FM-1 (JieLi AC791N): a PS1-era jungle break chopper. Plan and decisions: `docs/plan.md` (keep it
current). Forked on 2026-11-15 from LOAF BREAKER (FLEXOR, https://github.com/FLEXOR-R/loaf-breaker, 4f8722c), itself
CHOPPAH's fork (Laurens Profittlich). LOAF's own notes, PRD and README are in `docs/upstream/`: read them as a map of
the code inherited, never as rules for AFFINE (their release, push and changelog rules are FLEXOR's).
The owner's lessons from MONO2K and Demake: `../FM1-FIRMWARE-HANDBOOK.md`. Add to it after every AFFINE update.

## The owner's rules

- **Never push to FLEXOR's or Laurens' repositories.** The `upstream` remote is fetch-only on purpose; leave it so.
- AFFINE is public since 2026-11-16 at the owner's word: https://github.com/InigoHernandez/affine (`origin`), its
  site on GitHub Pages from `web/` (`.github/workflows/pages.yml`, nothing built there). Push every finished, checked
  change with a line in `CHANGELOG.md` (newest day first, a kind on every line). Ask before tags, releases, the
  installer, or the Firmware Hub.
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
- Budgets (FM-1, 567ab94): app slot 581,564 B (image 548,064); RAM .data+.bss 77,832 of 98,304; pool 328,252 of
  344,064. `./build.sh` prints them: read the `ok image` line after every change.

## AFFINE's own parts (where to look)

- `app/dsp/psx.c`: the PSX effects and the PSX bus (run by `brk_fx_process` after the chain, before the limiter:
  CODEC, SPU VERB, CRUSH, REDUX, SKIP). Settings in `set.psx` (psx_bus_t), applied by `psx_apply` (app.c), which
  folds in the held keys VRB, SKP, CRS. LOAF's plate reverb is retired (a REVERB slot passes the sound on).
  GRIT is the colour stage (`psx_color_set`: rate, RAW/FILTERED, ADPCM, CRUNCH, SMOOTH) and XA runs on its Q16 clock
  (no drift); SPU VERB has INPUT (`psx_verb_input`: drives its 16-bit maths), a send that glides in each block, a type
  or a SIZE leap faded out and in, and SIZE knob moves gliding tick by tick (`verb_tick_glide`: fractional reads, write
  heads that fill what they pass). After SKIP, CONSOLE OUT (low-pass, DC blocker, MONO, a slight cubic clip). The colour's
  own settings are `set.pxc` (psx_color_t, settings_t's tail since 16 Nov 2026: an older save loads its defaults, the
  sound as before), sent by `psx_apply` (`psx_bus_color`): COLOR's RATE (RAW in its steps), CRUNCH (PCM below 0),
  SMOOTH, AMOUNT (scales the codec's mix, CRUNCH, CONSOLE and SEND from 0 clean to 1 at their knobs), OUT's CONSOLE,
  MONO, SEND, INPUT; `psx_preset` the four presets (the PSX page's PRESET item, K1). Checks: `tests/engine/psx_color_demo.c`,
  `psx_color2_demo.c` (`PSX_COLOR2_RENDERS=1` renders to build/review/psx_color2/), scenario `psxcolor`; costs:
  `psx_color_bench.c` (Mac instructions a sample with the FM-1's flags).
- `app/ui/affont.c`: every page writes in AFFINE's pixel font (`app/aff_font.h`, CC BY-SA 4.0): macros route LOAF's
  `cv_text` / `text_w` to it (FONT_B 2x; FONT_T, FONT_XS 1x). New text: use `aff_text` / `aff_knob_cell`.
- `app/ui/aff.c`: the POLYGON look (palette and dither ramps from the skin, HOME's picture, `aff_knob_cell`, the title
  screen `aff_title`); `home_draw` in main.c feeds HOME. HOME's knobs: FILTER, CRUSH, REDUX, VERB; the gate is HOME's
  SETTINGS K2. The foot on every page is HOME's panel (main.c `draw_foot`).
- Skins (common.c): AFFINE's five, POLYGON first; `C_BLACK` is the skin's ground inside the UI.
- `tools/contact_sheet.py NAMES`: the simulator's screenshots side by side (after `sh tests/scenarios.sh`).
- `app/ui/psxp.c`: the PSX page (FX's fourth part, `FX_PSX()`): seven items on OCT (PRESET, its K1 the four presets;
  VERB, CODEC, COLOR, LOFI, SKIP, OUT); its text through `pp_text` (the host's audit sees it).
- The controls (phase 2, `docs/plan.md` section 3): `app/ui/main.c`'s DM_* / nav_* map each page button to a domain,
  its parts (the button again) and items (OCT); the encoders are the tempo, the break, the pattern everywhere.
- BOOM (LOAF's kick stripped): `app/dsp/kick.c` and `app/ui/kick.c`.
- Tests: `./build.sh test` (engine + PSX checks), `sh tests/scenarios.sh` (every host scenario; 8 inherited
  failures listed as KNOWN in it), `tests/look/home.c` (HOME drawn off the device).

## Build

Local tools: the JieLi toolchain in `~/.jieli/toolchain`, the AC79 files in `~/fw-AC79_AIoT_SDK`, Docker through Colima.
This Mac's Pillow has no libraqm; until AFFINE has its pixel font, build with the scratch shim that drops the
`tnum` feature, and Python from `../mono2k/.venv`:

```
PYTHON=../mono2k/.venv/bin/python PYTHONPATH=<shim dir> ./build.sh          # app/build/affine.fwsc
PYTHON=../mono2k/.venv/bin/python PYTHONPATH=<shim dir> ./build.sh --host   # app/build/host/affine_host
./build.sh test                                                             # the engine's tests, plain cc
```

## The site (`web/`)

MONO2K's structure (one 12-column grid with dotted rules, numbered sections, the playbar and drawers) in POLYGON.
`sh web/build.sh` gathers what it takes from the build: `emu/` (`./build.sh --web`: the firmware in WebAssembly;
`--copy` skips the build), `screens/` (the simulator's shots, `sh tests/scenarios.sh` first, as palette PNGs) and
`sounds/` (psx_demo.c's renders, AAC). Serve it with `.claude/launch.json`'s affine-web (port 8781). `site.js` the
skins (they recolour the page, the panel and the FM-1's screenshots), the dither patterns and the motion; `scene.js`
the hero (aff.c's title screen in JS); `panel.js` and `play.js` the FM-1 (from fm1-base's emulator page, MPL-2.0);
`chip.js` the PSX bus heard; `perform.js` the black keys (held on the emulator too). Bump `?v=` in index.html with
every changed file. Little text: a title and one line a section.
Components (since 2026-11-17, after shadcn/ui, transitions.dev and Libraries.dev; style.css's head lists them): one
`.btn` (`--primary` / `--outline` / `--solid`, `--sm` / `--lg`), `.card` with `.card__head` / `.card__foot`, `.badge`,
`kbd` a keycap; tokens `--s05`..`--s16`, `--h-sm/md/lg`, `--pad` (content at most `--max` 1440 px) and transitions.dev's
motion tokens. The pixel reveals stay stepped; layout motion takes the tokens (the drawers' accordion in play.js, the
subnav's sliding bar in site.js, the chain's block from chip.js's `--on`). `[hidden]` always wins over a display.
The installer's look is `tools/install_theme.py` (regenerate only `install/index.html` with make_site.py and it, the
firmware untouched). Checks: `node tools/site_shot.mjs URL OUT 1440,1024,768,390,360 [--js=...]` (headless Chrome over
the DevTools protocol: true phone widths, errors, overflow) and `tools/site_motion.mjs` (GIFs of the motion).

Releases: `./build.sh --release X.Y` (identity `FM-1_2815` + XXYYZZ), then `RELEASE=X.Y sh web/build.sh` puts the same
file in `web/firmware/` and the installer in `web/install/` (fm1-base's, themed by `tools/install_theme.py`;
`UNTESTED=1` says on it that the package has not run on an FM-1); tag `vX.Y` on the commit, a GitHub release with the
`.fwsc` whose digest matches the site's. The Hub (`../FIRMWARE-HUB-HANDBOOK.md`): `fmhub.json` and `docs/hub/` are
ready, the mail waits for the tests on a real FM-1; its resume of an interrupted update wants `FM-1_9NN`, which
AFFINE's identity is not (the owner's call). 0.1: 2026-11-16 at the owner's word ("publish"), before any FM-1 had
run it (sha 98eb4c10). 0.2: 2026-11-16 on the site's installer only (the owner: "upload for instaling fast"),
untested on an FM-1 (sha aa1b17ac); then tag v0.2 (188bac2) and its GitHub release at the owner's word ("Yes, now"). 0.3: 2026-11-16 on the installer (the owner: "update go"), the
controls redesign, untested on an FM-1 (sha 5337d76b); then tag v0.3 (d9f74e9) and its GitHub release on
2026-11-17 at the owner's word ("yes").

## Layout

`app/app.c` is the core (settings, patterns, MIDI, the render); it ends by including `app/ui/*.c` in order: one
unity build. `app/dsp/*.c` are separate `o2` units (brk, brk_fx, kick, perc, ovl; AFFINE's psx).

Platform: 44.1 kHz stereo, 256-frame blocks; 96 KB RAM + 336 KB pool; 240x240 screen; 16 white and 11 black keys,
14 buttons, 7 encoders, the MASTER pot. Store objects of at most 3840 B; data area 0x97000-0xDFFFF.
