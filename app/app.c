/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH on fm1-base: the app's unity build (compiled into fm1-base's device/unity.c after the platform, so gfx.c's
 * cv_* drawing and the plat_* calls are in scope) and the glue between the contract (app.h) and the engine
 * (dsp/brk.c, dsp/brk_fx.c: their own -O2 units).
 *
 * M1 scope (PRD 13): the built-in break, 16 slices on the white keys, the 11 pads on the black keys, play / reset,
 * tempo and tap tempo, pitch on OCT-/OCT+, the generator, the four FX and the DJ filter, settings kept by SAVE.
 * Breaks: the bank built into the image (app/bank_data.h, which ./build.sh generates from breaks/breaks.txt), chosen
 * on ALGORITHM; a build with no bank falls back to one synthesised break. */
#include <stddef.h>
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#include <stdlib.h>                  /* (getenv: LOAF_ASK, the first start's question) */
#endif
#include "adpcm.h"
#include "brk_bank.h"
#include "brk_math.h"
#include "brk_synth.h"
#include "kick.h"
#include "ovl.h"
#include "psx.h"                     /* AFFINE: the PSX bus */
#include "perc.h"
#if defined(__has_include)
#if __has_include("bank_data.h")
#include "bank_data.h"
#define CH_HAVE_BANK 1
#endif
#endif
#ifndef CH_HAVE_BANK
#define CH_HAVE_BANK 0
#endif
#if CH_LOGO
#include "splash_data.h"            /* (LOAF BREAKER's splash and title: gone in AFFINE, CH_LOGO 0) */
#include "title_data.h"
#endif
#include "face_data.h"              /* AFFINE: the VINYL pad's record label (tools/make_label.py; LOAF BREAKER's face before) */

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x31464641u            /* "AFF1" (AFFINE, 15 Nov 2026: the hardcore kick's settings gone, the rest
                                          * compacted; LOAF BREAKER's "CHP7" and any other magic load the defaults) */
/* From AFF1 on the settings only grow at the end: a shorter save (an older firmware's) loads into the front and the
 * fields after it keep their defaults (SET_SIZE_MIN: AFF1's size; an append keeps it). A change that isn't an append
 * needs a new magic (and loses the saves). Append new fields at the end (after pxc, the PSX colour: 16 Nov 2026) */
typedef struct { perc_voice_t v[2]; float volume; } perc2_t;   /* LOAF BREAKER: the snare's and the hat's, as saves
                                          * have them (perc_t has every drum now) */
typedef struct {
    uint32_t magic;
    uint16_t tempo_x10;                  /* 400..3000 */
    int8_t pitch;                        /* semitones */
    uint8_t swing;                       /* 50..75 % */
    brk_gen_t gen;
    brk_fx_t fx;
    uint8_t brk;                         /* the break: 0 the synthesised one, 1.. the bank's slots */
    uint8_t clk_in;                      /* MIDI clock in: 0 off, 1 follow (the tempo, start / stop) */
    uint8_t clk_out;                     /* MIDI clock out: 0 off, 1 on (24 a beat while playing, start / stop) */
    uint8_t ble_off;                     /* Bluetooth MIDI: 0 on (the default), 1 off */
    uint8_t midi_to;                     /* where MIDI goes: 0 USB and Bluetooth, 1 USB, 2 Bluetooth */
    uint8_t pitch_keep;                  /* pitch: 1 KEEP LENGTH (granular; the default), 2 SPEED (repitch) */
    uint8_t grain_ms;                    /* its grains, 10..80 ms */
    uint8_t gate;                        /* how much of each slice sounds, 5..100 % (100: off) */
    uint8_t snap;                        /* where a key lands while playing: SNAP_SL index + 1 */
    uint8_t brk_level;                   /* the MIXER's BREAK fader, 0..100 % */
    uint8_t drums_chain;                 /* the drums (BOOM, snare, hat, clap): 1 through the FX chain, 0 around it */
    uint8_t pattern;                     /* the pattern playing, 0..NPAT-1 (patterns: their own store object) */
    uint8_t bta_on;                      /* Bluetooth audio out (GLOBAL, third part): on at power-on, the last device
                                          * connected again */
    uint8_t bta_codec;                   /* its SBC coding: 0 joint stereo (fm1-base's), 1 plain stereo */
    brk_padset_t pads;                   /* the pads' own settings (K1-K4 while a pad is held) */
    brk_keyset_t keys[BRK_KEYS];         /* each white key's loop: length, pitch, start, mode (K1-K4 while held) */
    brk_slicefx_t slx[BRK_KEYS];         /* each slice's own effect and level (a key held, FX: its other layer) */
    kick_t kick;                         /* AFFINE: BOOM, the light kick (PRD K-*): its sound and level */
    perc2_t perc;                        /* the snare and the hat (PRD K-10): their sounds and level (LOAF BREAKER:
                                          * the others in pmore) */
    uint8_t rsv2[8];                     /* (LOAF BREAKER 0.9.45: the ARP keys, pads.c ARPK_TAG) */
    uint8_t ovl_amt[16];                 /* each white key's effect's amount, 0..100 (OV_*) */
    uint8_t ovl_rate;                    /* CHOP's rate (GATE, WAH, PAN follow it): 0 1/4 .. 3 1/32 */
    uint8_t ovl_sweep;                   /* DIVE's and RISE's sweep: 0 1/4 bar .. 3 2 bars */
    uint8_t ovl_latch;                   /* 1: a white key switches its effect on and off (0: on while held) */
    uint8_t ovl_rsv;
    brk_mbc_t mbc[BRK_FX_SLOTS];         /* MB COMP's detail per FX slot (PRD A-49a; it moves with its slot) */
    brk_fxx_t fxx[BRK_FX_SLOTS];         /* every slot's extras, its MORE and MOD pages (PRD A-49b) */
    uint8_t skin;                        /* (GLOUPI BREAKS' skin) */
    int8_t kick_dj;                      /* GLOUPI BREAKS: HOME's DRUM FLT (SEQ's K2 too): -100 low-pass .. 100
                                          * high-pass, every drum */
    uint8_t half_mt_off;                 /* GLOUPI BREAKS: HALF SPEED's K2, bit 0 MASTER TEMPO off; LOAF BREAKER: bit 1
                                          * its K3 LATCH (a press on, a press off); bit 2 KIL's LATCH (KIL held, K1) */
    uint8_t ovl_pad;                     /* GLOUPI BREAKS: the ARP black key, bits 0-3 its OVERLAY effect (OV_*),
                                          * bit 5 OFF (K3: the effects browsed silent), bits 6-7 THRU (K2): 0 BOTH,
                                          * 1 BREAK (BOOM around the chain and the overlays), 2 BOOM (the overlays on
                                          * BOOM alone) */
    uint8_t auto_save;                   /* K2 AUTO SAVE: 1, a minute after a change, saved once stopped */
    uint8_t vinyl;                       /* the VINYL BREAK key's THRU (K3): 0 BOTH, 1 BREAK, 2 BOOM (and the drums) */
    uint8_t kpre;                        /* AFFINE: BOOM's PRESET, 0..BOOM_PRESETS-1 (808, JUNGLE, DUB, TIGHT: ui/kick.c) */
    uint8_t duck;                        /* LOAF BREAKER (crossbreed): DUCK, the break down under each kick, 0 off .. 100
                                          * (MIX › MASTER) */
    perc_voice_t pmore[1];               /* the CLAP's sound (PV_CLAP) */
#define PERC_MEMS 10u
    uint8_t pslot[3];                    /* the memory playing, PV_SNARE..PV_CLAP 0..9 (SNARE 1..10...): perc_voice() is
                                          * it */
    uint8_t drop_brk;                    /* LOAF BREAKER (crossbreed): the break a drop brings in, 1.. (0: the same one;
                                          * DROP's K3) */
    perc_voice_t pmem[3][PERC_MEMS];     /* each as left (PRESETS on EDIT) */
    uint8_t comp;                        /* COMP, the whole mix compressed, 0 off .. 100 (its threshold 0 .. -36 dB) */
    union {                              /* (LOAF BREAKER, 11 Nov 2026: the STAB's sound, unused since 0.209, holds the
                                          * black keys' layout: bk[0] BK_TAG, bk[1] the preset BKP_*, bk[2..12] the
                                          * CUSTOM layout, a function BKF_* each key: AFFINE, 16 Nov 2026, unused, the
                                          * CUSTOM row gone) */
        perc_voice_t pstab_was;
        uint8_t bk[24];
    } bkeys;
    uint8_t sslot;                       /* (the STAB's memory playing: PV_STAB's place, perc_slot) */
    perc_voice_t smem[PERC_MEMS];        /* (LOAF BREAKER: the drums' extras, perc_x()) */
    uint8_t drop_bars;                   /* the build: DROP_BARS's index (0: none, the drop at the bar's end) */
    uint8_t drop_after;                  /* the pattern the drop brings in: 0 the next one, 1.. that one */
    uint8_t drop_crash;                  /* the drop's crash, 0 off .. 100 */
    uint8_t trk;                         /* the track TRACKS edits and PLAY plays, 0..NTRACKS-1 */
    uint8_t duck_att, duck_rel;          /* DUCK: its attack 0.1 .. 30 ms, its release 20 .. 800 ms (0..100) */
    uint8_t duck_what;                   /* 0 the break, 1 the break and the drums (not BOOM) */
    uint8_t comp_ratio;                  /* COMP: COMP_RATIO's index */
    uint8_t comp_att, comp_rel;          /* its attack 0.1 .. 30 ms, its release 20 .. 800 ms (0..100) */
    uint8_t max_out;                     /* LOAF BREAKER: MIX › MASTER's MAX OUT, the output's most at MASTER full: 0
                                          * the default (-3 dB), else 1 + its dB under full scale (0..12) */
    uint8_t drop_style;                  /* the build: DS_* (0 the snare roll) */
    uint8_t drop_sweep;                  /* its filter: 0 HIGH-PASS, 1 LOW-PASS, 2 none, 3 either, by chance */
    uint8_t drop_riser;                  /* its rising noise, 0 none .. 100 (50: as it always was) */
    uint8_t drop_gap;                    /* silence before the drop: DROP_GAP's index (0 none) */
    uint8_t drop_accel;                  /* BUILD's tempo rise to the drop: 0 .. 30 % */
    uint8_t scrfx;                       /* GLO EFFECTS: the screen effects' AMOUNT, 0..100 */
    uint8_t wzoom;                       /* HOME › SETTINGS' WAVE ZOOM: CDJ_Z's index, 3 one bar */
    uint8_t sfx_kick, sfx_drop, sfx_len; /* GLO FX SET K2-K4, the screen effects on a BOOM, at the drop, how long; 0..100,
                                          * 50 as it was */
    uint8_t sfx_on;                      /* GLO EFFECTS, the screen effects on, several at once: bit e - 1 for SFX_* e */
    uint8_t novice;                      /* (LOAF BREAKER's MODE, 1 BEGINNER. AFFINE, 16 Nov 2026, the owner: one mode
                                          * for everyone, the full AFFINE; 1 only while the TUTORIAL runs, its steps
                                          * written for BEGINNER: NOVICE below. A save's 1 alone loads as the full AFFINE) */
    uint8_t pgrp;                        /* LOAF BREAKER: the patterns' group in RAM, 0..3 (P1-16 .. P49-64); pattern
                                          * its slot there */
    uint8_t brkv;                        /* LOAF BREAKER (0.234): EXPERT's SEQ grid, the break's rows shown, BRKV_TAG |
                                          * 0 HIDDEN, 1 the first 16 steps, 2 all 32: brk_view() */
    uint8_t wstyle;                      /* LOAF BREAKER: the waves' colours (HOME's CDJ view, the drums' RGB view): 0
                                          * REKORDBOX, 1 SERATO; HOME › SETTINGS K2 */
    uint8_t tut;                         /* LOAF BREAKER: the TUTORIAL (tutor.c): 0 off, 1.. its step running, 0xFE
                                          * finished, 0xFD the mode asked */
    uint8_t pxv;                         /* LOAF BREAKER: PX_OK: smem holds the drums' extras, perc_x() */
    psx_bus_t psx;                       /* AFFINE: the PSX bus (psx.h): HOME's gestures (crush, redux, send), SPU
                                          * VERB, CODEC, CRUSH and REDUX's ways, SKIP; vthrow live, never saved set */
    uint8_t grp_ours;                    /* AFFINE: bit g: pattern group g's objects were saved by AFFINE (LOAF BREAKER
                                          * and CHOPPAH write the same addresses, the same sizes: a size can't tell) */
    uint8_t rsv_aff[3];
    /* (AFF1's first size ends here) */
    psx_color_t pxc;                     /* AFFINE, 16 Nov 2026: the PSX colour (psx.h): COLOR's RATE, CRUNCH (and PCM),
                                          * SMOOTH, AMOUNT; OUT's CONSOLE, MONO, SEND, INPUT; the last preset. 28 B; a
                                          * save from before has none: the defaults, the sound as it was (GRIT at
                                          * CODEC's RATE, AMOUNT 1, CONSOLE 0) */
    /* (append new fields here, and move SET_SIZE_MIN on only with a new magic) */
} settings_t;
#define PX_OK 0xA5u
#define BRKV_TAG 0xB0u
static uint32_t brk_view(void);          /* (below: set is declared after) */
/* (LOAF BREAKER, 0.244: FILL gone, set.fill_cfg unused) */
enum { DS_SNARE, DS_KICK, DS_BOTH, DS_STUTTER, DS_SWEEP, DS_RANDOM, DS_N };
static const uint8_t DROP_GAP[5] = {0u, 1u, 2u, 4u, 8u};   /* its steps: none, a 16th, an 8th, a beat, half a bar */
static const float COMP_RATIO[8] = {1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 20.0f};
static float att_ms(uint32_t v) { return 0.1f * brk_exp2f_c((float)(v > 100u ? 100u : v) * 0.0825f); }
static float rel_ms(uint32_t v) { return 20.0f * brk_exp2f_c((float)(v > 100u ? 100u : v) * 0.0532f); }
#define SET_SAVED sizeof(settings_t)    /* what SAVE writes and a load reads at most */
#define SET_SIZE_MIN (offsetof(settings_t, pxv) + 1u)   /* the shortest save that loads: AFF1's first size */
#define SET_SIZE_PXC offsetof(settings_t, pxc)   /* a save from before the PSX colour (16 Nov 2026) */
_Static_assert(SET_SIZE_PXC % 4u == 0u && sizeof(psx_color_t) == 28u, "the PSX colour appended, no gap before it");
_Static_assert(sizeof(settings_t) <= 3840u, "a store object holds 3840 bytes");
static settings_t set;
static int tut_on(void);                 /* (tutor.c: the TUTORIAL running) */
/* AFFINE (16 Nov 2026, the owner: "one mode for everyone"): LOAF BREAKER's BEGINNER only while the TUTORIAL runs (its
 * steps were written for it: tut_start sets set.novice, its end and quit clear it); never else, whatever a save holds */
#define NOVICE (set.novice && tut_on())
/* LOAF BREAKER (11 Nov 2026, FLEXOR): the black keys' functions, BKF_*: the eleven pads (brk.h PAD_*: FREEZE and DUB
 * back), then ARP (an OVERLAY effect), KIL (the sequencer's kicks quiet). A layout: the function of each key, left to
 * right. Presets: CHOPPAH (CHOPPAH's keys), CUSTOM (each key its own: ARP › BLACK KEYS). (AFFINE: DST, SUB, DRV and
 * the FLEXOR LAYOUT gone with the hardcore kick; 16 Nov 2026, the owner: two rows, AFFINE and PUNCH, CUSTOM gone: a save
 * holding it plays AFFINE's, and GATE, RANDOM, DUB, ARP, KIL alone only from what REC recorded) */
enum { BKF_ARP = BRK_NPADS, BKF_KIL, BKF_VRB, BKF_SKP, BKF_CRS, BKF_N };   /* (AFFINE: VRB the mix thrown into SPU
                                          * VERB, SKP the disc slipping, CRS the bits dropped, each while held) */
enum { BKP_AFFINE, BKP_CHOPPAH, BKP_CUSTOM, BKP_PUNCH, BKP_N };   /* (AFFINE's first: a new FM-1's. The values are saved
                                          * (bk[1]): AFFINE 16 Nov 2026 adds PUNCH at the end; CHOPPAH leaves the rows
                                          * (FX + OCT) but a save holding it still plays its layout; CUSTOM: AFFINE's) */
/* AFFINE (16 Nov 2026, the owner): the PUNCH row, eleven fixed combos, each several of the functions above held
 * together with its own settings while held (an EP-133 punch-in's way: a signature, not editable). Codes 32..42:
 * bk_fn gives them on the PUNCH row (and REC records them). BKC_T below: each one's functions and
 * settings; never saved, applied to copies (pads_push, fx_send) while one is held or plays from the pattern */
#define BKC_0 32u
#define BKC_N 11u
#define BKC(c) ((uint32_t)(c) - BKC_0 < BKC_N)
enum { CB_STUTTER, CB_BUZZ, CB_REVVERB, CB_SLOWMO, CB_TAPESTOP, CB_GLITCH, CB_WASH, CB_CHOP, CB_DUBECHO, CB_SHATTER,
       CB_DROPOUT };
#define BK_TAG 0xB4u
#define BK_VIEW 14u                      /* (bk[14]: HOME's VIEW, 0 the waveform, 1 the slices, 0.9.2) */
#define BK_BHALF 13u                     /* (bk[13]: BREAK TIME, 0 NORMAL, 1 HALF: the break at half the tempo, 0.256) */
static const uint8_t BK_LAY[11] = {PAD_ROLL8, PAD_ROLL16, PAD_ROLL32, PAD_REVERSE, PAD_FREEZE, PAD_HALF,
                                   PAD_TAPESTOP, PAD_GATE, PAD_RANDOM, PAD_FILTER, PAD_DUB};   /* (CHOPPAH's) */
static const uint8_t BK_LAY_AFF[11] = {PAD_ROLL8, PAD_ROLL16, PAD_ROLL32, PAD_REVERSE, PAD_HALF, PAD_TAPESTOP,
                                       PAD_FREEZE, BKF_SKP, BKF_VRB, BKF_CRS, PAD_FILTER};   /* (AFFINE's: the plan's) */
#define BK_PLAT 15u                      /* (bk[15]: 0xA0 | the LATCH bits of REVERSE, FREEZE, GATE, RANDOM (their K4),
                                          * 0.9.26, FLEXOR; tagged: an older save's byte there the STAB's leftover) */
#define BK_FTHRU 16u                     /* (bk[16]: 0xC0 | the FILTER key's THRU, its K4: 0 BREAK, 1 DRUMS, 2 BOTH) */
#define BK_SEQ 17u                       /* (bk[17]: 0xD0 | SEQ's SETTINGS, 0.9.31, FLEXOR: bit 0 FOLLOW off, bit 1 the
                                          * break's rows 16 (else 32) in the FULL view) */
static uint32_t bk_seq(void) { return (set.bkeys.bk[BK_SEQ] & 0xF0u) == 0xD0u ? set.bkeys.bk[BK_SEQ] & 15u : 0u; }
static uint32_t bk_lat(void) { return (set.bkeys.bk[BK_PLAT] & 0xF0u) == 0xA0u ? set.bkeys.bk[BK_PLAT] & 15u : 0u; }
static uint32_t bk_fthru(void)
{
    uint32_t v = set.bkeys.bk[BK_FTHRU];
    return (v & 0xF0u) == 0xC0u && (v & 15u) <= 2u ? v & 15u : 0u;
}
static int lat_bit(uint32_t pad)         /* (a pad's place in bk_lat, -1: none) */
{
    return pad == PAD_REVERSE ? 0 : pad == PAD_FREEZE ? 1 : pad == PAD_GATE ? 2 : pad == PAD_RANDOM ? 3 : -1;
}
static uint32_t bk_preset(void)          /* the row in use (a new FM-1 and RESET ALL: AFFINE; AFFINE, 16 Nov 2026: a save
                                          * holding CUSTOM, gone, AFFINE) */
{
    if (set.bkeys.bk[0] != BK_TAG || set.bkeys.bk[1] >= BKP_N || set.bkeys.bk[1] == BKP_CUSTOM)
        return BKP_AFFINE;
    return set.bkeys.bk[1];
}
static uint32_t bk_fn(uint32_t j)        /* black key j's (0..10) function (AFFINE: or its PUNCH combo, BKC_0..) */
{
    uint32_t p = bk_preset();
    j %= 11u;
    if (p == BKP_PUNCH)
        return BKC_0 + j;
    return p == BKP_CHOPPAH ? BK_LAY[j] : BK_LAY_AFF[j];
}
static void bk_defaults(void)
{
    uint32_t j;
    set.bkeys.bk[0] = BK_TAG;
    set.bkeys.bk[1] = BKP_AFFINE;        /* (a new FM-1, RESET ALL, and the simulator: AFFINE's; the simulator's own
                                          * CUSTOM layout gone with CUSTOM, 16 Nov 2026) */
    for (j = 0; j < 11u; j++)
        set.bkeys.bk[2u + j] = BK_LAY_AFF[j];   /* (CUSTOM's old place: unused) */
    set.bkeys.bk[BK_BHALF] = 0u;
    set.bkeys.bk[BK_PLAT] = 0xA0u;
    set.bkeys.bk[BK_FTHRU] = 0xC0u;
}
static uint32_t brk_view(void) { return (set.brkv & 0xF0u) == BRKV_TAG && (set.brkv & 15u) <= 2u ? set.brkv & 15u : 2u; }
static uint32_t max_out_db(void) { return set.max_out ? (set.max_out - 1u > 12u ? 12u : set.max_out - 1u) : 3u; }
static perc_voice_t *perc_voice(uint32_t v)   /* LOAF BREAKER: a drum's sound, PV_* (the snare and the hat in perc,
                                          * the rest in pmore) */
{
    v %= PERC_VOICES;
    static perc_voice_t none;            /* (PV_STAB: an empty place, silent; its bytes the black keys') */
    return v < 2u ? &set.perc.v[v] : v == PV_CLAP ? &set.pmore[0] : &none;
}
/* LOAF BREAKER (11 Nov 2026): a drum's extras (PX_*: its drop, its saturation, LIFT, ROOM, BEGINNER's FX, its preset),
 * the memory playing's, in the MELODY's old place: smem, 240 bytes, three drums of ten memories of PX_N */
_Static_assert(sizeof(((settings_t *)0)->smem) == 3u * PERC_MEMS * PX_N, "smem holds the drums' extras");
static uint8_t *perc_xm(uint32_t v, uint32_t i) { return (uint8_t *)(void *)set.smem + ((v % 3u) * PERC_MEMS + i % PERC_MEMS) * PX_N; }
static uint8_t *perc_x(uint32_t v) { return perc_xm(v, set.pslot[v % 3u]); }
static uint8_t *perc_slot(uint32_t v)    /* its memory playing */
{
    v %= PERC_VOICES;
    return v == PV_STAB ? &set.sslot : &set.pslot[v];
}
static perc_voice_t *perc_mem(uint32_t v, uint32_t i)   /* its memory i */
{
    v %= PERC_VOICES;
    i %= PERC_MEMS;
    return v == PV_STAB ? &set.smem[i] : &set.pmem[v][i];
}
static uint8_t set_dirty;
static uint8_t foot_dirty;               /* a setting changed: the slicer's foot shows the new value */

/* AFFINE: BOOM to the engine: the kick playing, as it is (LOAF BREAKER's KICK LAB, KICK 1-20, MORPH, CLEAN SUB, the
 * DST / SUB / DRV switches: gone) */
static void kick_send(void) { kick_set(&set.kick); }
static void settings_defaults(void)
{
    uint32_t k;
    uint8_t *z = (uint8_t *)&set;
    for (k = 0; k < sizeof set; k++)     /* (every field: no leftovers from a save that didn't load) */
        z[k] = 0u;
    set.magic = SET_MAGIC;
    psx_bus_defaults(&set.psx);          /* (AFFINE: the PSX bus at rest: the gestures at 0, SPACE ECHO waiting) */
    psx_color_defaults(&set.pxc);        /* (the PSX colour as it was before it: AMOUNT 1, CONSOLE 0, GRIT's rate) */
    set.scrfx = 50u;                     /* (the screen effect's AMOUNT: half) */
    set.sfx_on = 0u;                     /* (the screen effects: none at the start and after RESET ALL; the site's
                                          * screenshots clean) */
    set.wzoom = 3u;                      /* (the scrolling wave: a bar across, as it was) */
    bk_defaults();                       /* (the black keys: AFFINE's row) */
    set.skin = 0x80u;                    /* (AFFINE: POLYGON, common.c's SKIN_NEW | SKIN_DEFAULT) */
    set.novice = 0u;                     /* (AFFINE: one mode, the full AFFINE; BEGINNER only inside the TUTORIAL) */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    if (getenv("LOAF_ASK"))              /* (LOAF BREAKER, 0.229: the first start's question, for its scenario) */
        set.tut = 0xFDu;
#else
    set.tut = 0xFDu;                     /* (LOAF BREAKER, 0.229: the first start asks, tutor.c TUT_ASK; AFFINE: TUTORIAL
                                          * or PLAY) */
#endif
    set.sfx_kick = set.sfx_drop = set.sfx_len = 50u;
    set.tempo_x10 = 1700u;               /* jungle tempo */
    set.swing = 50u;
    set.gen.anchor = 0.7f;
    set.gen.roll = 0.3f;
    set.gen.fill = 0.5f;
    set.gen.phrase_bars = 4u;
    brk_fx_defaults(&set.fx);
    set.pitch_keep = 1u;
    set.grain_ms = 30u;
    set.gate = 100u;
    set.snap = 5u;                       /* 1 slice */
    set.brk_level = 100u;
    set.drums_chain = 0u;                /* (0.9.2, FLEXOR: AROUND the FX chain by default, any mode) */
    brk_padset_defaults(&set.pads);
    for (k = 0; k < BRK_KEYS; k++) {
        brk_keyset_defaults(&set.keys[k]);
        brk_slicefx_defaults(&set.slx[k]);
    }
    kick_defaults(&set.kick);            /* (AFFINE: BOOM's 808, set.kpre 0) */
    {                                    /* (the snare and the hat where saves have them, the rest in pmore) */
        perc_t d;
        perc_defaults(&d);
        set.perc.v[0] = d.v[0];
        set.perc.v[1] = d.v[1];
        set.perc.volume = d.volume;
        set.pmore[0] = d.v[PV_CLAP];
        for (k = 0; k < PERC_VOICES * PERC_MEMS; k++)   /* (the memories begin as the presets, again and again) */
            perc_preset(perc_mem(k / PERC_MEMS, k % PERC_MEMS), k / PERC_MEMS, k % PERC_MEMS % PERC_PRESETS);
    }
    memset(set.smem, 0, sizeof set.smem);   /* (LOAF BREAKER: the drums' extras: none, as they always sounded) */
    set.pxv = PX_OK;
    set.brkv = BRKV_TAG | 2u;            /* (LOAF BREAKER: the break's 32 steps on SEQ's grid) */
    for (k = 0; k < 16u; k++)            /* the overlays: full, the sweeps and the dirt a little less */
        set.ovl_amt[k] = k == OV_DIVE || k == OV_RISE || k == OV_CRUSH || k == OV_DIST || k == OV_BOOM ? 70u : 100u;
    set.ovl_rate = 2u;                   /* 1/16 */
    set.ovl_sweep = 2u;                  /* a bar */
    for (k = 0; k < BRK_FX_SLOTS; k++) {
        brk_mbc_defaults(&set.mbc[k]);
        brk_fxx_defaults(&set.fxx[k], set.fx.slot[k].type);
    }
    set.duck_att = 19u;                  /* (LOAF BREAKER: DUCK ~0.3 ms in, ~75 ms back; COMP 4:1, ~3 ms, ~150 ms) */
    set.duck_rel = 36u;
    set.comp_ratio = 3u;
    set.comp_att = 59u;
    set.comp_rel = 55u;
    set.drop_bars = 3u;                  /* (LOAF BREAKER: DROP: a 4-bar build, into the next pattern, the crash full) */
    set.drop_crash = 100u;
    set.drop_riser = 50u;                /* (the KICK FX tail: zeros, empty slots, FXT_EMPTY) */
}

static int pitch_keeps(void) { return set.pitch_keep != 2u; }   /* KEEP LENGTH unless SPEED was chosen */

/* the snap: where a white key pressed while playing lands, in slices (GLOBAL, second half, K3) */
#define SNAPS 7u
static const float SNAP_SL[SNAPS] = {0.0f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
static const char *const SNAP_NAME[SNAPS] = {"OFF", "1/8 SLICE", "1/4 SLICE", "1/2 SLICE", "1 SLICE", "2 SLICES",
                                             "4 SLICES"};
static const char *const SNAP_SHORT[SNAPS] = {"OFF", "1/8 SL", "1/4 SL", "1/2 SL", "1 SL", "2 SL", "4 SL"};
static uint32_t snap_index(void) { return set.snap && set.snap <= SNAPS ? set.snap - 1u : 4u; }

static void skin_apply(uint32_t s);    /* (ui/common.c) */
static uint32_t skin_old(uint32_t s); /* (ui/common.c: an older save's skin, 0..17, to the 1.0's) */
/* LOAF BREAKER: the FX chain's part of settings_apply (its four slots, their extras), and BOOM's (its sound, the ARP's
 * REESE in its key): a knob of theirs applies them alone (changed_fx, changed_kick) */
static uint8_t verb_held, skip_held, crush_held;   /* AFFINE: the VRB, SKP, CRS keys down (main.c): the mix into SPU
                                          * VERB at full; the disc slipping at 90 %; 4 bits */
static void psx_apply(void)
{
    psx_bus_t b = set.psx;
    b.vthrow = verb_held ? 1.0f : 0.0f;
    if (skip_held) {
        b.skchance = b.skchance > 0.9f ? b.skchance : 0.9f;
        b.skmix = 1.0f;
    }
    if (crush_held && b.crush < 0.86f)
        b.crush = 0.86f;
    psx_bus_set(&b);
    psx_bus_color(&set.pxc);             /* (the PSX colour: COLOR, OUT, AMOUNT) */
}
/* AFFINE: the PUNCH row's combos (BKC_0 + CB_*): each one's functions (bit BKF_*), its name (10 at most) and three
 * letters (the foot's EFFECT), and its settings while it sounds: only those of the functions it has (a key of the
 * same function held beside it plays them too). Rates: brk.h BRK_ROLL_DIVS (1 1/8, 2 1/16, 3 1/32), BRK_GATE_DIVS
 * (1 1/16), brk_fx.h BRK_DUB_DIVS (1 3/16). The filter: -100 low-pass .. 100 high-pass, its resonance and speed x100
 * (speed 0: ~0.6 s to get there, 50: ~40 ms). Tuned on the simulator's renders (build/review/sounds/affine-punch-tour) */
#define BF(f) (1u << (f))
typedef struct {
    const char *name, *tag;
    uint16_t fm;                         /* its functions */
    uint8_t roll, roll2;                 /* its roll's rate; after a beat held (STUTTER), 0 none */
    int8_t flt;                          /* the FILTER's target, x100 */
    uint8_t fres, fspd;                  /* its resonance, its speed, x100 */
    uint8_t rnd, rndp;                   /* RANDOM: the chance a step jumps x100, its pitch (semitones) */
    uint8_t gdiv, glen;                  /* GATE: its rate, its open part x100 */
    uint8_t frz;                         /* FREEZE: its loop (0 the slice, 1 a half ..) */
    uint8_t stop4;                       /* VINYL BREAK: beats down, x4 */
    uint8_t half, mt;                    /* HALF SPEED: its speed x100; 1: the pitch drops with it (master tempo off) */
    uint8_t ddiv, dfb, dtone, dmix;      /* DUB: its time, feedback, tone, mix x100 */
} bkc_t;
static const bkc_t BKC_T[BKC_N] = {   /* each: its functions; then roll, then | filter target, res, speed |
                                       * random %, st | gate rate, open | freeze | stop | half, mt | dub time,
                                       * feedback, tone, mix */
    {"STUTTER", "STU", BF(PAD_ROLL16) | BF(PAD_FILTER),
     2, 3,  66, 45, 2,  0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
    {"BUZZ", "BUZ", BF(PAD_ROLL32) | BF(BKF_CRS),
     3, 0,  0, 0, 0,  0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
    {"REV VERB", "RVB", BF(PAD_REVERSE) | BF(BKF_VRB),
     0, 0,  0, 0, 0,  0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
    {"SLOW MO", "SLO", BF(PAD_HALF) | BF(PAD_FILTER),
     0, 0,  -45, 30, 10,  0, 0,  0, 0,  0,  0,  50, 1,  0, 0, 0, 0},
    {"TAPE STOP", "TPS", BF(PAD_TAPESTOP) | BF(BKF_VRB),
     0, 0,  0, 0, 0,  0, 0,  0, 0,  0,  8,  0, 0,  0, 0, 0, 0},
    {"GLITCH", "GLT", BF(BKF_SKP) | BF(PAD_RANDOM),
     0, 0,  0, 0, 0,  50, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
    {"WASH", "WSH", BF(PAD_FREEZE) | BF(BKF_VRB) | BF(PAD_FILTER),
     0, 0,  -62, 20, 5,  0, 0,  0, 0,  1,  0,  0, 0,  0, 0, 0, 0},
    {"CHOP", "CHP", BF(PAD_GATE) | BF(BKF_CRS),
     0, 0,  0, 0, 0,  0, 0,  1, 50,  0,  0,  0, 0,  0, 0, 0, 0},
    {"DUB ECHO", "ECH", BF(PAD_ROLL8) | BF(PAD_DUB),
     1, 0,  0, 0, 0,  0, 0,  0, 0,  0,  0,  0, 0,  1, 60, 45, 60},
    {"SHATTER", "SHT", BF(PAD_RANDOM) | BF(BKF_CRS) | BF(BKF_SKP),
     0, 0,  0, 0, 0,  100, 7,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
    {"DROP OUT", "DRP", BF(BKF_KIL) | BF(PAD_FILTER),
     0, 0,  50, 25, 50,  0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0, 0},
};
#define BF_ROLLS (BF(PAD_ROLL8) | BF(PAD_ROLL16) | BF(PAD_ROLL32))
static uint32_t bk_fmask(uint32_t c)     /* a function's or a combo's functions, bit BKF_* (none: 0) */
{
    return c < BKF_N ? 1u << c : BKC(c) ? BKC_T[c - BKC_0].fm : 0u;
}
static uint8_t cmb_on;                   /* the combo whose settings the engine has now (BKC_0 + CB_*), 0 none */
static uint8_t cmb_fast;                 /* (STUTTER held a beat: its roll at its second rate) */
static void pads_push(void)              /* the pads' settings to the engine: the player's, a combo's over them */
{
    brk_padset_t p = set.pads;
    if (BKC(cmb_on)) {
        const bkc_t *c = &BKC_T[cmb_on - BKC_0];
        if (c->fm & BF_ROLLS) {          /* (the three rolls' rates: the combo has one of them) */
            p.roll_div[0] = p.roll_div[1] = p.roll_div[2] = cmb_fast && c->roll2 ? c->roll2 : c->roll;
            p.roll_level = 100u;
            p.roll_decay = 0.0f;
            p.roll_pitch = 0.0f;
        }
        if (c->fm & BF(PAD_REVERSE))
            p.rev_speed = 1.0f;
        if (c->fm & BF(PAD_FREEZE)) {
            p.frz_len = c->frz;
            p.frz_pitch = 0.0f;
        }
        if (c->fm & BF(PAD_HALF))
            p.half_speed = (float)c->half * 0.01f;
        if (c->fm & BF(PAD_TAPESTOP)) {
            p.stop_beats = (float)c->stop4 * 0.25f;
            p.spin_steps = 1.0f;
        }
        if (c->fm & BF(PAD_GATE)) {
            p.gate_div = c->gdiv;
            p.gate_len = (float)c->glen * 0.01f;
        }
        if (c->fm & BF(PAD_RANDOM)) {
            p.rnd_amount = (float)c->rnd * 0.01f;
            p.rnd_pitch = (float)c->rndp;
        }
    }
    brk_padset(&p);
    brk_half_master(BKC(cmb_on) && (BKC_T[cmb_on - BKC_0].fm & BF(PAD_HALF)) && BKC_T[cmb_on - BKC_0].mt ? 0
                    : !(set.half_mt_off & 1u));
}
static void fx_send(void)                /* the FX chain's settings to the engine (the TUTORIAL's two slots; a combo's
                                          * FILTER and DUB over the player's) */
{
    brk_fx_t f = set.fx;
    uint32_t k;
    if (NOVICE)                          /* (LOAF BREAKER, 0.229: BEGINNER's chain two slots, the others muted; AFFINE:
                                          * the TUTORIAL's) */
        for (k = 2u; k < BRK_FX_SLOTS; k++)
            f.slot[k].on = 0u;
    if (BKC(cmb_on)) {
        const bkc_t *c = &BKC_T[cmb_on - BKC_0];
        if (c->fm & BF(PAD_FILTER)) {
            f.flt_target = (float)c->flt * 0.01f;
            f.flt_res = (float)c->fres * 0.01f;
            f.flt_speed = (float)c->fspd * 0.01f;
        }
        if (c->fm & BF(PAD_DUB)) {
            f.dub_div = c->ddiv;
            f.dub_fb = (float)c->dfb * 0.01f;
            f.dub_tone = (float)c->dtone * 0.01f;
            f.dub_mix = (float)c->dmix * 0.01f;
        }
    }
    brk_fx_set(&f);
}
static void fx_apply(void)
{
    uint32_t k;
    fx_send();
    brk_fx_flt_thru(bk_fthru());         /* (the FILTER key's THRU, 0.9.26) */
    psx_apply();                         /* (AFFINE: the PSX bus, with the VERB key's throw as it is now) */
    for (k = 0; k < BRK_FX_SLOTS; k++) {
        brk_fx_set_mbc(k, &set.mbc[k]);
        brk_fx_set_fxx(k, &set.fxx[k]);
    }
    brk_fx_tempo((float)set.tempo_x10 * 0.1f);
}
static void kick_apply(void)
{
    ovl_reese_hz(kick_pitch_hz(KL_SUB, set.kick.l[KL_SUB].pitch));   /* (the ARP's REESE in BOOM's key) */
    kick_send();
}
static void perc_apply(void)             /* all the drums' voices for the engine (LOAF BREAKER: the MELODY's too) */
{
    perc_t pa;
    uint32_t k;
    for (k = 0; k < PERC_VOICES; k++)
        pa.v[k] = *perc_voice(k);
    pa.volume = set.perc.volume;
    perc_set(&pa);
    for (k = 0; k < 3u; k++)             /* (LOAF BREAKER: their extras) */
        perc_set_x(k, perc_x(k));
}
static void settings_apply(void)
{
    brk_tempo((float)set.tempo_x10 * 0.1f);
    brk_half_time(set.bkeys.bk[BK_BHALF] & 1u);   /* (LOAF BREAKER, 0.256: SYSTEM › MIDI K4 BREAK TIME) */
    brk_pitch((float)set.pitch);
    brk_swing((float)set.swing * 0.01f);
    brk_gen(&set.gen);
    fx_apply();
    set.pads.roll_pitch = 0.0f;          /* LOAF BREAKER: the rolls' pitch gone (an older save's: off) */
    pads_push();                         /* (AFFINE: a PUNCH combo's over them while it sounds; HALF's master tempo) */
    brk_pitch_mode(pitch_keeps(), (float)(set.grain_ms ? set.grain_ms : 30u));
    brk_vinyl((set.vinyl & 3u) != 2u);
    brk_gate((float)(set.gate ? set.gate : 100u) * 0.01f);
    brk_snap(SNAP_SL[snap_index()]);
    kick_apply();
    perc_apply();
    skin_apply(set.skin & 0x7Fu);
    {
        uint32_t k;
        for (k = 0; k < BRK_KEYS; k++) {
            brk_keyset(k, &set.keys[k]);
            brk_slicefx(k, &set.slx[k]);
        }
    }
}

static uint8_t apply_due, apply_kick, apply_fx;   /* LOAF BREAKER: a change waiting for settings_apply, or for the
                                          * kick's or the FX chain's part alone (apply_flush) */
static void changed(void)
{
    set_dirty = 1u;
    foot_dirty = 1u;
    apply_due = 1u;                      /* (TEST: applied once a pass, after the input: a knob turned fast, its many
                                          * detents one settings_apply, not one each) */
}
static void changed_kick(void)          /* (a change to the kick alone: its part applied) */
{
    set_dirty = 1u;
    foot_dirty = 1u;
    apply_kick = 1u;
}
static void changed_fx(void)            /* (a change to the FX chain alone: its part applied) */
{
    set_dirty = 1u;
    foot_dirty = 1u;
    apply_fx = 1u;
}
static void apply_flush(void)
{
    if (apply_due)
        settings_apply();
    else {
        if (apply_kick)
            kick_apply();
        if (apply_fx)
            fx_apply();
    }
    apply_due = apply_kick = apply_fx = 0u;
}

/* ------------------------------------------------------------------ patterns --- */
/* the pattern sequencer (PRD B-20..B-22): 16 patterns, each 32 steps of the break (2 bars: slice or follow, pitch,
 * ratchet, reverse; the engine plays them, brk_step_set) and 16-step lanes for the kick, the snare and the hat (each
 * bar). Saved with SAVE in their own store object. PRESETS picks one: while playing it takes over at the end of the
 * loop */
#define NPAT 16u
#define PAT_STEPS 32u
#define PAT_MAGIC 0x33544150u            /* "PAT3" (GLOUPI BREAKS: the kick over four bars) */
#define PAT2_MAGIC 0x32544150u           /* "PAT2": the kick two bars; loaded with them twice */
#define PAT1_MAGIC 0x31544150u           /* "PAT1": the kick one bar; loaded with it four times */
enum { LANE_KICK, LANE_SNARE, LANE_HAT, NLANES };
/* LOAF BREAKER: the drums the pattern plays (LANE_* the first three, pat_t's lane[]), DR_SNARE.. the PV_* voices + 1 */
enum { DR_KICK, DR_SNARE, DR_HAT, DR_CLAP, NDRUMS };   /* (LOAF BREAKER, 0.209: the MELODY's out: kicks and drums only) */
#define NDRUMS_SAVED 5u                  /* (pats.hits' layout in the saves: the MELODY's place kept) */
typedef struct {
    uint8_t st[PAT_STEPS][4];            /* per step: slice + 2 (0 off, 1 follow the loop, 2.. slice 1..), pitch
                                          * (int8), flags (bit 0 reverse, bits 1-2 ratchet - 1), spare (GLOUPI
                                          * BREAKS: steps 0-3's the kick's bars 3 and 4, kick_bar; 4-5's the
                                          * snare's bar 2, snare_bar; 6's the lanes' lengths, PAT_LEN) */
    uint16_t lane[NLANES];               /* bit n: the drum hits on step n of each bar (the kick: of its first) */
    uint16_t kick2;                      /* GLOUPI BREAKS: the kick's second bar (steps 17-32); was PAT1's rsv */
} pat_t;                                 /* 136 bytes */
/* LOAF BREAKER: the TRACKS (ENV): each a list of patterns, each played its loops (a loop: the pattern's length), and
 * DROPs between them (the build, then the drop into the pattern after it), to its END; then from its start again,
 * a stop, or the last pattern held. Saved with the patterns (after them: an older save, its patterns alone, loads) */
#define NTRACKS 8u
#define TRK_ITEMS 32u
#define TI_END 0u                        /* it[]: 0 the end, 1..NPAT that pattern, TI_DROP a drop */
#define TI_DROP 0xFEu
#define TI_GAP 0xFDu                     /* LOAF BREAKER: a gap (ENV held on TRACKS): to be filled, played past */
typedef struct {
    uint8_t it[TRK_ITEMS];
    uint8_t rep[TRK_ITEMS];              /* a pattern's loops, 1..16 */
    uint8_t end;                         /* at its end: 0 LOOP, 1 STOP, 2 HOLD */
    uint8_t rsv[3];
} track_t;
/* LOAF BREAKER: each effect's settings as last left, kept when a slot turns to another effect and back (FX's
 * PRESETS); saved after the tracks. 0..65535 for 0..1 */
#define FXM_N (4u + BRK_FXX)             /* an effect's P1-P3, MIX, then its extras (MORE, MOD) */
typedef struct {
    uint16_t fx[BRK_NFXT][FXM_N];
    uint16_t fx_ok;                      /* bit t: fx[t] holds them (none: the effect's starting settings) */
    uint16_t rsv;
} fxmem_t;
typedef struct {
    uint32_t magic;
    pat_t p[NPAT];
    track_t trk[NTRACKS];
    fxmem_t mem;
    uint8_t kn[NPAT][13];                /* LOAF BREAKER: the kick's notes past its 38th step (kick_note) */
    uint8_t hits[NPAT][NDRUMS_SAVED];    /* LOAF BREAKER: each drum's HITS in the pattern (SEQ's K2): 0, 1 once a step
                                          * (as ever), 2..4 that many times in its step */
} pats_t;
static pats_t pats PLAT_POOL;            /* (LOAF BREAKER: in the pool, 3.7 KB of RAM freed) */
#define PATS_SAVED sizeof(pats_t)        /* (AFFINE's, what SAVE writes) */
#define PATS_SIZE_OLD offsetof(pats_t, trk)   /* (an older save, its patterns alone) */
#define PATS_SIZE_TRK offsetof(pats_t, mem)   /* (one with the tracks, before the effects' memory; LOAF BREAKER's later
                                          * ones are laid out otherwise: not read) */
/* LOAF BREAKER: each drum step's own settings (SEQ: a step held, K2-K4), a byte each: bits 0-1 HITS - 1 (it taps 1..4
 * times in its step), bits 2-4 CHANCE (0: always .. 7: one time in eight), bits 5-7 NUDGE (0 on the step .. 7 that
 * many 16ths of it late). The kick's 64 steps, then the others' 32 each; their own store object (OBJ_STEP). An older
 * save: none, and the pattern's HITS a drum (pats.hits) on each of its steps */
#define STEPB_N 192u
#define STEPS_MAGIC 0x31505453u           /* "STP1" */
typedef struct {
    uint32_t magic;
    uint8_t b[NPAT][STEPB_N];
    uint8_t tb[8][32];                   /* LOAF BREAKER: each track item's break (TRACKS K4): 0 the one playing, 1..
                                          * that break + 1 (NTRACKS x TRK_ITEMS; an older save: none) */
} steps_t;                               /* (AFFINE: the kicks' knobs, the track items' kicks, STYLE's presets gone) */
#define STEPS_SIZE_OLD offsetof(steps_t, tb)
_Static_assert(sizeof(steps_t) <= 3840u, "a store object holds 3840 bytes");

static steps_t steps PLAT_POOL;

/* LOAF BREAKER: 64 patterns, four groups of 16 (P1-16, P17-32, P33-48, P49-64). The group in RAM is pats.p, pats.kn,
 * pats.hits and steps.b (set.pattern its slot, set.pgrp the group); each group has its own three store objects
 * (OBJ_PG0, OBJ_KG0, OBJ_SG0 + g). A save writes the group in RAM to its own (OBJ_PAT and OBJ_STEP keep the tracks and
 * the rest, and the group in RAM: an older firmware reads that one as P1-16); another group comes in from its own (a
 * read: the sound plays on). Leaving a group whose patterns changed since: saved first while stopped, refused while
 * playing (a write stops the sound for a moment): SAVE or STOP first */
#define NGRP 4u
#define NPAT_ALL (NPAT * NGRP)
#define KH_SIZE (sizeof pats.kn + sizeof pats.hits)
/* AFFINE (16 Nov 2026, the owner: "all the performance FX would be nice to be recorded if the rec button is active"):
 * the black keys recorded with REC, a byte a step of every pattern (64 x 32): 0 none, else the function or the combo
 * held over that step + 1 (BKF_* 0..15, BKC_0 + CB_* 32..42). Playing, a step's code acts as its key held over it.
 * Its own store object, OBJ_FREE0 (0xB6000, kept free for motion recording), written with the patterns (SAVE, a group
 * left: grp_save) when it changed since; read at boot from AFFINE's own store only (another firmware's: none) */
#define FXR_MAGIC 0x31525846u            /* "FXR1" */
typedef struct {
    uint32_t magic;
    uint8_t s[NPAT_ALL][PAT_STEPS];
} fxrec_t;                               /* 2052 bytes */
_Static_assert(sizeof(fxrec_t) <= 3840u, "a store object holds 3840 bytes");
static fxrec_t fxr PLAT_POOL;
static uint8_t fxr_dirty;                /* (changed since it was last written) */
static uint8_t *fxr_pat(uint32_t pi) { return fxr.s[(set.pgrp % NGRP * NPAT + pi % NPAT) % NPAT_ALL]; }   /* (pattern
                                          * pi of the group in RAM: its 32 steps) */
static void fxr_clear(uint32_t pi, uint32_t from, uint32_t n)   /* its steps from, n of them, none */
{
    uint8_t *r = fxr_pat(pi);
    uint32_t k;
    for (k = from; k < from + n && k < PAT_STEPS; k++)
        if (r[k]) {
            r[k] = 0u;
            fxr_dirty = 1u;
        }
}
static int fxr_save(void) { return plat_store_save(OBJ_FREE0, &fxr, sizeof fxr) == 0; }
static uint32_t bk_code_ok(uint32_t v)   /* a step's byte as it plays: a code + 1 there is, else 0 */
{
    return v && (v - 1u < BKF_N || BKC(v - 1u)) ? v : 0u;
}
/* (0.9.30: the snare's, hat's, clap's bars 3 and 4, every group's: dx_of below) */
#define DX_MAGIC 0x31435844u             /* "DXC1" */
typedef struct {
    uint32_t magic;
    uint16_t b[NGRP][NPAT][6];           /* the snare's bars 3, 4, the hat's, the clap's */
} dx_t;
static dx_t dx PLAT_POOL;
static uint8_t dx_old;                   /* (a save before it: bits 5-7 of the lengths cleared as a group comes in) */
static const pat_t *dx_tp;
static uint8_t dx_tg, dx_ts;
static void dx_lens(void);
_Static_assert(offsetof(pats_t, hits) == offsetof(pats_t, kn) + sizeof(((pats_t *)0)->kn), "kn, hits: one run");
static uint32_t grp_sum;                 /* the group's hash as it came in or was saved */
static uint8_t grp_new;                  /* the group has none of its own objects yet (an older save's P1-16) */
static uint32_t pat_cur(void) { return set.pgrp % NGRP * NPAT + set.pattern % NPAT; }   /* 0..63 */
static uint32_t grp_hash(void)
{
    const uint8_t *r[4] = {(const uint8_t *)pats.p, pats.kn[0], steps.b[0], (const uint8_t *)dx.b[set.pgrp % NGRP]};
    const uint32_t n[4] = {sizeof pats.p, KH_SIZE, sizeof steps.b, sizeof dx.b[0]};
    uint32_t h = 2166136261u, i, k;
    for (k = 0; k < 4u; k++)
        for (i = 0; i < n[k]; i++)
            h = (h ^ r[k][i]) * 16777619u;
    return h;
}
static int grp_changed(void) { return grp_new || grp_hash() != grp_sum; }
static uint8_t wiped;                    /* LOAF BREAKER (0.263): SYSTEM › RESET WIPE MEMORY done: nothing saved until a
                                          * restart (track.c wipe_step) */
static int grp_save(void)                /* the group in RAM to its objects: 1 done */
{
    uint32_t g = set.pgrp % NGRP;
    int ok;
    if (wiped)
        return 0;
    if (!(set.grp_ours >> g & 1u)) {     /* (AFFINE: the group AFFINE's from now on; the settings say so once) */
        set.grp_ours |= (uint8_t)(1u << g);
        (void)plat_store_save(OBJ_SETTINGS, &set, SET_SAVED);
    }
    ok = plat_store_save(OBJ_PG0 + g, pats.p, sizeof pats.p) == 0 &&
             plat_store_save(OBJ_KG0 + g, pats.kn, KH_SIZE) == 0 &&
             plat_store_save(OBJ_SG0 + g, steps.b, sizeof steps.b) == 0 &&   /* (LOAF BREAKER, 0.209: OBJ_NG0 + g, the
                                          * MELODY's notes, no more) */
             plat_store_save(OBJ_DX, &dx, sizeof dx) == 0 &&   /* (0.9.30: the drums' bars 3 and 4, every group's) */
             (!fxr_dirty || fxr_save());   /* (AFFINE: the black keys recorded, every pattern's) */
    if (ok) {
        fxr_dirty = 0u;
        grp_sum = grp_hash();
        grp_new = 0u;
        dx_old = 0u;                     /* (OBJ_DX written: lengths of 64 kept from now on) */
    }
    return ok;
}
static int grp_has(uint32_t g)           /* the group's own objects there, whole (AFFINE: and AFFINE's) */
{
    return (set.grp_ours >> (g % NGRP) & 1u) && plat_store_size(OBJ_PG0 + g) == (int)sizeof pats.p && plat_store_size(OBJ_KG0 + g) == (int)KH_SIZE &&
           plat_store_size(OBJ_SG0 + g) == (int)sizeof steps.b;
}
static void grp_blank(void)              /* the group in RAM: 16 empty patterns (pat_defaults' own) */
{
    uint32_t i, k;
    for (i = 0; i < NPAT; i++) {
        for (k = 0; k < PAT_STEPS; k++) {
            pats.p[i].st[k][0] = 1u;
            pats.p[i].st[k][1] = pats.p[i].st[k][2] = pats.p[i].st[k][3] = 0u;
        }
        pats.p[i].lane[LANE_KICK] = pats.p[i].lane[LANE_SNARE] = pats.p[i].lane[LANE_HAT] = 0u;
        pats.p[i].kick2 = 0u;
        pats.p[i].st[6][3] = 27u;        /* (0.9.32, FLEXOR: every drum 32 steps, whatever the mode: PAT_ALL32) */
    }
    memset(pats.kn, 0, KH_SIZE);
    memset(steps.b, 0, sizeof steps.b);
}
static void grp_load(uint32_t g)         /* group g into RAM (none saved yet: empty) */
{
    g %= NGRP;
    if (!grp_has(g) || plat_store_load(OBJ_PG0 + g, pats.p, sizeof pats.p) != (int)sizeof pats.p ||
        plat_store_load(OBJ_KG0 + g, pats.kn, KH_SIZE) != (int)KH_SIZE ||
        plat_store_load(OBJ_SG0 + g, steps.b, sizeof steps.b) != (int)sizeof steps.b) {
        grp_blank();
        memset(dx.b[g], 0, sizeof dx.b[g]);
    }
    set.pgrp = (uint8_t)g;
    dx_lens();
    grp_sum = grp_hash();
    grp_new = 0u;
}
static int store_all(void)               /* SAVE: the settings, the patterns (the group in RAM too), the steps: 1 done
                                          * (AFFINE: the hardcore kick's four objects no more, OBJ_FREE0..3) */
{
    if (wiped)                           /* (LOAF BREAKER, 0.263: WIPE MEMORY done: nothing saved until a restart) */
        return 0;
    set.grp_ours |= (uint8_t)(1u << (set.pgrp % NGRP));   /* (AFFINE: the group in RAM is written below) */
    return plat_store_save(OBJ_SETTINGS, &set, SET_SAVED) == 0 && plat_store_save(OBJ_PAT, &pats, PATS_SAVED) == 0 &&
           plat_store_save(OBJ_STEP, &steps, sizeof steps) == 0 && grp_save();
}
static uint8_t *stepb(uint32_t pi, uint32_t d, uint32_t s)   /* pattern pi, drum d (DR_*), its step s */
{
    d %= NDRUMS;
    return &steps.b[pi % NPAT][d == DR_KICK ? (s & 63u) : 64u + (d - 1u) * 32u + (s & 31u)];
}
static void steps_defaults(void)          /* (none: from the patterns' HITS) */
{
    uint32_t pi, d, s;
    steps.magic = STEPS_MAGIC;
    memset(steps.tb, 0, sizeof steps.tb);
    for (pi = 0; pi < NPAT; pi++)
        for (d = 0; d < NDRUMS; d++)
            for (s = 0; s < (d == DR_KICK ? 64u : 32u); s++) {
                uint32_t h = pats.hits[pi][d];
                *stepb(pi, d, s) = (uint8_t)(h >= 2u && h <= 4u ? h - 1u : 0u);
            }
}
_Static_assert(sizeof pats <= 3840u, "a store object holds 3840 bytes");
/* LOAF BREAKER: BUILD and DROP. SEL clicked three times, playing (build_n, the main loop): from the next bar the
 * drums' steps give way to a snare roll that speeds up (quarters, eighths, 16ths, then 32nds, a quarter of the build
 * each), louder and louder, the whole mix high-passed higher and higher under a rising noise; after its bars (DROP's
 * BUILD: 1, 2, 4, 8; none: the bar playing ends in the drop; or three clicks again: at the next bar),
 * the DROP: everything back at once, the next pattern's drums from its first step (drum_next, ready from the main
 * loop), a crash on it (drop_n counts them: the screen flashes and moves to that pattern) */
static const uint8_t DROP_BARS[5] = {0u, 1u, 2u, 4u, 8u};   /* the build's bars (DROP's K1) */
static volatile uint8_t build_bars = 4u; /* the next build's (the main loop's, as it asks for it) */
static volatile uint32_t build_len;      /* the build's steps (the render's) */
static volatile uint8_t build_wait;      /* asked for: it starts at the next bar (none: the drop at this bar's end) */
static volatile float drop_gain = 1.0f;  /* the crash's level */
static volatile uint8_t b_gapm;          /* (0.9.26, FLEXOR: what the GAP keeps: 0 nothing, 1 the break, 2 the drums, 3
                                          * the echoes (the sources quiet, the FX ringing on); a track's DROP item's K4) */
static volatile uint8_t b_style, b_sweep, b_gap;   /* (the main loop's, as it asks: this build's style (DS_*, chance
                                          * picked), its filter (0 HP, 1 LP, 2 none), its gap (steps)) */
static volatile float b_riser = 1.0f;    /* its noise's level (1: as it always was) */
/* LOAF BREAKER: the roll pads' KICK ROLL (their THRU KICK or BOTH): the pad held + 1 (0 none, the main loop's); the
 * render hits the kick at that pad's rate from the next step, the pattern's kicks out meanwhile */
static volatile uint8_t kroll;
static volatile uint8_t kick_hold;       /* LOAF BREAKER: KIL (the tenth black key) held: the sequencer's kicks quiet */
#define LIM_LA 64u                       /* LOAF BREAKER: the output limiter's look-ahead, samples (1.5 ms) */
static inline float lim_soft(float x)    /* over 0.95: rounded off, never reaching 1 */
{
    float a = x < 0.0f ? -x : x;
    if (a <= 0.95f)
        return x;
    a = 0.95f + 0.05f * (1.0f - 1.0f / (1.0f + (a - 0.95f) * 20.0f));
    return x < 0.0f ? -a : a;
}         /* LOAF BREAKER TEST: ARP held: FILL (the steps' FILL / NOT FILL, the snare's
                                          * roll on the bar's last beat) */
static volatile uint32_t kick_hit_n;     /* (the kick's hits so far: the simulator's checks) */
/* LOAF BREAKER: the drums' HITS (pats.hits): a hit's repeats still to come, samples from this block's start */
#define HQ_N 16u
static struct { int32_t at; uint8_t kick, who; float pm; } hq[HQ_N];
static uint32_t hq_n;
static void hq_push(int32_t at, uint8_t kick, uint8_t who, float pm);
/* a drum's hit on step si of pattern pi, at off (len: a 16th): its step's CHANCE (0: skipped), its NUDGE (later) and
 * its HITS (the repeats in its step) queued; 1: it sounds at off itself */
static uint32_t step_rng = 0x9E3779B9u;
static uint32_t pat_cycle;               /* LOAF BREAKER TEST: the pattern's loops (its first step), for 1:2 and 2:2 */
static int step_hit(uint32_t pi, uint32_t d, uint32_t si, int32_t off, int32_t len, uint8_t kick, uint8_t who, float pm)
{
    uint32_t b = *stepb(pi, d, si), hn = (b & 3u) + 1u, ch = (b >> 2) & 7u, nd = b >> 5, j;
    int32_t at;
    if (ch >= 1u && ch <= 3u) {          /* LOAF BREAKER TEST: CHANCE 75 %, 50 %, 25 % */
        step_rng = step_rng * 1664525u + 1013904223u;
        if ((step_rng >> 30) < ch)
            return 0;
    } else if ((ch == 4u && (pat_cycle & 1u)) || (ch == 5u && !(pat_cycle & 1u)))   /* (1:2 the first loop of two,
                                          * 2:2 the second; FILL, NOT FILL: always, FILL gone in 0.244) */
        return 0;
    at = off + (int32_t)nd * len / 16;
    if (nd)
        hq_push(at, kick, who, pm);
    for (j = 1; j < hn; j++)
        hq_push(at + len * (int32_t)j / (int32_t)hn, kick, who, pm);
    return !nd;
}
static void hq_push(int32_t at, uint8_t kick, uint8_t who, float pm)
{
    if (hq_n < HQ_N) {
        hq[hq_n].at = at;
        hq[hq_n].kick = kick;
        hq[hq_n].who = who;
        hq[hq_n].pm = pm;
        hq_n++;
    }
}
static volatile uint8_t build_n;         /* three clicks: a build, or (building) the drop */
static volatile uint16_t drum_next[NDRUMS][4];   /* the next pattern's drums, for the drop */
static volatile uint8_t drop_to = 0xFFu; /* the pattern the drop brings in (its break's steps, at the bar before) */
static volatile uint8_t drum_next_ok;
static volatile uint8_t drop_n;
static volatile float comp_gr;           /* LOAF BREAKER: COMP's reduction now, dB (the KNOB page's meter) */
/* LOAF BREAKER: what leaves the mix (before MASTER), a column a 15th of a step (44100 / BPM samples): its lows (under
 * ~200 Hz), mids, highs (over ~2.5 kHz) and peak, 0..255, for HOME's CDJ view (cdj.c); a ring, scope_w the columns
 * written so far */
#define SCOPE_N 480u                     /* (LOAF BREAKER: two bars, for WAVE ZOOM's widest) */
static uint8_t scope[SCOPE_N][4] PLAT_POOL;   /* (in the pool since 0.9.43: the RAM for the screen mirror) */
static volatile uint8_t scope_on = 1u;   /* (LOAF BREAKER, 0.226: the main loop's: the CDJ view on screen; else the
                                          * render spares the scope's work) */
static volatile uint32_t scope_w;
static volatile float build_p = -1.0f;   /* the build's progress 0..1, -1 none (the screen's and the lights') */
static volatile uint16_t drum_now[NDRUMS][4];   /* the pattern playing's drums, bar by bar of four (their lengths
                                          * played out: pat_lanes_out), for the render */
static int8_t pat_next = -1;             /* a pattern picked while playing: it starts at the loop's end */

static void trk_defaults(void)          /* every track empty; the first a start: P1, P2, a drop, P3 */
{
    static const uint8_t T1[5] = {1u, 2u, TI_DROP, 3u, TI_END}, R1[5] = {4u, 4u, 1u, 8u, 1u};
    uint32_t t, i;
    for (t = 0; t < NTRACKS; t++)
        for (i = 0; i < TRK_ITEMS; i++) {
            pats.trk[t].it[i] = t == 0u && i < 5u ? T1[i] : TI_END;
            pats.trk[t].rep[i] = t == 0u && i < 5u ? R1[i] : 1u;
            pats.trk[t].end = 0u;
        }
}
static void pat_defaults(void)
{
    uint32_t i, k;
    pats.magic = PAT_MAGIC;
    memset(dx.b[set.pgrp % NGRP], 0, sizeof dx.b[0]);
    for (i = 0; i < NPAT; i++) {
        pat_t *p = &pats.p[i];
        for (k = 0; k < PAT_STEPS; k++) {
            p->st[k][0] = 1u;            /* follow the loop */
            p->st[k][1] = p->st[k][2] = p->st[k][3] = 0u;
        }
        p->lane[LANE_KICK] = p->lane[LANE_SNARE] = p->lane[LANE_HAT] = 0u;   /* the drums start empty */
        p->kick2 = 0u;
        p->st[6][3] = 27u;               /* (0.9.32, FLEXOR: the kick, the snare, the hat and the clap 32 steps, whatever
                                          * the mode: PAT_ALL32, below; BEGINNER's kick 16 in 0.246-0.9.31, 32 before) */
    }
    trk_defaults();
    pats.mem.fx_ok = 0u;
    for (i = 0; i < NPAT; i++) {
        for (k = 0; k < 13u; k++)
            pats.kn[i][k] = 0u;
        for (k = 0; k < NDRUMS_SAVED; k++)
            pats.hits[i][k] = 0u;
    }
    memset(steps.b, 0, sizeof steps.b);  /* (AFFINE, 16 Nov 2026: the drum steps' own HITS, CHANCE, NUDGE, and the track
                                          * items' breaks, with them: RESET ALL wrote the old ones into every group) */
    memset(steps.tb, 0, sizeof steps.tb);
}
/* GLOUPI BREAKS: the kick over four bars (64 steps), bar b (0..3) 16 bits: lane[LANE_KICK], kick2, and bars 3 and 4
 * in the spare bytes of steps 0-3 (st[0..3][3], low byte first) */
static uint32_t kick_bar(const pat_t *p, uint32_t b)
{
    b &= 3u;
    if (b < 2u)
        return b ? p->kick2 : p->lane[LANE_KICK];
    return (uint32_t)p->st[(b - 2u) * 2u][3] | (uint32_t)p->st[(b - 2u) * 2u + 1u][3] << 8;
}
static void kick_bar_set(pat_t *p, uint32_t b, uint32_t m)
{
    b &= 3u;
    if (b == 0u)
        p->lane[LANE_KICK] = (uint16_t)m;
    else if (b == 1u)
        p->kick2 = (uint16_t)m;
    else {
        p->st[(b - 2u) * 2u][3] = (uint8_t)m;
        p->st[(b - 2u) * 2u + 1u][3] = (uint8_t)(m >> 8);
    }
}
/* the snare over two bars: bar 1 lane[LANE_SNARE], bar 2 in the spare bytes of steps 4-5 */
static uint32_t snare_bar(const pat_t *p, uint32_t b)
{
    return b & 1u ? (uint32_t)p->st[4][3] | (uint32_t)p->st[5][3] << 8 : p->lane[LANE_SNARE];
}
static void snare_bar_set(pat_t *p, uint32_t b, uint32_t m)
{
    if (b & 1u) {
        p->st[4][3] = (uint8_t)m;
        p->st[5][3] = (uint8_t)(m >> 8);
    } else
        p->lane[LANE_SNARE] = (uint16_t)m;
}
/* the lanes' lengths (step 6's spare byte; 0, an older save's: the kick 64 steps, the snare 16) */
#define PAT_LEN(p) ((p)->st[6][3])
#define PAT_KICK32 1u                    /* the kick 32 steps (bars 1-2 again and again), else 64 */
#define PAT_SNARE32 2u                   /* the snare 32 steps (bars 1-2), else 16 (bar 1 every bar) */
#define PAT_KICK16 4u                    /* LOAF BREAKER: the kick 16 steps (bar 1 every bar), over PAT_KICK32 */
#define PAT_ALL32 27u                    /* (0.9.32, FLEXOR: every drum 32 steps, a new pattern's: the kick, the snare 2, the
                                          * hat 8, the clap 16) */
static uint32_t kick_bars(const pat_t *p)   /* the kick's bars played: 1, 2 or 4 */
{
    return PAT_LEN(p) & PAT_KICK16 ? 1u : PAT_LEN(p) & PAT_KICK32 ? 2u : 4u;
}
/* LOAF BREAKER: every drum, bar b of its two (the kick's four): the hat's second in the spare bytes of steps 7-8, the
 * clap's two in 9-12; 32 steps or 16: step 6's spare byte, bit 3 the hat, bit 4 the clap */
static const uint8_t LEN_BIT[NDRUMS] = {0u, PAT_SNARE32, 8u, 16u};   /* (bit 5: the MELODY's, 0.200-0.208) */
/* LOAF BREAKER (0.9.30, FLEXOR): the snare, the hat and the clap over four bars too (64 steps: step 6's spare byte, bits
 * 5-7); their bars 3 and 4, every group's, in their own store object (OBJ_DX); a step's HITS, CHANCE, NUDGE there:
 * steps 33-64 share those of 1-32 (stepb). dx_tp: another group's pattern read in for a drop (track.c), its place */
static const uint8_t LEN64[NDRUMS] = {0u, 32u, 64u, 128u};
static uint16_t *dx_of(const pat_t *p, uint32_t d, uint32_t b)
{
    uint32_t i = (d - 1u) % 3u * 2u + (b & 1u);
    if (p >= pats.p && p < pats.p + NPAT)
        return &dx.b[set.pgrp % NGRP][p - pats.p][i];
    return p && p == dx_tp ? &dx.b[dx_tg % NGRP][dx_ts % NPAT][i] : 0;
}
static void dx_lens(void)                /* (a save from before them: no drum 64 steps long) */
{
    uint32_t i;
    if (dx_old)
        for (i = 0; i < NPAT; i++)
            PAT_LEN(&pats.p[i]) &= 0x1Fu;
}
static uint32_t drum_at(uint32_t d, uint32_t b) { return d == DR_HAT ? 7u : 9u + (b & 1u) * 2u; }
static uint32_t drum_bar(const pat_t *p, uint32_t d, uint32_t b)
{
    uint32_t k;
    if (d == DR_KICK)
        return kick_bar(p, b);
    if (b & 2u) {                        /* (bars 3 and 4: OBJ_DX's) */
        const uint16_t *x = dx_of(p, d, b);
        return x ? *x : 0u;
    }
    if (d == DR_SNARE)
        return snare_bar(p, b);
    if (d == DR_HAT && !(b & 1u))
        return p->lane[LANE_HAT];
    k = drum_at(d, b);
    return (uint32_t)p->st[k][3] | (uint32_t)p->st[k + 1u][3] << 8;
}
static void drum_bar_set(pat_t *p, uint32_t d, uint32_t b, uint32_t m)
{
    uint32_t k;
    if (d == DR_KICK)
        kick_bar_set(p, b, m);
    else if (b & 2u) {
        uint16_t *x = dx_of(p, d, b);
        if (x)
            *x = (uint16_t)m;
    } else if (d == DR_SNARE)
        snare_bar_set(p, b, m);
    else if (d == DR_HAT && !(b & 1u))
        p->lane[LANE_HAT] = (uint16_t)m;
    else {
        k = drum_at(d, b);
        p->st[k][3] = (uint8_t)m;
        p->st[k + 1u][3] = (uint8_t)(m >> 8);
    }
}
/* LOAF BREAKER (crossbreed): the kick's notes, one a step of its four bars (s 0..63), -8 .. +7 semitones, four bits
 * each: steps 1-38 in the spare bytes of steps 14-32 (st[13..31][3]), the rest in pats.kn (a pattern of pats.p only:
 * another copy keeps its own, kick_notes_copy) */
static uint8_t *kick_note_byte(const pat_t *p, uint32_t s)
{
    uint32_t pi = (uint32_t)(p - pats.p);
    s &= 63u;
    if (s < 38u)
        return (uint8_t *)&p->st[13u + s / 2u][3];
    return p >= pats.p && pi < NPAT ? &pats.kn[pi][(s - 38u) / 2u] : 0;
}
static int32_t kick_note(const pat_t *p, uint32_t s)
{
    const uint8_t *b = kick_note_byte(p, s);
    uint32_t v = b ? (s & 1u ? *b >> 4 : *b & 15u) : 0u;
    return v >= 8u ? (int32_t)v - 16 : (int32_t)v;
}
static void kick_note_set(pat_t *p, uint32_t s, int32_t v)
{
    uint8_t *b = kick_note_byte(p, s);
    uint32_t n = (uint32_t)(v < -8 ? -8 : v > 7 ? 7 : v) & 15u;
    if (b)
        *b = (uint8_t)(s & 1u ? (*b & 0x0Fu) | n << 4 : (*b & 0xF0u) | n);
}
static uint32_t drum_bars(const pat_t *p, uint32_t d)   /* its bars played: 1, 2 or 4 */
{
    if (d == DR_KICK)
        return kick_bars(p);
    return PAT_LEN(p) & LEN64[d] ? 4u : PAT_LEN(p) & LEN_BIT[d] ? 2u : 1u;
}
static void pat_step_of(uint32_t pi, uint32_t k)   /* step k of pattern pi, to the engine */
{
    const uint8_t *b = pats.p[pi % NPAT].st[k];
    brk_step_t s;
    s.slice = (int8_t)(b[0] >= 2u ? (int32_t)b[0] - 2 : BRK_NO_SLICE);
    s.on = (uint8_t)(b[0] != 0u);
    s.pitch = (int8_t)b[1];
    s.flags = (uint8_t)(b[2] & 1u ? STEP_REVERSE : 0u);
    s.ratchet = (uint8_t)(((b[2] >> 1) & 3u) + 1u);
    s.prob = 100u;
    s.rsv[0] = s.rsv[1] = 0u;
    brk_step_set(k, &s);
}
static void pat_step_out(uint32_t k) { pat_step_of(set.pattern, k); }   /* step k of the pattern playing */
static void pat_lanes_out(void)
{
    const pat_t *p = &pats.p[set.pattern % NPAT];
    uint32_t l, d;
    for (d = 0; d < NDRUMS; d++)
        for (l = 0; l < 4u; l++)
            drum_now[d][l] = (uint16_t)drum_bar(p, d, l % drum_bars(p, d));
}
static void pat_apply(void)              /* the pattern playing, all of it to the engine */
{
    uint32_t k;
    for (k = 0; k < PAT_STEPS; k++)
        pat_step_out(k);
    pat_lanes_out();
}
static void pat_read_back(void)          /* the engine's steps into the pattern playing (after RANDOMIZE) */
{
    uint32_t k;
    for (k = 0; k < PAT_STEPS; k++) {
        brk_step_t s;
        uint8_t *b = pats.p[set.pattern % NPAT].st[k];
        brk_step_get(k, &s);
        b[0] = (uint8_t)(!s.on ? 0u : s.slice >= 0 ? (uint32_t)s.slice + 2u : 1u);
        b[1] = (uint8_t)s.pitch;
        b[2] = (uint8_t)((b[2] & 0xF8u) | (s.flags & STEP_REVERSE ? 1u : 0u) |   /* (bits 3-7: the stab's, kept) */
                         ((uint32_t)(s.ratchet ? s.ratchet - 1u : 0u) & 3u) << 1);
    }
}

/* ------------------------------------------------------------------- engine --- */
static int16_t pcm_a[CH_PCM_MAX] PLAT_POOL;
#define CH_DUB_LEN 30720u                /* the dub delay's line: 0.70 s a side, 3/8 down to 97 BPM, 1/2 to 172 */
static uint8_t dub_line[CH_DUB_LEN] PLAT_POOL;   /* (AFFINE: 8-bit mu-law, half LOAF's 16-bit memory) */
static int16_t spu_work[PSXV_WORDS] PLAT_POOL;   /* AFFINE: SPU VERB's work area, 64 KB (every PS1 preset) */
static uint8_t skip_ring[2u * PSX_SKIP_FRAMES];  /* AFFINE: SKIP's capture, 24 KB of RAM (LOAF's plate had 36 KB) */
static int16_t ovl_line[2u * OVL_LINE] PLAT_POOL;   /* the overlays' WOBBLE and JET (4 KB) */
static brk_break_t brk0;
static brk_break_t brk_cur[2];           /* the bank's break playing (pcm_a holds it), and the next one's: the render
                                          * keeps its own while the next is written */
static uint32_t n_synth;                 /* 0; 1 in a build with no bank: a synthesised break (brk_synth.c) */
static uint32_t n_breaks;                /* then the bank's valid slots */
static int32_t brk_next = -1;            /* a break chosen, waiting for the next bar (break_ask), or -1 */
static uint8_t wave_px[240];             /* the waveform's height per column, for the screen */
#define BLK_N 16u                        /* the slicer view's blocks: one per white key */
#define BLK_W 26u                        /* columns of waveform in a block */
static uint8_t blk_px[BLK_N][BLK_W];     /* each slice's waveform, 0..19 */
static uint8_t blk_n;                    /* blocks with a slice behind them */
static volatile uint8_t muted;
static volatile float meter_brk, meter_kick, meter_perc;   /* the MIXER's channels: the break, the kick, the snare
                                                           * and hat, before the chain */
static volatile float meter_l, meter_r;  /* the output's peaks as it leaves (after MASTER: 1 = full scale), falling
                                          * ~20 dB a second: the render writes, the header's meter reads */
static volatile uint8_t out_sound;       /* LOAF BREAKER: a block was heard (over -60 dB, after MASTER): the render sets
                                          * it, AUTO SAVE clears it (no save while a tail, an echo, still sounds) */
#define LR_TEST_LEN (4u * BRK_SR)
static volatile uint32_t lr_test;        /* the left / right test (BT AUDIO K3 while streaming): samples left of it */

static void wave_build(const brk_break_t *b)
{
    uint32_t x, i;
    for (x = 0; x < 240u; x++) {
        uint32_t a = b->len * x / 240u, z = b->len * (x + 1u) / 240u, pk = 0u;
        for (i = a; i < z; i++) {
            int32_t s = b->pcm[i] < 0 ? -b->pcm[i] : b->pcm[i];
            if ((uint32_t)s > pk)
                pk = (uint32_t)s;
        }
        wave_px[x] = (uint8_t)(pk * 24u / 32768u);
    }
    blk_n = (uint8_t)(b->nslices < BLK_N ? b->nslices : BLK_N);
    for (x = 0; x < BLK_N; x++) {
        uint32_t a = x < blk_n ? b->slice[x] : 0u, z = x < blk_n ? b->slice[x + 1] : 0u, c;
        for (c = 0; c < BLK_W; c++) {
            uint32_t i0 = a + (z - a) * c / BLK_W, i1 = a + (z - a) * (c + 1u) / BLK_W, pk = 0u;
            for (i = i0; i < i1; i++) {
                int32_t v = b->pcm[i] < 0 ? -b->pcm[i] : b->pcm[i];
                if ((uint32_t)v > pk)
                    pk = (uint32_t)v;
            }
            blk_px[x][c] = (uint8_t)(pk * 19u / 32768u);
        }
    }
}

/* a bank's slot i (0..), or 0 when absent or not sane (size: the bytes the bank may span; CHOPPAH 1.4's) */
static const bank_slot_t *bank_check(const uint8_t *bank, uint32_t size, uint32_t i)
{
    const bank_header_t *h = (const bank_header_t *)bank;
    const bank_slot_t *s;
    if (!bank || h->magic != BANK_MAGIC || h->version != BANK_VERSION || i >= h->nslots || i >= BANK_SLOTS ||
        16u + 168u * (i + 1u) > size)
        return 0;
    s = (const bank_slot_t *)(bank + 16u + 168u * i);
    if (s->data_off + s->data_len > size || s->len > CH_PCM_MAX || s->data_len < (s->len + 1u) / 2u ||
        !s->nslices || s->nslices > BANK_SLICES || !s->bars || s->slice[s->nslices] != s->len || !s->bpm_x100)
        return 0;
    return s;
}

/* LOAF BREAKER (0.9.32, from CHOPPAH 1.4, FLEXOR: the site's BREAKS page): the player's own breaks, in two banks
 * (zones) of up to CU_MAX each, outside the store's objects, written whole over USB from the site's break manager
 * (app_sysex, ui/main.c). Present, they come after the bank's, zone 0's then zone 1's (the breaks saved by number
 * keep theirs); a RESET ALL and WIPE MEMORY leave them (not in the store map).
 * Zone 0: PLAT_XDATA0 (16 KB) and the data area's start up to OBJ_FREE0 (0xB6000), one run of 140 KB;
 * zone 1: PLAT_XDATA1 (92 KB, up to the settings). In the two XDATA windows the update loader takes a sector whose
 * bytes 0xF06..0xF07 read "AT" for an update record (plat.h): the page never sends one so (it nudges a nibble), and a
 * write that makes one is undone (cu_at_ok). A 0.9.32 TEST's zone 0 started at 0x97000 (CU_OLD, 124 KB): read there
 * while zone 0 holds none, until zone 0 is written again */
#define CU_MAX 4u                        /* breaks in a zone */
#define CU_NZ 2u
#define CU_OLD 0x97000u
static const uint32_t CU_ZB[CU_NZ] = {0x93000u, 0xE5000u};   /* each zone's start */
static const uint32_t CU_ZS[CU_NZ] = {35u, 23u};               /* and its sectors */
#define CU_CAP(z) (CU_ZS[z] * 4096u)
static uint32_t n_user, n_bank;          /* the player's breaks (each zone checked whole: cu_scan); the bank's */
static uint32_t n_uz[CU_NZ], cu_at[CU_NZ], cu_cap[CU_NZ];   /* each zone's breaks, where its bank is read, its room */
static uint32_t crc32_of(const uint8_t *p, uint32_t n)   /* zlib's */
{
    uint32_t c = ~0u, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static uint32_t cu_check(uint32_t at, uint32_t cap)   /* the breaks of a bank at at, every one sane, or 0 */
{
    const uint8_t *p = plat_data(at);
    const bank_header_t *h = (const bank_header_t *)p;
    uint32_t k;
    if (!p || h->magic != BANK_MAGIC || !h->nslots || h->nslots > CU_MAX || h->data_size > cap - 16u ||
        crc32_of(p + 16, h->data_size) != h->crc32)
        return 0u;
    for (k = 0; k < h->nslots; k++)
        if (!bank_check(p, cap, k))
            return 0u;
    return h->nslots;
}
static void cu_scan(void)
{
    uint32_t z;
    n_user = 0u;
    for (z = 0; z < CU_NZ; z++) {
        cu_at[z] = CU_ZB[z];
        cu_cap[z] = CU_CAP(z);
        n_uz[z] = cu_check(cu_at[z], cu_cap[z]);
        if (!z && !n_uz[z] && (n_uz[z] = cu_check(CU_OLD, 31u * 4096u)) != 0u) {   /* (a 0.9.32 TEST's) */
            cu_at[z] = CU_OLD;
            cu_cap[z] = 31u * 4096u;
        }
        n_user += n_uz[z];
    }
}
/* bytes 0xF06..0xF07 of the sectors [at, at + n) that lie in an XDATA window: none "AT" (an update record's mark) */
static int cu_at_ok(uint32_t at, uint32_t n)
{
    uint32_t s;
    for (s = at & ~0xFFFu; s < at + n; s += 4096u) {
        const uint8_t *q = plat_data(s + 0xF06u);
        if ((s < PLAT_DATA_LO || s >= PLAT_DATA_HI) && q && q[0] == 'A' && q[1] == 'T')
            return 0;
    }
    return 1;
}
/* LOAF BREAKER (0.9.45, FLEXOR: the site's IMPORT page): the player's drum samples, a kit of three (the snare, the hat,
 * the clap), IMA ADPCM, in the two sectors of OBJ_BLE (0xDE000, 8 KB: no Bluetooth in LOAF BREAKER, nothing else
 * writes there), written whole over USB SysEx (zone 2, ui/main.c kit_sysex) and read where they lie as they play
 * (perc_set_sample). A 16-byte header {KIT_MAGIC, the crc32 of what follows it, its size, 0}, then for each drum
 * {offset from the kit's start, its samples, its rate, its name (12)}, 24 bytes, then the data. WIPE MEMORY erases it
 * (the store map's), RESET ALL leaves it */
#define KIT_AT 0xDE000u
#define KIT_CAP 8192u
#define KIT_MAGIC 0x314B5244u               /* "DRK1" */
#define KIT_HEAD (16u + 3u * 24u)
typedef struct {
    uint32_t off, len, rate;
    char name[12];
} kit_ent_t;
static uint8_t kit_has;                  /* (bit v: drum v has its sample) */
static const kit_ent_t *kit_ent(uint32_t v)   /* (drum v's, or 0) */
{
    const uint8_t *p = plat_data(KIT_AT);
    return p && kit_has >> (v % 3u) & 1u ? (const kit_ent_t *)(const void *)(p + 16u + (v % 3u) * 24u) : 0;
}
static void kit_scan(void)
{
    const uint8_t *p = plat_data(KIT_AT);
    const uint32_t *h = (const uint32_t *)(const void *)p;
    uint32_t v;
    kit_has = 0u;
    if (p && h[0] == KIT_MAGIC && h[2] >= KIT_HEAD - 16u && h[2] <= KIT_CAP - 16u && crc32_of(p + 16, h[2]) == h[1])
        for (v = 0; v < 3u; v++) {
            const kit_ent_t *e = (const kit_ent_t *)(const void *)(p + 16u + v * 24u);
            if (e->len && e->off >= KIT_HEAD && e->rate >= 8000u && e->rate <= 48000u && e->len <= 2u * KIT_CAP &&
                e->off + (e->len + 1u) / 2u <= 16u + h[2])
                kit_has |= (uint8_t)(1u << v);
        }
    for (v = 0; v < 3u; v++) {
        const kit_ent_t *e = kit_ent(v);
        perc_set_sample(v, e ? p + e->off : 0, e ? e->len : 0u, e ? e->rate : 0u);
    }
}
static void kit_name(uint32_t v, char *t)  /* (drum v's sample, its name; "" none) */
{
    const kit_ent_t *e = kit_ent(v);
    uint32_t k;
    for (k = 0; k < 12u; k++)
        t[k] = e && e->name[k] >= ' ' && e->name[k] <= 'z' ? e->name[k] : 0;
    t[12] = 0;
}
/* break i's slot and its bank: the bank built into the image (or a synthesised Amen), then the player's */
static const bank_slot_t *slot_of(uint32_t i, const uint8_t **bank)
{
    uint32_t z;
    *bank = 0;
#if CH_HAVE_BANK
    if (i >= n_synth && i - n_synth < n_bank) {
        *bank = CH_BANK;
        return bank_check(CH_BANK, CH_BANK_SIZE, i - n_synth);
    }
#endif
    if (i < n_synth + n_bank)
        return 0;
    i -= n_synth + n_bank;
    for (z = 0; z < CU_NZ; i -= n_uz[z++])
        if (i < n_uz[z]) {
            *bank = plat_data(cu_at[z]);
            return bank_check(*bank, cu_cap[z], i);
        }
    return 0;
}
static void breaks_count(void)
{
    cu_scan();
    n_synth = n_bank = 0u;
#if CH_HAVE_BANK
    while (n_bank < BANK_SLOTS && bank_check(CH_BANK, CH_BANK_SIZE, n_bank))
        n_bank++;
#endif
    if (!n_bank)                         /* a build without a bank still has a break: a synthesised Amen */
        n_synth = 1u;
    n_breaks = n_synth + n_bank + n_user;
}

/* break i's name, for the screen before it is in */
static const char *break_name(uint32_t i)
{
    static char t[13];
    const uint8_t *bk;
    const bank_slot_t *s = slot_of(i, &bk);
    uint32_t k;
    for (k = 0; k < 12u; k++)
        t[k] = s ? s->name[k] : 0;
    t[12] = 0;
    return s ? t : brk0.name;
}

/* break i into pcm_a and the descriptor the render isn't using (whatever plays from pcm_a meanwhile is muted, see
 * break_poll); hand it to the render after */
static const brk_break_t *break_load(uint32_t i)
{
    const uint8_t *bk;
    const bank_slot_t *s = slot_of(i, &bk);
    brk_break_t *d = brk_get_break() == &brk_cur[0] ? &brk_cur[1] : &brk_cur[0];
    const brk_break_t *b = &brk0;
    uint32_t k;
    if (s) {
        adpcm_state_t st = {0, 0, 0};
        adpcm_decode(&st, bk + s->data_off, pcm_a, s->len);
        d->pcm = pcm_a;
        d->len = s->len;
        d->rate = s->rate;
        d->bpm_x100 = s->bpm_x100;
        d->bars = s->bars;
        d->nslices = s->nslices;
        for (k = 0; k <= s->nslices; k++)
            d->slice[k] = s->slice[k];
        for (k = 0; k < 12u; k++)
            d->name[k] = s->name[k];
        d->name[12] = 0;
        b = d;
    } else {                             /* a built-in style, played into RAM now */
        if (i >= n_synth)
            i = 0u;
#if !CH_HAVE_BANK                        /* (a bank build has no synthesised breaks: their code stays out) */
        if (brk_synth_style_len(i, CH_RATE) <= CH_PCM_MAX)
            brk_synth_style(pcm_a, CH_RATE, i, &brk0);
#endif
    }
    set.brk = (uint8_t)i;
    wave_build(b);
    return b;
}

void app_init(void)
{
    int ours;                            /* (AFFINE: the store is AFFINE's: its settings' magic) */
    brk_init();
    brk_fx_init();
    brk_fx_delay_buffer(dub_line, CH_DUB_LEN);
    psx_verb_init(spu_work, PSXV_WORDS); /* (AFFINE: the PSX bus's memory) */
    psx_skip_init(skip_ring, PSX_SKIP_FRAMES);
    kick_init();
    perc_init();
    ovl_init(ovl_line);
    breaks_count();                      /* the bank's (breaks/breaks.txt), then the player's own (0.9.32) */
    kit_scan();                          /* (0.9.45: the player's drum samples) */
    settings_defaults();                 /* (what a shorter, older save doesn't reach keeps these) */
    {
        int sz = plat_store_size(OBJ_SETTINGS);
        if (sz < (int)SET_SIZE_MIN || sz > (int)SET_SAVED || plat_store_load(OBJ_SETTINGS, &set, (uint32_t)sz) != sz)
            set.magic = 0u;
        if (set.magic == SET_MAGIC && !(set.skin & 0x80u))   /* (LOAF BREAKER, 0.237: a save from before the 1.0's
                                          * skins: its place in the 18 before, the nearest kept) */
            set.skin = (uint8_t)(0x80u | skin_old(set.skin));
        if (set.pxv != PX_OK) {          /* (LOAF BREAKER: a save from before the drums' extras: smem the MELODY's
                                          * leftovers, cleared) */
            memset(set.smem, 0, sizeof set.smem);
            set.pxv = PX_OK;
        }
        if (set.bkeys.bk[0] != BK_TAG || set.bkeys.bk[1] >= BKP_N)
            bk_defaults();               /* (LOAF BREAKER: a save from before the BLACK KEYS: the STAB's leftovers) */
    }
    /* AFFINE: AFFINE's own settings in the store, or this store is another firmware's (LOAF BREAKER, CHOPPAH write the
     * same addresses): then its patterns, steps, drums' bars and groups are not read either, the FM-1 starts fresh */
    ours = set.magic == SET_MAGIC;
    if (set.magic != SET_MAGIC || set.tempo_x10 < 400u || set.tempo_x10 > 3000u || set.swing < 50u ||
        set.swing > 75u) {
        settings_defaults();
        if (n_bank)
            set.brk = (uint8_t)n_synth;  /* the bank's first break */
    }
    if (set.brk >= n_breaks)
        set.brk = 0u;
    trk_defaults();
    pats.mem.fx_ok = 0u;   /* (an older save: no effect remembered) */
    {
        int sz = plat_store_size(OBJ_PAT);   /* (LOAF BREAKER: a save from before the tracks: its patterns) */
        if (!ours || (sz != (int)PATS_SAVED && sz != (int)PATS_SIZE_OLD && sz != (int)PATS_SIZE_TRK) ||
            plat_store_load(OBJ_PAT, &pats, (uint32_t)sz) != sz)
            pats.magic = 0u;
    }
    if (pats.magic != PAT_MAGIC && pats.magic != PAT2_MAGIC && pats.magic != PAT1_MAGIC)
        pat_defaults();
    dx_old = 0u;
    if (!ours || plat_store_load(OBJ_DX, &dx, sizeof dx) != (int)sizeof dx || dx.magic != DX_MAGIC) {
        memset(&dx, 0, sizeof dx);       /* (0.9.30: the drums' bars 3 and 4; a save before them: none) */
        dx.magic = DX_MAGIC;
        dx_old = 1u;
        dx_lens();
    }
    {                                    /* AFFINE: the black keys recorded; another firmware's store: none (and one
                                          * AFFINE left there before it: written over with nothing at the next SAVE) */
        int ok = plat_store_load(OBJ_FREE0, &fxr, sizeof fxr) == (int)sizeof fxr && fxr.magic == FXR_MAGIC;
        fxr_dirty = (uint8_t)(ok && !ours);
        if (!ok || !ours) {
            memset(&fxr, 0, sizeof fxr);
            fxr.magic = FXR_MAGIC;
        }
    }
    int ssz = plat_store_load(OBJ_STEP, &steps, sizeof steps), sok;
    sok = ours && (ssz == (int)sizeof steps || ssz == (int)STEPS_SIZE_OLD) && steps.magic == STEPS_MAGIC;
    if (!sok)
        steps_defaults();                /* (LOAF BREAKER: the drum steps' own settings) */
    else if (ssz == (int)STEPS_SIZE_OLD)
        memset(steps.tb, 0, sizeof steps.tb);   /* (a save before the tracks' breaks) */
    if (sok && pats.magic != PAT_MAGIC) {   /* an older save: its kick again over four bars, as it played */
        uint32_t i;
        for (i = 0; i < NPAT; i++) {
            pat_t *p = &pats.p[i];
            if (pats.magic == PAT1_MAGIC)    /* CHOPPAH: one bar */
                p->kick2 = p->lane[LANE_KICK];
            kick_bar_set(p, 2u, kick_bar(p, 0u));
            kick_bar_set(p, 3u, kick_bar(p, 1u));
        }
        pats.magic = PAT_MAGIC;
    }
    set.pgrp %= NGRP;                    /* LOAF BREAKER: the group saved with its own objects: those; none (a save from
                                          * before the 64): OBJ_PAT's, written to its own now (the sound not started) */
    if (ours && grp_has(set.pgrp))
        grp_load(set.pgrp);
    else {
        grp_sum = grp_hash();
        grp_new = (uint8_t)(ours && plat_store_size(OBJ_PAT) > 0);
        if (grp_new)
            (void)grp_save();
    }
    brk_set_break(break_load(set.brk));  /* boot: the audio isn't running yet */
    settings_apply();
    pat_apply();
}

/* ------------------------------------------------------- MIDI clock, Bluetooth --- */
#define PKT_RT(b) (0x0Fu | (uint32_t)(b) << 8)   /* a real-time byte as a USB-MIDI packet (CIN 0xF) */
#define MIDI_SLICE0 48u                  /* C3: slice 1 */
static void midi_slice(int s, int on);   /* (with the UI: they play as the keys, and REC records them) */
static void midi_drum(uint32_t lane);
#define MS_PER_SAMPLE (1000.0f / (float)BRK_SR)
#define CLKQ 64u
/* (times in plat_ms: the 1 kHz tick sends at that grain anyway, a beat of ticks averages the tempo, and the host
 * simulator keeps plat_ms in its own time) */
static volatile uint32_t clkq_due[CLKQ]; /* clock out: when each tick is due (plat_ms); the render writes them */
static volatile uint32_t clkq_w, clkq_r; /* and the 1 kHz tick sends them */
static volatile uint32_t sync_n16, sync_ms, sync_c;   /* clock in: the outside 16th begun, when, and a count */
static uint32_t sync_seen;
#define EXT_FIT 96u                      /* GLOUPI BREAKS: the ticks the tempo is fitted over, four beats */
static struct {
    uint32_t ring[EXT_FIT], i;           /* the last ticks' stamps (plat_ticks24's); ticks in the ring's run */
    uint32_t last, n;                    /* the last tick; ticks since the start (FA) */
    float bpm, used;                     /* the tempo they give, smoothed; the one the engine has */
    uint8_t run, port;                   /* started (FA / FB, not yet FC); where they come from */
} ext;

/* GLOUPI BREAKS: the clock's tempo and the newest tick's time, from a straight line through the ring's ticks (least
 * squares). The ticks are read in the main loop, late now and then by a screen drawn meanwhile (several at once): one
 * tick's time against another's a beat before swung the tempo by 2 BPM; the line through four beats of them barely
 * moves. LOAF BREAKER: the ring holds where each tick landed (plat_midi_in_t24, fm1.toml midi_stamp: 24 MHz, within
 * 0.05 ms of its landing), the line through them steadier still. Returns 0 until there are two beats' worth; *at: the
 * newest tick as the line has it (ms, from t24, the newest's own stamp) */
static int ext_fit(uint32_t t24, float *bpm, int32_t *at)
{
    uint32_t m = ext.i < EXT_FIT ? ext.i : EXT_FIT, k;
    float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f, n, per;
    if (m < 48u)
        return 0;
    for (k = 0; k < m; k++) {            /* x: the tick, oldest 0; y: its time, ms before the newest */
        float x = (float)k, y = -(float)(t24 - ext.ring[(ext.i - m + k) % EXT_FIT]) * (1.0f / 24000.0f);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }
    n = (float)m;
    per = (sxy - sx * sy / n) / (sxx - sx * sx / n);   /* ms a tick */
    if (per <= 0.0f)
        return 0;
    *bpm = 2500.0f / per;                /* 24 ticks a beat: 60000 / (24 per) */
    *at = (int32_t)(sy / n + per * ((n - 1.0f) - sx / n) + (sy < 0.0f ? -0.5f : 0.5f));
    return 1;
}

static uint32_t midi_dest(void)
{
    return set.midi_to == 1u ? TO_USB : set.midi_to == 2u ? TO_BLE : TO_USB | TO_BLE;
}

static int ext_active(void)              /* following an outside clock: on, and ticks in the last half second */
{
    return set.clk_in && ext.i && plat_ms() - ext.last < 500u;
}

/* PLAY: the transport, and with clock out on, MIDI start / stop after it. HOME + PLAY (restart): back to bar 1,
 * and while playing a start again */
static void transport(int on, int restart)
{
    if (restart)
        brk_reset();
    else
        brk_play(on);
    if (set.clk_out && (on || !restart))
        plat_midi_send(PKT_RT(on ? 0xFAu : 0xFCu), midi_dest());
}

int app_ble_at_boot(void) { return !set.ble_off; }

/* main loop, every pass: MIDI in. With clock in on, ticks set the tempo (a beat's worth, smoothed) and every beat
 * the loop's phase (brk_sync, in the render); start, continue and stop drive the transport */
void app_loop(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = pkt >> 8 & 0xFFu, t = plat_ms(), t24 = plat_midi_in_t24();   /* (t24: where it landed) */
        if ((st & 0xE0u) == 0x80u) {     /* notes, any channel: 48..63 (C3..D#4) the slices, held as the white keys;
                                          * 36 the kick, 38 the snare, 42 / 44 / 46 the hat (a drum machine's) */
            uint32_t note = pkt >> 16 & 0x7Fu, on = (st & 0xF0u) == 0x90u && (pkt >> 24 & 0x7Fu);
            if (note >= MIDI_SLICE0 && note < MIDI_SLICE0 + 16u)
                midi_slice((int)(note - MIDI_SLICE0), (int)on);
            else if (on && (note == 36u || note == 38u || note == 39u || note == 42u || note == 44u || note == 46u))
                midi_drum(note == 36u ? DR_KICK : note == 38u ? DR_SNARE : note == 39u ? DR_CLAP : DR_HAT);   /* (LOAF
                                          * BREAKER: 39 the clap) */
            continue;
        }
        if (!set.clk_in)
            continue;
        if (st == 0xF8u) {
            float bpm;
            int32_t at = 0;
            if (ext.i && t - ext.last > 250u)   /* the clock gone a while (stopped, unplugged): a fresh fit */
                ext.i = 0u;
            ext.ring[ext.i % EXT_FIT] = t24;
            ext.i++;
            ext.last = t;
            ext.port = (uint8_t)port;
            if (ext_fit(t24, &bpm, &at) && bpm >= 40.0f && bpm <= 300.0f) {
                float d = bpm - ext.bpm;     /* eased; a real change (over 2 BPM) followed at once */
                ext.bpm = ext.bpm > 0.0f ? ext.bpm + d * (d > 2.0f || d < -2.0f ? 0.5f : 0.05f) : bpm;
                d = ext.bpm - ext.used;
                if (d >= 0.05f || d <= -0.05f) {   /* the engine's tempo moved only past a twentieth of a BPM */
                    ext.used = ext.bpm;
                    brk_tempo(ext.bpm);
                    brk_fx_tempo(ext.bpm);
                    set.tempo_x10 = (uint16_t)(ext.bpm * 10.0f + 0.5f);   /* (shown; not a change to save) */
                }
            } else
                at = 0;
            if (ext.run) {               /* each beat: where the outside clock is, for the render */
                if (ext.n % 24u == 0u) {
                    sync_n16 = ext.n / 6u;
                    sync_ms = t - (plat_ticks24() - t24) / 24000u + (uint32_t)at;   /* (the beat's time as the
                                          * line has it, from where it landed: not when it was read) */
                    sync_c++;
                }
                ext.n++;
            }
        } else if (st == 0xFAu || st == 0xFBu) {   /* start (continue starts again too: no song position yet) */
            ext.n = 0u;
            ext.run = 1u;
            brk_play(0);
            brk_play(1);
            muted = 0u;
        } else if (st == 0xFCu) {
            ext.run = 0u;
            brk_play(0);
        }
    }
}

/* 1 kHz: the clock ticks the render marked, each sent at its moment */
void app_tick(void)
{
    uint32_t now = plat_ms();
    while (clkq_r != clkq_w && (int32_t)(now - clkq_due[clkq_r % CLKQ]) >= 0) {
        plat_midi_send(PKT_RT(0xF8u), midi_dest());
        clkq_r++;
    }
}

/* a meter: this block's peak, or the last one falling (about 20 dB a second) */
static float meter(const float *x, uint32_t n, float last)
{
    float pk = 0.0f;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float a = x[i] < 0.0f ? -x[i] : x[i];
        if (a > pk)
            pk = a;
    }
    last *= 0.987f;
    return pk > last ? pk : last;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    /* (LOAF BREAKER: the block's buffers in the pool, 10 KB of RAM freed: the pool is RAM as fast) */
    static float mono[APP_BLOCK] PLAT_POOL, l[APP_BLOCK] PLAT_POOL, r[APP_BLOCK] PLAT_POOL, echo[APP_BLOCK] PLAT_POOL,
                 verb[APP_BLOCK] PLAT_POOL;
    static float kick[APP_BLOCK] PLAT_POOL, perc[APP_BLOCK] PLAT_POOL;
    static float kl[APP_BLOCK] PLAT_POOL, kr[APP_BLOCK] PLAT_POOL;   /* LOAF BREAKER: the kick through the overlays
                                          * alone (THRU KICK) */
    static float spd[APP_BLOCK] PLAT_POOL, tape0 = 1.0f;   /* LOAF BREAKER: the VINYL BREAK's speed on the drums */
    static uint8_t b_on, b_drop, b_rew;  /* LOAF BREAKER: building; the drop asked; the drop armed for the next
                                          * step (the engine rewound to the pattern's first: brk_rewind) */
    static uint32_t b_steps;             /* steps built */
    static int32_t half_due = -1;        /* the roll's 32nd between two steps: samples from this block's start */
    static float crash;                  /* the drop's crash, its level */
    static uint8_t gap_now;              /* LOAF BREAKER: the GAP before the drop: silent now */
    int vin = (set.vinyl & 3u) != 1u;    /* (THRU BOTH or KICK: the drums slow too) */
    uint32_t thru = (set.ovl_pad >> 6) & 3u;   /* the ARP key's THRU: 0 BOTH, 1 BREAK, 2 KICK */
    int kick_dry = thru != 0u;           /* the kick apart from the chain (BREAK: around the overlays too) */
    float gain = muted ? 0.0f : (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f *
                 brk_exp2f(-(float)max_out_db() * 0.16610f);   /* (LOAF BREAKER: MAX OUT's headroom) */
    brk_status_t st;
    uint32_t t0 = plat_ms(), done = 0u, total = n;
    if (sync_c != sync_seen) {           /* the outside clock's beat, and how long ago it came */
        float age = (float)(t0 - sync_ms) / MS_PER_SAMPLE;
        sync_seen = sync_c;
        if (age < (float)BRK_SR * 0.25f)
            brk_sync(sync_n16, age);
    }
    while (n) {
        uint32_t m = n < APP_BLOCK ? n : APP_BLOCK, i, ngev = 0u;
        uint16_t gev_at[4];              /* (the GAP: where it begins or ends in this block) */
        uint8_t gev_v[4], gap_was = gap_now;
        brk_sends(echo, verb);           /* the slices' throws into the delay and the reverb */
        brk_render(mono, m);
        brk_status(&st);
        {                                /* the MIXER's BREAK fader (eased: no zipper), on the break and its throws */
            static float lvl = 1.0f;
            float want = (float)set.brk_level * 0.01f;
            for (i = 0; i < m; i++) {
                lvl += (want - lvl) * 0.002f;
                mono[i] *= lvl;
                echo[i] *= lvl;
                verb[i] *= lvl;
            }
            meter_brk = meter(mono, m, meter_brk);
        }
        if (set.clk_out) {               /* clock out: each tick due as its sample plays (a block from now) */
            uint16_t off[16];
            uint32_t k, nt = brk_clock_ticks(off, 16u);
            for (k = 0; k < nt && clkq_w - clkq_r < CLKQ; k++) {
                clkq_due[clkq_w % CLKQ] = t0 + (uint32_t)((float)(done + off[k] + total) * MS_PER_SAMPLE + 0.5f);
                clkq_w++;
            }
        }
        done += m;
        {                                /* the drums: on the steps of the bar their lanes have, at their samples */
            uint16_t off[8], ka[8], pa[8];
            float kpm[8];                /* (crossbreed: each kick's note, a factor) */
            uint8_t idx[8], who[8];
            uint32_t k, ne = brk_step_events(off, idx, 8u), nk = 0u, np = 0u;
            static int32_t kr_next = -1;     /* (the KICK ROLL's next hit, samples from this block's start; -1:
                                          * waiting for a step) */
            static uint8_t kr_seen;
            uint32_t krp = kroll;
            {                            /* LOAF BREAKER: three clicks: a build from the next bar (none: the drop
                                          * at this bar's end), or (building) the drop at the bar */
                static uint8_t seen;
                if (build_n != seen) {
                    seen = build_n;
                    if (b_on)
                        b_drop = 1u;
                    else if (st.playing)
                        build_wait = 1u;
                }
            }
            if (!st.playing)
                b_on = b_drop = b_rew = build_wait = gap_now = 0u;   /* (stopped: no build) */
            if (b_on && half_due >= 0 && half_due < (int32_t)m) {   /* (the 32nd between two steps) */
                pa[np] = (uint16_t)half_due;
                who[np++] = 1u << PV_SNARE;
            }
            if (half_due >= 0 && (half_due -= (int32_t)m) < 0)
                half_due = -1;
            if (krp != kr_seen) {        /* (a roll pad down or up: from its next step) */
                kr_seen = (uint8_t)krp;
                kr_next = -1;
            }
            for (k = 0; k < ne; k++) {
                uint32_t s = idx[k] & 15u;   /* the step's place in the bar (idx: in four bars, the kick's) */
                if (!idx[k])
                    pat_cycle++;
                if (build_wait && s == 0u && build_bars) {   /* the build asked for: from this bar's first step */
                    build_wait = 0u;
                    b_on = 1u;
                    b_drop = b_rew = 0u;
                    b_steps = 0u;
                    build_len = 16u * build_bars;
                    half_due = -1;
                }
                if (b_on) {              /* LOAF BREAKER: the build's roll in the drums' place */
                    if (b_rew && s == 0u) {   /* the drop: the new pattern's first step (rewound at the bar's
                                          * last), everything back, its drums at once */
                        uint32_t dd, bb;
                        if (gap_now && ngev < 4u) {   /* (the GAP ends with the drop) */
                            gev_at[ngev] = off[k];
                            gev_v[ngev++] = 0u;
                            gap_now = 0u;
                        }
                        b_on = b_drop = b_rew = 0u;
                        half_due = -1;
                        if (drum_next_ok)
                            for (dd = 0; dd < NDRUMS; dd++)
                                for (bb = 0; bb < 4u; bb++)
                                    drum_now[dd][bb] = drum_next[dd][bb];
                        drum_next_ok = 0u;
                        drop_n++;
                        crash = drop_gain;
                    } else if (build_len) {
                        uint32_t q4 = b_steps * 4u / build_len, div = q4 < 1u ? 4u : q4 < 2u ? 2u : 1u, q;
                        uint32_t sty = b_style, rem = b_drop ? 16u - s : build_len - b_steps;
                        if (b_gap && rem <= b_gap && !gap_now && ngev < 4u) {   /* (LOAF BREAKER: the GAP: silence
                                          * till the drop) */
                            gev_at[ngev] = off[k];
                            gev_v[ngev++] = 1u;
                            gap_now = 1u;
                        }
                        if (!(b_steps % div) && sty <= DS_BOTH) {   /* (quarters, eighths, 16ths, 32nds: a quarter
                                          * each; LOAF BREAKER: the snare's, the kick's, or the two in turn) */
                            int kick = sty == DS_KICK || (sty == DS_BOTH && !((b_steps / div) & 1u));
                            if (kick && nk < 8u) {
                                kpm[nk] = 1.0f;
                                ka[nk++] = off[k];
                            } else if (!kick && np < 8u) {
                                pa[np] = off[k];
                                who[np++] = 1u << PV_SNARE;
                                if (q4 >= 3u)
                                    half_due = (int32_t)off[k] + (int32_t)((float)BRK_SR * 7.5f / brk_get_tempo());
                            }
                        }
                        if (b_steps < build_len)
                            b_steps++;
                        if (s == 15u && (b_drop || b_steps >= build_len)) {   /* the bar's last step: the next
                                          * is the drop, the new pattern's step 1 (its break's steps in now) */
                            b_rew = 1u;
                            brk_rewind();
                            if (drop_to < NPAT)
                                for (q = 0; q < PAT_STEPS; q++)
                                    pat_step_of(drop_to, q);
                        }
                        if (sty != DS_SWEEP)     /* (SWEEP: the drums play on under it) */
                            continue;
                    }
                } else if (build_wait && !build_bars && s == 15u) {   /* no build: this bar plays out, the drop
                                          * on the next step (the new pattern's first) */
                    uint32_t q;
                    build_wait = 0u;
                    b_on = b_rew = 1u;
                    b_drop = 0u;
                    build_len = 0u;
                    brk_rewind();
                    if (drop_to < NPAT)
                        for (q = 0; q < PAT_STEPS; q++)
                            pat_step_of(drop_to, q);
                }
                if (krp && kr_next < 0)  /* (the KICK ROLL: from this step) */
                    kr_next = (int32_t)off[k];
                uint32_t w = 0u, d, bar = idx[k] >> 4 & 3u;
                for (d = DR_SNARE; d < NDRUMS; d++)   /* (LOAF BREAKER: every drum, its bar) */
                    w |= (uint32_t)(drum_now[d][bar] >> s & 1u) << (d - 1u);
                if (drum_now[DR_KICK][bar] >> s & 1u && !krp) {   /* (a KICK ROLL: its hits instead) */
                    const pat_t *pp = &pats.p[set.pattern % NPAT];   /* (its note: this step of its bar) */
                    uint32_t ks = (bar % drum_bars(pp, DR_KICK)) * 16u + s;
                    int32_t kn = kick_note(pp, ks);
                    float f = kn ? brk_exp2f((float)kn * (1.0f / 12.0f)) : 1.0f;
                    if (step_hit(set.pattern, DR_KICK, ks, (int32_t)off[k],   /* (LOAF BREAKER: the step's CHANCE,
                                          * NUDGE, HITS) */
                                 (int32_t)((float)BRK_SR * 15.0f / brk_get_tempo()), 1u, 0u, f) && nk < 8u) {
                        kpm[nk] = f;
                        ka[nk++] = off[k];
                    }
                }
                if (w && np < 8u) {      /* (LOAF BREAKER: each drum's step, its CHANCE, NUDGE, HITS) */
                    uint32_t dd2, now = 0u;
                    int32_t len = (int32_t)((float)BRK_SR * 15.0f / brk_get_tempo());
                    const pat_t *pp = &pats.p[set.pattern % NPAT];
                    for (dd2 = DR_SNARE; dd2 < NDRUMS; dd2++)
                        if (w >> (dd2 - 1u) & 1u &&
                            step_hit(set.pattern, dd2, (bar % drum_bars(pp, dd2)) * 16u + s, (int32_t)off[k], len, 0u,
                                     (uint8_t)(1u << (dd2 - 1u)), 1.0f))
                            now |= 1u << (dd2 - 1u);
                    if (now) {
                        pa[np] = off[k];
                        who[np++] = (uint8_t)now;
                    }
                }
            }
            {                            /* LOAF BREAKER: the HITS due in this block, in with the steps' (in order) */
                uint32_t q = 0u, a, b;
                if (!st.playing)
                    hq_n = 0u;
                while (q < hq_n) {
                    if (hq[q].at < (int32_t)m) {
                        if (hq[q].kick && nk < 8u) {
                            kpm[nk] = hq[q].pm;
                            ka[nk++] = (uint16_t)(hq[q].at < 0 ? 0 : hq[q].at);
                        } else if (!hq[q].kick && np < 8u) {
                            pa[np] = (uint16_t)(hq[q].at < 0 ? 0 : hq[q].at);
                            who[np++] = hq[q].who;
                        }
                        hq[q] = hq[--hq_n];
                    } else {
                        hq[q].at -= (int32_t)m;
                        q++;
                    }
                }
                for (a = 1; a < nk; a++)         /* (the kick's hits rising) */
                    for (b = a; b > 0u && ka[b - 1u] > ka[b]; b--) {
                        uint16_t to = ka[b];
                        float tp = kpm[b];
                        ka[b] = ka[b - 1u];
                        kpm[b] = kpm[b - 1u];
                        ka[b - 1u] = to;
                        kpm[b - 1u] = tp;
                    }
                for (a = 1; a < np; a++)         /* (the others' rising) */
                    for (b = a; b > 0u && pa[b - 1u] > pa[b]; b--) {
                        uint16_t to = pa[b];
                        uint8_t tw = who[b];
                        pa[b] = pa[b - 1u];
                        who[b] = who[b - 1u];
                        pa[b - 1u] = to;
                        who[b - 1u] = tw;
                    }
            }
            if (kick_hold)
                nk = 0u;                 /* LOAF BREAKER: KIL held: the sequencer's kicks quiet (a roll still plays) */
            if (krp && st.playing) {     /* the KICK ROLL's hits in this block, at its pad's rate */
                static const float RS[BRK_ROLL_DIVS] = {4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 4.0f / 3.0f, 2.0f / 3.0f,
                                                        1.0f / 3.0f, 6.0f, 3.0f, 1.5f};   /* (16ths: brk.c's ROLL_STEPS) */
                int32_t per = (int32_t)(RS[set.pads.roll_div[(krp - 1u) % 3u] % BRK_ROLL_DIVS] * (float)BRK_SR * 15.0f /
                                        brk_get_tempo());
                nk = 0u;                 /* (the roll, alone, in order) */
                while (kr_next >= 0 && kr_next < (int32_t)m && nk < 8u && per > 0) {
                    kpm[nk] = 1.0f;
                    ka[nk++] = (uint16_t)kr_next;
                    kr_next += per;
                }
                if (kr_next >= (int32_t)m)
                    kr_next -= (int32_t)m;
            }
            if (vin && (st.pads >> PAD_TAPESTOP & 1u))
                nk = np = 0u;            /* the vinyl held: the drums' steps quiet, as the break's */
            int vs = vin && (tape0 < 1.0f || st.tape < 1.0f);
            if (vs)
                for (i = 0; i < m; i++)  /* (the engine's ramp, this block's end; between, straight) */
                    spd[i] = tape0 + (st.tape - tape0) * (float)(i + 1u) / (float)m;
            tape0 = st.tape;
            kick_speed(vs ? spd : 0);
            perc_speed(vs ? spd : 0);
            kick_hit_n += nk;
            kick_pitches(kpm);
            kick_render(kick, m, ka, nk);   /* (AFFINE: BOOM, straight: LOAF BREAKER's KICK EQ and KICK FX gone) */
            perc_render(perc, m, pa, who, np);
            if (set.duck) {              /* LOAF BREAKER (crossbreed): DUCK: the break (and its throws; DRUMS: the
                                          * other drums too) pushed down at each kick, in its attack, back up over its
                                          * release (KNOB) */
                static float dt, de;
                float depth = (float)(set.duck > 100u ? 100u : set.duck) * 0.01f;
                float da = 1.0f / (1.0f + att_ms(set.duck_att) * 44.1f), dr = 1.0f - 1.0f / (rel_ms(set.duck_rel) * 44.1f);
                int dp = set.duck_what == 1u;
                uint32_t j = 0u;
                for (i = 0; i < m; i++) {
                    float g;
                    while (j < nk && ka[j] <= i) {
                        dt = 1.0f;
                        j++;
                    }
                    de += (dt - de) * da;
                    dt *= dr;
                    g = 1.0f - depth * de;
                    mono[i] *= g;
                    echo[i] *= g;
                    verb[i] *= g;
                    if (dp)
                        perc[i] *= g;
                }
            }
            brk_fx_drums_dj(kick, perc, m, (float)set.kick_dj * 0.01f);   /* HOME's DRUM FLT (LOAF BREAKER: every drum,
                                          * not the break) */
            build_p = b_on ? (build_len ? (float)b_steps / (float)build_len : 1.0f) : -1.0f;
            if (b_on && build_len) {     /* (the roll louder as it builds) */
                float g = 0.35f + 0.65f * build_p;
                for (i = 0; i < m; i++)
                    perc[i] *= g;
            }
            meter_kick = meter(kick, m, meter_kick);
            meter_perc = meter(perc, m, meter_perc);
            if (!kick_dry)               /* (LOAF BREAKER: THRU BREAK or KICK, the kick joins after the chain) */
                for (i = 0; i < m; i++)
                    kick[i] += perc[i];
        }
        if ((gap_was || ngev) && b_gapm) {   /* (0.9.26: a GAP that keeps something: the others quiet here, before
                                          * the chain, in ~1 ms, as the whole mix below otherwise) */
            static float gk = 1.0f;
            uint32_t j = 0u, on = gap_was, km = b_gapm;
            for (i = 0; i < m; i++) {
                while (j < ngev && gev_at[j] <= i)
                    on = gev_v[j++];
                gk += ((on ? 0.0f : 1.0f) - gk) * 0.02f;
                if (km != 1u) {          /* (DRUMS, ECHOES: the break and its throws) */
                    mono[i] *= gk;
                    echo[i] *= gk;
                    verb[i] *= gk;
                }
                if (km != 2u) {          /* (BREAK, ECHOES: the kick and the drums) */
                    kick[i] *= gk;
                    perc[i] *= gk;
                }
            }
        }
        /* the break through the DJ filter, the drums joining it into the chain (or around it) */
        {
            const float *drums = kick_dry ? perc : kick;   /* (the kick apart: the snare and the hat alone here) */
            brk_fx_late_limit((uint32_t)kick_dry);   /* (0.9.31: then the limiter after the kick joins, brk_fx_add_dry) */
            brk_fx_process(mono, set.drums_chain ? drums : 0, echo, verb, set.drums_chain ? 0 : drums, l, r, m,
                           st.pads);
        }
        {                                /* the overlays (ARP's white keys), on the whole mix: on the engine's beat while
                                          * playing, a clock of their own stopped */
            static float beat;
            float spb = (float)BRK_SR * 600.0f / (float)(set.tempo_x10 ? set.tempo_x10 : 1700u);
            if (st.playing)
                beat = (float)(st.bar % 64u) * 4.0f + ((float)(st.bar16 < 0 ? 0 : st.bar16) + st.phase) * 0.25f;
            if (thru == 2u) {            /* LOAF BREAKER: THRU KICK, the overlays on the kick alone, the rest clean */
                for (i = 0; i < m; i++)
                    kl[i] = kr[i] = kick[i];
                ovl_process(kl, kr, m, beat, spb);
                brk_fx_add_dry(l, r, kl, kr, m);
            } else {
                ovl_process(l, r, m, beat, spb);
                if (kick_dry)            /* THRU BREAK: the kick around the chain and the overlays, clean */
                    brk_fx_add_dry(l, r, kick, kick, m);
            }
            if (!st.playing) {
                beat += (float)m / spb;
                if (beat >= 256.0f)
                    beat -= 256.0f;
            }
        }
        if ((b_on && build_len) || crash > 1e-4f) {   /* LOAF BREAKER: the build's high-pass and riser over
                                          * everything; the drop's crash */
            static float h1l, h1r, h2l, h2r, cl;
            static uint32_t nz = 0x1234567u;
            int bu = b_on && build_len && !(gap_now && b_gapm), sw = b_sweep;   /* (LOAF BREAKER: its filter: 0 HP, 1 LP, 2
                                          * none; 0.9.26: a GAP that keeps something, heard open) */
            float p = bu ? build_p : 0.0f, rz = gap_now ? 0.0f : 0.22f * p * p * b_riser;   /* (0.9.26: the riser quiet in
                                          * the GAP whatever it keeps) */
            float kk = brk_clampf(sw == 1 ? 6.2832f * 18000.0f * brk_exp2f(-p * 5.5f) / (float)BRK_SR
                                          : 6.2832f * 30.0f * brk_exp2f(p * 6.5f) / (float)BRK_SR, 0.0f, 0.9f);
            for (i = 0; i < m; i++) {
                float n;
                nz = nz * 1664525u + 1013904223u;
                n = (float)(int32_t)nz * (1.0f / 2147483648.0f);
                if (bu && sw == 0) {     /* (30 Hz up to ~2.7 kHz, two poles) */
                    float xl = l[i] + n * rz, xr = r[i] + n * rz;
                    h1l += (xl - h1l) * kk;
                    xl -= h1l;
                    h2l += (xl - h2l) * kk;
                    l[i] = xl - h2l;
                    h1r += (xr - h1r) * kk;
                    xr -= h1r;
                    h2r += (xr - h2r) * kk;
                    r[i] = xr - h2r;
                } else if (bu && sw == 1) {   /* (LOW-PASS: 18 kHz down to ~400 Hz, two poles) */
                    h1l += (l[i] + n * rz - h1l) * kk;
                    h2l += (h1l - h2l) * kk;
                    l[i] = h2l;
                    h1r += (r[i] + n * rz - h1r) * kk;
                    h2r += (h1r - h2r) * kk;
                    r[i] = h2r;
                } else if (bu) {         /* (no filter: the noise alone) */
                    l[i] += n * rz;
                    r[i] += n * rz;
                }
                if (crash > 1e-4f) {     /* the crash: noise above ~4 kHz, a long fall */
                    cl += (n - cl) * 0.45f;
                    l[i] += (n - cl) * crash * 0.3f;
                    r[i] += (n - cl) * crash * 0.3f;
                    crash *= 0.99993f;
                }
            }
        }
        if (lr_test) {                   /* the left / right test: a low beep on the left, then a high one on the
                                          * right, twice (4 s), over everything else */
            static float ph;
            uint32_t left = lr_test;
            for (i = 0; i < m; i++) {
                uint32_t t = LR_TEST_LEN - left, seg = t / (LR_TEST_LEN / 4u);   /* 0 L, 1 R, 2 L, 3 R */
                float on = (t % (LR_TEST_LEN / 4u)) < (LR_TEST_LEN / 4u) * 3u / 4u ? 0.5f : 0.0f;
                float x = on * brk_sin_turns(ph);
                ph += (seg & 1u ? 880.0f : 440.0f) / (float)BRK_SR;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                l[i] = seg & 1u ? 0.0f : x;
                r[i] = seg & 1u ? x : 0.0f;
                if (left > 1u)
                    left--;
            }
            lr_test = left > 1u ? left : 0u;
        }
        if ((gap_was || ngev) && !b_gapm) {   /* LOAF BREAKER: the GAP, silence (in ~1 ms) till the drop, the crash on it */
            static float gg = 1.0f;
            uint32_t j = 0u, on = gap_was;
            for (i = 0; i < m; i++) {
                while (j < ngev && gev_at[j] <= i)
                    on = gev_v[j++];
                gg += ((on ? 0.0f : 1.0f) - gg) * 0.02f;
                l[i] *= gg;
                r[i] *= gg;
            }
        }
        if (set.comp) {                  /* LOAF BREAKER: COMP (SEL + K3, ENV's KNOB): the whole mix over its threshold
                                          * (0 .. -36 dB) brought down by its ratio, the level followed both sides at
                                          * once (its attack, its release), half the reduction made up; then held
                                          * under full scale */
            static float cenv;
            float thr = -0.36f * (float)(set.comp > 100u ? 100u : set.comp), sl = 1.0f - 1.0f / COMP_RATIO[set.comp_ratio & 7u];
            float ca = 1.0f / (1.0f + att_ms(set.comp_att) * 44.1f), cr = 1.0f / (1.0f + rel_ms(set.comp_rel) * 44.1f);
            float mk = -thr * sl * 0.5f, gmax = 0.0f;
            for (i = 0; i < m; i++) {
                float a = brk_absf(l[i]) > brk_absf(r[i]) ? brk_absf(l[i]) : brk_absf(r[i]), over, gr, g;
                cenv += (a - cenv) * (a > cenv ? ca : cr);
                over = cenv > 1e-5f ? 6.0206f * brk_log2f(cenv) - thr : -1.0f;
                gr = over > 0.0f ? over * sl : 0.0f;
                if (gr > gmax)
                    gmax = gr;
                g = brk_exp2f((mk - gr) * 0.16610f);
                l[i] *= g;               /* (held under full scale by the limiter after) */
                r[i] *= g;
            }
            comp_gr = gmax > comp_gr * 0.97f ? gmax : comp_gr * 0.97f;
        } else
            comp_gr = 0.0f;
        {                                /* LOAF BREAKER: the last guard, a look-ahead limiter: the mix delayed 64 samples
                                          * (1.5 ms) while its level, seen ahead, sets the gain, which comes down smoothly
                                          * before a peak reaches the output (no sudden steps: they were the distortion
                                          * heard with many sounds at once) and goes back up over ~150 ms (slow enough
                                          * not to wobble the bass); what still passes 0.95 rounded off softly under 1 */
            static float ll[LIM_LA], lr[LIM_LA], lenv, lg = 1.0f;
            static uint32_t lw;
            for (i = 0; i < m; i++) {
                float a = brk_absf(l[i]) > brk_absf(r[i]) ? brk_absf(l[i]) : brk_absf(r[i]), tgt, dl, dr;
                if (a > lenv)
                    lenv = a;            /* (seen ahead: at once) */
                else
                    lenv += (a - lenv) * 0.00015f;   /* (~150 ms back) */
                tgt = lenv > 0.95f ? 0.95f / lenv : 1.0f;
                lg += (tgt - lg) * (tgt < lg ? 0.078f : 0.0003f);   /* (down within the look-ahead, up slowly) */
                dl = ll[lw];
                dr = lr[lw];
                ll[lw] = l[i];
                lr[lw] = r[i];
                lw = (lw + 1u) % LIM_LA;
                l[i] = lim_soft(dl * lg);
                r[i] = lim_soft(dr * lg);
            }
        }
        if (scope_on) {                  /* LOAF BREAKER: the CDJ view's columns (scope), from what leaves */
            static float l1, l2, pk[4];
            static uint32_t sn;
            uint32_t len = 441000u / (set.tempo_x10 >= 400u ? set.tempo_x10 : 1700u), k;
            for (i = 0; i < m; i++) {
                float x = 0.5f * (l[i] + r[i]), v[4];
                l1 += (x - l1) * 0.0285f;        /* (~200 Hz) */
                l2 += (x - l2) * 0.356f;         /* (~2.5 kHz) */
                v[0] = brk_absf(l1);
                v[1] = brk_absf(l2 - l1);
                v[2] = brk_absf(x - l2);
                v[3] = brk_absf(l[i]) > brk_absf(r[i]) ? brk_absf(l[i]) : brk_absf(r[i]);
                for (k = 0; k < 4u; k++)
                    if (v[k] > pk[k])
                        pk[k] = v[k];
                if (++sn >= len) {
                    static const float G[4] = {300.0f, 380.0f, 520.0f, 255.0f};   /* (the bands as heard: the highs
                                          * up, they are quieter) */
                    uint8_t *c = scope[scope_w % SCOPE_N];
                    for (k = 0; k < 4u; k++) {
                        c[k] = (uint8_t)brk_clampf(pk[k] * G[k], 0.0f, 255.0f);
                        pk[k] = 0.0f;
                    }
                    sn = 0u;
                    scope_w++;
                }
            }
        }
        {                                /* the header's meter: as it leaves, after MASTER */
            float out = gain * (1.0f / 8388608.0f), a = meter(l, m, 0.0f) * out, b = meter(r, m, 0.0f) * out;
            meter_l = a > meter_l * 0.987f ? a : meter_l * 0.987f;
            meter_r = b > meter_r * 0.987f ? b : meter_r * 0.987f;
            if (a > 0.001f || b > 0.001f)
                out_sound = 1u;
        }
        for (i = 0; i < m; i++) {
            lr[2u * i] = (int32_t)(l[i] * gain);
            lr[2u * i + 1u] = (int32_t)(r[i] * gain);
        }
        lr += 2u * m;
        n -= m;
    }
}

/* (the audio interrupt, right after a render that took over 90 % of its time: PRD E-7) */
void app_overload(void) { brk_fx_overload(); }

void app_panic(void)
{
    muted = 1u;
    brk_play(0);
}

/* ----------------------------------------------------------------------- UI --- */
#include "ui/affont.c"                /* AFFINE: every page in its pixel font (before the pages) */
#include "ui/common.c"
#include "ui/fx.c"
#include "ui/pads.c"
#include "ui/keys.c"
#include "ui/bta.c"
#include "ui/global.c"
#include "ui/ovl.c"
#include "ui/seq.c"
#include "ui/kick.c"
#include "ui/perc.c"
#include "ui/mixer.c"
#include "ui/track.c"                 /* (LOAF BREAKER: ENV's TRACKS, PLAY, DROP) */
#include "ui/tutor.c"                 /* (LOAF BREAKER: the TUTORIAL) */
#include "ui/screen.c"
#if CH_DEMO
#include "ui/demo.c"
#else                                    /* GLOUPI BREAKS: no demo (ui/demo.c left out): what main.c calls of it, empty */
static const struct { uint8_t on; } demo = {0u};
static void demo_start(uint32_t now) { (void)now; }
static void demo_end(void) {}
static void demo_frame(uint32_t now) { (void)now; }
static void demo_leds(uint32_t now) { (void)now; }
#endif
#include "ui/cdj.c"                   /* (LOAF BREAKER: HOME held: the break scrolling, as on a CDJ) */
#include "ui/aff.c"                   /* AFFINE: the POLYGON look: its palette, its pixel font, HOME's picture */
#include "ui/psxp.c"                  /* AFFINE: the PSX page, EFFECTS' fifth tab */
#include "ui/main.c"
#undef C_WHITE                           /* (gfx.c's white again after the UI: skins, common.c) */
#define C_WHITE 0xFFFFu
#undef C_BLACK                           /* (and its black: AFFINE's ground is the skin's in the UI only) */
#define C_BLACK 0x0000u
