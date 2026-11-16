/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: the PSX colour, phases 3 and 4 (app/dsp/psx.c), checked and rendered for A/B listening.
 *   psx_color2_demo BREAK.wav                    the checks (tests/engine/run.sh)
 *   psx_color2_demo BREAK.wav OUTDIR             the checks, then the renders (OUTDIR/index.txt says what each is)
 * Phase 3: SPU VERB's INPUT (the gain into its 16-bit model: levels, the saturation it drives, no clicks as it moves;
 * the send and a new type glide too), the echo presets' timing and feedback measured (as dub delays), XA on the Q16
 * clock (its latency steady over minutes, said by psx_xa_latency; its dry aligned; on and off without clicks).
 * Sources and measures: psx_color_demo.c's (included). */
#define PSX_COLOR_DEMO_LIB
#pragma GCC diagnostic ignored "-Wunused-function"
#include "psx_color_demo.c"

static int16_t spu[PSXV_WORDS];          /* SPU VERB's work area (the pool's on the FM-1) */

/* the lag (0..400) where out best matches in, n samples of in, out n + 402 long; interpolated */
static float lag_arr(const float *in, const float *out, uint32_t n)
{
    double best = -1e30, c[402];
    uint32_t lag, i, bl = 0;
    for (lag = 0; lag <= 401u; lag++) {
        double s = 0;
        for (i = 0; i < n; i++) s += (double)in[i] * out[i + lag];
        c[lag] = s;
        if (s > best) { best = s; bl = lag; }
    }
    if (bl > 0 && bl < 401u) {
        double y0 = c[bl - 1], y1 = c[bl], y2 = c[bl + 1], den = y0 - 2.0 * y1 + y2;
        return (float)bl + (den != 0.0 ? (float)(0.5 * (y0 - y2) / den) : 0.0f);
    }
    return (float)bl;
}

/* ---- XA: the latency over minutes (its float clocks wandered 14..181 samples before) ---- */
static void check_xa_clock(void)
{
    static float win_in[SR], win_out[SR + 402u];
    uint32_t half;
    for (half = 0; half < 2u; half++) {
        static const uint32_t AT[2][6] = {{2u, 60u, 120u, 180u, 240u, 297u}, {2u, 60u, 120u, 177u, 0u, 0u}};
        uint32_t nwin = half ? 4u : 6u, total = (AT[half][nwin - 1u] + 2u) * SR, i, k, s = 99u, cw = 0;
        float z1 = 0.0f, z2 = 0.0f, a = 1.0f - expf(-2.0f * (float)M_PI * 1200.0f / SR), lags[6], lo = 1e9f, hi = -1e9f;
        float bl[BLOCK], br[BLOCK], bi[BLOCK], said;
        psx_init();
        psx_xa_set(half, 0u, 1u);
        for (i = 0; i < total && cw < nwin; i += BLOCK) {
            uint32_t st = AT[half][cw] * SR;
            for (k = 0; k < BLOCK; k++) {
                z1 += a * (frand(&s) - z1);
                z2 += a * (z1 - z2);
                bl[k] = br[k] = bi[k] = 2.0f * z2;
            }
            psx_xa_process(bl, br, BLOCK, 1.0f);
            for (k = 0; k < BLOCK; k++) {
                uint32_t t = i + k;
                if (t >= st && t < st + SR) win_in[t - st] = bi[k];
                if (t >= st && t < st + SR + 402u) win_out[t - st] = bl[k];
                if (t == st + SR + 401u) {
                    lags[cw] = lag_arr(win_in, win_out, SR);
                    if (lags[cw] < lo) lo = lags[cw];
                    if (lags[cw] > hi) hi = lags[cw];
                    cw++;
                    if (cw < nwin) st = AT[half][cw] * SR;
                }
            }
        }
        said = psx_xa_latency();
        printf("  XA %s kHz: latency over %u s measured", half ? "18.9" : "37.8", AT[half][nwin - 1u] + 1u);
        for (k = 0; k < cw; k++) printf(" %.1f", lags[k]);
        printf(" samples (psx_xa_latency %.1f)\n", said);
        CHECK(cw == nwin && hi - lo < 1.0f, "XA %s: the latency wanders (%.1f .. %.1f samples)", half ? "18.9" : "37.8",
              lo, hi);
        CHECK(fabsf(0.5f * (lo + hi) - said) < 1.5f, "XA %s: latency measured %.1f, said %.1f", half ? "18.9" : "37.8",
              0.5f * (lo + hi), said);
    }
}

/* XA's 50 % mix over its delayed dry: halfway between the dry and the wet's own response, no comb */
static void xa_run(const buf_t *in, buf_t *out, uint32_t half, float mix)
{
    uint32_t i;
    psx_init();
    psx_xa_set(half, 0u, 1u);
    *out = buf_new(in->n);
    memcpy(out->l, in->l, in->n * 4);
    memcpy(out->r, in->r, in->n * 4);
    for (i = 0; i < in->n; i += BLOCK)
        psx_xa_process(out->l + i, out->r + i, in->n - i < BLOCK ? in->n - i : BLOCK, mix);
}
static void check_xa_mix(void)
{
    uint32_t half;
    for (half = 0; half < 2u; half++) {
        float dev = 0.0f, hz, top = (half ? 18900.0f : 37800.0f) * 0.3f;
        for (hz = 90.0f; hz < top; hz *= 1.12f) {
            buf_t in = src_sine(0.6f, hz, 0.5f), h, f;
            uint32_t a = SR / 3u;
            float al, w, want, d;
            xa_run(&in, &h, half, 0.5f);
            xa_run(&in, &f, half, 1.0f);
            al = (float)(rms_db(&h, a, in.n) - rms_db(&in, a, in.n));
            w = (float)(rms_db(&f, a, in.n) - rms_db(&in, a, in.n));
            want = 20.0f * log10f(0.5f + 0.5f * powf(10.0f, w / 20.0f));
            d = fabsf(al - want);
            if (d > dev) dev = d;
            buf_free(&in); buf_free(&h); buf_free(&f);
        }
        printf("  XA %s kHz mix 50 %%: at most %.2f dB off halfway (dry delayed; 90 Hz .. %.0f Hz)\n",
               half ? "18.9" : "37.8", dev, top);
        CHECK(dev < 0.5f, "XA %s: a 50 %% mix is %.2f dB off halfway (comb)", half ? "18.9" : "37.8", dev);
    }
    {   /* the bus: XA on and off and on, its mix moved, its rate changed (the fades, the glide) */
        buf_t in = src_sine(3.0f, 220.0f, 0.5f), out = buf_new(in.n);
        psx_bus_t b;
        uint32_t i;
        float worst;
        memcpy(out.l, in.l, in.n * 4); memcpy(out.r, in.r, in.n * 4);
        psx_init();
        psx_bus_defaults(&b);
        for (i = 0; i < in.n; i += BLOCK) {
            uint32_t seg = i / (SR / 5u), k = in.n - i < BLOCK ? in.n - i : BLOCK;
            b.codec = seg % 3u == 2u ? CODEC_OFF : CODEC_XA;
            b.cmix = seg % 5u == 3u ? 0.0f : seg % 5u == 1u ? 0.5f : 1.0f;
            b.xhalf = (uint8_t)(seg % 4u == 1u);
            psx_bus_set(&b);
            psx_bus_process(out.l + i, out.r + i, 0, k, 0.0f, 15000.0f);
        }
        worst = max_step(&out, SR / 10u, out.n);
        printf("  the bus: XA on, off, its mix and rate moved: largest step %.4f (the dry %.4f)\n", worst,
               max_step(&in, 0, in.n));
        CHECK(worst < 0.045f, "the bus clicks turning XA on or off: step %.4f", worst);
        buf_free(&in); buf_free(&out);
    }
}

/* ---- SPU VERB: INPUT, and the send, the type and INPUT moving without clicks ---- */
static void verb_wet(const buf_t *in, buf_t *out, uint32_t type, float input)
{
    uint32_t i;
    psx_verb_init(spu, PSXV_WORDS);
    psx_init();
    psx_verb_set(type, 1.0f, 1.0f, 0.0f, 0.0f);
    psx_verb_input(input);
    *out = buf_new(in->n);
    for (i = 0; i < in->n; i += BLOCK) {
        uint32_t k = in->n - i < BLOCK ? in->n - i : BLOCK;
        psx_verb_process(in->l + i, in->r + i, out->l + i, out->r + i, k);
    }
}
static void check_verb_input(const buf_t *drums)
{
    static const float G[4] = {0.5f, 1.0f, 2.0f, 4.0f};
    double lv[4], hd[4];
    uint32_t k;
    buf_t sine = src_sine(3.0f, 300.0f, 0.5f);
    for (k = 0; k < 4u; k++) {
        buf_t w, ws;
        verb_wet(drums, &w, PSXV_HALL, G[k]);
        lv[k] = rms_db(&w, SR, w.n);
        verb_wet(&sine, &ws, PSXV_HALL, G[k]);
        hd[k] = band_db(&ws, 700.0, 12000.0) - band_db(&ws, 250.0, 350.0);   /* the products over the tone */
        buf_free(&w); buf_free(&ws);
    }
    printf("  SPU VERB HALL, INPUT 0.5 / 1 / 2 / 4: the break's wet %.1f / %.1f / %.1f / %.1f dBFS; a 300 Hz tone at "
           "-6 dBFS, what the 16-bit maths add above it %.1f / %.1f / %.1f / %.1f dB\n", lv[0], lv[1], lv[2], lv[3],
           hd[0], hd[1], hd[2], hd[3]);
    CHECK(fabs(lv[1] - lv[0] - 6.02) < 0.7, "INPUT 0.5 is 6 dB under 1 (%.2f dB)", lv[1] - lv[0]);
    CHECK(lv[2] > lv[1] + 1.0 && lv[2] < lv[1] + 6.0, "INPUT 2: louder, the maths clipping (%+.2f dB)", lv[2] - lv[1]);
    CHECK(lv[3] < lv[2] + 4.0, "INPUT 4: the clipping holds it (%+.2f dB over 2)", lv[3] - lv[2]);
    CHECK(hd[3] > hd[2] + 3.0 && hd[2] > hd[0] + 3.0, "INPUT drives the saturation (%.1f %.1f %.1f dB)", hd[0], hd[2],
          hd[3]);
    buf_free(&sine);
}

/* the largest second difference (a step's mark, where a tone's own slope barely shows) */
static float max_bend(const buf_t *b, uint32_t a, uint32_t z)
{
    float m = 0;
    uint32_t i;
    if (z > b->n) z = b->n;
    for (i = a + 2u; i < z; i++) {
        float d = fabsf(b->l[i] - 2.0f * b->l[i - 1] + b->l[i - 2]), e = fabsf(b->r[i] - 2.0f * b->r[i - 1] + b->r[i - 2]);
        if (d > m) m = d;
        if (e > m) m = e;
    }
    return m;
}

/* the bus over a sine with SPU VERB sounding: one of its settings moved every quarter second (what: 0 INPUT, 1 the send
 * and the VERB key's throw, 2 the type, 3 SIZE a knob's step at a time on HALL, 4 the same on CHAOS, its sine held
 * almost forever); the largest step between samples against the same settings held (SIZE: the largest second
 * difference: a glide bends the pitch, and the tone through the room's paths swells and dips as they slide, so its
 * slope is no measure; a step shows in the second difference, the tone's own slope hardly) */
static float verb_bus(const buf_t *in, uint32_t what, int held, float *steady)
{
    static const float IN[6] = {1.0f, 2.0f, 0.5f, 4.0f, 1.0f, 0.25f}, SEND[5] = {0.0f, 1.0f, 0.3f, 1.0f, 0.6f};
    static const uint8_t TY[5] = {PSXV_HALL, PSXV_ROOM, PSXV_SPACE_ECHO, PSXV_STUDIO_L, PSXV_CHAOS};
    static const float SZ[5] = {0.5f, 0.52f, 0.54f, 0.52f, 0.5f};
    buf_t out = buf_new(in->n);
    psx_bus_t b;
    uint32_t i;
    float worst;
    memcpy(out.l, in->l, in->n * 4); memcpy(out.r, in->r, in->n * 4);
    psx_verb_init(spu, PSXV_WORDS);
    psx_init();
    psx_bus_defaults(&b);
    b.send = 0.6f;
    b.vtype = what == 4u ? PSXV_CHAOS : PSXV_HALL;
    b.vsize = 0.5f;
    for (i = 0; i < in->n; i += BLOCK) {
        uint32_t seg = i / (SR / 4u), k = in->n - i < BLOCK ? in->n - i : BLOCK, q = held >= 0 ? (uint32_t)held : seg;
        if (what == 0u)
            psx_verb_input(IN[q % 6u]);
        else if (what == 1u) {
            b.send = SEND[q % 5u];
            b.vthrow = q % 5u == 2u ? 1.0f : 0.0f;
        } else if (what == 2u)
            b.vtype = TY[q % 5u];
        else
            b.vsize = SZ[q % 5u];
        psx_bus_set(&b);
        psx_bus_process(out.l + i, out.r + i, 0, k, 0.0f, 15000.0f);
    }
    worst = what >= 3u ? max_bend(&out, SR / 2u, out.n) : max_step(&out, SR / 2u, out.n);
    if (steady && worst > *steady) *steady = worst;
    buf_free(&out);
    return worst;
}
static void check_verb_moves(void)
{
    static const char *const W[5] = {"INPUT", "the send and VRB", "the type", "SIZE (HALL)", "SIZE (CHAOS)"};
    buf_t in = src_sine(4.0f, 220.0f, 0.4f);
    uint32_t w;
    for (w = 0; w < 5u; w++) {
        float steady = 0.0f, worst;
        int h;
        for (h = 0; h < (w == 0u ? 6 : 5); h++)
            verb_bus(&in, w, h, &steady);
        worst = verb_bus(&in, w, -1, 0);
        printf("  SPU VERB, %-16s moved: largest %s %.4f (held %.4f)\n", W[w], w >= 3u ? "bend" : "step", worst,
               steady);
        CHECK(worst < steady * 1.35f, "SPU VERB clicks as %s moves: step %.4f against %.4f held", W[w], worst, steady);
    }
    buf_free(&in);
}

/* ---- the echo presets as dub delays: their repeats, measured from an impulse ---- */
static void check_echoes(void)
{
    static const uint32_t T[4] = {PSXV_DELAY, PSXV_CHAOS, PSXV_SPACE_ECHO, PSXV_HALF_ECHO};
    static const float SZ[3] = {0.0f, 0.5f, 1.0f};
    uint32_t t, z, i;
    for (t = 0; t < 4u; t++)
        for (z = 0; z < (t < 2u ? 3u : 1u); z++) {
            uint32_t n = SR * 5u, p0 = 0, p1 = 0;
            buf_t in = buf_new(n), out = buf_new(n);
            float ms[4], g[4], m0 = 0.0f, m1 = 0.0f, size = t < 2u ? SZ[z] : 1.0f, per;
            in.l[100] = in.r[100] = 0.5f;
            psx_verb_init(spu, PSXV_WORDS);
            psx_init();
            psx_verb_set(T[t], 1.0f, size, 0.0f, 0.0f);
            {                            /* (SIZE settled first) */
                float zl[BLOCK] = {0}, zr[BLOCK] = {0}, ol[BLOCK], or_[BLOCK];
                for (i = 0; i < 4u; i++) psx_verb_process(zl, zr, ol, or_, BLOCK);
            }
            for (i = 0; i < n; i += BLOCK)
                psx_verb_process(in.l + i, in.r + i, out.l + i, out.r + i, BLOCK);
            psx_verb_taps(T[t], size, ms, g);
            per = (t < 2u ? ms[0] : 400.0f) * (float)SR / 1000.0f;
            for (i = 100u; i < n; i++) {     /* the first repeat, then the second (past half a period on) */
                float a = fabsf(out.l[i]);
                if (i < 100u + (uint32_t)(per * 1.5f)) { if (a > m0) { m0 = a; p0 = i; } }
                else if (i < 100u + (uint32_t)(per * 2.5f)) { if (a > m1) { m1 = a; p1 = i; } }
            }
            if (t < 2u) {
                printf("  %-10s SIZE %3.0f %%: a repeat every %5.0f ms (taps say %5.0f), the first %+5.1f dB, the next "
                       "%+6.1f dB on it\n", PSXV_NAME[T[t]], size * 100.0f, (float)(p0 - 100u) * 1000.0f / SR, ms[0],
                       20.0f * log10f(m0 / 0.5f + 1e-9f), m1 > 1e-6f ? 20.0f * log10f(m1 / m0) : -99.0f);
                CHECK(fabsf((float)(p0 - 100u) * 1000.0f / SR - ms[0]) < 3.0f, "%s: its repeat at %.0f ms, taps say %.0f",
                      PSXV_NAME[T[t]], (float)(p0 - 100u) * 1000.0f / SR, ms[0]);
                if (T[t] == PSXV_CHAOS)
                    CHECK(p1 > 0u && fabsf((float)(p1 - p0) - (float)(p0 - 100u)) < 0.003f * SR,
                          "CHAOS repeats on its period (%u then %u)", p0 - 100u, p1 - p0);
            } else {
                printf("  %-10s SIZE 100 %%: the loudest early repeat at %5.0f ms (%+5.1f dB); tail %.1f s\n",
                       PSXV_NAME[T[t]], (float)(p0 - 100u) * 1000.0f / SR, 20.0f * log10f(m0 / 0.5f + 1e-9f),
                       psx_verb_tail(T[t]));
            }
            buf_free(&in); buf_free(&out);
        }
}

int main(int argc, char **argv)
{
    const char *brk = argc > 1 ? argv[1] : "breaks/huge/cw_amen_jungle.wav";
    buf_t drums = src_break(brk, 6.0f);
    if (!drums.n) return 1;
    check_verb_input(&drums);
    check_verb_moves();
    check_echoes();
    check_xa_mix();
    check_xa_clock();
    (void)argc;
    printf("%d of %d colour 2 checks passed\n", checks - fails, checks);
    buf_free(&drums);
    return fails ? 1 : 0;
}
