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
} aff_home_t;

#define AH_HORIZ 110                     /* the horizon's row */
#define AH_FAR 110                       /* the floor's far edge, its near edge, their x */
#define AH_NEAR 190
#define AH_NL (-40)
#define AH_NR 280
#define AH_FL 72
#define AH_FR 168
#define AH_PANEL 192                     /* the gestures' panel */
#define AH_ROWS 6                        /* tile rows on the floor */

/* one triangle of the floor with its texture coordinates (u across the slices 0..1, v near 0 .. far 1): u, v as
 * affine functions of the screen's x, y (no perspective: the PS1's way) */
typedef struct { float x[3], y[3], u[3], v[3]; } a_tri_t;
static void floor_tri(const a_tri_t *t, const aff_home_t *h, int32_t band, float vscroll)
{
    float d = (t->y[1] - t->y[2]) * (t->x[0] - t->x[2]) + (t->x[2] - t->x[1]) * (t->y[0] - t->y[2]);
    float a0, b0, a1, b1, ua, ub, uc, va, vb, vc;
    int32_t y, y0, y1;
    if (d > -0.001f && d < 0.001f)
        return;
    /* barycentric l0 = a0 x + b0 y + c0, l1 = a1 x + b1 y + c1 (l2 = 1 - l0 - l1) */
    a0 = (t->y[1] - t->y[2]) / d;
    b0 = (t->x[2] - t->x[1]) / d;
    a1 = (t->y[2] - t->y[0]) / d;
    b1 = (t->x[0] - t->x[2]) / d;
    /* u = ua x + ub y + uc (the same for v) */
    ua = a0 * (t->u[0] - t->u[2]) + a1 * (t->u[1] - t->u[2]);
    ub = b0 * (t->u[0] - t->u[2]) + b1 * (t->u[1] - t->u[2]);
    uc = t->u[2] - ua * t->x[2] - ub * t->y[2];
    va = a0 * (t->v[0] - t->v[2]) + a1 * (t->v[1] - t->v[2]);
    vb = b0 * (t->v[0] - t->v[2]) + b1 * (t->v[1] - t->v[2]);
    vc = t->v[2] - va * t->x[2] - vb * t->y[2];
    y0 = (int32_t)(t->y[0] < t->y[1] ? (t->y[0] < t->y[2] ? t->y[0] : t->y[2]) : (t->y[1] < t->y[2] ? t->y[1] : t->y[2]));
    y1 = (int32_t)(t->y[0] > t->y[1] ? (t->y[0] > t->y[2] ? t->y[0] : t->y[2]) : (t->y[1] > t->y[2] ? t->y[1] : t->y[2])) + 1;
    if (y0 < band)
        y0 = band;
    if (y1 > band + 60)
        y1 = band + 60;
    for (y = y0; y < y1; y++) {
        /* the row's span inside the triangle: each edge's crossing */
        float xs = 1e9f, xe = -1e9f, fy = (float)y + 0.5f;
        int32_t e, x, xa, xb;
        for (e = 0; e < 3; e++) {
            int32_t f = (e + 1) % 3;
            float ya = t->y[e], yb = t->y[f];
            if ((fy >= ya && fy < yb) || (fy >= yb && fy < ya)) {
                float xc = t->x[e] + (fy - ya) * (t->x[f] - t->x[e]) / (yb - ya);
                if (xc < xs)
                    xs = xc;
                if (xc > xe)
                    xe = xc;
            }
        }
        if (xe < xs)
            continue;
        xa = (int32_t)(xs + 0.5f);
        xb = (int32_t)(xe + 0.5f);
        if (xa < 0)
            xa = 0;
        if (xb > 240)
            xb = 240;
        {
            float u = ua * ((float)xa + 0.5f) + ub * fy + uc, v = va * ((float)xa + 0.5f) + vb * fy + vc;
            for (x = xa; x < xb; x++, u += ua, v += va) {
                int32_t col = (int32_t)(u * 16.0f), row = (int32_t)((v + vscroll) * (float)AH_ROWS);
                int32_t lit = 64 + (int32_t)((1.0f - v) * 192.0f), odd = (col + row) & 1;   /* (near bright) */
                uint16_t c;
                if (col < 0)
                    col = 0;
                if (col > 15)
                    col = 15;
                if (col == h->slice)
                    c = a_dith(odd ? &ar_lane : &ar_lane2, lit + 32, x, y);
                else
                    c = a_dith(odd ? &ar_floor1 : &ar_floor0, lit, x, y);
                cv_pset(x, y - band, c);
            }
        }
    }
}

static void home_floor(const aff_home_t *h, int32_t band)
{
    uint32_t q;
    float vscroll = (float)(h->steps % AH_ROWS) / (float)AH_ROWS + h->phase / (float)AH_ROWS;
    if (band + 60 <= AH_FAR || band >= AH_NEAR)
        return;
    for (q = 0; q < 4u; q++) {           /* four strips of four slices, each two triangles */
        float a = (float)q * 0.25f, b = a + 0.25f;
        float px[4] = {AH_NL + (AH_NR - AH_NL) * a, AH_NL + (AH_NR - AH_NL) * b, AH_FL + (AH_FR - AH_FL) * b,
                       AH_FL + (AH_FR - AH_FL) * a};
        float py[4] = {AH_NEAR, AH_NEAR, AH_FAR, AH_FAR}, pu[4] = {a, b, b, a}, pv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        static const uint8_t T[2][3] = {{0, 1, 2}, {0, 2, 3}};
        uint32_t k, i;
        for (k = 0; k < 2u; k++) {
            a_tri_t t;
            for (i = 0; i < 3u; i++) {
                t.x[i] = px[T[k][i]];
                t.y[i] = py[T[k][i]];
                t.u[i] = pu[T[k][i]];
                t.v[i] = pv[T[k][i]];
            }
            floor_tri(&t, h, band, vscroll);
        }
    }
}

/* the gem: six faces up, six down, turning; vertices on whole pixels */
static void home_gem(const aff_home_t *h, int32_t band)
{
    float ang = (float)(h->t_ms % 12000u) * (1.0f / 12000.0f), jolt = 0.0f, cx = 120.0f, cy = 72.0f, r = 26.0f;
    uint32_t since = h->t_ms - h->kick_ms, k, i;
    int32_t vx[8], vy[8];
    float vz[8], lum[12];
    uint8_t face[12][3];
    if (band >= 110 || band + 60 <= 26)
        return;
    if (since < 180u)                    /* a new slice: a jolt up and a turn ahead, gone in 0.18 s */
        jolt = 1.0f - (float)since / 180.0f;
    cy -= 6.0f * jolt;
    ang += 0.04f * jolt;
    {   /* the vertices: top, bottom, a ring of six; tilted toward us */
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
            vx[k] = (int32_t)(cx + P[k][0] * r * s + 0.5f);   /* (whole pixels: the jitter) */
            vy[k] = (int32_t)(cy + y2 * r * s + 0.5f);
            vz[k] = z2;
        }
    }
    for (k = 0; k < 6u; k++) {
        uint8_t a = (uint8_t)(2u + k), b = (uint8_t)(2u + (k + 1u) % 6u);
        face[k][0] = 0; face[k][1] = a; face[k][2] = b;
        face[6u + k][0] = 1; face[6u + k][1] = b; face[6u + k][2] = a;
        lum[k] = 0.62f + 0.38f * brk_sin_turns(ang + (float)k / 6.0f + 0.6f);
        lum[6u + k] = 0.45f + 0.4f * brk_sin_turns(ang + (float)k / 6.0f + 0.6f);
    }
    /* far faces first (the painter's way; the PS1 had no depth buffer either) */
    for (i = 0; i < 12u; i++) {
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
            if (cross > 0) {             /* facing us: filled flat, the light by its turn */
                int32_t x0 = ax < bx ? (ax < qx ? ax : qx) : (bx < qx ? bx : qx);
                int32_t x1 = ax > bx ? (ax > qx ? ax : qx) : (bx > qx ? bx : qx);
                int32_t y0 = ay < by ? (ay < qy ? ay : qy) : (by < qy ? by : qy);
                int32_t y1 = ay > by ? (ay > qy ? ay : qy) : (by > qy ? by : qy), x, y;
                int32_t l = (int32_t)((lum[best] + 0.35f * jolt) * 256.0f);
                const a_ramp_t *rp = best < 6u ? &ar_gem_hi : &ar_gem_lo;
                if (y0 < band)
                    y0 = band;
                if (y1 >= band + 60)
                    y1 = band + 59;
                for (y = y0; y <= y1; y++)
                    for (x = x0; x <= x1; x++) {
                        int32_t e0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
                        int32_t e1 = (qx - bx) * (y - by) - (qy - by) * (x - bx);
                        int32_t e2 = (ax - qx) * (y - qy) - (ay - qy) * (x - qx);
                        if (e0 >= 0 && e1 >= 0 && e2 >= 0)
                            cv_pset(x, y - band, a_dith(rp, l, x, y));
                    }
            }
            f[0] = 255u;                 /* (drawn) */
        }
    }
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
    h.steps = now / 600u;
    h.phase = (float)(now % 600u) / 600.0f;
    h.t_ms = now;
    h.kick_ms = now - 1000u;
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
