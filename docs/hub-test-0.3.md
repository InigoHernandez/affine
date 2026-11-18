# AFFINE 0.3: the Firmware Hub's tests

The Hub lists a firmware only after these, run on a real FM-1 with the release's file: `affine-0.3.fwsc`, SHA-256
`5337d76b…845c913e`, identity FM-1_2815000300. The play test is done (`docs/hardware-test-0.3.md`); these are about
getting on and off the FM-1.

About 20 minutes. Chrome or Edge, a data USB cable, plugged straight into the computer. Two pages:
AFFINE's [installer](https://inigohernandez.github.io/affine/install/) and [the Hub](https://thefirmwarehub.com).
Close any other page that uses MIDI. Step 1 may lose what AFFINE saved: back it up first (the installer's
**Back up AFFINE**).

## 1. From V15
1. On AFFINE's installer: **Back to the stock firmware**, then **Install the stock firmware**.
2. When the FM-1 runs M-VAVE's firmware, press **Install** (AFFINE 0.3).

**Expect:** the page says Done and AFFINE starts. A fresh FM-1 asks TUTORIAL or PLAY: pick PLAY.

## 2. USB
1. On a Mac: Audio MIDI Setup, Window › Show MIDI Studio.
2. On the Hub's page, connect the FM-1.

**Expect:** the device is named **AFFINE FM-1**. The Hub finds it (AFFINE isn't listed yet, so it may not name it).
Write down what each shows.

Then make a pattern you'd recognise and **SAVE** it while stopped.

## 3. Leave it (the one that matters most)
With AFFINE running, install another firmware **from the Hub** (MONO2K or Demake). Don't press any key yourself.

**Expect:** it installs and starts, with no OCT− rescue.

## 4. From another firmware
With that firmware running, AFFINE's installer, **Install**.

**Expect:** AFFINE starts.

## 5. What it keeps
Look for the pattern you saved in step 2.

**Expect:** it's back, or AFFINE starts fresh. Never garbage, never a crash. Note which.

## 6. An install cut short
1. AFFINE's installer, **Install** again.
2. While the bar runs, unplug the USB cable.
3. Plug it back in and press **Install**.

**Expect:** it carries on to Done and AFFINE starts. (The Hub itself won't resume AFFINE: that needs an `FM-1_9NN`
identity. This checks AFFINE's own installer.)

## If something fails
Write down the page's message. If the FM-1 won't start: hold **OCT−** while switching it on, then install again.

## Report
- From V15: ok / not.
- USB name: ____. What the Hub showed: ____.
- Left AFFINE for ____ from the Hub: ok / not.
- From ____ back to AFFINE: ok / not.
- The saved pattern: back / fresh.
- Install cut short: carried on / not.
