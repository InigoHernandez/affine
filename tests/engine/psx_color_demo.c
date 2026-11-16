/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: the colour stage (app/dsp/psx.c: GRIT's path, psx_color_set), checked and rendered for A/B listening.
 *   psx_color_demo BREAK.wav                     the checks (tests/engine/run.sh)
 *   psx_color_demo BREAK.wav OUTDIR              the checks, then the renders: OUTDIR/source_stage.wav (16-bit),
 *                                                OUTDIR/source_ab.wav (every stage 4 s in a row, 0.5 s apart) and
 *                                                OUTDIR/index.txt (each file's settings, peak and RMS)
 * Sources: a sine sweep (20 Hz - 20 kHz, 8 s, -6 dBFS), a pad (a minor-7th chord of detuned saws, a slow attack, a
 * soft low-pass) and a Cyberworm break.
 * Checks: nothing NaN or past full scale; levels near the dry's; the latency measured against psx_color_latency and
 * the dry delayed to it (a 50 % mix has no comb notch; the same mix over an undelayed dry has one); CRUNCH 0 is the
 * encoder as it was (bit for bit against a frozen copy of it below) and CRUNCH spoils it in order; rate changes,
 * RAW, the mix and the bus's GRIT on and off without clicks (the largest step between two samples stays bounded). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psx.h"

#define SR 44100u
#define BLOCK 256u
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { float *l, *r; uint32_t n; } buf_t;
static buf_t buf_new(uint32_t n) { buf_t b; b.n = n; b.l = calloc(n, 4); b.r = calloc(n, 4); return b; }
static void buf_free(buf_t *b) { free(b->l); free(b->r); b->l = b->r = 0; b->n = 0; }
static float frand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return (float)(int32_t)*s * (1.0f / 2147483648.0f); }

/* ---- sources ---- */
static buf_t src_sweep(float secs)        /* exponential, 20 Hz to 20 kHz, -6 dBFS, 10 ms fades */
{
    uint32_t i, n = (uint32_t)(secs * SR);
    buf_t b = buf_new(n);
    double k = log(20000.0 / 20.0), T = secs;
    for (i = 0; i < n; i++) {
        double t = (double)i / SR, ph = 2.0 * M_PI * 20.0 * T / k * (exp(t * k / T) - 1.0);
        float e = 1.0f, s;
        if (i < 441u) e = (float)i / 441.0f;
        if (n - i < 441u) e = (float)(n - i) / 441.0f;
        s = 0.5012f * e * (float)sin(ph);
        b.l[i] = b.r[i] = s;
    }
    return b;
}
static float blep(float t, float dt)     /* polyBLEP: the saws without their own aliases */
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static buf_t src_pad(float secs)          /* A minor 7 (A2 G3 C4 E4), three saws a note +-9 cents, spread; 2 s attack */
{
    static const float NOTE[4] = {110.0f, 196.0f, 261.63f, 329.63f}, DET[3] = {-9.0f, 0.0f, 9.0f};
    uint32_t i, n = (uint32_t)(secs * SR), v;
    buf_t b = buf_new(n);
    float ph[12], z[2][2] = {{0, 0}, {0, 0}}, g = tanf((float)M_PI * 1800.0f / SR), k = 1.4f, peak = 0.0f;
    uint32_t rs = 7u;
    for (v = 0; v < 12u; v++) ph[v] = 0.5f + 0.5f * frand(&rs);
    for (i = 0; i < n; i++) {
        float t = (float)i / SR, env, s[2] = {0, 0};
        uint32_t c;
        env = t < 2.0f ? 0.5f - 0.5f * cosf((float)M_PI * t / 2.0f) : 1.0f;
        if (t > secs - 1.0f) env *= 0.5f + 0.5f * cosf((float)M_PI * (t - (secs - 1.0f)));
        for (v = 0; v < 12u; v++) {
            float f = NOTE[v / 3u] * powf(2.0f, DET[v % 3u] / 1200.0f), dt = f / SR, x;
            ph[v] += dt;
            if (ph[v] >= 1.0f) ph[v] -= 1.0f;
            x = 2.0f * ph[v] - 1.0f - blep(ph[v], dt);
            s[0] += x * (v % 3u == 0u ? 1.0f : v % 3u == 1u ? 0.7f : 0.3f);
            s[1] += x * (v % 3u == 2u ? 1.0f : v % 3u == 1u ? 0.7f : 0.3f);
        }
        for (c = 0; c < 2u; c++) {      /* a soft low-pass at 1.8 kHz (SVF, Q 0.7) */
            float hp = (s[c] - (k + g) * z[c][0] - z[c][1]) / (1.0f + g * (g + k)), bp = g * hp + z[c][0], lp = g * bp + z[c][1];
            z[c][0] = g * hp + bp;
            z[c][1] = g * bp + lp;
            s[c] = lp * env;
        }
        b.l[i] = s[0];
        b.r[i] = s[1];
        if (fabsf(s[0]) > peak) peak = fabsf(s[0]);
        if (fabsf(s[1]) > peak) peak = fabsf(s[1]);
    }
    for (i = 0; i < n; i++) { b.l[i] *= 0.5012f / peak; b.r[i] *= 0.5012f / peak; }   /* -6 dBFS peak */
    return b;
}
static uint32_t rd32(FILE *f) { uint8_t b[4]; if (fread(b, 1, 4, f) != 4) return 0; return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static uint16_t rd16(FILE *f) { uint8_t b[2]; if (fread(b, 1, 2, f) != 2) return 0; return (uint16_t)(b[0] | b[1] << 8); }
static buf_t src_break(const char *path, float secs)   /* the break, looped, x 0.8 (as psx_demo.c) */
{
    FILE *f = fopen(path, "rb");
    uint32_t ch = 1, bits = 16, rate = SR, frames = 0, i, n = (uint32_t)(secs * SR);
    char id[5] = {0};
    float *L = 0, *R = 0;
    buf_t b = buf_new(n);
    if (!f) { printf("psx_color_demo: can't read %s\n", path); return b; }
    fseek(f, 12, SEEK_SET);
    while (fread(id, 1, 4, f) == 4) {
        uint32_t sz = rd32(f);
        if (!memcmp(id, "fmt ", 4)) {
            rd16(f); ch = rd16(f); rate = rd32(f); rd32(f); rd16(f); bits = rd16(f);
            fseek(f, (long)sz - 16, SEEK_CUR);
        } else if (!memcmp(id, "data", 4)) {
            frames = sz / (ch * bits / 8u);
            L = malloc(frames * 4); R = malloc(frames * 4);
            for (i = 0; i < frames; i++) {
                L[i] = (float)(int16_t)rd16(f) / 32768.0f;
                R[i] = ch > 1 ? (float)(int16_t)rd16(f) / 32768.0f : L[i];
                if (ch > 2) fseek(f, (long)(ch - 2) * 2, SEEK_CUR);
            }
            break;
        } else {
            fseek(f, (long)sz, SEEK_CUR);
        }
    }
    fclose(f);
    if (!frames || rate != SR || bits != 16) { printf("psx_color_demo: %s: want 44.1 kHz 16-bit\n", path); return b; }
    for (i = 0; i < n; i++) { b.l[i] = 0.8f * L[i % frames]; b.r[i] = 0.8f * R[i % frames]; }
    free(L); free(R);
    return b;
}
static buf_t src_noise(float secs, float fc)   /* white noise through two one-poles: a band for the latency */
{
    uint32_t i, n = (uint32_t)(secs * SR), s = 99u;
    buf_t b = buf_new(n);
    float a = 1.0f - expf(-2.0f * (float)M_PI * fc / SR), z1 = 0, z2 = 0;
    for (i = 0; i < n; i++) { z1 += a * (frand(&s) - z1); z2 += a * (z1 - z2); b.l[i] = b.r[i] = 2.0f * z2; }
    return b;
}
static buf_t src_sine(float secs, float hz, float amp)
{
    uint32_t i, n = (uint32_t)(secs * SR);
    buf_t b = buf_new(n);
    for (i = 0; i < n; i++) b.l[i] = b.r[i] = amp * sinf(2.0f * (float)M_PI * hz * (float)i / SR);
    return b;
}

/* ---- the stage ---- */
typedef struct { const char *name; uint32_t rate, raw, adpcm; float crunch, smooth; int today; const char *say; } var_t;
static void run(const buf_t *in, buf_t *out, const var_t *v, float mix)
{
    uint32_t i;
    psx_init();
    psx_grit_set(22050.0f, 4u, 0u);
    if (!v->today)
        psx_color_set(v->rate, v->raw, v->adpcm, v->crunch);
    *out = buf_new(in->n);
    memcpy(out->l, in->l, in->n * 4);
    memcpy(out->r, in->r, in->n * 4);
    if (!v->name || !strcmp(v->name, "dry"))
        return;
    for (i = 0; i < in->n; i += BLOCK) {
        uint32_t k = in->n - i < BLOCK ? in->n - i : BLOCK;
        psx_grit_process(out->l + i, out->r + i, k, mix);
    }
}
static double rms_db(const buf_t *b, uint32_t a, uint32_t z)
{
    double s = 0;
    uint32_t i;
    if (z > b->n) z = b->n;
    for (i = a; i < z; i++) s += 0.5 * ((double)b->l[i] * b->l[i] + (double)b->r[i] * b->r[i]);
    return 10.0 * log10(s / (z > a ? z - a : 1) + 1e-20);
}
static float peak_of(const buf_t *b, int *bad)
{
    float p = 0;
    uint32_t i;
    *bad = 0;
    for (i = 0; i < b->n; i++) {
        if (!isfinite(b->l[i]) || !isfinite(b->r[i])) *bad = 1;
        if (fabsf(b->l[i]) > p) p = fabsf(b->l[i]);
        if (fabsf(b->r[i]) > p) p = fabsf(b->r[i]);
    }
    return p;
}
/* the latency: the lag (0..400) where the output best matches the input, interpolated */
static float xcorr_lag(const buf_t *in, const buf_t *out)
{
    double best = -1e30, c[402];
    uint32_t lag, i, bl = 0, a = SR / 2u, z = in->n - 500u;
    for (lag = 0; lag <= 401u; lag++) {
        double s = 0;
        for (i = a; i < z; i++) s += (double)in->l[i] * out->l[i + lag];
        c[lag] = s;
        if (s > best) { best = s; bl = lag; }
    }
    if (bl > 0 && bl < 401u) {
        double y0 = c[bl - 1], y1 = c[bl], y2 = c[bl + 1], den = y0 - 2.0 * y1 + y2;
        return (float)bl + (den != 0.0 ? (float)(0.5 * (y0 - y2) / den) : 0.0f);
    }
    return (float)bl;
}
static float max_step(const buf_t *b, uint32_t a, uint32_t z)
{
    float m = 0;
    uint32_t i;
    if (z > b->n) z = b->n;
    for (i = a + 1u; i < z; i++) {
        float d = fabsf(b->l[i] - b->l[i - 1]), e = fabsf(b->r[i] - b->r[i - 1]);
        if (d > m) m = d;
        if (e > m) m = e;
    }
    return m;
}

/* ---- the encoder as it was (psx.c at d5c95a1, frozen here): CRUNCH 0 must be it, bit for bit ---- */
static const int16_t RPRED[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
static int32_t rsat(int32_t x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }
static float ref_try(const int16_t *blk, int32_t *ho, int32_t *hoo, uint32_t f, int32_t sh, int32_t qmin, int32_t qmax,
                     int16_t *out)
{
    int32_t o = *ho, oo = *hoo, k = 12 - sh, half;
    uint32_t j;
    float err = 0.0f;
    if (k < 0) k = 0;
    half = k ? 1 << (k - 1) : 0;
    for (j = 0; j < 28u; j++) {
        int32_t p = (o * RPRED[f][0] + oo * RPRED[f][1] + 32) >> 6, q = (blk[j] - p + half) >> k, s, d;
        q = q < qmin ? qmin : q > qmax ? qmax : q;
        s = rsat(((q * (1 << 12)) >> sh) + p);
        d = s - blk[j];
        err += (float)(d * d);
        oo = o;
        o = s;
        if (out) out[j] = (int16_t)s;
    }
    if (out) { *ho = o; *hoo = oo; }
    return err;
}
static void ref_unit(const int16_t *blk, int32_t *ho, int32_t *hoo, uint32_t bits, uint32_t filter, int16_t *out)
{
    uint32_t f, f0 = 0, f1 = 4, j, cand[2] = {0, 0};
    int32_t qmax = (1 << (bits - 1u)) - 1, qmin = -qmax - 1, peak[5] = {0, 0, 0, 0, 0}, sh[2] = {12, 12};
    float e0 = -1.0f, e1 = -1.0f, ea, eb;
    if (filter && filter <= 5u) f0 = f1 = filter - 1u;
    for (f = f0; f <= f1; f++) {
        int32_t o = *ho, oo = *hoo;
        float e = 0.0f;
        for (j = 0; j < 28u; j++) {
            int32_t p = (o * RPRED[f][0] + oo * RPRED[f][1] + 32) >> 6, r = blk[j] - p;
            e += (float)r * (float)r;
            r = r < 0 ? -r : r;
            if (r > peak[f]) peak[f] = r;
            oo = o;
            o = blk[j];
        }
        if (e0 < 0.0f || e < e0) { e1 = e0; cand[1] = cand[0]; e0 = e; cand[0] = f; }
        else if (e1 < 0.0f || e < e1) { e1 = e; cand[1] = f; }
    }
    if (e1 < 0.0f) cand[1] = cand[0];
    for (j = 0; j < 2u; j++) {
        int32_t s;
        for (s = 12; s > 0; s--)
            if (((int32_t)qmax << 12 >> s) >= peak[cand[j]]) break;
        sh[j] = s;
    }
    ea = ref_try(blk, ho, hoo, cand[0], sh[0], qmin, qmax, 0);
    eb = cand[1] != cand[0] ? ref_try(blk, ho, hoo, cand[1], sh[1], qmin, qmax, 0) : ea;
    j = eb < ea ? 1u : 0u;
    ref_try(blk, ho, hoo, cand[j], sh[j], qmin, qmax, out);
}

static double codec_snr(const int16_t *in, const int16_t *out, uint32_t n)
{
    double s = 0, e = 0;
    uint32_t i;
    for (i = 0; i < n; i++) { double d = (double)out[i] - in[i]; s += (double)in[i] * in[i]; e += d * d; }
    return 10.0 * log10((s + 1e-9) / (e + 1e-9));
}

static void check_encoder(const buf_t *pad, const buf_t *drums)
{
    const uint32_t units = 2000u, n = units * 28u;
    int16_t *in = malloc(n * 2), *a = malloc(n * 2), *b = malloc(n * 2);
    uint32_t sig, bits, filt, i, same = 1;
    double snr[3][3];
    for (sig = 0; sig < 3u; sig++) {
        const buf_t *s = sig == 0 ? pad : drums;
        for (i = 0; i < n; i++) {        /* the pad, the break, and the break 40 dB down (a quiet tail) */
            float x = s->l[(i + SR) % s->n] * (sig == 2 ? 0.01f : 1.0f);
            in[i] = (int16_t)lrintf(fmaxf(-32768.0f, fminf(32767.0f, x * 32767.0f)));
        }
        for (bits = 2; bits <= 4u; bits++)
            for (filt = 0; filt <= 5u; filt++) {
                int32_t ho = 0, hoo = 0;
                uint32_t u;
                psx_adpcm_roundtrip(in, a, units, bits, filt, 0.0f);
                for (u = 0; u < units; u++) ref_unit(in + 28u * u, &ho, &hoo, bits, filt, b + 28u * u);
                if (memcmp(a, b, n * 2)) same = 0;
            }
        for (i = 0; i < 3u; i++) {
            psx_adpcm_roundtrip(in, a, units, 4u, 0u, 0.5f * (float)i);
            snr[sig][i] = codec_snr(in, a, n);
        }
    }
    CHECK(same, "CRUNCH 0 is the encoder as it was (bits 2..4, auto and each predictor, three signals)");
    printf("  SPU-ADPCM 4-bit, SNR at CRUNCH 0 / 0.5 / 1:  pad %.1f / %.1f / %.1f dB   break %.1f / %.1f / %.1f   "
           "break -40 dB %.1f / %.1f / %.1f\n", snr[0][0], snr[0][1], snr[0][2], snr[1][0], snr[1][1], snr[1][2],
           snr[2][0], snr[2][1], snr[2][2]);
    for (sig = 0; sig < 3u; sig++)
        CHECK(snr[sig][0] > snr[sig][1] + 3.0 && snr[sig][1] > snr[sig][2] + 3.0,
              "CRUNCH spoils in order (signal %u: %.1f %.1f %.1f dB)", sig, snr[sig][0], snr[sig][1], snr[sig][2]);
    free(in); free(a); free(b);
}

/* the stage's response at frequency hz, mix 0.5 (dry delayed), the wet alone, and, for the counter-check, the same mix
 * made here over an undelayed dry: output RMS against input RMS, dB */
static void comb_probe(uint32_t rate, float hz, float *aligned, float *wet, float *undelayed)
{
    var_t v = {"probe", rate, 0u, 0u, 0.0f, 1.0f, 0, ""};
    buf_t in = src_sine(0.6f, hz, 0.5f), half, full, u;
    uint32_t i, a = SR / 3u;
    run(&in, &half, &v, 0.5f);
    run(&in, &full, &v, 1.0f);
    u = buf_new(in.n);
    for (i = 0; i < in.n; i++) { u.l[i] = 0.5f * (in.l[i] + full.l[i]); u.r[i] = 0.5f * (in.r[i] + full.r[i]); }
    *aligned = (float)(rms_db(&half, a, in.n) - rms_db(&in, a, in.n));
    *wet = (float)(rms_db(&full, a, in.n) - rms_db(&in, a, in.n));
    *undelayed = (float)(rms_db(&u, a, in.n) - rms_db(&in, a, in.n));
    buf_free(&in); buf_free(&half); buf_free(&full); buf_free(&u);
}

static void check_latency(void)
{
    static const uint32_t RATES[4] = {PSXC_RATE_44K, PSXC_RATE_22K, PSXC_RATE_11K, PSXC_RATE_8K};
    buf_t in = src_noise(2.0f, 1200.0f), out;
    uint32_t k, raw;
    for (k = 0; k < 4u; k++)
        for (raw = 0; raw < 2u; raw++) {
            var_t v = {"lat", RATES[k], raw, 0u, 0.0f, 1.0f, 0, ""};
            float lag, said;
            run(&in, &out, &v, 1.0f);
            lag = xcorr_lag(&in, &out);
            said = psx_color_latency();
            printf("  latency %-5s %-8s measured %6.1f samples (%.2f ms), psx_color_latency %6.1f\n",
                   PSXC_RATE_NAME[RATES[k]], raw ? "RAW" : "FILTERED", lag, lag * 1000.0f / SR, said);
            CHECK(fabsf(lag - said) < 1.5f, "latency %s %s: measured %.1f, said %.1f", PSXC_RATE_NAME[RATES[k]],
                  raw ? "RAW" : "FILTERED", lag, said);
            buf_free(&out);
        }
    buf_free(&in);
    for (k = 1; k < 4u; k++) {           /* the 50 % mix: halfway between the dry and the wet's own response (its soft
                                          * top) at every frequency, no comb; over an undelayed dry, notches */
        float dev = 0, ulo = 0, top = PSXC_RATE_HZ[RATES[k]] * 0.3f, hz;
        for (hz = 90.0f; hz < top; hz *= 1.09f) {
            float a, w, u, want, d;
            comb_probe(RATES[k], hz, &a, &w, &u);
            want = 20.0f * log10f(0.5f + 0.5f * powf(10.0f, w / 20.0f));
            d = fabsf(a - want);
            if (d > dev) dev = d;
            if (u < ulo) ulo = u;
        }
        printf("  mix 50 %% at %-5s: at most %.2f dB off halfway (dry delayed); %.1f dB notches over an undelayed dry "
               "(90 Hz .. %.0f Hz)\n", PSXC_RATE_NAME[RATES[k]], dev, ulo, top);
        CHECK(dev < 0.5f, "%s: a 50 %% mix is %.2f dB off halfway (comb)", PSXC_RATE_NAME[RATES[k]], dev);
        CHECK(ulo < -10.0f, "%s: the counter-check (undelayed dry) should notch (%.1f dB)", PSXC_RATE_NAME[RATES[k]], ulo);
    }
}

/* changes mid-signal: a sine, the knob moved every 0.25 s; the largest step between samples against the steady one */
static void check_clicks(void)
{
    static const uint32_t SEQ[8] = {PSXC_RATE_11K, PSXC_RATE_8K, PSXC_RATE_22K, PSXC_RATE_44K, PSXC_RATE_8K,
                                    PSXC_RATE_44K, PSXC_RATE_11K, PSXC_RATE_22K};
    uint32_t pass, i;
    for (pass = 0; pass < 4u; pass++) {
        buf_t in = src_sine(3.0f, 220.0f, 0.5f), out = buf_new(in.n), st;
        float mix = pass == 1u ? 0.5f : 1.0f, worst, steady;
        uint32_t adpcm = pass >= 2u;
        var_t v = {"steady", PSXC_RATE_8K, 0u, adpcm, 0.0f, 1.0f, 0, ""};
        memcpy(out.l, in.l, in.n * 4); memcpy(out.r, in.r, in.n * 4);
        psx_init();
        for (i = 0; i < in.n; i += BLOCK) {
            uint32_t seg = i / (SR / 4u), k = in.n - i < BLOCK ? in.n - i : BLOCK;
            if (pass == 3u)              /* RAW and the mix moved too */
                psx_color_set(SEQ[seg & 7u], seg & 1u, 1u, 0.0f);
            else
                psx_color_set(SEQ[seg & 7u], 0u, adpcm, 0.0f);
            psx_grit_process(out.l + i, out.r + i, k, pass == 3u ? (seg & 2u ? 0.6f : 1.0f) : mix);
        }
        worst = max_step(&out, SR / 10u, out.n);
        run(&in, &st, &v, mix);          /* the steadiest it gets: 8 kHz all the way */
        steady = max_step(&st, SR / 10u, st.n);
        for (i = 0; i < 4u; i++) {       /* and the other rates held */
            buf_t s2;
            var_t v2 = v;
            v2.rate = SEQ[i];
            run(&in, &s2, &v2, mix);
            if (max_step(&s2, SR / 10u, s2.n) > steady) steady = max_step(&s2, SR / 10u, s2.n);
            buf_free(&s2);
        }
        printf("  rate changes %-22s largest step %.4f (held rates %.4f, the dry %.4f)\n",
               pass == 0 ? "(ADPCM off)" : pass == 1 ? "(ADPCM off, mix 50 %)" : pass == 2 ? "(ADPCM)" :
               "(ADPCM, RAW and mix)", worst, steady, max_step(&in, 0, in.n));
        CHECK(worst < steady * 1.35f, "rate changes click: largest step %.4f against %.4f held", worst, steady);
        buf_free(&in); buf_free(&out); buf_free(&st);
    }
    {   /* the bus: GRIT on and off and on, its mix moved, over a sine (the fade in, the fade out) */
        buf_t in = src_sine(3.0f, 220.0f, 0.5f), out = buf_new(in.n);
        psx_bus_t b;
        float worst;
        memcpy(out.l, in.l, in.n * 4); memcpy(out.r, in.r, in.n * 4);
        psx_init();
        psx_bus_defaults(&b);
        b.crate = 11025.0f;
        for (i = 0; i < in.n; i += BLOCK) {
            uint32_t seg = i / (SR / 5u), k = in.n - i < BLOCK ? in.n - i : BLOCK;
            b.codec = seg % 3u == 2u ? CODEC_OFF : CODEC_GRIT;
            b.cmix = seg % 5u == 3u ? 0.0f : seg % 5u == 1u ? 0.5f : 1.0f;
            psx_bus_set(&b);
            psx_bus_process(out.l + i, out.r + i, 0, k, 0.0f, 15000.0f);
        }
        worst = max_step(&out, SR / 10u, out.n);
        printf("  the bus: GRIT on, off, its mix moved: largest step %.4f (the dry %.4f)\n", worst, max_step(&in, 0, in.n));
        CHECK(worst < 0.045f, "the bus clicks turning GRIT on or off: step %.4f", worst);
        buf_free(&in); buf_free(&out);
    }
}

/* ---- the renders ---- */
static const var_t VARS[] = {
    {"dry", 0, 0, 0, 0.0f, 1.0f, 0, "the source"},
    {"grit_today", PSXC_RATE_GRIT, 0, 1, 0.0f, 1.0f, 1, "GRIT as it was: 22.05 kHz, FILTERED, ADPCM, CRUNCH 0"},
    {"r22k_filt", PSXC_RATE_22K, 0, 0, 0.0f, 1.0f, 0, "RATE 22.05 kHz FILTERED, ADPCM off, SMOOTH 1"},
    {"r22k_raw", PSXC_RATE_22K, 1, 0, 0.0f, 1.0f, 0, "RATE 22.05 kHz RAW, ADPCM off, SMOOTH 1"},
    {"r11k_filt", PSXC_RATE_11K, 0, 0, 0.0f, 1.0f, 0, "RATE 11.025 kHz FILTERED, ADPCM off, SMOOTH 1"},
    {"r11k_raw", PSXC_RATE_11K, 1, 0, 0.0f, 1.0f, 0, "RATE 11.025 kHz RAW, ADPCM off, SMOOTH 1"},
    {"r8k_filt", PSXC_RATE_8K, 0, 0, 0.0f, 1.0f, 0, "RATE 8 kHz FILTERED, ADPCM off, SMOOTH 1"},
    {"r8k_raw", PSXC_RATE_8K, 1, 0, 0.0f, 1.0f, 0, "RATE 8 kHz RAW, ADPCM off, SMOOTH 1"},
    {"adpcm_c0", PSXC_RATE_11K, 0, 1, 0.0f, 1.0f, 0, "11.025 kHz FILTERED, ADPCM, CRUNCH 0, SMOOTH 1"},
    {"adpcm_c50", PSXC_RATE_11K, 0, 1, 0.5f, 1.0f, 0, "11.025 kHz FILTERED, ADPCM, CRUNCH 0.5, SMOOTH 1"},
    {"adpcm_c100", PSXC_RATE_11K, 0, 1, 1.0f, 1.0f, 0, "11.025 kHz FILTERED, ADPCM, CRUNCH 1, SMOOTH 1"},
    {"ps1", PSXC_RATE_11K, 0, 1, 0.25f, 1.0f, 0, "the full PS1: 11.025 kHz FILTERED, ADPCM, CRUNCH 0.25, SMOOTH 1"},
};
#define NVARS (sizeof VARS / sizeof VARS[0])

static void save(const char *dir, const char *name, const buf_t *b)
{
    char path[600];
    FILE *f;
    uint32_t i, bytes = b->n * 4u, v;
    uint16_t h[4] = {1, 2, 4, 16};
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("psx_color_demo: can't write %s\n", path); return; }
    fwrite("RIFF", 1, 4, f); v = 36u + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); fwrite(&h[0], 2, 1, f); fwrite(&h[1], 2, 1, f);
    v = SR; fwrite(&v, 4, 1, f); v = SR * 4u; fwrite(&v, 4, 1, f); fwrite(&h[2], 2, 1, f); fwrite(&h[3], 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (i = 0; i < b->n; i++) {
        float x[2] = {b->l[i], b->r[i]};
        int k;
        for (k = 0; k < 2; k++) {
            int16_t s = (int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, x[k])) * 32767.0f);
            fwrite(&s, 2, 1, f);
        }
    }
    fclose(f);
}

static void render(const char *dir, const char *brk)
{
    static const char *SRC[3] = {"sweep", "pad", "drums"};
    char idx[600];
    FILE *ix;
    uint32_t s, k, i;
    snprintf(idx, sizeof idx, "%s/index.txt", dir);
    ix = fopen(idx, "w");
    if (!ix) { printf("psx_color_demo: can't write %s\n", idx); return; }
    fprintf(ix, "AFFINE colour stage renders (tests/engine/psx_color_demo.c). 44.1 kHz 16-bit stereo, mix 1 (the stage "
                "alone), BITS 4, FILTER auto.\nSources: sweep = sine sweep 20 Hz - 20 kHz, 8 s, -6 dBFS; pad = A minor 7 "
                "of detuned saws, 2 s attack, low-pass 1.8 kHz, 8 s, -6 dBFS peak; drums = %s looped, 8 s.\n"
                "Peak and RMS: dBFS over the whole file.\n\n", brk);
    for (s = 0; s < 3u; s++) {
        buf_t in = s == 0 ? src_sweep(8.0f) : s == 1 ? src_pad(8.0f) : src_break(brk, 8.0f);
        buf_t ab_in = s == 0 ? src_sweep(4.0f) : s == 1 ? src_pad(8.0f) : src_break(brk, 4.0f);
        buf_t ab = buf_new((uint32_t)(NVARS * 4.5f * SR));
        uint32_t ab_at = 0, a0 = s == 1 ? 2u * SR : 0u;   /* (the pad's A/B: its seconds 2..6, past the attack) */
        fprintf(ix, "== %s ==\n", SRC[s]);
        for (k = 0; k < NVARS; k++) {
            buf_t out, o2;
            char name[64];
            int bad;
            float pk;
            run(&in, &out, &VARS[k], 1.0f);
            snprintf(name, sizeof name, "%s_%s", SRC[s], VARS[k].name);
            save(dir, name, &out);
            pk = peak_of(&out, &bad);
            fprintf(ix, "%-18s.wav  peak %6.1f dBFS  RMS %6.1f dBFS  latency %5.1f samples  %s\n", name,
                    20.0f * log10f(pk + 1e-9f), rms_db(&out, 0, out.n), k ? psx_color_latency() : 0.0f, VARS[k].say);
            run(&ab_in, &o2, &VARS[k], 1.0f);  /* the A/B: 4 s of it, 20 ms fades, 0.5 s of silence after */
            for (i = 0; i < 4u * SR && a0 + i < o2.n; i++) {
                float e = 1.0f;
                if (i < 882u) e = (float)i / 882.0f;
                if (4u * SR - i < 882u) e = (float)(4u * SR - i) / 882.0f;
                ab.l[ab_at + i] = o2.l[a0 + i] * e;
                ab.r[ab_at + i] = o2.r[a0 + i] * e;
            }
            fprintf(ix, "%22s A/B at %5.1f s\n", "", (float)ab_at / SR);
            ab_at += (uint32_t)(4.5f * SR);
            buf_free(&out); buf_free(&o2);
        }
        {
            char name[64];
            snprintf(name, sizeof name, "%s_ab", SRC[s]);
            save(dir, name, &ab);
            fprintf(ix, "%s_ab.wav: every stage above in order, 4 s each, 0.5 s apart\n\n", SRC[s]);
        }
        buf_free(&in); buf_free(&ab_in); buf_free(&ab);
    }
    fclose(ix);
    printf("  renders: %s (index.txt)\n", dir);
}

int main(int argc, char **argv)
{
    const char *brk = argc > 1 ? argv[1] : "breaks/huge/cw_amen_jungle.wav";
    buf_t pad = src_pad(8.0f), drums = src_break(brk, 8.0f), sweep = src_sweep(8.0f);
    uint32_t s, k;
    if (!drums.n) return 1;
    for (s = 0; s < 3u; s++) {           /* every stage on every source: finite, under full scale, near the dry level */
        const buf_t *in = s == 0 ? &sweep : s == 1 ? &pad : &drums;
        double dry = rms_db(in, 0, in->n);
        for (k = 1; k < NVARS; k++) {
            buf_t out;
            int bad;
            float pk;
            double lv;
            run(in, &out, &VARS[k], 1.0f);
            pk = peak_of(&out, &bad);
            lv = rms_db(&out, 0, out.n) - dry;
            CHECK(!bad, "%s on source %u: NaN", VARS[k].name, s);
            CHECK(pk < 1.2f, "%s on source %u: peak %.2f", VARS[k].name, s, pk);
            CHECK(lv > (s == 0 ? -3.0 : VARS[k].crunch > 0.6f ? -2.5 : -1.5) && lv < 1.5,   /* (the sweep's top
                                          * octaves go, by design, under 8-11k; CRUNCH's truncation loses a little) */
                  "%s on source %u: level %+.1f dB against dry", VARS[k].name, s, lv);
            buf_free(&out);
        }
    }
    check_encoder(&pad, &drums);
    check_latency();
    check_clicks();
    if (argc > 2)
        render(argv[2], brk);
    printf("%d of %d colour checks passed\n", checks - fails, checks);
    buf_free(&pad); buf_free(&drums); buf_free(&sweep);
    return fails ? 1 : 0;
}
