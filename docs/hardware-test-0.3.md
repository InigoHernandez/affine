# AFFINE 0.3 on the FM-1: the new controls, the chip's presets, SIZE

`affine-0.3.fwsc`, SHA-256 `5337d76b77960e2cf5e43829c41d4c6a05d80bc018c1584a274f8f56845c913e`, identity
FM-1_2815000300. Install it from the [installer](https://inigohernandez.github.io/affine/install/) (Chrome or Edge,
USB). 0.1's patterns load as they were. Another firmware's saves are left alone. To go back, use the same page's
"Back to the stock firmware".

**Result (2026-11-18, the owner's FM-1): every step good.** The three choices in section 9 stay as they are.

About 30 minutes. Keep a break playing (PLAY) unless a step says otherwise.

## 0. The meter, first
GLO (MIDI), then turn K3 right: **METER ON**. Every second the header says `CPU nn% PK nn%` (the average and the
peak of the audio's time). Leave it on for the whole test.

If AFFINE asks **TUTORIAL or PLAY** (a fresh FM-1), pick PLAY. You can start the tutorial later from GLO twice
(SETUP), K3 right.

## 1. The encoders do the same on every page
On HOME, FX, EDIT and SEQ, turn each encoder:
- SELECT changes the tempo.
- ALGORITHM changes the break.
- PRESETS changes the pattern. While playing, the header says **NEXT Pn** until the pattern changes at the loop's end.

**Expect:** no page gives them another job.

## 2. Pages, parts, items
1. Press each page button once from another page: it opens its first part. Press it again: the next part, round.
   It acts when you let go.
2. FX from HOME: PLAY, CHAIN, SLOT, PSX, SENDS, PLAY…
3. OCT− / OCT+ on a part with items (FX › PSX, EDIT › SNARE) step through them; the header names each, and they stop
   at the ends. On HOME and EDIT › BREAK, OCT changes the pitch.

**Expect:** the header always says where you are, and nothing moves pages by surprise.

## 3. The black keys, both rows
1. Hold keys 1 to 11 one by one: row AFFINE (rolls, REVERSE, HALF, VINYL, FREEZE, SKIP, VERB, CRUSH, FILTER).
2. FX + OCT+: row PUNCH. Hold each: STUTTER, BUZZ, REV VERB, SLOW MO, TAPE STOP, GLITCH, WASH, CHOP, DUB ECHO,
   SHATTER, DROP OUT. FX + OCT−: back to AFFINE.

**Expect:** each sounds while held and stops when you let go. No OVERLOAD message and no crackle, even on GLITCH,
SHATTER and WASH. Note the meter's PK on the heaviest.

## 4. The chip's four presets: the CPU
1. FX four times (PSX), then OCT− until the header says PRESET.
2. K1 steps the presets: **SUBTLE GLUE, POLY PAD, CRUNCHY TAIL, DUB SPACE**. Give each 30 s.
3. Write down each one's CPU and PK.
4. On DUB SPACE, also hold VERB, SKIP and CRUSH and turn HOME's four knobs (HOME, K1-K4).

**Expect:** DUB SPACE is the heaviest. No "OVERLOAD" or "CPU FULL", no dropouts or crackles, and the screen keeps
moving.

## 5. SIZE without clicks
1. On PSX, OCT+ to VERB. K1 TYPE HALL, K4 DEPTH up.
2. Turn K2 SIZE slowly from one end to the other, then fast.
3. K1 TYPE CHAOS, then DELAY: SIZE now steps in note values, so the echoes keep time with the tempo.

**Expect:** the tail bends smoothly. A slight pitch glide while turning is normal; a click or zipper is not. A big
jump fades out and back in.

## 6. INIT, and undo
1. Make it busy: a pattern, HOME's knobs up, a preset on PSX.
2. SEL + HOME, then HOME within 3 s ("INIT? PRESS HOME").
3. SEL + SAVE.

**Expect:** at step 2 you hear the bare break: the pattern cleared and every effect off, but the tempo, the break
and the sounds kept. At step 3 everything comes back; SEL + SAVE again takes it off again.

## 7. No clear without a question
1. Press REC on several pages: it only starts and stops recording.
2. Hold SEQ for 0.7 s ("CLEAR Pn?"): SEQ again within 3 s clears the pattern; anything else cancels.

## 8. Saving
SAVE while stopped, power off, power on.

**Expect:** the pattern, the sounds and the PSX settings are back.

## 9. Three choices to keep or change
- **ENV › DROP** (ENV three times): each white key drops into its pattern.
- **SEL + REC** on FX › SLOT no longer resets the effect (REC just records).
- **A drum's MEMORY** (EDIT three times: SNARE; OCT+ to MEMORY): K1 picks a memory, K2 COPY, K3 PASTE.

## Report
- Each preset's CPU / PK (section 4), and any OVERLOAD or CPU FULL message.
- A click on SIZE: which TYPE, slow or fast.
- Any page where an encoder, a page button or OCT did something you didn't expect.
- The three choices in section 9: keep or change.
