/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH's kick (kick.h); AFFINE's BOOM. */
#define BRK_ENGINE 1                     /* (LOAF BREAKER, 0.226: the maths inlined into the per-sample code, as
                                          * brk_fx.c's) */
#include "kick.h"
#include "brk.h"
#include "brk_math.h"

const char *const KICK_LAYER_NAME[KICK_LAYERS] = {"SUB", "PUNCH", "GRIT", "CLICK"};
const char *const KICK_PRESET_NAME[KICK_PRESETS] = {"909", "TECHNO", "EARLY HC", "ROTTERDAM", "INDUSTRIAL", "DISTORTO",
                                                     "TERROR", "SPINE", "909 DRIVE", "GABBER", "MILLENNIUM",
                                                     "DOOMCORE", "808", "JUNGLE", "DUB", "TIGHT"};

static inline float soft(float x) { return x / (1.0f + brk_absf(x)); }

/* one kick sounding, its layers' state (the screen's pictures render their own) */
typedef struct {
    float ph[KICK_LAYERS], ph2, env[KICK_LAYERS], pe, pe2;
    float lp;                            /* CLICK: its noise's low-pass (what it keeps is above it) */
    float pm;                            /* (crossbreed) this hit's pitch, a factor (kick_pitches): 1 as set */
    uint32_t rng;
    float last, declick;
} kv_t;

/* the block's settings, mapped */
typedef struct {
    float f[KICK_LAYERS];                /* each layer's frequency step (cycles a sample) */
    float dec[KICK_LAYERS];              /* each envelope's factor a sample */
    float lvl[KICK_LAYERS];
    float sub_sat, punch_sweep, grit_index, grit_noise, click_noise;
    float click_k;                       /* the click's high-pass coefficient */
    float sdrop, pe_dec, pe2_dec;        /* the SUB's drop, the pitch sweeps' factors */
    float g, hard, make, vol;            /* the sum: drive, how hard the clip, make-up, volume */
} kc_t;

static volatile kick_t p_k;
static volatile uint32_t p_hits;         /* a count: kick_hit */
static uint32_t hits_seen;
static kv_t kv;
static const float *ks_sp;               /* LOAF BREAKER: the vinyl's speed (kick_speed), 0 none */
static float ks_ph, ks_a, ks_b, ks_last; /* its phase between two of the kick's samples, the two; the last out */
static uint8_t ks_on;
static const float *ks_pm;               /* (crossbreed) the next render's hits' pitches (kick_pitches), 0 none */

/* the factor a sample that falls 40 dB (to where a kick is heard as gone) in ms milliseconds */
static float decay_factor(float ms) { return brk_exp2f_c(-6.644f / (ms * 0.001f * (float)BRK_SR)); }

/* the knobs' 0..1 as each layer's frequency and its decay (to -40 dB) */
static const float P_LO[KICK_LAYERS] = {30.0f, 50.0f, 40.0f, 1000.0f};
static const float P_OCT[KICK_LAYERS] = {1.585f, 2.0f, 3.32f, 3.585f};
static const float D_LO[KICK_LAYERS] = {60.0f, 20.0f, 20.0f, 1.0f};
static const float D_HI[KICK_LAYERS] = {1500.0f, 400.0f, 800.0f, 40.0f};
BRK_COLD float kick_pitch_hz(uint32_t layer, float p)
{
    layer %= KICK_LAYERS;
    return P_LO[layer] * brk_exp2f_c(brk_clampf(p, 0.0f, 1.0f) * P_OCT[layer]);
}
BRK_COLD float kick_decay_ms(uint32_t layer, float d)
{
    layer %= KICK_LAYERS;
    d = brk_clampf(d, 0.0f, 1.0f);
    return D_LO[layer] + (D_HI[layer] - D_LO[layer]) * d * d;
}

BRK_COLD static void kc_make(const kick_t *k, kc_t *c)
{
    uint32_t i;
    for (i = 0; i < KICK_LAYERS; i++) {
        const kick_layer_t *l = &k->l[i];
        c->f[i] = kick_pitch_hz(i, l->pitch) / (float)BRK_SR;
        c->dec[i] = decay_factor(kick_decay_ms(i, l->decay));
        c->lvl[i] = brk_clampf(l->level, 0.0f, 1.0f);
    }
    /* (AFFINE: the SUB's tone from a pure sine (0.12: the soft clip barely touches it) up to LOAF's most saturated) */
    c->sub_sat = 0.12f * brk_exp2f_c(5.4f * brk_clampf(k->l[KL_SUB].tone, 0.0f, 1.0f));
    c->punch_sweep = 3.0f + 14.0f * brk_clampf(k->l[KL_PUNCH].tone, 0.0f, 1.0f);
    c->grit_index = 0.5f + 7.0f * brk_clampf(k->l[KL_GRIT].tone, 0.0f, 1.0f);
    c->grit_noise = 0.5f * brk_clampf(k->l[KL_GRIT].tone, 0.0f, 1.0f);
    c->click_noise = brk_clampf(k->l[KL_CLICK].tone, 0.0f, 1.0f);
    c->click_k = brk_clampf(brk_tan_pi_c(c->f[KL_CLICK]) * 2.0f, 0.02f, 0.98f);
    c->sdrop = 2.0f * brk_exp2f_c(brk_clampf((float)k->drop, -50.0f, 50.0f) * 0.04f);
    c->pe_dec = decay_factor(120.0f * brk_exp2f_c(brk_clampf((float)k->dtime, -50.0f, 50.0f) * 0.04f));
    c->pe2_dec = decay_factor(40.0f);    /* PUNCH's fast sweep */
    {
        float in = brk_clampf(k->intensity, 0.0f, 1.0f), sg;
        c->g = 0.3f + 30.0f * in * in;   /* (0: near clean, the clip only rounding the peaks) */
        c->hard = in > 0.5f ? (in - 0.5f) * 1.6f : 0.0f;
        sg = c->g / (1.0f + c->g);       /* a full-scale sum through the clip: kept there */
        c->make = 0.95f / (sg * (1.0f - c->hard) + c->hard);
    }
    c->vol = brk_clampf(k->volume, 0.0f, 1.0f);
}

static void kv_hit(kv_t *v)
{
    uint32_t i;
    v->declick = v->last;                /* the kick still sounding fades out under the new one */
    for (i = 0; i < KICK_LAYERS; i++) {
        v->ph[i] = 0.0f;
        v->env[i] = 1.0f;
    }
    v->ph2 = 0.0f;
    v->pe = v->pe2 = 1.0f;
    v->lp = 0.0f;
}

static inline float noise(kv_t *v)
{
    v->rng = v->rng * 1664525u + 1013904223u;
    return (float)(int32_t)v->rng * (1.0f / 2147483648.0f);
}

static inline float kv_tick(kv_t *v, const kc_t *c)
{
    float x[KICK_LAYERS], y = 0.0f, d, out;
    uint32_t i;
    /* SUB: a sine dropping to its pitch, saturated by its tone */
    x[KL_SUB] = soft(brk_sin_turns(v->ph[KL_SUB]) * c->sub_sat) * (1.0f + c->sub_sat) / c->sub_sat;
    v->ph[KL_SUB] += c->f[KL_SUB] * v->pm * (1.0f + c->sdrop * v->pe);
    /* PUNCH: a sine swept down fast from far above */
    x[KL_PUNCH] = brk_sin_turns(v->ph[KL_PUNCH]);
    v->ph[KL_PUNCH] += c->f[KL_PUNCH] * v->pm * (1.0f + c->punch_sweep * v->pe2);
    /* GRIT: FM (the modulator at 1.41x), driven, noise by its tone */
    x[KL_GRIT] = soft(4.0f * (brk_sin_turns(v->ph[KL_GRIT] + c->grit_index * 0.159f * brk_sin_turns(v->ph2)) +
                              c->grit_noise * noise(v)));
    v->ph[KL_GRIT] += c->f[KL_GRIT] * v->pm * (1.0f + v->pe);
    v->ph2 += c->f[KL_GRIT] * v->pm * 1.41f * (1.0f + v->pe);
    /* CLICK: noise high-passed at its pitch, and a click ringing there; tone between them */
    {
        float n = noise(v), ring = brk_sin_turns(v->ph[KL_CLICK]);
        v->lp += (n - v->lp) * c->click_k;
        x[KL_CLICK] = (n - v->lp) * c->click_noise + ring * (1.0f - c->click_noise);
        v->ph[KL_CLICK] += c->f[KL_CLICK];
    }
    for (i = 0; i < KICK_LAYERS; i++) {
        if (v->ph[i] >= 1.0f)
            v->ph[i] -= (float)(uint32_t)v->ph[i];
        y += x[i] * v->env[i] * c->lvl[i];
        v->env[i] *= c->dec[i];
    }
    if (v->ph2 >= 1.0f)
        v->ph2 -= (float)(uint32_t)v->ph2;
    v->pe *= c->pe_dec;
    v->pe2 *= c->pe2_dec;
    /* the sum: a drive into a clip, soft and then (INTENSITY over half) hard */
    d = y * c->g;
    out = (soft(d) * (1.0f - c->hard) + brk_clampf(d, -1.0f, 1.0f) * c->hard) * c->make * c->vol;
    v->last = out;
    out += v->declick;
    v->declick *= 0.985f;
    return out;
}

BRK_COLD void kick_init(void)
{
    uint32_t i;
    for (i = 0; i < KICK_LAYERS; i++)
        kv.env[i] = 0.0f;
    kv.declick = kv.last = 0.0f;
    kv.rng = 0x5eed1u;
    kv.pm = 1.0f;
    ks_last = 0.0f;
    ks_on = 0u;
    hits_seen = p_hits;
}

BRK_COLD void kick_set(const kick_t *k) { *(kick_t *)&p_k = *k; }
void kick_hit(void) { p_hits++; }
void kick_speed(const float *sp) { ks_sp = sp; }
void kick_pitches(const float *pm) { ks_pm = pm; }

void kick_render(float *out, uint32_t n, const uint16_t *at, uint32_t nat)
{
    kick_t k = *(const kick_t *)&p_k;
    kc_t c;
    uint32_t i, j = 0;
    if (p_hits != hits_seen) {           /* a MIDI note: now */
        hits_seen = p_hits;
        kv_hit(&kv);
        kv.pm = 1.0f;
    } else if (!nat && kv.env[KL_SUB] < 3e-5f && kv.env[KL_PUNCH] < 3e-5f && kv.env[KL_GRIT] < 3e-5f &&
               kv.env[KL_CLICK] < 3e-5f && brk_absf(kv.declick) < 1e-5f) {
        for (i = 0; i < n; i++)          /* gone quiet, nothing coming: no work */
            out[i] = 0.0f;
        kv.last = ks_last = 0.0f;
        ks_on = 0u;
        return;
    }
    kc_make(&k, &c);
    if (ks_sp && !ks_on) {               /* the vinyl begins: from the kick as it is */
        ks_a = ks_b = ks_last;
        ks_ph = 0.0f;
    }
    ks_on = ks_sp != 0;
    for (i = 0; i < n; i++) {
        while (j < nat && at[j] <= i) {  /* the steps' kicks, on their samples */
            kv_hit(&kv);
            kv.pm = ks_pm ? ks_pm[j] : 1.0f;   /* (crossbreed: this step's note) */
            j++;
        }
        if (!ks_sp)
            out[i] = kv_tick(&kv, &c);
        else {                           /* the vinyl: the kick's own time runs at the speed, read between */
            float s = ks_sp[i];
            ks_ph += s;
            if (ks_ph >= 1.0f) {
                ks_ph -= 1.0f;
                ks_a = ks_b;
                ks_b = kv_tick(&kv, &c);
            }
            out[i] = (ks_a + (ks_b - ks_a) * ks_ph) * (s < 0.02f ? s * 50.0f : 1.0f);
        }
    }
    ks_last = out[n - 1u];
}

/* a hit of k offline, for the pictures: a voice of its own, from silence */
BRK_COLD static void kv_offline(const kick_t *k, kc_t *c, kv_t *v)
{
    kc_make(k, c);
    v->rng = 0x5eed1u;
    v->pm = 1.0f;
    v->last = 0.0f;
    kv_hit(v);
    v->declick = 0.0f;
}
BRK_COLD void kick_shape(const kick_t *k, uint8_t *px, uint32_t n, uint32_t ms, uint32_t height)
{
    kc_t c;
    kv_t v;
    uint32_t i, total = ms * (BRK_SR / 1000u), per = total / (n ? n : 1u);
    kv_offline(k, &c, &v);
    for (i = 0; i < n; i++) {
        uint32_t s;
        float pk = 0.0f;
        for (s = 0; s < per; s++) {
            float a = brk_absf(kv_tick(&v, &c));
            if (a > pk)
                pk = a;
        }
        px[i] = (uint8_t)(brk_clampf(pk, 0.0f, 1.0f) * (float)height + 0.5f);
    }
}
BRK_COLD void kick_wave(const kick_t *k, int8_t *px, uint32_t n, uint32_t ms, uint32_t height)
{
    kc_t c;
    kv_t v;
    uint32_t i, total = ms * (BRK_SR / 1000u), per = total / (n ? n : 1u);
    kv_offline(k, &c, &v);
    for (i = 0; i < n; i++) {
        uint32_t s;
        float best = 0.0f;
        for (s = 0; s < per; s++) {
            float x = kv_tick(&v, &c);
            if (brk_absf(x) > brk_absf(best))
                best = x;
        }
        px[i] = (int8_t)(brk_clampf(best, -1.0f, 1.0f) * (float)height);
    }
}

/* the built-in kicks: per layer level, pitch, decay, tone; then intensity. 0..11 LOAF BREAKER's (their DISTO, SYNTH,
 * KICK LAB and EQ parts gone with those, AFFINE), 12..15 BOOM's */
static const float PRESET[KICK_PRESETS][KICK_LAYERS * 4 + 1] = {
    {0.85f, 0.50f, 0.62f, 0.15f, 0.75f, 0.55f, 0.35f, 0.55f, 0.00f, 0.30f, 0.30f, 0.30f, 0.25f, 0.25f, 0.25f, 0.20f,
     0.20f},                             /* 909 */
    {0.90f, 0.25f, 0.58f, 0.30f, 0.60f, 0.40f, 0.30f, 0.35f, 0.15f, 0.25f, 0.25f, 0.20f, 0.30f, 0.55f, 0.20f, 0.50f,
     0.35f},                             /* TECHNO */
    {0.90f, 0.55f, 0.52f, 0.85f, 0.85f, 0.60f, 0.50f, 0.75f, 0.20f, 0.40f, 0.30f, 0.40f, 0.40f, 0.55f, 0.20f, 0.30f,
     0.80f},                             /* EARLY HC */
    {0.90f, 0.30f, 0.60f, 0.60f, 0.70f, 0.45f, 0.45f, 0.70f, 0.55f, 0.30f, 0.55f, 0.70f, 0.35f, 0.60f, 0.20f, 0.50f,
     0.85f},                             /* ROTTERDAM */
    {0.85f, 0.25f, 0.70f, 0.55f, 0.60f, 0.50f, 0.35f, 0.50f, 0.55f, 0.45f, 0.55f, 0.55f, 0.40f, 0.45f, 0.25f, 0.70f,
     0.75f},                             /* INDUSTRIAL */
    {0.85f, 0.35f, 0.55f, 0.80f, 0.85f, 0.50f, 0.40f, 0.80f, 0.70f, 0.40f, 0.50f, 0.80f, 0.40f, 0.55f, 0.25f, 0.45f,
     1.00f},                             /* DISTORTO */
    {0.90f, 0.30f, 0.92f, 0.60f, 0.70f, 0.45f, 0.45f, 0.70f, 0.00f, 0.30f, 0.55f, 0.70f, 0.35f, 0.60f, 0.20f, 0.50f,
     0.69f},                             /* TERROR */
    {0.90f, 0.30f, 0.70f, 0.60f, 0.70f, 0.45f, 0.45f, 0.70f, 0.00f, 0.30f, 0.55f, 0.70f, 0.35f, 0.60f, 0.20f, 0.50f,
     0.50f},                             /* SPINE */
    {0.85f, 0.50f, 0.65f, 0.35f, 0.80f, 0.55f, 0.40f, 0.65f, 0.10f, 0.30f, 0.30f, 0.30f, 0.30f, 0.30f, 0.25f, 0.25f,
     0.75f},                             /* 909 DRIVE */
    {0.90f, 0.45f, 0.66f, 0.70f, 0.80f, 0.50f, 0.45f, 0.75f, 0.25f, 0.30f, 0.40f, 0.40f, 0.35f, 0.50f, 0.20f, 0.40f,
     0.95f},                             /* GABBER */
    {0.90f, 0.35f, 0.85f, 0.55f, 0.75f, 0.50f, 0.40f, 0.70f, 0.15f, 0.30f, 0.50f, 0.40f, 0.30f, 0.55f, 0.20f, 0.40f,
     0.85f},                             /* MILLENNIUM */
    {0.95f, 0.12f, 0.95f, 0.60f, 0.65f, 0.35f, 0.50f, 0.50f, 0.20f, 0.20f, 0.60f, 0.30f, 0.25f, 0.30f, 0.20f, 0.30f,
     0.80f},                             /* DOOMCORE */
    /* AFFINE's BOOM (15 Nov 2026): clean subs on a note, the drive low (SUB pitch: E1 0.289, G1 0.447, D1 0.184, A1
     * 0.552) */
    {0.95f, 0.289f, 0.765f, 0.05f, 0.22f, 0.40f, 0.25f, 0.30f, 0.00f, 0.30f, 0.30f, 0.30f, 0.04f, 0.45f, 0.20f, 0.20f,
     0.10f},                             /* 808: E1, ~900 ms, a gentle drop */
    {0.90f, 0.447f, 0.486f, 0.20f, 0.50f, 0.50f, 0.30f, 0.50f, 0.00f, 0.30f, 0.30f, 0.30f, 0.12f, 0.50f, 0.22f, 0.55f,
     0.22f},                             /* JUNGLE: G1, ~400 ms, punchier */
    {0.95f, 0.184f, 0.890f, 0.00f, 0.12f, 0.30f, 0.30f, 0.20f, 0.00f, 0.30f, 0.30f, 0.30f, 0.00f, 0.40f, 0.20f, 0.20f,
     0.05f},                             /* DUB: D1, ~1.2 s, deep and soft */
    {0.90f, 0.552f, 0.250f, 0.15f, 0.50f, 0.55f, 0.20f, 0.60f, 0.00f, 0.30f, 0.30f, 0.30f, 0.25f, 0.60f, 0.25f, 0.40f,
     0.20f},                             /* TIGHT: A1, ~150 ms, a little click */
};
/* their SUB's DROP and its TIME (kick_t's drop, dtime; LOAF's kicks 0, 0: x3 over 120 ms, as their engine had it) */
static const int8_t PRESET_D[KICK_PRESETS][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {-30, -5}, {0, -20}, {-40, 10}, {10, -30},
};

BRK_COLD void kick_preset(kick_t *k, uint32_t i)
{
    const float *p = PRESET[i % KICK_PRESETS];
    uint32_t l;
    for (l = 0; l < KICK_LAYERS; l++) {
        k->l[l].level = p[l * 4u];
        k->l[l].pitch = p[l * 4u + 1u];
        k->l[l].decay = p[l * 4u + 2u];
        k->l[l].tone = p[l * 4u + 3u];
    }
    k->intensity = p[KICK_LAYERS * 4u];
    k->drop = PRESET_D[i % KICK_PRESETS][0];
    k->dtime = PRESET_D[i % KICK_PRESETS][1];
    k->rsv[0] = k->rsv[1] = 0u;
}

BRK_COLD void kick_defaults(kick_t *k)
{
    kick_preset(k, KICK_PRESET_808);     /* (AFFINE: BOOM's 808) */
    k->volume = 0.8f;
}
