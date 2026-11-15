/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE's look (POLYGON, the owner's pick, 2026-11-15): its palette, its pixel font and HOME's picture.
 *   The page: night, ordered dither (a 4x4 Bayer matrix), one hot colour for what plays now, one cool for amounts.
 *   HOME: the break's 16 slices as a floor of tiles seen in perspective, drawn as the PS1 drew textures, without
 *   perspective correction (affine: the checker bends along each triangle's diagonal), in four strips as games cut
 *   their floors; it scrolls a row a step, the slice playing lit. Above it a low-poly gem, its vertices snapped to
 *   whole pixels (it jitters as it turns), kicked by each new slice. Under it the four gestures, two by two.
 * Drawn in bands of the canvas's rows (240 x 60): aff_home_band(h, top) draws the screen's rows top .. top + 59 with
 * cv_rect / cv_pset only. The font is app/aff_font.h (MONO2K's, CC BY-SA 4.0), whole-pixel scales. */

#define A_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* the palette: each colour one job. In the firmware the skin's (common.c: POLYGON's by default); off the device
 * (tests/look/home.c) POLYGON's own */
#ifdef NSKINS
#define A_BG0 C_BLACK                        /* night: the page (the skin's ground) */
#define A_BG1 C_BLOCK                        /* the cards, empty bars */
#define A_INK C_WHITE                        /* text, values */
#define A_DIM C_GRAY                         /* labels, rules */
#define A_HOT C_HI                           /* what plays now (one colour, one job) */
#define A_COOL C_AMB                         /* the effects' amounts */
#else
#define A_BG0 A_RGB(6, 8, 20)
#define A_BG1 A_RGB(24, 22, 52)
#define A_INK A_RGB(236, 230, 214)
#define A_DIM A_RGB(120, 118, 150)
#define A_HOT A_RGB(255, 92, 40)
#define A_COOL A_RGB(70, 214, 200)
#endif

static const uint8_t A_BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/* a ramp: n colours from a to b (RGB 8-bit triples), the dither picks between neighbours */
#define A_RAMP 5u
typedef struct { uint16_t c[A_RAMP]; } a_ramp_t;
static void a_ramp3(a_ramp_t *r, const uint8_t *a, const uint8_t *b)
{
    uint32_t i, k;
    for (i = 0; i < A_RAMP; i++) {
        uint32_t ch[3];
        for (k = 0; k < 3u; k++)
            ch[k] = (uint32_t)((int32_t)a[k] + ((int32_t)b[k] - (int32_t)a[k]) * (int32_t)i / (int32_t)(A_RAMP - 1u));
        r->c[i] = A_RGB(ch[0], ch[1], ch[2]);
    }
}
static void a_mix3(uint8_t *o, const uint8_t *a, const uint8_t *b, int32_t pct)   /* o = a + (b - a) x pct % */
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        o[k] = (uint8_t)((int32_t)a[k] + ((int32_t)b[k] - (int32_t)a[k]) * pct / 100);
}
/* t 0..256 to a ramp's colour at pixel (x, y): the step below or above by the Bayer matrix */
static inline uint16_t a_dith(const a_ramp_t *r, int32_t t, int32_t x, int32_t y)
{
    int32_t v = t * (int32_t)(A_RAMP - 1u), i;
    if (v <= 0)
        return r->c[0];
    if (v >= 256 * (int32_t)(A_RAMP - 1u))
        return r->c[A_RAMP - 1u];
    i = v >> 8;
    return r->c[i + ((v & 255) >> 4 > A_BAYER[y & 3][x & 3] ? 1 : 0)];
}
static a_ramp_t ar_sky, ar_floor0, ar_floor1, ar_lane, ar_lane2, ar_gem_hi, ar_gem_lo, ar_bar;
static uint8_t a_ready;
static uint32_t a_skin = 0xFFu;          /* (the skin the ramps were made for) */
/* the dither ramps from the palette: the sky from the ground to the cards, the floor's two tiles, the lane and the
 * gem from the hot colour, the gauges from the cards to the amounts' colour */
static void aff_init(void)
{
    static const uint8_t POLY[8][3] = {{40, 36, 84}, {84, 80, 130}, {120, 118, 150}, {70, 214, 200}, {255, 92, 40},
                                       {24, 22, 52}, {236, 230, 214}, {6, 8, 20}};
    const uint8_t (*c)[3] = POLY;
    uint8_t t1[3], t2[3];
#ifdef NSKINS
    c = SKIN_RGB[skin_idx()];
    a_skin = skin_idx();
#endif
    a_ramp3(&ar_sky, c[7], c[5]);
    a_ramp3(&ar_floor0, c[7], c[0]);
    a_mix3(t1, c[0], c[1], 50);
    a_ramp3(&ar_floor1, c[7], t1);
    a_ramp3(&ar_lane, c[7], c[4]);
    a_mix3(t1, c[4], c[6], 40);
    a_ramp3(&ar_lane2, c[7], t1);
    a_mix3(t1, c[4], c[7], 55);
    a_mix3(t2, c[4], c[6], 45);
    a_ramp3(&ar_gem_hi, t1, t2);
    a_mix3(t1, c[4], c[7], 85);
    a_mix3(t2, c[4], c[7], 20);
    a_ramp3(&ar_gem_lo, t1, t2);
    a_ramp3(&ar_bar, c[5], c[3]);
    a_ready = 1u;
}
static void aff_check(void)              /* (the ramps made again after the skin changes) */
{
#ifdef NSKINS
    if (!a_ready || a_skin != skin_idx())
        aff_init();
#else
    if (!a_ready)
        aff_init();
#endif
}

/* (the font: app/ui/affont.c, included before the pages: every page writes in it) */

/* ------------------------------------------------------------------------------------------------- HOME --- */
typedef struct {
    const char *brk;                     /* the break's name */
    uint32_t bpm_x10;
    int32_t slice;                       /* sounding, -1 none */
    uint32_t pat;                        /* the pattern, 0..63 */
    uint32_t bar, beat;                  /* for the position: bar (1..), beat (1..4) */
    uint32_t steps;                      /* steps played (the floor scrolls a row a step) */
    float phase;                         /* how far into the step, 0..1 */
    float g[4];                          /* the four knobs' gauges, 0..1 (-1: none; bipolar ones -1..1) */
    const char *gl[4];                   /* their names */
    const char *gvp[4];                  /* their values as words */
    uint8_t bip;                         /* bit i: knob i drawn from the middle (FILTER) */
    const char *pad;                     /* a pad held: its name (0: none) */
    const char *msg0, *msg1;             /* a message (app_say) for a moment: in the header's first line (0: none) */
    uint32_t t_ms;                       /* the clock (the gem turns with it) */
    uint32_t kick_ms;                    /* when the slice last changed (the gem's jolt) */
    uint8_t playing;
    /* what the performance does, for the picture (aff_tick): the output's level, the gestures as they sound (a key
     * held included), the engine's pads, the roll's rate, the vinyl's speed, the gate's level, a skip sounding */
    float level;                         /* the output's peak now, 0..1 */
    float crush, redux, send, dj;        /* 0..1, 0..1, 0..1, -1..1 (the DJ filter where it is, a throw too) */
    uint32_t pads;                       /* bit PAD_*: active */
    uint8_t roll_div, skip;              /* the roll's rate (a roll held), a skip sounding */
    float tape, gate;                    /* 1 running .. 0 stopped; the gate's level now */
} aff_home_t;

#define AH_HORIZ 110                     /* the horizon's row: the vanishing point at (AH_VX, AH_HORIZ) */
#define AH_VX 120
#define AH_NEAR 190                      /* the floor's near edge */
#define AH_PANEL 192                     /* the gestures' panel */
#define AH_LANE 20.0f                    /* a lane's width at the near edge (16 of them: 320 px, wider than the screen) */
#define AH_ROWLEN 0.55f                  /* a tile's depth in the floor's units (the near edge at 1) */
#define AH_ROLLS (1u << PAD_ROLL8 | 1u << PAD_ROLL16 | 1u << PAD_ROLL32)

/* the scene as it moves: advanced once a frame by aff_tick, read by every band */
static struct {
    uint32_t ms;                         /* the last frame's clock */
    float ang, spin;                     /* the gem's turn (turns) and its speed now */
    float bob, pulse, peak;              /* the bob's phase; the pulse (0..1, the break's level); the level's top */
    float lift, sink;                    /* the gem up (a pulse), down (the vinyl stopping) */
    float scroll;                        /* the floor, in tiles (toward us) */
    float glow[16];                      /* each lane's afterglow (the slices just played) */
    float hist_ang[8], hist_y[8];        /* the gem's last positions (VERB's and DUB's ghosts) */
    uint32_t hist_i;
    int32_t slice;                       /* the slice last seen */
    float tear;                          /* SKIP's tear, 0..1 */
    uint32_t rnd;
    float roll_ph;                       /* the roll's own clock (its pulses) */
} an = {0, 0, 0, 0, 0, 0.05f, 0, 0, 0, {0}, {0}, {0}, 0, -2, 0, 0x9E3779B9u, 0};

/* the dither with blocks (CRUSH's coarser grain) and fewer steps (its posterising) */
static inline uint16_t a_dithq(const a_ramp_t *r, int32_t t, int32_t x, int32_t y, int32_t k, int32_t levels)
{
    int32_t L = levels < 2 ? 2 : levels > (int32_t)A_RAMP ? (int32_t)A_RAMP : levels, v, i, ri;
    x >>= k;
    y >>= k;
    v = t * (L - 1);
    if (v <= 0)
        return r->c[0];
    if (v >= 256 * (L - 1))
        return r->c[A_RAMP - 1u];
    i = (v >> 8) + ((v & 255) >> 4 > A_BAYER[y & 3][x & 3] ? 1 : 0);
    ri = i * (int32_t)(A_RAMP - 1u) / (L - 1);
    return r->c[ri];
}

static float a_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* once a frame: the clock, the speeds the performance sets, the pulse from the break's level, the afterglow */
static void aff_tick(const aff_home_t *h)
{
    float dt = (float)(h->t_ms - an.ms) * 0.001f, sp = 1.0f, bps = (float)h->bpm_x10 * 0.1f / 60.0f, target, k;
    uint32_t i;
    if (an.ms == 0u || dt < 0.0f || dt > 0.2f)
        dt = 0.033f;
    an.ms = h->t_ms;
    /* the speed: HALF, REVERSE, the vinyl's spin, FREEZE (the picture follows what the break does) */
    if (h->pads >> PAD_HALF & 1u)
        sp *= 0.5f;
    if (h->pads >> PAD_TAPESTOP & 1u)
        sp *= h->tape;
    if (h->pads >> PAD_REVERSE & 1u)
        sp = -sp;
    if (h->pads >> PAD_FREEZE & 1u)
        sp = 0.0f;
    if (h->skip)                         /* (a skip: the picture stutters, held where it was) */
        sp *= 0.15f;
    /* the gem's turn: slow and calm stopped; on the tempo playing (a turn every two bars) */
    target = h->playing ? bps / 8.0f : 1.0f / 16.0f;
    an.spin += (target * sp - an.spin) * (1.0f - brk_exp2f(-dt * 6.0f));
    an.ang += an.spin * dt;
    if (an.ang > 1000.0f || an.ang < -1000.0f)
        an.ang -= (float)(int32_t)an.ang;
    an.bob += dt * (h->playing ? bps * 0.5f : 0.2f);
    if (an.bob > 1.0f)
        an.bob -= 1.0f;
    /* the pulse: the break's level against its own recent top (the MASTER knob doesn't matter), a fast rise, a
     * slower fall; a roll retriggers it at its rate */
    an.peak = h->level > an.peak ? h->level : an.peak * (1.0f - dt * 0.4f);
    if (an.peak < 0.02f)
        an.peak = 0.02f;
    target = h->playing ? a_clamp01(h->level / an.peak) : 0.0f;
    if ((h->pads & AH_ROLLS) && h->playing) {   /* (the roll's rate: 1/8, 1/16, 1/32 .. a beat's 2, 4, 8 ..) */
        static const float RATE[4] = {2.0f, 4.0f, 8.0f, 16.0f};
        an.roll_ph += dt * bps * RATE[h->roll_div & 3u];
        if (an.roll_ph >= 1.0f) {
            an.roll_ph -= (float)(int32_t)an.roll_ph;
            an.pulse = 1.0f;
        }
    }
    k = target > an.pulse ? 1.0f - brk_exp2f(-dt * 40.0f) : 1.0f - brk_exp2f(-dt * 7.0f);
    an.pulse += (target - an.pulse) * k;
    an.lift += (an.pulse * 7.0f - an.lift) * (1.0f - brk_exp2f(-dt * 12.0f));
    an.sink += (((h->pads >> PAD_TAPESTOP & 1u) ? (1.0f - h->tape) * 22.0f : 0.0f) - an.sink) *
               (1.0f - brk_exp2f(-dt * 8.0f));
    /* the floor: a tile a step toward us playing, a slow drift stopped */
    an.scroll += dt * sp * (h->playing ? bps * 4.0f : 0.35f);
    if (an.scroll > 4096.0f || an.scroll < -4096.0f)
        an.scroll -= (float)(int32_t)an.scroll;
    /* the lanes' afterglow: the slice played lights its lane, which fades over ~0.4 s */
    if (h->slice != an.slice) {
        an.slice = h->slice;
        if (h->slice >= 0 && h->slice < 16)
            an.glow[h->slice] = 1.0f;
    }
    k = brk_exp2f(-dt * 20.0f);          /* (half gone in 50 ms: a trail of a step or two behind the slice playing; the
                                          * break plays its slices in order, so a longer one lit a whole block) */
    for (i = 0; i < 16u; i++)
        an.glow[i] *= k;
    if (h->slice >= 0 && h->slice < 16)
        an.glow[h->slice] = 1.0f;
    if (h->gate < 0.99f && (h->pads >> PAD_GATE & 1u))   /* (the gate chops the light too) */
        for (i = 0; i < 16u; i++)
            an.glow[i] *= h->gate;
    /* the gem's trail (for the echoes' ghosts) */
    an.hist_i = (an.hist_i + 1u) & 7u;
    an.hist_ang[an.hist_i] = an.ang;
    an.hist_y[an.hist_i] = an.lift;
    an.tear += ((h->skip ? 1.0f : 0.0f) - an.tear) * (1.0f - brk_exp2f(-dt * 18.0f));
}

/* the floor in one-point perspective: a row y under the horizon is depth z = 80 / (y - horizon) (the near edge at
 * z 1); across it, x's place in the floor is (x - AH_VX) x z: sixteen even lanes converging on the vanishing point,
 * the tiles closer together as they go. Lit: the slice's lane and the lanes' afterglow; fog by depth (the filter
 * moves it); REDUX in blocks; CRUSH coarser and fewer steps */
static void home_floor(const aff_home_t *h, int32_t band)
{
    int32_t y, y0 = band > AH_HORIZ + 1 ? band : AH_HORIZ + 1, y1 = band + 60 < AH_NEAR ? band + 60 : AH_NEAR;
    int32_t blk = 1 + (int32_t)(h->redux * 5.0f + 0.5f), dk = h->crush > 0.66f ? 2 : h->crush > 0.25f ? 1 : 0;
    int32_t levels = h->crush > 0.8f ? 2 : h->crush > 0.5f ? 3 : (int32_t)A_RAMP;
    float lp = h->dj < 0.0f ? -h->dj : 0.0f, hp = h->dj > 0.0f ? h->dj : 0.0f;
    if (y1 <= y0)
        return;
    y0 = y0 / blk * blk;                 /* (REDUX's blocks on a grid of their own) */
    for (y = y0; y < y1; y += blk) {
        float d = (float)(y - AH_HORIZ) + 0.5f * (float)blk, z = 80.0f / d, depth, fog;
        float tile = z / AH_ROWLEN + an.scroll, wx;
        int32_t row = (int32_t)tile, x, lit_row;
        if (tile < 0.0f && (float)row != tile)
            row--;
        depth = a_clamp01((z - 1.0f) / 12.0f);   /* 0 near .. 1 at the horizon */
        fog = 1.0f - depth * (0.45f + 0.55f * lp) - lp * 0.45f - (1.0f - depth) * 0.85f * hp;   /* (LP: the fog
                                          * closes in, the far gone first; HP: the near fades) */
        lit_row = (int32_t)(a_clamp01(fog) * 200.0f) + 40;
        wx = ((float)0 - (float)AH_VX + 0.5f * (float)blk) * z;
        for (x = 0; x < 240; x += blk, wx += z * (float)blk) {
            float u = wx / AH_LANE + 8.0f, fr;
            int32_t lane = (int32_t)u, odd, t;
            uint16_t c;
            if (u < 0.0f || u >= 16.0f) {
                if (y - band >= 0 && blk == 1)
                    cv_pset(x, y - band, A_BG0);
                else
                    cv_rect(x, y - band, blk, blk, A_BG0);
                continue;
            }
            fr = u - (float)lane;
            odd = (lane + row) & 1;
            if (fr < 0.045f * z || fr > 1.0f - 0.045f * z) {   /* the lane lines, thinner far away */
                t = lit_row * 3 / 4;
                c = a_dithq(&ar_floor1, t, x, y, dk, levels);
            } else if (lane == h->slice) {   /* the slice playing: lit whole */
                t = lit_row + 40;
                c = a_dithq(odd ? &ar_lane : &ar_lane2, t, x, y, dk, levels);
            } else if (an.glow[lane] > 0.2f) {   /* the ones just played: a dim warm trail */
                float g = an.glow[lane];
                t = (int32_t)((float)lit_row * (0.1f + 0.4f * g));
                c = a_dithq(&ar_lane, t, x, y, dk, levels);
            } else {
                t = lit_row * (odd ? 1 : 3) / (odd ? 1 : 4);
                c = a_dithq(odd ? &ar_floor1 : &ar_floor0, t, x, y, dk, levels);
            }
            if (blk == 1)
                cv_pset(x, y - band, c);
            else
                cv_rect(x, y - band, blk, blk, c);
        }
    }
}

/* the gem: six faces up, six down, turning; vertices on whole pixels (the PS1's jitter). cx, cy its centre, r its
 * size, ang its turn; bright 0..1+; ghost: every other pixel and darker (the console's half-transparency); blk:
 * REDUX's blocks; dk, levels: CRUSH's dither */
static void gem_draw(int32_t band, float cx, float cy, float r, float ang, float bright, int ghost, int32_t blk,
                     int32_t dk, int32_t levels, float shimmer)
{
    uint32_t k, i;
    int32_t vx[8], vy[8];
    float vz[8], lum[12];
    uint8_t face[12][3];
    {
        float P[8][3];
        P[0][0] = 0.0f; P[0][1] = -1.25f; P[0][2] = 0.0f;
        P[1][0] = 0.0f; P[1][1] = 1.0f; P[1][2] = 0.0f;
        for (k = 0; k < 6u; k++) {
            float t = ang + (float)k / 6.0f;
            P[2 + k][0] = brk_sin_turns(t + 0.25f);
            P[2 + k][1] = 0.0f;
            P[2 + k][2] = brk_sin_turns(t);
        }
        for (k = 0; k < 8u; k++) {
            float y2 = P[k][1] * 0.9394f - P[k][2] * 0.3429f, z2 = P[k][1] * 0.3429f + P[k][2] * 0.9394f;
            float s = 1.0f / (1.0f + 0.25f * z2);
            vx[k] = (int32_t)(cx + P[k][0] * r * s + 0.5f);
            vy[k] = (int32_t)(cy + y2 * r * s + 0.5f);
            vz[k] = z2;
        }
    }
    for (k = 0; k < 6u; k++) {
        uint8_t a = (uint8_t)(2u + k), b = (uint8_t)(2u + (k + 1u) % 6u);
        float sh = shimmer > 0.0f ? 0.25f * shimmer * brk_sin_turns(shimmer * 3.0f + (float)k / 3.0f) : 0.0f;
        face[k][0] = 0; face[k][1] = a; face[k][2] = b;
        face[6u + k][0] = 1; face[6u + k][1] = b; face[6u + k][2] = a;
        lum[k] = 0.62f + 0.38f * brk_sin_turns(ang + (float)k / 6.0f + 0.6f) + sh;
        lum[6u + k] = 0.45f + 0.4f * brk_sin_turns(ang + (float)k / 6.0f + 0.6f) + sh;
    }
    for (i = 0; i < 12u; i++) {          /* far faces first (the painter's way: the PS1 had no depth buffer) */
        uint32_t best = 12u;
        float bz = -9.0f;
        for (k = 0; k < 12u; k++) {
            float z = vz[face[k][0]] + vz[face[k][1]] + vz[face[k][2]];
            if (face[k][0] != 255u && z > bz) {
                bz = z;
                best = k;
            }
        }
        if (best == 12u)
            break;
        {
            uint8_t *f = face[best];
            int32_t ax = vx[f[0]], ay = vy[f[0]], bx = vx[f[1]], by = vy[f[1]], qx = vx[f[2]], qy = vy[f[2]];
            int32_t cross = (bx - ax) * (qy - ay) - (by - ay) * (qx - ax);
            if (cross > 0) {
                int32_t x0 = ax < bx ? (ax < qx ? ax : qx) : (bx < qx ? bx : qx);
                int32_t x1 = ax > bx ? (ax > qx ? ax : qx) : (bx > qx ? bx : qx);
                int32_t y0 = ay < by ? (ay < qy ? ay : qy) : (by < qy ? by : qy);
                int32_t y1 = ay > by ? (ay > qy ? ay : qy) : (by > qy ? by : qy), x, y;
                int32_t l = (int32_t)(lum[best] * bright * 256.0f);
                const a_ramp_t *rp = best < 6u ? &ar_gem_hi : &ar_gem_lo;
                if (ghost)
                    l = l / 2;
                x0 = x0 / blk * blk;
                y0 = y0 / blk * blk;
                if (y0 < band - blk)
                    y0 = (band - blk) / blk * blk;
                if (y1 >= band + 60)
                    y1 = band + 59;
                for (y = y0; y <= y1; y += blk)
                    for (x = x0; x <= x1; x += blk) {
                        int32_t sx = x + blk / 2, sy = y + blk / 2;
                        int32_t e0 = (bx - ax) * (sy - ay) - (by - ay) * (sx - ax);
                        int32_t e1 = (qx - bx) * (sy - by) - (qy - by) * (sx - bx);
                        int32_t e2 = (ax - qx) * (sy - qy) - (ay - qy) * (sx - qx);
                        if (e0 < 0 || e1 < 0 || e2 < 0 || (ghost && ((x / blk + y / blk) & 1)))
                            continue;
                        if (blk == 1)
                            cv_pset(x, y - band, a_dithq(rp, l, x, y, dk, levels));
                        else
                            cv_rect(x, y - band, blk, blk, a_dithq(rp, l, x, y, dk, levels));
                    }
            }
            f[0] = 255u;
        }
    }
}

/* the gem and what the performance does to it: its ghosts (VERB, DUB: the echoes trail it), the roll's copies
 * orbiting, the freeze's shimmer, the vinyl sinking it */
static void home_gem(const aff_home_t *h, int32_t band)
{
    int32_t blk = 1 + (int32_t)(h->redux * 5.0f + 0.5f), dk = h->crush > 0.66f ? 2 : h->crush > 0.25f ? 1 : 0;
    int32_t levels = h->crush > 0.8f ? 2 : h->crush > 0.5f ? 3 : (int32_t)A_RAMP, n, g;
    float bob = h->playing ? 0.0f : 2.5f * brk_sin_turns(an.bob), breath = h->playing ? 0.0f : 0.03f *
                brk_sin_turns(an.bob * 0.5f);
    float cy = 72.0f + bob - an.lift + an.sink, r = 26.0f * (1.0f + 0.12f * an.pulse + breath);
    float bright = 1.0f + 0.35f * an.pulse, shimmer = (h->pads >> PAD_FREEZE & 1u) ? (float)(h->t_ms % 4000u) / 4000.0f
                   : 0.0f;
    float echo = h->send > (h->pads >> PAD_DUB & 1u ? 0.6f : 0.0f) ? h->send : (h->pads >> PAD_DUB & 1u ? 0.6f : 0.0f);
    if (band >= 112 || band + 60 <= 20)
        return;
    n = echo > 0.05f ? 1 + (int32_t)(echo * 2.5f) : 0;   /* (the echoes: up to three ghosts, older, higher, fainter) */
    for (g = n; g >= 1; g--) {
        uint32_t hi = (an.hist_i + 8u - (uint32_t)(g * 2)) & 7u;
        gem_draw(band, 120.0f, cy - (float)g * 7.0f - an.hist_y[hi] * 0.5f + an.lift * 0.5f, r * (1.0f - 0.06f *
                 (float)g), an.hist_ang[hi] - (float)g * 0.03f, bright * (1.0f - 0.2f * (float)g), 1, blk, dk, levels,
                 0.0f);
    }
    if ((h->pads & AH_ROLLS) && h->playing) {   /* a roll: copies orbiting, more the finer it rolls */
        int32_t m = 2 + (int32_t)(h->roll_div & 3u), j;
        for (j = 0; j < m; j++) {
            float a = an.ang * 2.0f + (float)j / (float)m;
            gem_draw(band, 120.0f + 44.0f * brk_sin_turns(a + 0.25f), cy + 10.0f * brk_sin_turns(a), r * 0.32f,
                     -an.ang * 3.0f, bright * 0.9f, 0, blk, dk, levels, 0.0f);
        }
    }
    gem_draw(band, 120.0f, cy, r, an.ang, bright, 0, blk, dk, levels, shimmer);
}

static void home_bar(int32_t x, int32_t y, int32_t w, float amt, int bipolar, int32_t band)
{
    int32_t xx, yy;
    for (yy = y; yy < y + 3; yy++) {
        if (yy < band || yy >= band + 60)
            continue;
        for (xx = x; xx < x + w; xx++) {
            int32_t t = (xx - x) * 256 / (w - 1), on, k;
            if (bipolar && amt < 0.0f) {
                int32_t lo = (int32_t)((0.5f + amt * 0.5f) * 256.0f);
                on = t >= lo && t <= 128;
                k = on ? (128 - t) * 256 / (128 - lo + 1) : 0;
            } else if (bipolar) {
                int32_t hi = (int32_t)((0.5f + amt * 0.5f) * 256.0f);
                on = t >= 128 && t <= hi;
                k = on ? (t - 128) * 256 / (hi - 128 + 1) : 0;
            } else {
                int32_t hi = (int32_t)(amt * 256.0f);
                on = t <= hi;
                k = on ? t * 256 / (hi + 1) : 0;
            }
            if (on)
                cv_pset(xx, yy - band, a_dith(&ar_bar, 80 + k * 176 / 256, xx, yy));
            else if (!((xx + yy) & 1))
                cv_pset(xx, yy - band, A_BG1);
        }
        if (bipolar)
            cv_pset(x + w / 2, yy - band, A_INK);
    }
}

/* a knob's cell: its name left, its value right, both 2x if they fit the width, else the name at 1x, then the value;
 * its gauge under them (norm -1: none; bip: from the middle, -1..1). y: the capitals' top on the screen */
static void aff_knob_cell(int32_t x, int32_t y, int32_t w, const char *name, const char *v, float norm, int bip,
                          uint16_t ncol, int32_t band)
{
    int32_t lw, vw, lz, vz;
    if (!name || !name[0])
        return;
    v = v ? v : "";
    lw = aff_text_w(name, 2);
    vw = aff_text_w(v, 2);
    lz = lw + vw + 6 <= w ? 2 : 1;
    lw = aff_text_w(name, lz);
    vz = lw + vw + 6 <= w ? 2 : 1;
    aff_text(x, lz == 2 ? y : y + 3, name, lz, ncol, band, 0);
    aff_text(x + w, vz == 2 ? y : y + 3, v, vz, A_INK, band, 1);
    if (norm >= 0.0f || (bip && norm >= -1.0f))
        home_bar(x, y + 13, w, norm, bip, band);
}

static void aff_home_band(const aff_home_t *h, int32_t band)
{
    int32_t x, y, i;
    char t[24];
    aff_check();
    /* the sky: night down to the horizon, dithered */
    for (y = band; y < band + 60 && y < AH_PANEL; y++) {
        int32_t k = y < AH_HORIZ ? y * 256 / AH_HORIZ : 0;
        for (x = 0; x < 240; x++)
            cv_pset(x, y - band, y < AH_HORIZ ? a_dith(&ar_sky, k, x, y) : A_BG0);
    }
    if (band + 60 > AH_PANEL)
        cv_rect(0, AH_PANEL - band, 240, 240 - AH_PANEL, A_BG0);
    if (band <= AH_HORIZ && band + 60 > AH_HORIZ)
        cv_rect(0, AH_HORIZ - band, 240, 1, A_DIM);
    home_floor(h, band);
    home_gem(h, band);
    if (an.tear > 0.05f && band < AH_PANEL) {   /* SKIP: the picture tears, rows slipping sideways (a disc's misread) */
        int32_t y, s0 = (int32_t)(an.tear * 18.0f);
        for (y = band < 40 ? 40 : band; y < band + 60 && y < AH_PANEL; y += 3) {   /* (under the header) */
            uint32_t q = (uint32_t)(y * 2654435761u) ^ (h->t_ms / 70u * 40503u);
            if ((q >> 9) % 9u == 0u)     /* (a row dropped out; now and then hot) */
                cv_rect((int32_t)((q >> 16) % 120u), y - band, 120 + s0 * 4, 1, (q >> 12) % 4u ? A_BG1 : A_HOT);
            else if ((q >> 9) % 7u == 0u)   /* (a sliver of a misread) */
                cv_rect((int32_t)((q >> 14) % 200u), y - band, s0 + 8, 1, A_INK);
        }
    }
    /* the header: the break and the tempo, then the pattern, the slice (or the pad held), the position */
    if (band < 40 && h->msg0) {          /* a message for a moment: what was turned and to what */
        int32_t w1 = h->msg1 ? aff_text_w(h->msg1, 2) : 0;
        aff_text(8, 8, h->msg0, 2, A_INK, band, 0);
        if (h->msg1 && 8 + aff_text_w(h->msg0, 2) + 10 + w1 <= 232)
            aff_text(232, 8, h->msg1, 2, A_HOT, band, 1);
    }
    if (band < 40) {
        if (!h->msg0)
            aff_text(8, 8, h->brk ? h->brk : "", 2, A_INK, band, 0);
        t[0] = 0;
        if (!h->msg0) {
            uint32_t b = h->bpm_x10;
            int32_t n = 0;
            char d[8];
            if (b % 10u) {
                d[n++] = (char)('0' + b % 10u);
                d[n++] = '.';
            }
            b /= 10u;
            do {
                d[n++] = (char)('0' + b % 10u);
                b /= 10u;
            } while (b && n < 7);
            for (i = 0; i < n; i++)
                t[i] = d[n - 1 - i];
            t[n] = 0;
        }
        if (!h->msg0) {
            aff_text(232, 8, t, 2, A_INK, band, 1);
            if (h->playing && h->phase < 0.5f && 8 - band < 60)   /* the beat, lit on the step */
                cv_rect(196, 10 - band, 2, 6, A_HOT);
        }
        t[0] = (char)('A' + h->pat / 16u);
        t[1] = (char)('0' + (h->pat % 16u + 1u) / 10u);
        t[2] = (char)('0' + (h->pat % 16u + 1u) % 10u);
        t[3] = 0;
        aff_text(8, 24, t, 2, A_DIM, band, 0);
        if (h->pad) {
            aff_text(120, 24, h->pad, 2, A_HOT, band, 2);
        } else if (h->slice >= 0) {
            t[0] = 'S'; t[1] = 'L'; t[2] = 'I'; t[3] = 'C'; t[4] = 'E'; t[5] = ' ';
            t[6] = (char)('0' + (h->slice + 1) / 10);
            t[7] = (char)('0' + (h->slice + 1) % 10);
            t[8] = 0;
            aff_text(120, 24, t, 2, A_HOT, band, 2);
        }
        {
            uint32_t b = h->bar % 1000u;
            int32_t n = 0;
            char d[8];
            do {
                d[n++] = (char)('0' + b % 10u);
                b /= 10u;
            } while (b);
            for (i = 0; i < n; i++)
                t[i] = d[n - 1 - i];
            t[n++] = '.';
            t[n++] = (char)('0' + h->beat % 10u);
            t[n] = 0;
        }
        aff_text(232, 24, t, 2, A_DIM, band, 1);
    }
    /* the four gestures, two by two: label left, value right, a dithered bar under */
    if (band + 60 > AH_PANEL) {
        cv_rect(0, AH_PANEL - band, 240, 1, A_DIM);
        for (i = 0; i < 4; i++) {
            int32_t gx = 8 + (i & 1) * 120, gy = 198 + (i >> 1) * 21;
            aff_knob_cell(gx, gy, 106, h->gl[i], h->gvp[i], h->g[i], (int)(h->bip >> i & 1u) && i == 0, A_DIM, band);
        }
    }
}

/* AFFINE's title screen (the boot, the start screen, RESET ALL's): the night, the floor drifting toward us, the gem
 * turning, AFFINE large above it; under the floor up to three lines: l1 at 2x (the break), l2 hot (PRESS PLAY), l3
 * small and dim (the credit); top: the version, small */
static void aff_title_band(int32_t band, uint32_t now, const char *top, const char *l1, const char *l2, const char *l3)
{
    aff_home_t h;
    int32_t x, y;
    uint8_t *z = (uint8_t *)&h;
    uint32_t i;
    for (i = 0; i < sizeof h; i++)
        z[i] = 0u;
    aff_check();
    h.slice = -1;
    h.t_ms = now;
    h.tape = 1.0f;
    h.gate = 1.0f;
    h.bpm_x10 = 1700u;
    if (band == 0)                       /* (once a frame: the first band) */
        aff_tick(&h);
    for (y = band; y < band + 60 && y < AH_PANEL; y++) {
        int32_t k = y < AH_HORIZ ? y * 256 / AH_HORIZ : 0;
        for (x = 0; x < 240; x++)
            cv_pset(x, y - band, y < AH_HORIZ ? a_dith(&ar_sky, k, x, y) : A_BG0);
    }
    if (band + 60 > AH_PANEL)
        cv_rect(0, AH_PANEL - band, 240, 240 - AH_PANEL, A_BG0);
    if (band <= AH_HORIZ && band + 60 > AH_HORIZ)
        cv_rect(0, AH_HORIZ - band, 240, 1, A_DIM);
    home_floor(&h, band);
    home_gem(&h, band);
    if (top && top[0])
        aff_text(120, 4, top, 1, A_DIM, band, 2);
    aff_text(120, 16, "AFFINE", 4, A_INK, band, 2);
    if (l1 && l1[0])
        aff_text(120, 198, l1, 2, A_INK, band, 2);
    if (l2 && l2[0])
        aff_text(120, 215, l2, 1, A_HOT, band, 2);
    if (l3 && l3[0])
        aff_text(120, 228, l3, 1, A_DIM, band, 2);
}
static void aff_title(uint32_t now, const char *top, const char *l1, const char *l2, const char *l3)
{
    int32_t t;
    for (t = 0; t < 240; t += 60) {
        cv_begin(240, 60, A_BG0);
        aff_title_band(t, now, top, l1, l2, l3);
        cv_blit(0, (uint32_t)t);
    }
}
