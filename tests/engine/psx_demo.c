/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: the PSX effects (app/dsp/psx.c) over a real break, checked and rendered.
 *   psx_demo BREAK.wav BPM OUTDIR      (tests/engine/run.sh runs it on a Cyberworm Amen)
 * Checks: nothing NaN or past full scale, levels near the dry break's, the reverbs' tails in order (ROOM shortest,
 * CHAOS longest), FREEZE holding, the codecs' latency steady. Renders: one WAV per effect, the dry break first, then
 * the effect with its knobs moving, as a player would; and a tour of all of them. */
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

static float *brk;                       /* the break, mono, 44.1 kHz, looped */
static uint32_t brk_n;
static int16_t work[PSXV_WORDS];
static uint8_t skipring[2 * 9216];

static uint32_t rd32(FILE *f) { uint8_t b[4]; if (fread(b, 1, 4, f) != 4) return 0; return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static uint16_t rd16(FILE *f) { uint8_t b[2]; if (fread(b, 1, 2, f) != 2) return 0; return (uint16_t)(b[0] | b[1] << 8); }

static int load(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t ch = 1, bits = 16, rate = SR;
    char id[5] = {0};
    if (!f) return 0;
    fseek(f, 12, SEEK_SET);
    while (fread(id, 1, 4, f) == 4) {
        uint32_t sz = rd32(f);
        if (!memcmp(id, "fmt ", 4)) {
            rd16(f); ch = rd16(f); rate = rd32(f); rd32(f); rd16(f); bits = rd16(f);
            fseek(f, (long)sz - 16, SEEK_CUR);
        } else if (!memcmp(id, "data", 4)) {
            uint32_t i, frames = sz / (ch * bits / 8u);
            brk = malloc(frames * sizeof(float));
            for (i = 0; i < frames; i++) {
                float s = 0;
                uint32_t c;
                for (c = 0; c < ch; c++)
                    s += (float)(int16_t)rd16(f) / 32768.0f;
                brk[i] = s / (float)ch;
            }
            brk_n = frames;
            break;
        } else {
            fseek(f, (long)sz, SEEK_CUR);
        }
    }
    fclose(f);
    if (rate != SR || bits != 16) { printf("psx_demo: %s is %u Hz %u bit (want 44.1 kHz 16-bit)\n", path, rate, bits); return 0; }
    return brk_n > 0;
}

typedef struct { float *l, *r; uint32_t n, cap; } track_t;
static void put(track_t *t, float l, float r)
{
    if (t->n == t->cap) { t->cap = t->cap ? 2 * t->cap : SR * 60; t->l = realloc(t->l, t->cap * 4); t->r = realloc(t->r, t->cap * 4); }
    t->l[t->n] = l; t->r[t->n] = r; t->n++;
}
static void save(const char *dir, const char *name, const track_t *t)
{
    char path[512];
    FILE *f;
    uint32_t i, bytes = t->n * 4u;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    fwrite("RIFF", 1, 4, f); i = 36 + bytes; fwrite(&i, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    i = 16; fwrite(&i, 4, 1, f);
    { uint16_t a = 1, c = 2, b = 4, d = 16; uint32_t r = SR, br = SR * 4; fwrite(&a, 2, 1, f); fwrite(&c, 2, 1, f);
      fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&b, 2, 1, f); fwrite(&d, 2, 1, f); }
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (i = 0; i < t->n; i++) {
        float v[2] = {t->l[i], t->r[i]};
        int k;
        for (k = 0; k < 2; k++) {
            float x = v[k] > 1.0f ? 1.0f : v[k] < -1.0f ? -1.0f : v[k];
            int16_t s = (int16_t)lrintf(x * 32767.0f);
            fwrite(&s, 2, 1, f);
        }
    }
    fclose(f);
}


/* the effects as one function each: block in place (l, r), the time t in seconds (the knobs move with it) */
typedef void (*fx_fn)(float *l, float *r, uint32_t n, float t, float beat, float spb);
static float bpm = 170.0f;
static void fx_crush(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    float bits = 12.0f - 10.0f * (0.5f - 0.5f * cosf(t * 0.6f));   /* 12 down to 2 bits and back */
    (void)beat; (void)spb;
    psx_crush_set(bits, t < 10.0f ? CRUSH_ROUND : CRUSH_DITHER, t > 13.0f ? 0.4f : 0.0f);
    psx_crush_process(l, r, n, 1.0f);
}
static void fx_redux(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    float hz = 22050.0f * powf(2.0f, -6.0f * (0.5f - 0.5f * cosf(t * 0.55f)));   /* 22 kHz to 345 Hz and back */
    (void)beat; (void)spb;
    psx_redux_set(hz, t < 7.0f ? REDUX_HARD : t < 12.0f ? REDUX_SOFT : REDUX_JITTER, 0.6f);
    psx_redux_process(l, r, n, 1.0f);
}
static void fx_grit(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    static const float RATES[4] = {32000.0f, 22050.0f, 16000.0f, 11025.0f};
    uint32_t k = (uint32_t)(t / 3.0f) & 3u;
    (void)beat; (void)spb;
    psx_grit_set(RATES[k], t > 12.0f ? 3u : 4u, 0u);
    psx_grit_process(l, r, n, 1.0f);
}
static void fx_xa(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    (void)beat; (void)spb;
    psx_xa_set(t > 8.0f, 0u, 1u);
    psx_xa_process(l, r, n, 1.0f);
}
static void fx_skip(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    psx_skip_set(t < 8.0f ? 2u : 1u, t < 8.0f ? 0.3f : 0.5f, t > 11.0f ? 0.8f : 0.0f);
    psx_skip_process(l, r, n, beat, spb, 1.0f);
}
static uint32_t verb_type;
static float verb_freeze_at = -1.0f;
static void fx_verb(float *l, float *r, uint32_t n, float t, float beat, float spb)
{
    float wl[BLOCK], wr[BLOCK], sl[BLOCK], sr[BLOCK];
    uint32_t i;
    float send = 1.0f, fr = 0.0f;
    (void)beat; (void)spb;
    if (verb_freeze_at >= 0.0f && t >= verb_freeze_at) { fr = 1.0f; }
    psx_verb_set(verb_type, 0.8f, 1.0f, 0.15f, fr);
    for (i = 0; i < n; i++) { sl[i] = l[i] * send; sr[i] = r[i] * send; }
    psx_verb_process(sl, sr, wl, wr, n);
    for (i = 0; i < n; i++) { l[i] = l[i] * (fr > 0.0f ? 0.0f : 1.0f) + 0.7f * wl[i]; r[i] = r[i] * (fr > 0.0f ? 0.0f : 1.0f) + 0.7f * wr[i]; }
}

/* dry seconds, then the effect; returns the effect part's rms against the dry part's (dB) */
static float run(track_t *t, fx_fn fx, float dry_s, float fx_s, float tail_s, float *peak_out, int *bad)
{
    uint32_t total = (uint32_t)((dry_s + fx_s + tail_s) * SR), i = 0, start = t->n, dry_n = (uint32_t)(dry_s * SR),
             fx_n = (uint32_t)(fx_s * SR);
    float spb = 60.0f * SR / bpm, peak = 0.0f;
    double ad = 0, af = 0;
    uint32_t nd = 0, nf = 0;
    *bad = 0;
    while (i < total) {
        float l[BLOCK], r[BLOCK];
        uint32_t k, n = total - i < BLOCK ? total - i : BLOCK;
        for (k = 0; k < n; k++) {
            float x = (i + k) < dry_n + fx_n ? brk[(i + k) % brk_n] : 0.0f;
            l[k] = r[k] = x * 0.8f;
        }
        if (i >= dry_n)
            fx(l, r, n, (float)(i - dry_n) / SR, (float)i / spb, spb);
        for (k = 0; k < n; k++) {
            float a = fabsf(l[k]) > fabsf(r[k]) ? fabsf(l[k]) : fabsf(r[k]);
            if (!(l[k] == l[k]) || !(r[k] == r[k])) *bad = 1;
            if (a > peak) peak = a;
            if (i + k < dry_n) { ad += (double)l[k] * l[k]; nd++; }
            else if (i + k < dry_n + fx_n) { af += 0.5 * ((double)l[k] * l[k] + (double)r[k] * r[k]); nf++; }
            put(t, l[k], r[k]);
        }
        i += n;
    }
    (void)start;
    *peak_out = peak;
    return 10.0f * log10f((float)((af / (nf ? nf : 1)) / (ad / (nd ? nd : 1) + 1e-12)));
}

/* a reverb's tail: an impulse (a short click) in, the time until the wet output falls 60 dB under its peak */
static float tail_t60(uint32_t type, float freeze)
{
    uint32_t i, last = 0, n = SR * 8u;
    float peak = 0.0f;
    float *e = malloc(n * sizeof(float));
    psx_verb_init(work, PSXV_WORDS);
    psx_verb_set(type, 1.0f, 1.0f, 0.0f, 0.0f);
    for (i = 0; i < n; i += BLOCK) {
        float in[BLOCK], wl[BLOCK], wr[BLOCK];
        uint32_t k;
        for (k = 0; k < BLOCK; k++) in[k] = (i + k) < 441u ? ((i + k) & 8u ? 0.5f : -0.5f) : 0.0f;
        if (freeze > 0.0f && i >= SR / 2u) psx_verb_set(type, 1.0f, 1.0f, 0.0f, freeze);
        psx_verb_process(in, in, wl, wr, BLOCK);
        for (k = 0; k < BLOCK && i + k < n; k++) e[i + k] = fabsf(wl[k]) + fabsf(wr[k]);
    }
    /* a 50 ms envelope */
    for (i = 0; i < n; i++) { float a = 0; uint32_t k; if (i % 2205) continue; for (k = 0; k < 2205 && i + k < n; k++) if (e[i + k] > a) a = e[i + k];
        if (a > peak) peak = a; if (a > peak * 0.001f) last = i; }
    free(e);
    return (float)last / SR;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "breaks/huge/cw_amen_jungle.wav", *out = argc > 3 ? argv[3] : ".";
    track_t tour = {0};
    float peak, lv;
    int bad;
    uint32_t v;
    bpm = argc > 2 ? (float)atof(argv[2]) : 175.0f;
    if (!load(path)) { printf("psx_demo: can't read %s\n", path); return 1; }
    psx_verb_init(work, PSXV_WORDS);
    psx_skip_init(skipring, 9216u);
    psx_init();

    {   /* the reverbs' tails: in the presets' order of size, CHAOS near forever, FREEZE holding */
        float t[PSXV_N];
        for (v = 0; v < PSXV_N; v++) { t[v] = tail_t60(v, 0.0f); printf("  %-11s tail %.2f s\n", PSXV_NAME[v], t[v]); }
        CHECK(t[PSXV_ROOM] < t[PSXV_HALL], "ROOM %.2f s should be shorter than HALL %.2f s", t[PSXV_ROOM], t[PSXV_HALL]);
        CHECK(t[PSXV_STUDIO_S] < t[PSXV_STUDIO_L], "STUDIO S %.2f s < STUDIO L %.2f s", t[PSXV_STUDIO_S], t[PSXV_STUDIO_L]);
        CHECK(t[PSXV_CHAOS] > 5.0f, "CHAOS rings past 5 s (%.2f)", t[PSXV_CHAOS]);
        CHECK(t[PSXV_DELAY] < 2.0f, "DELAY is one echo (%.2f s)", t[PSXV_DELAY]);
        CHECK(tail_t60(PSXV_ROOM, 1.0f) > 7.0f, "FREEZE holds ROOM's tail");
    }

    {   /* each effect over the break: no NaN, nothing far past full scale, the level near the dry one */
        static const struct { const char *name; fx_fn fx; float lo, hi; } FX[] = {
            {"crush", fx_crush, -3.0f, 3.0f}, {"redux", fx_redux, -4.0f, 2.0f}, {"grit", fx_grit, -3.0f, 2.0f},
            {"xa", fx_xa, -3.0f, 2.0f}, {"skip", fx_skip, -3.0f, 2.0f}};
        uint32_t k;
        for (k = 0; k < sizeof FX / sizeof FX[0]; k++) {
            track_t t = {0};
            psx_init();
            psx_verb_init(work, PSXV_WORDS);
            psx_skip_init(skipring, 9216u);
            lv = run(&t, FX[k].fx, 4.0f, 16.0f, 0.5f, &peak, &bad);
            printf("  %-11s level %+.1f dB against dry, peak %.2f\n", FX[k].name, lv, peak);
            CHECK(!bad, "%s: NaN", FX[k].name);
            CHECK(peak < 1.2f, "%s: peak %.2f", FX[k].name, peak);
            CHECK(lv > FX[k].lo && lv < FX[k].hi, "%s: level %+.1f dB", FX[k].name, lv);
            save(out, FX[k].name, &t);
            run(&tour, FX[k].fx, 2.0f, 12.0f, 0.5f, &peak, &bad);
            free(t.l); free(t.r);
        }
    }
    {   /* the nine reverbs: four bars each, the last bar frozen for CHAOS */
        track_t t = {0};
        for (v = 0; v < PSXV_N; v++) {
            psx_verb_init(work, PSXV_WORDS);
            verb_type = v;
            verb_freeze_at = -1.0f;
            lv = run(&t, fx_verb, 0.0f, 4.0f * 4.0f * 60.0f / bpm, 1.5f, &peak, &bad);
            printf("  %-11s peak %.2f\n", PSXV_NAME[v], peak);
            CHECK(!bad, "%s: NaN", PSXV_NAME[v]);
            CHECK(peak < 1.5f, "%s: peak %.2f", PSXV_NAME[v], peak);
        }
        save(out, "verbs", &t);
        psx_verb_init(work, PSXV_WORDS);
        verb_type = PSXV_SPACE_ECHO;
        verb_freeze_at = 6.0f;
        run(&tour, fx_verb, 0.0f, 10.0f, 2.0f, &peak, &bad);
        free(t.l); free(t.r);
    }
    save(out, "tour", &tour);
    printf("%d of %d PSX checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
