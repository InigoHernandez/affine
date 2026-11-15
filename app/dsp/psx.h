/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE's PS1-era effects: the sound chip's reverb (SPU VERB), its sample codec (SPU GRIT), its CD audio codec (XA),
 * and digital lofi (CRUSH, REDUX, SKIP). AFFINE's own code, written from psx-spx (https://psx-spx.consoledev.net/,
 * the public reference for the console's hardware); the only numbers taken from the hardware are documented ones:
 * the reverb presets' registers, the reverb resampler's taps and the ADPCM predictors, each credited where it is used.
 * Platform-free: <stdint.h> and brk_math.h only. Parameters are set from the main loop (words read once a block);
 * the *_process functions run in the render. Stereo float in and out, 44.1 kHz. */
#ifndef PSX_H
#define PSX_H
#include <stdint.h>

/* ---- SPU VERB: the reverb unit, at 22.05 kHz with saturating 16-bit maths. The work area is the caller's (the
 * pool): PSXV_WORDS 16-bit cells hold every preset (psx-spx gives Chaos Echo and Delay 10000h bytes at least) */
#define PSXV_WORDS 32768u                /* 64 KB; a power of two */
enum { PSXV_ROOM, PSXV_STUDIO_S, PSXV_STUDIO_M, PSXV_STUDIO_L, PSXV_HALL, PSXV_HALF_ECHO, PSXV_SPACE_ECHO,
       PSXV_CHAOS, PSXV_DELAY, PSXV_N };
extern const char *const PSXV_NAME[PSXV_N];
void psx_verb_init(int16_t *work, uint32_t words);   /* words: a power of two, PSXV_WORDS for every preset */
/* type PSXV_*; depth: the output volume 0..1; size: the presets' addresses scaled (0..1: a quarter .. as the
 * hardware, past 1 only where the work area allows); tone: 0 bright .. 1 dark (a low-pass into it: AFFINE's);
 * freeze: 0..1, the input closed and the reflections held (1: almost forever) */
void psx_verb_set(uint32_t type, float depth, float size, float tone, float freeze);
/* in: what is sent into it; out: the reverb alone (wet), written. Delay through the resamplers: 38 samples */
void psx_verb_process(const float *inl, const float *inr, float *outl, float *outr, uint32_t n);

/* ---- SPU GRIT and XA: the sound encoded and decoded through the console's ADPCM in real time, 28-sample units.
 * GRIT: SPU-ADPCM, five predictors, at a chosen rate, played back through a four-point cosine-window interpolator
 * (a Nuttall window: psx-spx describes the console's table as one); XA: XA-ADPCM, four predictors, at 37.8 or
 * 18.9 kHz. bits: 4 as the console (8 for XA's 8-bit mode); fewer is AFFINE's (2..3, grittier) */
typedef struct { float rate; uint32_t bits, filter, xa, stereo; } psx_codec_p;   /* filter 0 auto, 1..5 one forced */
void psx_grit_set(float rate, uint32_t bits, uint32_t filter);   /* rate 11025..44100 Hz */
void psx_grit_process(float *l, float *r, uint32_t n, float mix);
void psx_xa_set(uint32_t half_rate, uint32_t bits8, uint32_t stereo);   /* half_rate: 18.9 kHz, else 37.8 */
void psx_xa_process(float *l, float *r, uint32_t n, float mix);

/* ---- CRUSH: bit depth 16..1 (bits, fractional: the steps glide), the way the bits go, and a bit flipped */
enum { CRUSH_TRUNC, CRUSH_ROUND, CRUSH_DITHER, CRUSH_MODES };
extern const char *const CRUSH_MODE_NAME[CRUSH_MODES];
void psx_crush_set(float bits, uint32_t mode, float mask);   /* mask 0 none .. 1: an XOR on one bit, low to high */
void psx_crush_process(float *l, float *r, uint32_t n, float mix);

/* ---- REDUX: the sample rate down to 200 Hz: HARD (held, every alias), SOFT (a low-pass before), JITTER (the clock
 * wanders: jitter 0..1) */
enum { REDUX_HARD, REDUX_SOFT, REDUX_JITTER, REDUX_MODES };
extern const char *const REDUX_MODE_NAME[REDUX_MODES];
void psx_redux_set(float hz, uint32_t mode, float jitter);
void psx_redux_process(float *l, float *r, uint32_t n, float mix);

/* ---- SKIP: a disc that can't read: on the tempo, a fraction of the beat repeats or the laser seeks. Its capture is
 * the caller's (RAM): 2 x frames bytes, mu-law stereo (12288 frames: 0.28 s, a 1/8 note at 107 BPM and up) */
#define PSX_SKIP_FRAMES 12288u
void psx_skip_init(uint8_t *ring, uint32_t frames);
/* div: the length, 0..3 = 1/64 1/32 1/16 1/8 note; chance 0..1 a skip at each length's boundary; seek 0..1 the pitch
 * diving while it searches */
void psx_skip_set(uint32_t div, float chance, float seek);
extern const char *const SKIP_DIV_NAME[4];
uint32_t psx_skip_active(void);          /* 1: a skip is sounding now (for the screen) */
/* beat: where the block starts, in beats; spb: samples a beat (the engine's) */
void psx_skip_process(float *l, float *r, uint32_t n, float beat, float spb, float mix);

void psx_init(void);                     /* every effect cleared and off (after psx_verb_init / psx_skip_init) */

/* ---- the PSX bus: the effects in one fixed order after the FX chain, before the limiter (brk_fx_process runs it):
 *   CODEC (GRIT or XA: the sources as the console's samples) -> SPU VERB (one reverb, as the console has: the mix sent
 *   by SEND, the slices' throws, the VERB pad) -> CRUSH -> REDUX -> SKIP (they take the reverb's tail with them).
 * The four gestures (HOME's knobs) are FILTER (brk_fx_t.dj), crush, redux and send; the rest are the PSX page's */
enum { CODEC_OFF, CODEC_GRIT, CODEC_XA, CODECS };
extern const char *const CODEC_NAME[CODECS];
typedef struct {
    float crush;                         /* 0 off .. 1: 16 bits down to 2 */
    float redux;                         /* 0 off .. 1: 44.1 kHz down to 200 Hz */
    float send;                          /* 0..1: the mix into SPU VERB */
    float vthrow;                        /* 0..1: the VERB pad's throw (the mix sent at full while it's held) */
    uint8_t vtype, cmode, rmode, codec;  /* PSXV_*, CRUSH_*, REDUX_*, CODEC_* */
    float vdepth, vsize, vtone, vfreeze; /* SPU VERB: psx_verb_set's */
    float cmask, rjit;                   /* CRUSH's MASK, REDUX's JITTER */
    float crate, cmix;                   /* CODEC: GRIT's rate (11025..44100), its mix */
    uint8_t cbits, cfilt, xhalf, xstereo;/* GRIT's bits (2..4) and forced predictor (0 auto); XA 18.9 kHz, stereo */
    uint8_t skdiv, rsv[3];               /* SKIP: 0..3 = 1/64 .. 1/8 */
    float skchance, skseek, skmix;
} psx_bus_t;
void psx_bus_defaults(psx_bus_t *b);
void psx_bus_set(const psx_bus_t *b);    /* main loop: copied at the next block */
/* the render: l, r in place; vsend: more into SPU VERB (mono; 0 none); beat: where the block starts, in beats; spb:
 * samples a beat */
void psx_bus_process(float *l, float *r, const float *vsend, uint32_t n, float beat, float spb);
/* the overload guard's level (brk_fx's: slots shed, the render over 90 %): 1 the CODEC rests, 2 SKIP too, 3 SPU VERB
 * too (CRUSH and REDUX cost next to nothing) */
void psx_bus_shed(uint32_t level);
float psx_crush_bits(float crush);       /* a gesture's value as the screen says it: bits; Hz */
float psx_redux_hz(float redux);
uint32_t psx_verb_busy(void);            /* 1: SPU VERB is sounding (its tail, a freeze) */
/* for the screen: a preset's four early echoes (its combs: ms after the sound goes in, at SIZE, and their gains
 * -1..1) and its tail (seconds to -60 dB at SIZE 1, measured by tests/engine/psx_demo.c) */
void psx_verb_taps(uint32_t type, float size, float *ms, float *gain);
float psx_verb_tail(uint32_t type);

/* 8-bit mu-law (G.711), for the delay line: 14-bit range in, a byte out and back */
uint8_t psx_ulaw_enc(int32_t s);         /* s: -32768..32767 */
extern const int16_t PSX_ULAW_DEC[256];

#endif
