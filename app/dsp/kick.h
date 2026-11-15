/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH's kick (PRD K-*), AFFINE's BOOM: a synthesised kick of four layers, light and clean, for jungle's 808s.
 *   SUB    the low foundation: a sine dropping to its pitch, a long tail
 *   PUNCH  the hit and the body: a sine swept fast from far above (909 style)
 *   GRIT   the dirty mids: FM through a drive, noise mixed in by its tone
 *   CLICK  the transient: a short burst of noise and a click
 * Each layer has a level, pitch, decay and tone; the layers sum into INTENSITY (a drive into a clipper). AFFINE (15 Nov
 * 2026): LOAF BREAKER's hardcore extras gone (TERROR, the layers' effects, the KICK LAB, STACK, TOK, TAIL GATE, RUMBLE,
 * REVERSE BASS, GLIDE, CLEAN SUB); the SUB's pitch drop and its time are the kick's own. Platform-free like the break
 * engine. Settings from the main loop (kick_set, read once a block); triggers at sample offsets in the render. */
#pragma once
#include <stdint.h>

enum { KL_SUB, KL_PUNCH, KL_GRIT, KL_CLICK, KICK_LAYERS };
extern const char *const KICK_LAYER_NAME[KICK_LAYERS];

typedef struct {
    float level;                         /* 0..1 */
    float pitch;                         /* 0..1: SUB 30..90 Hz, PUNCH 50..200 Hz, GRIT 40..400 Hz, CLICK 1..12 kHz */
    float decay;                         /* 0..1: to -40 dB in SUB 60..1500 ms, PUNCH 20..400, GRIT 20..800, CLICK
                                          * 1..40 */
    float tone;                          /* 0..1: SUB its saturation (0 a pure sine), PUNCH its sweep, GRIT FM depth and
                                          * noise, CLICK noise or click */
} kick_layer_t;                          /* 16 bytes */

typedef struct {
    kick_layer_t l[KICK_LAYERS];
    float intensity;                     /* 0..1: the sum's drive into the clipper (0 clean) */
    float volume;                        /* 0..1 */
    int8_t drop;                         /* the SUB's pitch drop, -50 .. +50: where it starts above its note, x1.5 .. x9
                                          * (0: x3, an octave and a half) */
    int8_t dtime;                        /* how long the drop takes (to -40 dB), -50 .. +50: 30 .. 480 ms (0: 120) */
    uint8_t rsv[2];
} kick_t;                                /* 76 bytes (its steps are the pattern's: the app's) */

/* the built-in kicks: the table's indices never move (append new ones). 0..11 LOAF BREAKER's hardcore kicks (kept in
 * their places, no longer offered); 12..15 BOOM's: 808, JUNGLE, DUB, TIGHT */
#define KICK_PRESETS 16u
#define KICK_PRESET_808 12u              /* (AFFINE: a long round sub, a gentle pitch drop, ~900 ms) */
#define KICK_PRESET_JUNGLE 13u           /* (punchier, ~400 ms) */
#define KICK_PRESET_DUB 14u              /* (deep and soft, ~1.2 s) */
#define KICK_PRESET_TIGHT 15u            /* (short, ~150 ms, a little click) */
extern const char *const KICK_PRESET_NAME[KICK_PRESETS];
void kick_preset(kick_t *k, uint32_t i);   /* the sound of a built-in kick (the volume kept) */
void kick_defaults(kick_t *k);           /* 808, volume 0.8 */
float kick_pitch_hz(uint32_t layer, float pitch);   /* a layer's pitch knob (0..1) in Hz, for the screen */
float kick_decay_ms(uint32_t layer, float decay);   /* its decay knob in ms (to -40 dB) */

void kick_init(void);
void kick_set(const kick_t *k);          /* main loop: read at the next render */
void kick_hit(void);                     /* main loop (a MIDI note): a kick at the next render */
/* render: n samples into out (overwritten); hits at the sample offsets at[0..nat-1] (rising) */
void kick_render(float *out, uint32_t n, const uint16_t *at, uint32_t nat);
/* LOAF BREAKER: the next render's speed, a sample each (0..1: the VINYL BREAK slowing the kick like a record), or 0
 * (as it is). Set from the render, before kick_render */
void kick_speed(const float *sp);
/* LOAF BREAKER (crossbreed): the next render's hits' pitches, a factor each (pm[k] for at[k]; 0: as set). Set from the
 * render, before kick_render */
void kick_pitches(const float *pm);
/* the kick's shape, for the screen: n peaks (0..1) of the first ms milliseconds, rendered offline (main loop) */
void kick_shape(const kick_t *k, uint8_t *px, uint32_t n, uint32_t ms, uint32_t height);
/* LOAF BREAKER: a hit of k drawn as its wave (the BOOM page, the editor): n columns over ms, each the column's sample
 * furthest from 0 (signed, -height .. +height) */
void kick_wave(const kick_t *k, int8_t *px, uint32_t n, uint32_t ms, uint32_t height);
