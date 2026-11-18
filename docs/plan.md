# AFFINE: the plan

A PS1-era jungle break chopper for the M-VAVE FM-1. Breaks cut into slices on the keys, performance effects under
every key and knob, and the sound of the PS1's sound chip: its reverbs, its sample grit, its CD audio. Expect this plan
to be rewritten as the owner hears and sees things.

## 0. Decisions

**The owner (2026-11-15):**
- Fork **LOAF BREAKER** (FLEXOR, MPL-2.0, commit 4f8722c), itself CHOPPAH's fork (Laurens Profittlich). Strip the
  hardcore kick; add the PSX effects, performance controls and our own look.
- **Breaks:** Cyberworm's royalty-free Amens built in, plus the site's import page for one's own.
- **Name:** AFFINE (after the PS1's warped, wobbling textures).
- Follow the MONO2K and Demake guidelines (`../FM1-FIRMWARE-HANDBOOK.md`).

**Defaults I chose (say if any is wrong):**
- FM-1 first. The engine stays platform-free, so a Move build stays possible later.
- Identity `FM-1_2815` (0xAFF). The hub's catalogue has no pattern that matches its release ids. USB name "AFFINE FM-1".
- MPL-2.0, as both upstreams. Every file keeps its notice. LOAF BREAKER and CHOPPAH are credited in the README, the
  site and `firmware.json`.
- A fresh git history, starting at one import commit that names LOAF's 4f8722c. This keeps out of AFFINE's repo the
  breaks that aren't royalty free (FLEXOR's, the classic records) and FLEXOR's photos. LOAF's notes are kept for
  reference in `docs/upstream/`.
- The repo stays local until the owner says it goes to GitHub.

## 1. Keep, strip, add

| Keep from LOAF BREAKER | Strip | Add (AFFINE) |
|---|---|---|
| 16 slices on the white keys, hold to loop | the hardcore kick: DISTO, SCREAM, DISTO +, KICK LAB, STACK, TOK, TAIL GATE, REVERSE BASS, KICK 1-20, kick FX slots, kick EQ, knob STYLES | **SPU VERB**: the PS1 sound chip's reverb, its nine presets |
| The generator (the break rearranged live) | the kick's RGB and CLASSIC views, the CDJ scroll in rekordbox colours | **CRUSH** and **REDUX**, digital lofi in depth |
| Black-key pads (rolls, reverse, freeze, half, vinyl, gate, random, filter, dub) | FLEXOR's breaks, the classic records | **SPU GRIT** (the PS1's sample codec) and **XA** (its CD audio codec) |
| The overlay effects on the white keys (ARP) | LOAF's splash, title, face and egg art | **FILTERS**: more types, each a throw |
| 64 patterns, snare, hat, clap lanes, steps with NOTE, HITS, COND, NUDGE | the TUTORIAL's kick lessons (rewritten later) | **HOME as the instrument**: four lofi gestures always under the hands |
| Songs: tracks, drops, builds | the 24 skins (become PSX palettes) | **Motion record**: pads and gestures recorded into the pattern |
| MIX: comp, duck, max out | the Studio editor's kick pages | **Scenes A/B** and a MORPH between them |
| Import pages for breaks and drum samples | | **A light kick (BOOM)**: one clean sub kick with a pitch drop, for jungle's 808s |
| Host simulator, web emulator, installer, CI soak | | PSX look: screen, splash, site |

## 2. The PSX effects

All of them are AFFINE's own code, written from public descriptions of the PS1's hardware. The only things taken from
the hardware are documented numbers: the reverb presets' registers and the resampler's taps, from psx-spx, the
public PS1 hardware reference (credited in the source). No ROM, no SDK code, no emulator code.

| Effect | What it does | Knobs | Memory | CPU (estimate, to measure) |
|---|---|---|---|---|
| **SPU VERB** | The PS1's reverb unit: 22.05 kHz, saturating 16-bit maths, the documented 39-tap resampler. Nine presets: ROOM, STUDIO S, STUDIO M, STUDIO L, HALL, HALF ECHO, SPACE ECHO, CHAOS, DELAY | TYPE, DEPTH (output volume), SIZE (AFFINE's: the addresses scaled 0.5-1x), TONE (into it), FREEZE (feedback held, input closed) | one 64 KB work area (holds every preset, per psx-spx's sizes) | ~2.5 M multiply-adds a second, ~2 % of a core |
| **CRUSH** | Bit depth 16 down to 1, with three ways to lose the bits: TRUNC (as a cheap DAC), ROUND, DITHER (noise under the steps); then a bit mask: AND or XOR mangling for digital breakage | BITS, MODE, MASK, MIX | none | tiny |
| **REDUX** | Sample-rate reduction from 44.1 kHz down to 200 Hz: HARD (sample and hold, every alias kept), SOFT (a filter before it), JITTER (the clock wanders, a cheap converter) | RATE, MODE, JITTER, MIX | none | tiny |
| **SPU GRIT** | The sound through the PS1's sample codec: 4-bit ADPCM in 28-sample blocks with its five predictors, then played back with a 4-point Gaussian at a chosen rate: the PS1's soft, slightly noisy sample sound | RATE (11 to 44.1 kHz), BITS (4 as the PS1, down to 2), FILTER (auto, or one predictor forced), MIX | ~1 KB | small: one block encoded every 28 samples |
| **XA** | The PS1's CD audio codec (XA-ADPCM, 4 predictors) at 37.8 or 18.9 kHz, mono or stereo: the sound of the PS1's FMV and in-game CD music | RATE (37.8 / 18.9), BITS (4 or 8), STEREO, MIX | ~1 KB | small |
| **SKIP** | A disc read error: the last fraction of a beat repeats or jumps, and the laser seeks; on the tempo | LENGTH (1/32 to 1/4), CHANCE, SEEK (the pitch dives while it searches), MIX | the old reverb's 36 KB in RAM, as a capture of the mix | tiny |
| **FILTER** | LOAF's multimode SVF, plus types: LP, BP, HP, NOTCH, COMB+, COMB-, VOWEL | CUTOFF, RESO, TYPE, MIX | none | small |

Every effect is matched to its dry level within ~2 dB and measured (handbook 2, Levels).

## 3. Performance

The owner's ask: super performative. What it means on the FM-1:

1. **HOME is the instrument.** Four knobs are always the four big gestures, with the screen answering every turn:
   K1 FILTER (left low-pass, right high-pass), K2 CRUSH (bits), K3 REDUX (rate), K4 VERB (into SPU VERB).
2. **Black keys: eleven pads, every one latchable.** Held: on while held. Tapped twice: latched until tapped again.
   K1-K4 tune the pad held. AFFINE's layout: R8, R16, R32, REVERSE, HALF, TAPE STOP, FREEZE, SKIP, VERB THROW (a
   burst into SPU VERB: the old DUB pad on the PS1's echoes), CRUSH THROW, FILTER THROW.
3. **White keys on FX › PLAY: the overlays** (ARP's until controls phase 2). LOAF's sixteen, with PSX ones in for
   the weakest: SPU, XA, SKIP, FREEZE.
4. **Motion record.** With REC on, pad presses and the four gestures are written into the pattern's steps, and play
   back every loop. SEQ's clear (SEQ held, then SEQ) and INIT clear them with the pattern. Room: the kick's four freed store objects hold one group of 16 patterns each.
5. **Scenes A/B and MORPH.** Two snapshots of the gestures and the FX chain. HOME's second page: K4 MORPH glides
   between them, a white key stores one (to be placed: PRESETS is the pattern on every page since phase 2).

### The black keys' rows, PUNCH, REC (built 2026-11-16, branch `punch`; CUSTOM gone, branch `controls1`)

The owner's decisions (2026-11-16): **rows** of black-key effects, two: AFFINE (row A, the layout above) and PUNCH
(row B). FX held + OCT- AFFINE, FX held + OCT+ PUNCH, each directly; the header says "ROW B / PUNCH" (no page, no FX
fader, no pitch). The FX fader's toggle moved to FX's release (held 0.7 s alone). The rows are `bk[1]`'s saved
values: AFFINE 0, CHOPPAH 1 (out of the rows, a save holding it still plays it), CUSTOM 2 (gone: a save holding it
plays AFFINE's; `bk[2..12]`, its layout, unused), PUNCH 3. A third row, CUSTOM (FX + a black key + a knob gave the key
any function; ARP › BLACK KEYS edited the layout), was built and taken out the same day (the owner's controls
cleanup): GATE, RANDOM, DUB, ARP and KIL alone are on no row now, but a pattern that recorded them still plays them.

- **PUNCH**: eleven fixed combos (an EP-133 punch-in's way: a signature, not editable), each several of the functions
  held together with its own settings while it sounds, never saved (copies to the engine: `pads_push`, `fx_send` in
  app.c; the table `BKC_T`, codes 32..42): STUTTER (roll 1/16, 1/32 after a beat, the filter rising to a high-pass),
  BUZZ (roll 1/32 + crush), REV VERB (reverse + verb throw), SLOW MO (half speed an octave down + low-pass), TAPE STOP
  (vinyl break + verb throw), GLITCH (skip + random), WASH (freeze + verb throw + low-pass), CHOP (gate 1/16 + crush),
  DUB ECHO (roll 1/8 + dub), SHATTER (random + crush + skip), DROP OUT (BOOM quiet + high-pass). Heard:
  `build/review/sounds/affine-punch-tour.wav` (its times beside it).
- **REC records the black keys** (the motion record's first part, 4. above): playing, each step reached while one is
  held takes its code (the last pressed), quantised as the slices; played back as held over those steps, merged with
  what is held. 64 x 32 bytes in OBJ_FREE0 ("FXR1", 2052 B), written with the patterns, read from AFFINE's own store
  only. SEQ's grid marks the steps recorded (a bar under the step). The other three free objects stay for the gestures.

### Controls cleanup, phase 1 (2026-11-16, branch `controls1`)

The owner's decisions: fewer, safer gestures, one mode for everyone.

- **Two rows**, AFFINE and PUNCH (above); OVERLAY (ARP) keeps one tab, ARP EFFECTS.
- **No clear without a question.** Gone: SEL + REC (the pattern, or a lane on PATTERN), REC held 0.7 s on PATTERN (the
  break's steps and recordings), SEQ held + REC (the drums). REC toggles recording as it goes down, on every page.
  Kept: SEQ held 0.7 s, "CLEAR Pn?", SEQ within 3 s; a lane's CLR knob; SEL + REC on an EFFECTS slot (its effect
  reset).
- **No two-layer shortcuts**: SEL + SELECT (the drums' filter: a roll key's K3 still turns it), SEL + ALGORITHM (BREAK
  TIME, the next break), SEL + ARP to PATTERN › SETTINGS: gone; with SEL held those do what they do alone. SEL + ARP on
  a drum's page (copy, paste its sound) and SEL + PRESETS (the pattern) stay.
- **One mode.** LOAF BREAKER's BEGINNER / EXPERT gone: the full AFFINE always (a save holding BEGINNER too). The
  TUTORIAL still runs in BEGINNER inside it (its steps were written for it: `set.novice` while it runs, app.c
  `NOVICE`). SYSTEM › SETUP K3 is the TUTORIAL (right: start, left: quit); the first start asks TUTORIAL or PLAY. A
  white key held: its knobs, always.
- **INIT**, SEL + HOME, then HOME within 3 s ("INIT? PRESS HOME"; anything else, or 3 s: nothing): the pattern
  cleared as SEQ's clear does, and the sound back to the bare break: HOME's FILTER, CRUSH, REDUX, VERB and the drums'
  filter 0, the gate full, every black key's latch and LATCH off, the OVERLAY's latched effects and LATCH off, the
  row AFFINE, the FX chain's slots bypassed (their effects kept), the PSX bus clean (CODEC off, SKIP off, SPU VERB's
  freeze off, COLOR's AMOUNT 0, CONSOLE 0, MONO off, its SEND 0). Kept: the tempo, the break, BOOM and the drums'
  sounds, MIX, songs, the skin, MIDI, the other patterns. SEL + SAVE brings all of it back (seq.c `undo_ifx`, kept
  with the pattern's undo), and again takes it off.
- **RESET EVERYTHING** clears the drum steps' own settings, the track items' breaks and every group's drum bars 3-4
  (they survived it, written into all four groups).

### Controls, phase 2 (2026-11-16, branch `controls2`)

The owner's decisions: "simplify and add more coherence to the workflow ... remove redundancies and locate FX same
place because ARP has other effects too ... more intuitive and predictable all the actions". Navigation is UI state:
no saved field changed.

**Global rules**

1. **Encoders, every page:** SELECT the tempo (EXT CLOCK when a clock leads), ALGORITHM the break (next / previous),
   PRESETS the pattern (playing: queued to the loop's end, the header says NEXT Pn until it lands; stopped: at once).
   No page gives them another job; SEL + PRESETS is PRESETS.
2. **Page buttons** (HOME, FX, EDIT, SEQ, ARP, ENV, LFO, GLO), each a domain named on the screen as the button is
   printed: from another domain, its first part; on its own, the next part, round; at the release, so a hold means
   one other thing. Anything else done while one is held (a button, a key, a knob): no part at its release. The
   holds: SEQ 0.7 s CLEAR Pn? (SEQ again clears), FX 0.7 s alone the FX fader 0 / 100 %, ENV 0.45 s on TRACKS a gap,
   LFO + K1-K3 the levels, SEQ + white key 1-4 a bar copied, SEQ + K1 the lane shifted.
3. **OCT- / OCT+:** on a part with items, the previous / next item (the ends stay; the header names it); on a part
   without, the pitch (both: 0). REC on and stopped: step recording's, every page. FX + OCT- / OCT+: the rows
   AFFINE / PUNCH from any page.
4. **K1-K4:** the item's knobs.
5. **The header:** the tab bar under it names the DOMAIN, a pip for each part (the open one lit), the PART in a lit
   tab, the item at the right; the header says them as a part opens. HOME's PERFORM and FX › PSX draw their own (PSX:
   FX PSX, the item, the parts' pips). The SEL card: SEL's five, then the part's three lines.

**The map** (`app/ui/main.c`, DM_* and nav_*)

| Button | Parts (button again) | Items (OCT) | Knobs |
| --- | --- | --- | --- |
| HOME | PERFORM · SETTINGS | none (OCT the pitch) | PERFORM: FILTER CRUSH REDUX VERB. SETTINGS: VIEW (waveform / slices, HOME held before) GATE ZOOM BREAK TIME |
| FX | PLAY · CHAIN · SLOT · PSX · SENDS | PLAY: the rows AFFINE, PUNCH. CHAIN: slot 1-4. SLOT: MAIN, MORE, MOD (an MB COMP its six). PSX: PRESET, VERB, CODEC, COLOR, LOFI, SKIP, OUT. SENDS: none | PLAY: AMOUNT RATE SWEEP LATCH of the last effect (the white keys the sixteen effects, ARP's OVERLAY before). CHAIN: TYPE (PRESETS before), MIX (the slot's K4: SEND on a DELAY, MID FREQ on an EQ), ON/OFF (SEL + FX before), POSITION (FX held + ALGORITHM before). SLOT: the effect's. PSX: the stage's; PRESET's K1 the four presets (PRESETS before). SENDS (MIX's before): FX, DRUMS, ARP, ROLL |
| EDIT | BREAK · BOOM · SNARE · HAT · CLAP | BREAK: none (OCT the pitch). BOOM: BOOM, SHAPE. Drums: SOUND, SHAPE, MORE, MEMORY | BREAK (SYSTEM's PITCH part's code before, kick.c `brkp_*`): SPEED/KEEP GRAIN SNAP LEVEL. BOOM: K1 its preset. MEMORY: K1 the memory 1-10 (PRESETS before), K2 COPY, K3 PASTE (SEL + ARP before) |
| SEQ | STEPS · DRUMS · SETTINGS | STEPS, DRUMS: the lanes top to bottom with their bars (ALGORITHM before); from another domain, BOOM's lane | the lane's / the step's, as before. SETTINGS: SWING, BREAK ROWS, FOLLOW |
| ARP | GENERATOR | none | COMPLEXITY ANCHOR ROLL FILL (HOME's second part before) |
| ENV | TRACKS · PLAY · DROP · BUILD | TRACKS: the song's items (ALGORITHM before). BUILD: BUILD, TEMPO RISE | TRACKS: K1 the track (PRESETS before), PUT, LOOPS, BREAK. PLAY: TRACK FROM END. DROP: BUILD AFTER BREAK CRASH, and a white key the drop into its pattern (SEL + ENV before). TEMPO RISE: K1 (PRESETS before) |
| LFO | LEVELS · MASTER · DYN | none | as before (SENDS went to FX) |
| GLO | MIDI · SETUP · SCREEN · RESET | SCREEN: EFFECTS, FX SET, VIEWS | VIEWS: K1 the drums' view RGB / WAVE / CLASSIC (EDIT held before). RESET keeps its five turns and GLO |

On CHAIN, SLOT, PSX and SENDS the white keys stay slices. Entering CHAIN, PSX, SENDS or PLAY puts the slot on MAIN;
SLOT is the slot CHAIN chose.

**SEL after phase 2:** tapped the card; twice locked; SEL + SAVE undo; SEL + HOME, then HOME INIT; SEL + SEQ copy /
paste; SEL + PLAY on TRACKS the start item; SEL with a white key held that key back to defaults. Nothing else (in the
TUTORIAL, SEL + GLO still skips a step). Gone: SEL + REC's effect reset (no replacement), SEL + EDIT, SEL + ENV, SEL +
ARP, SEL + FX (moved, above); HOME held and EDIT held (their views: knobs, above).

**Details chosen where the owner left them open:** a drum's memory on a MEMORY item (its first page's knobs are all
taken), with COPY and PASTE beside it; the drum views on GLO › SCREEN's VIEWS item (one place, not a drum item); the
drop on ENV › DROP's white keys (each its pattern), the one place a page could hold it; CHAIN's K2 the slot's own K4
(its name shown: SEND, MID F); CHAIN's K3 right on, left off; PSX's PRESET K1 shows its place (2/4), the picture its
name; SLOT on an empty slot has the one item MAIN; OCT on the items stops at the ends (no wrap).

**The TUTORIAL** (28 steps): HOME, PLAY, slices, ALGORITHM; HOME again (SETTINGS), again (PERFORM); a black key; FX
(PLAY), two effects, OCT+ (PUNCH), OCT- (AFFINE); SEQ, two BOOMs, OCT+ to the snare and one; EDIT (BREAK), again
(BOOM), PLAY (stop), keys 1-4; EDIT again (SNARE), K1, K2; PLAY, FX, again (CHAIN), K1 an effect; ENV three times
(DROP), a white key; SAVE.

Checks: scenarios nav_global, nav_fx, nav_edit, nav_seq, nav_song; every older one rewritten for the map (105 of 113,
the 8 known); the parts' sheet `build/review/controls2-sheet.png`.

### The PSX bus (built, 6a2a425)

LOAF's chain runs per sample from RAM, where nothing may call out; the PSX effects work on blocks. So they sit on a
fixed bus after the chain, before the limiter, run by `brk_fx_process`:
CODEC (GRIT or XA) -> SPU VERB -> CRUSH -> REDUX -> SKIP. SPU VERB is one reverb, as the console had one: the mix by
SEND (HOME's K4), the slices' reverb throws and the VERB pad all go into it. CRUSH, REDUX and SKIP come after it, so
they take its tail with them. LOAF's plate is retired (a REVERB slot passes the sound on).

### The PSX colour (2026-11-16, branches `psxcolor`, `psxcolor2`)

The PS1's sample sound as a stage of its own, in four phases (the owner's spec):
1. GRIT's path grown into the colour stage: RATE (GRIT's, 44.1, 22.05, 11.025, 8 kHz), RAW or FILTERED, the SPU-ADPCM
   round trip with CRUNCH, the dry delayed to its latency; one Q16 clock (41553c9).
2. psx-spx's gaussian table and SMOOTH (nearest .. linear .. gaussian) (f286cb5).
3. SPU VERB's INPUT (the gain into its 16-bit model, 0.25..4: above 1 it saturates as loud games did; twice in is
   +4 dB out on a break, a tone gains products 50 dB up at 4); the send glides within a block; a new type (or a leap
   of SIZE) fades out, clears, fades in; a knob's SIZE moves glide a tick at a time (fractional reads, write heads
   that write every cell they pass, three semitones of bend at most; they jumped once a block before). The echo
   presets as dub delays, measured: DELAY one repeat (186..743 ms by SIZE, no feedback), CHAOS a repeat on the same
   period at -0.07 dB each (near forever, inverted), SPACE ECHO a diffuse cloud from ~80 ms (4.2 s tail), HALF ECHO a
   dense 16 ms flutter. XA on the colour stage's Q16 clock: its latency 38.9 samples at 37.8 kHz and 77.6 at 18.9,
   the same over 5 minutes (it wandered 14..181 before); its dry aligned; on and off faded. XA 558 -> 440 Mac
   instructions a sample; SPU VERB 330, 505 while SIZE glides (c72e996).
4. CONSOLE OUT after SKIP (two one-poles -6 dB at 20 kHz .. 8 kHz, a 10 Hz DC blocker, MONO, a cubic clip: harmonics
   44 dB under a -6 dBFS tone at 1; 63 Mac instructions a sample). AMOUNT 0..1 scales the codec's mix (GRIT or XA),
   CRUNCH, CONSOLE and the PSX's SEND into SPU VERB from neutral to their knobs: 0 is the dry bit for bit, 1 as the
   knobs say (the default). The PSX page grows COLOR (AMOUNT, RATE: GRIT's, 44K, 22K, 22K RAW, 11K, 11K RAW, 8K,
   8K RAW; CRUNCH, PCM below 0; SMOOTH) and OUT (CONSOLE, MONO, SEND, INPUT: VERB's four knobs were taken); SIZE steps
   note values at the tempo on CHAOS and DELAY; PRESETS the four presets (SUBTLE GLUE, POLY PAD, CRUNCHY TAIL,
   DUB SPACE), every stage at once, each within 0.7 dB of the dry. settings_t grows by psx_color_t (28 B, 2088 ->
   2116): an older save loads it at its defaults (the bus bit for bit as at 0b455f5 but for SPU VERB's smoothing).
   Full chain, Mac instructions a sample: GLUE 421, PAD 654, TAIL 565, DUB 724 (a core ~1570). Renders:
   build/review/psx_color2/. Waiting: the owner's ear, the FM-1's CPU meter with a preset on.

## 4. The look

**The owner picked A, POLYGON (2026-11-15).** HOME is built (1cc85ac, `app/ui/aff.c`, off the device in
`tests/look/home.c`); the rest of the UI follows it after the kick strip. LOAF's pages are laid out for Barlow;
moving them to the 2x pixel font is a page-by-page job, HOME first.

Three directions were drawn as stills before any UI was built (handbook: settle the look cheaply). All
follow the MONO2K/Demake rules: one pixel grid, crisp, big type, dithered light, micro-motion, a palette whose
colours each have a job, no rainbows, no brand marks.

- **A. Polygon.** Low-poly shapes, gouraud-shaded and drawn without perspective correction (textures bend), the PS1's
  4x4 ordered dither over 15-bit colour, vertices that jitter to the pixel grid.
- **B. Memory card.** The patterns as save blocks, each a 16 x 16 icon with three frames of animation; menus as the
  era's console menus: deep blue gradients, bevelled bars.
- **C. Disc.** The break as a spinning disc; the slices as its sectors; the effects as tracks on a CD-player face.

## 5. Budget

FM-1 build at the import (568,332 B image, 13.2 KB free; RAM 97,372 of 98,304; pool 340,876 of 344,064).
With the PSX bus (6a2a425): image 578,132; RAM 89,400; pool 339,100. Done so far: the plate's 36 KB of RAM back,
SKIP's capture 24 KB of RAM (mu-law), the delay line mu-law (61 -> 31 KB of pool), the break buffer 4.8 -> 4.0 s
(37 KB of pool), SPU VERB's 64 KB in the pool. The plan as made:

| | Frees | Costs |
|---|---|---|
| Flash | the kick's DSP and UI (est. 40-50 KB), LOAF's art (done: 5 KB more) | PSX effects ~10 KB, AFFINE's art ~15 KB, a pixel font (smaller than Barlow's 25 KB) |
| RAM | the old plate reverb's 36 KB, the kick's FX memories 3 KB | SKIP's capture 32 KB, the new effects' state ~2 KB |
| Pool | DUB's line 60 KB, the kick's rumble line 8 KB, KICK 11-20 2 KB | SPU VERB's work area 64 KB |
| CPU | the kick's render and its FX slots | SPU VERB ~2 %, the rest small |

The second core (fm1-base can build it in) stays in reserve: SPU VERB moves there if the meter says so.
**Measure the device's CPU on day one** (handbook, short path 5): LOAF's METER (SYSTEM > MIDI K3) on the
first AFFINE build, read while playing.

## 6. Steps

1. **Import** (done): the fork, the royalty-free bank, the identity, this plan.
2. **Engine** (done, 06dc900): the PSX effects as one platform-free module (`app/dsp/psx.c`), 38 checks and demo
   WAVs in `./build.sh test`, a host bench (`build/engine/psx_bench`). Waiting: the owner's ear.
3. **The look** (done: the owner picked POLYGON).
4. **Strip the kick** (done, merged 32d9637): LOAF's hardcore kick out (image -37 KB, RAM -6 KB, pool -10 KB),
   BOOM in (808, JUNGLE, DUB, TIGHT; TUNE, DECAY, LEVEL; SHAPE in EXPERT), settings magic AFF1, OBJ_FREE0..3 kept.
5. **Wire the effects** (done): the PSX bus (6a2a425); HOME's four gestures (1eeec85); the PSX page, EFFECTS'
   fifth tab (6488140); the black keys VRB, SKP, CRS and AFFINE's layout (574fac2). Still: PSX overlays on ARP.
6. **The picked look** (done, 567ab94): HOME and the PSX page drawn whole in POLYGON; every other page in the pixel
   font (small labels 1x, names and values 2x), AFFINE's five skins (POLYGON first), the foot as HOME's panel,
   AFFINE's title on the boot, the start screen and RESET ALL. Still: page bodies to 2x one by one, the update
   screen (felucca-bsp's today).
7. **Motion record and scenes.** The black keys recorded with REC and the PUNCH row (2026-11-16, branch `punch`);
   still: the four gestures recorded, scenes A/B.
8. **Device:** the owner's FM-1 test (0.3: done 2026-11-18, every step good, `docs/hardware-test-0.3.md`), the CPU meter read; then the manual, the hub kit.
9. **The site** (done 2026-11-16, live on GitHub Pages): MONO2K's structure in POLYGON, the firmware in the page,
   the installer. Still: the import page restyled (LOAF's `app/site/import.html`), a manual.
10. **Release 0.1** (done 2026-11-16, at the owner's "publish", untested on hardware): v0.1 and the installer. The Hub
   mail after the FM-1 tests (`../FIRMWARE-HUB-HANDBOOK.md` section 5); decide the identity (FM-1_9NN for the Hub's resume).

## 7. Open, for the owner

- Which ten breaks go into the bank? (A first pick at 160-175 BPM is in; a listening page can come with the demos.)
- BOOM: keep a light kick for jungle subs, or the breaks' own kicks only? (Not answered: BOOM is built, the plan's
  default; its four presets in build/review/sounds/affine-boom-tour.m4a.)
- The sounds: the effects tour and the nine reverbs.
