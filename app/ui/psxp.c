/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: the PSX page, FX's fourth part (controls phase 2; EFFECTS' fifth tab before), the PSX bus's settings (psx.h;
 * HOME's knobs keep the amounts: FILTER, CRUSH, REDUX, VERB). Seven items on OCT: PRESET (K1 the four presets, psx.h
 * PSXP_*, every stage at once; PRESETS before), then:
 *   VERB   TYPE (the nine), SIZE (on CHAOS and DELAY in note values at the tempo), TONE, DEPTH; the picture: the
 *          preset's own early echoes (its combs, from its registers) and its tail, the sound going in lit
 *   CODEC  MODE (OFF, GRIT, XA), RATE, BITS, MIX; the picture: a wave as the codec's steps hold it
 *   COLOR  AMOUNT, RATE (GRIT's, 44K, 22K, 22K RAW, 11K, 11K RAW, 8K, 8K RAW), CRUNCH (PCM below 0), SMOOTH; the
 *          picture: the wave at the rate, played back as SMOOTH does it, the codec's noise floor rising with CRUNCH,
 *          the interpolation's kernel (box, triangle, psx-spx's gaussian)
 *   LOFI   CRUSH's MODE and MASK, REDUX's MODE and JITTER; the picture: the bits' staircase
 *   SKIP   LENGTH, CHANCE, SEEK, MIX; the picture: the disc, the slipping sector lit
 *   OUT    CONSOLE, MONO, SEND (the PSX's own into SPU VERB), INPUT (SPU VERB's): the SPU's mixer and the console's
 *          output (VERB's four knobs are taken: TYPE, SIZE, TONE, DEPTH); the picture: CONSOLE's roll-off and clip
 *          curve, the reverb's input as INPUT drives it and SEND fills it, the two speakers (one dot: MONO)
 * Drawn whole in the POLYGON look (app/ui/aff.c), as HOME. Part of app.c's unity, after aff.c. */

enum { PP_PRESET, PP_VERB, PP_CODEC, PP_COLOR, PP_LOFI, PP_SKIP, PP_OUT, PP_N };
static const char *const PP_NAME[PP_N] = {"PRESET", "VERB", "CODEC", "COLOR", "LOFI", "SKIP", "OUT"};
static uint8_t pp_part;                  /* the item shown (not saved; AFFINE, controls phase 2: OCT walks them, PRESET
                                          * first, its K1 the four presets) */
static const char *const VERB_SHORT[PSXV_N] = {"ROOM", "STD S", "STD M", "STD L", "HALL", "HALF", "SPACE", "CHAOS",
                                               "DELAY"};   /* (the presets' names short: HOME's picture, this page) */
static const float GRIT_RATES[6] = {11025.0f, 16000.0f, 22050.0f, 32000.0f, 37800.0f, 44100.0f};

static const char *psxp_name(int i)
{
    static const char *const N[PP_N][4] = {{"PRESET", "", "", ""}, {"TYPE", "SIZE", "TONE", "DEPTH"},
                                           {"MODE", "RATE", "BITS", "MIX"},
                                           {"AMOUNT", "RATE", "CRUNCH", "SMOOTH"}, {"CRUSH", "MASK", "REDUX", "JITTER"},
                                           {"LENGTH", "CHANCE", "SEEK", "MIX"}, {"CONSOLE", "MONO", "SEND", "INPUT"}};
    return N[pp_part % PP_N][i & 3];
}

/* SIZE on CHAOS and DELAY (their repeats clean, on one period: psx_verb_taps' first comb): note values at the tempo.
 * SIZE keeps its meaning (the addresses' scale); the knob steps between the notes that fit (1/16 .. 3/4) */
#define PP_NOTES 7
static const float PP_NOTE_B[PP_NOTES] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};   /* in beats */
static const char *const PP_NOTE_NAME[PP_NOTES] = {"1/16", "1/8", "3/16", "1/4", "3/8", "1/2", "3/4"};
static int pp_echo(void) { return set.psx.vtype == PSXV_CHAOS || set.psx.vtype == PSXV_DELAY; }
static float pp_note_size(uint32_t k)    /* SIZE that puts the repeat on note k at the tempo; < 0 or > 1: it can't */
{
    float ms[4], g[4], bpm = (float)set.tempo_x10 * 0.1f, per;
    psx_verb_taps(set.psx.vtype, 1.0f, ms, g);   /* (the period at SIZE 100 %: scale 1) */
    per = PP_NOTE_B[k % PP_NOTES] * 60000.0f / (bpm > 40.0f ? bpm : 40.0f);
    return (per / ms[0] - 0.25f) / 0.75f;
}
static int pp_note_at(void)              /* the note SIZE is on now, -1 none */
{
    uint32_t k;
    for (k = 0; k < PP_NOTES; k++) {
        float z = pp_note_size(k) - set.psx.vsize;
        if (z < 0.004f && z > -0.004f)
            return (int)k;
    }
    return -1;
}
static void pp_note_turn(int32_t d)      /* SIZE to the next note that fits, up or down */
{
    int32_t k;
    float best = -1.0f;
    for (k = 0; k < PP_NOTES; k++) {
        float z = pp_note_size((uint32_t)k);
        if (z < 0.0f || z > 1.0f)
            continue;
        if ((d > 0 && z > set.psx.vsize + 0.004f && (best < 0.0f || z < best)) ||
            (d < 0 && z < set.psx.vsize - 0.004f && (best < 0.0f || z > best)))
            best = z;
    }
    if (best >= 0.0f)
        set.psx.vsize = best;
}

static void pp_db_text(float g, char *t)   /* INPUT: 2 -> "+6 DB" */
{
    int32_t db = (int32_t)(6.0206f * brk_log2f(g) + (g < 1.0f ? -0.5f : 0.5f));
    t[0] = db > 0 ? '+' : 0;
    t[1] = 0;
    int_text(db, t + (db > 0 ? 1 : 0));
    text_cat(t, " DB");
}
static float pp_db_norm(float g) { return brk_clampf((6.0206f * brk_log2f(g) + 12.0f) / 24.0f, 0.0f, 1.0f); }

static uint32_t grit_rate_idx(float hz)
{
    uint32_t k, best = 0u;
    for (k = 1u; k < 6u; k++)
        if ((hz > GRIT_RATES[k] ? hz - GRIT_RATES[k] : GRIT_RATES[k] - hz) <
            (hz > GRIT_RATES[best] ? hz - GRIT_RATES[best] : GRIT_RATES[best] - hz))
            best = k;
    return best;
}

static void psxp_value(int i, char *t)
{
    const psx_bus_t *b = &set.psx;
    t[0] = 0;
    switch (pp_part % PP_N) {
    case PP_PRESET:                      /* (K1: the preset the stages came from, its place, 2/4; "-" once a stage is
                                          * turned. Its name is the picture's, lit, and the header's as it turns: the
                                          * panel's cell holds no SUBTLE GLUE beside its name) */
        if (i == 0 && set.pxc.preset) {
            int_text((int32_t)set.pxc.preset, t);
            text_cat(t, "/4");
        } else if (i == 0)
            text_cat(t, "-");
        break;
    case PP_VERB:
        if (i == 0)
            text_cat(t, VERB_SHORT[b->vtype % PSXV_N]);
        else if (i == 1 && pp_echo() && pp_note_at() >= 0)
            text_cat(t, PP_NOTE_NAME[pp_note_at()]);
        else if (i == 1) {               /* SIZE: the presets' addresses scaled, a quarter .. as the console */
            int_text((int32_t)((0.25f + 0.75f * b->vsize) * 100.0f + 0.5f), t);
            text_cat(t, "%");
        } else if (i == 2)
            text_cat(t, b->vtone < 0.05f ? "OPEN" : b->vtone < 0.5f ? "WARM" : "DARK");
        else
            pct_text(b->vdepth, t);
        break;
    case PP_CODEC:
        if (i == 0)
            text_cat(t, CODEC_NAME[b->codec % CODECS]);
        else if (i == 1) {
            float hz = b->codec == CODEC_XA ? (b->xhalf ? 18900.0f : 37800.0f) : b->crate;
            tenths_text((int32_t)(hz / 100.0f + 0.5f), t);
            text_cat(t, "K");
        } else if (i == 2) {
            int_text(b->codec == CODEC_XA ? 4 : b->cbits, t);
            text_cat(t, " BIT");
        } else
            pct_text(b->cmix, t);
        break;
    case PP_COLOR: {
        const psx_color_t *c = &set.pxc;
        if (i == 0)
            pct_text(c->amount, t);
        else if (i == 1 && c->rate == PSXR_GRIT) {   /* (GRIT's step: CODEC's RATE) */
            tenths_text((int32_t)(b->crate / 100.0f + 0.5f), t);
            text_cat(t, "K");
        } else if (i == 1)
            text_cat(t, PSXR_NAME[c->rate % PSXR_N]);
        else if (i == 2) {
            if (c->adpcm)
                pct_text(c->crunch, t);
            else
                text_cat(t, "PCM");
        } else
            pct_text(c->smooth, t);
        break;
    }
    case PP_OUT: {
        const psx_color_t *c = &set.pxc;
        if (i == 0)
            pct_text(c->console, t);
        else if (i == 1)
            text_cat(t, c->mono ? "MONO" : "STEREO");
        else if (i == 2)
            pct_text(c->vsend, t);
        else
            pp_db_text(c->vinput, t);
        break;
    }
    case PP_LOFI:
        if (i == 0)
            text_cat(t, CRUSH_MODE_NAME[b->cmode % CRUSH_MODES]);
        else if (i == 1) {
            if (b->cmask < 0.05f)
                text_cat(t, "OFF");
            else {
                text_cat(t, "BIT ");
                int_text(4 + (int32_t)(b->cmask * 10.0f + 0.5f), t + 4);
            }
        } else if (i == 2)
            text_cat(t, REDUX_MODE_NAME[b->rmode % REDUX_MODES]);
        else
            pct_text(b->rjit, t);
        break;
    default:
        if (i == 0)
            text_cat(t, SKIP_DIV_NAME[b->skdiv & 3u]);
        else if (i == 1)
            pct_text(b->skchance, t);
        else if (i == 2)
            pct_text(b->skseek, t);
        else
            pct_text(b->skmix, t);
        break;
    }
}

static float psxp_norm(int i)            /* the knob's gauge, 0..1, -1 none (a choice) */
{
    const psx_bus_t *b = &set.psx;
    switch (pp_part % PP_N) {
    case PP_PRESET: return -1.0f;
    case PP_VERB: return i == 0 ? -1.0f : i == 1 ? b->vsize : i == 2 ? b->vtone : b->vdepth;
    case PP_CODEC: return i == 3 ? b->cmix : -1.0f;
    case PP_COLOR: return i == 0 ? set.pxc.amount : i == 1 ? -1.0f : i == 2 ? set.pxc.crunch : set.pxc.smooth;
    case PP_OUT: return i == 0 ? set.pxc.console : i == 1 ? -1.0f : i == 2 ? set.pxc.vsend : pp_db_norm(set.pxc.vinput);
    case PP_LOFI: return i == 1 ? b->cmask : i == 3 ? b->rjit : -1.0f;
    default: return i == 0 ? -1.0f : i == 1 ? b->skchance : i == 2 ? b->skseek : b->skmix;
    }
}

static void psxp_preset_turn(int32_t d);
static void psxp_turn(int i, int32_t d)
{
    psx_bus_t *b = &set.psx;
    char t[16];
    int s = d > 0 ? 1 : -1;
    switch (pp_part % PP_N) {
    case PP_PRESET:                      /* (K1: the four presets, every stage at once; PRESETS until phase 2) */
        if (i == 0)
            psxp_preset_turn(d);
        return;
    case PP_VERB:
        if (i == 0)
            b->vtype = (uint8_t)clamp((int32_t)b->vtype + s, 0, PSXV_N - 1);
        else if (i == 1 && pp_echo())
            pp_note_turn(d);             /* (CHAOS, DELAY: the repeat on a note at the tempo) */
        else if (i == 1)
            b->vsize = knob_f(b->vsize, d, 0.02f, 0.0f, 1.0f);
        else if (i == 2)
            b->vtone = knob_f(b->vtone, d, 0.02f, 0.0f, 1.0f);
        else
            b->vdepth = knob_f(b->vdepth, d, 0.02f, 0.0f, 1.0f);
        break;
    case PP_CODEC:
        if (i == 0)
            b->codec = (uint8_t)clamp((int32_t)b->codec + s, 0, CODECS - 1);
        else if (i == 1) {
            if (b->codec == CODEC_XA)
                b->xhalf = (uint8_t)(s < 0);
            else
                b->crate = GRIT_RATES[clamp((int32_t)grit_rate_idx(b->crate) + s, 0, 5)];
        } else if (i == 2)
            b->cbits = (uint8_t)clamp((int32_t)b->cbits + s, 2, 4);
        else
            b->cmix = knob_f(b->cmix, d, 0.02f, 0.0f, 1.0f);
        break;
    case PP_COLOR: {
        psx_color_t *c = &set.pxc;
        if (i > 0 && b->codec == CODEC_OFF)
            b->codec = CODEC_GRIT;       /* (the colour is GRIT's path: a turn here switches it on) */
        if (i == 0)
            c->amount = knob_f(c->amount, d, 0.02f, 0.0f, 1.0f);
        else if (i == 1)
            c->rate = (uint8_t)clamp((int32_t)c->rate + s, 0, PSXR_N - 1);
        else if (i == 2) {               /* CRUNCH, and below 0 the codec off: 16-bit PCM at the rate */
            if (!c->adpcm) {
                if (s > 0) {
                    c->adpcm = 1u;
                    c->crunch = 0.0f;
                }
            } else if (s < 0 && c->crunch <= 0.0f)
                c->adpcm = 0u;
            else
                c->crunch = knob_f(c->crunch, d, 0.02f, 0.0f, 1.0f);
        } else
            c->smooth = knob_f(c->smooth, d, 0.02f, 0.0f, 1.0f);
        break;
    }
    case PP_OUT: {
        psx_color_t *c = &set.pxc;
        if (i == 0)
            c->console = knob_f(c->console, d, 0.02f, 0.0f, 1.0f);
        else if (i == 1)
            c->mono = (uint8_t)(s > 0);
        else if (i == 2)
            c->vsend = knob_f(c->vsend, d, 0.02f, 0.0f, 1.0f);
        else {                           /* INPUT: whole dB, -12 .. +12 */
            int32_t db = (int32_t)(6.0206f * brk_log2f(c->vinput) + (c->vinput < 1.0f ? -0.5f : 0.5f)) + s;
            db = clamp(db, -12, 12);
            c->vinput = brk_exp2f((float)db / 6.0206f);
        }
        break;
    }
    case PP_LOFI:
        if (i == 0)
            b->cmode = (uint8_t)clamp((int32_t)b->cmode + s, 0, CRUSH_MODES - 1);
        else if (i == 1)
            b->cmask = knob_f(b->cmask, d, 0.1f, 0.0f, 1.0f);
        else if (i == 2)
            b->rmode = (uint8_t)clamp((int32_t)b->rmode + s, 0, REDUX_MODES - 1);
        else
            b->rjit = knob_f(b->rjit, d, 0.02f, 0.0f, 1.0f);
        break;
    default:
        if (i == 0)
            b->skdiv = (uint8_t)clamp((int32_t)b->skdiv + s, 0, 3);
        else if (i == 1)
            b->skchance = knob_f(b->skchance, d, 0.02f, 0.0f, 1.0f);
        else if (i == 2)
            b->skseek = knob_f(b->skseek, d, 0.02f, 0.0f, 1.0f);
        else
            b->skmix = knob_f(b->skmix, d, 0.02f, 0.0f, 1.0f);
        break;
    }
    psxp_value(i, t);
    app_say(psxp_name(i), t);
    changed_fx();
}

static void psxp_preset_turn(int32_t d)  /* PRESET's K1: the four presets, every stage at once */
{
    int32_t k = set.pxc.preset;
    if (!k)
        k = d > 0 ? 1 : PSXP_N;
    else
        k = clamp(k + (d > 0 ? 1 : -1), 1, PSXP_N);
    psx_preset((uint32_t)k - 1u, &set.psx, &set.pxc, (float)set.tempo_x10 * 0.1f);
    app_say("PRESET", PSXP_NAME[k - 1]);
    changed_fx();
}

/* ---------------------------------------------------------------------------------------------- the picture --- */
/* the stage: rows 44..186 between the header and the panel */
#define PP_TOP 44
#define PP_BOT 186
/* a text at the screen's (x, y) (y: its capitals' top), in the strip at band: aff_text, and the host's text audit sees
 * it (affont.c: past an edge, under 2x, two that overlap); each placed inside one strip */
static int32_t pp_text(int32_t x, int32_t y, const char *s, uint16_t c, int32_t band, int align)
{
    int32_t x0 = align == 1 ? x - aff_text_w(s, 2) : align == 2 ? x - aff_text_w(s, 2) / 2 : x;
    if (y + (int32_t)AFF_CAP_H * 2 <= band || y >= band + 60)
        return x0 + aff_text_w(s, 2);    /* (another strip's) */
    aff_aud_add(x0, y - band, 2, s);
    return aff_text(x, y, s, 2, c, band, align);
}
static void pp_px(int32_t x, int32_t y, uint16_t c, int32_t band)
{
    if (y >= band && y < band + 60 && x >= 0 && x < 240)
        cv_pset(x, y - band, c);
}
static void pp_span(int32_t x, int32_t y0, int32_t y1, uint16_t c, int32_t band)   /* a column from y0 to y1 */
{
    int32_t y, a = y0 < y1 ? y0 : y1, b = y0 < y1 ? y1 : y0;
    for (y = a; y <= b; y++)
        pp_px(x, y, c, band);
}
static void pp_vbar(int32_t x, int32_t y0, int32_t y1, const a_ramp_t *r, int32_t band)
{
    int32_t y;
    for (y = y0 < band ? band : y0; y < y1 && y < band + 60; y++)
        cv_pset(x, y - band, a_dith(r, (y1 - y) * 256 / (y1 - y0 + 1) / 2 + 128, x, y));
}

static void pp_draw_verb(int32_t band, uint32_t now)
{
    const psx_bus_t *b = &set.psx;
    float ms[4], g[4], span, tail = psx_verb_tail(b->vtype) * (0.25f + 0.75f * b->vsize);
    int32_t k, x, base = PP_BOT - 14;
    psx_verb_taps(b->vtype, b->vsize, ms, g);
    span = ms[0];
    for (k = 1; k < 4; k++)
        if (ms[k] > span && (g[k] > 0.01f || g[k] < -0.01f))
            span = ms[k];
    span = span * 1.25f + 10.0f;
    /* the tail: a dithered decay from the sound in (x 12) to its end, -60 dB at the right edge scaled to the tail */
    for (x = 12; x < 228; x++) {
        float tt = (float)(x - 12) / 216.0f * (span * 0.001f) / (tail > 0.05f ? tail : 0.05f);
        int32_t h = (int32_t)(96.0f * brk_exp2f(-tt * 9.97f)), y;   /* (-60 dB over the tail: 2^-9.97) */
        for (y = base - h; y < base; y++)
            if (y >= band && y < band + 60 && y >= PP_TOP)
                cv_pset(x, y - band, a_dith(&ar_floor1, 96 + (y - (base - h)) * 2, x, y));
    }
    /* the sound going in, then the four echoes: pillars where the combs read, as tall as their gains */
    pp_vbar(12, base - 110, base, &ar_lane, band);
    pp_vbar(13, base - 110, base, &ar_lane, band);
    for (k = 0; k < 4; k++) {
        float a = g[k] < 0.0f ? -g[k] : g[k];
        int32_t px = 12 + (int32_t)(ms[k] / span * 216.0f), h = (int32_t)(a * 110.0f);
        if (a < 0.01f || px >= 228)
            continue;
        pp_vbar(px, base - h, base, &ar_bar, band);
        pp_vbar(px + 1, base - h, base, &ar_bar, band);
    }
    if (base >= band && base < band + 60)
        cv_rect(8, base - band, 224, 1, A_DIM);
    {                                    /* the tail's length and the span drawn, under the axis */
        char t[16];
        t[0] = 0;
        tenths_text((int32_t)(tail * 10.0f + 0.5f), t);
        text_cat(t, " S");
        pp_text(8, base + 9, "TAIL", A_DIM, band, 0);   /* (rows 181..190: in the strip 180..239) */
        pp_text(232, base + 9, t, A_INK, band, 1);
    }
    (void)now;
}

static void pp_draw_codec(int32_t band, uint32_t now)
{
    const psx_bus_t *b = &set.psx;
    float hz = b->codec == CODEC_XA ? (b->xhalf ? 18900.0f : 37800.0f) : b->crate;
    int32_t bits = b->codec == CODEC_XA ? 4 : b->cbits, x, mid = (PP_TOP + PP_BOT) / 2 - 6, last = mid;
    float hold = 0.0f, ph = 0.0f, step = 6000.0f / hz;   /* (a wave drawn as if at ~40 px a cycle; its samples held at
                                          * the codec's rate, its steps by the bits) */
    float levels = (float)(1 << (bits - 1)) * 2.0f;
    for (x = 8; x < 232; x++) {
        float w = brk_sin_turns((float)(x - 8) / 52.0f + (float)(now % 4000u) / 4000.0f) * 0.8f +
                  0.2f * brk_sin_turns((float)(x - 8) / 13.0f);
        int32_t y, ys, a, c;
        ph += step;
        if (b->codec == CODEC_OFF || ph >= 1.0f) {
            ph -= ph >= 1.0f ? 1.0f : 0.0f;
            hold = b->codec == CODEC_OFF ? w : (float)(int32_t)(w * levels * 0.5f + (w < 0.0f ? -0.5f : 0.5f)) /
                                              (levels * 0.5f);
        }
        ys = mid - (int32_t)(w * 54.0f);
        y = mid - (int32_t)(hold * 54.0f);
        if (ys >= band && ys < band + 60)
            cv_pset(x, ys - band, A_DIM);    /* (the wave as it was) */
        a = last < y ? last : y;
        c = last < y ? y : last;
        for (; a <= c; a++)              /* (the steps the codec keeps, joined) */
            if (a >= band && a < band + 60 && a >= PP_TOP)
                cv_pset(x, a - band, b->codec == CODEC_OFF ? A_INK : A_HOT);
        last = y;
    }
}

static void pp_draw_lofi(int32_t band)
{
    const psx_bus_t *b = &set.psx;
    float bits = psx_crush_bits(set.psx.crush);
    int32_t x0 = 56, y0 = PP_TOP + 6, sz = 128, x, levels = (int32_t)brk_exp2f(bits - 1.0f + 0.5f);
    if (levels > 64)
        levels = 64;
    if (levels < 1)
        levels = 1;
    for (x = 0; x < sz; x++) {           /* the transfer: in across, out up, the staircase CRUSH leaves */
        float in = (float)x / (float)(sz - 1) * 2.0f - 1.0f, q = (float)levels;
        float out = set.psx.crush < 0.01f ? in : (float)(int32_t)(in * q + (in < 0.0f ? -0.5f : 0.5f)) / q;
        int32_t y = y0 + sz / 2 - (int32_t)(out * (float)(sz / 2 - 1)), yd = y0 + sz / 2 - (int32_t)(in * (float)(sz / 2 - 1));
        if (yd >= band && yd < band + 60)
            cv_pset(x0 + x, yd - band, A_DIM);
        if (y >= band && y < band + 60) {
            cv_pset(x0 + x, y - band, A_HOT);
            if (y + 1 < band + 60)
                cv_pset(x0 + x, y + 1 - band, A_HOT);
        }
    }
    if (y0 >= band && y0 < band + 60)
        cv_rect(x0, y0 - band, sz, 1, A_BG1);
    (void)b;
}

static void pp_draw_skip(int32_t band, uint32_t now)
{
    const psx_bus_t *b = &set.psx;
    int32_t cx = 120, cy = (PP_TOP + PP_BOT) / 2 - 4, r0 = 14, r1 = 62, x, y;
    uint32_t lit = (now / (60u + (3u - (b->skdiv & 3u)) * 40u)) % 16u;   /* (a sector slipping, as if) */
    for (y = cy - r1; y <= cy + r1; y++) {
        if (y < band || y >= band + 60)
            continue;
        for (x = cx - r1; x <= cx + r1; x++) {
            int32_t dx = x - cx, dy = y - cy, rr = dx * dx + dy * dy;
            float a;
            uint32_t sec;
            if (rr > r1 * r1 || rr < r0 * r0)
                continue;
            a = (float)(dy < 0 ? 0.5f : 0.0f);
            {                            /* the sector by the angle (an octant table and a ratio: no atan) */
                float fx = (float)dx, fy = (float)dy, ax = fx < 0.0f ? -fx : fx, ay = fy < 0.0f ? -fy : fy;
                float t = ax > ay ? ay / ax * 0.125f : 0.25f - ax / (ay + 0.0001f) * 0.125f;   /* 0..0.25 */
                a = fx >= 0.0f ? (fy >= 0.0f ? t : 1.0f - t) : (fy >= 0.0f ? 0.5f - t : 0.5f + t);
            }
            sec = (uint32_t)(a * 16.0f) & 15u;
            if (b->skchance > 0.0f && sec == lit)
                cv_pset(x, y - band, a_dith(&ar_lane, 160 + (rr % 7) * 8, x, y));
            else
                cv_pset(x, y - band, a_dith(&ar_floor1, 120 + ((rr >> 4) % 3) * 30 - (int32_t)(sec & 1u) * 40, x, y));
        }
    }
}

/* COLOR: the wave at the colour's rate (RAW: its top kept, aliased into the samples; FILTERED: gone first), played back
 * as SMOOTH does it (the steps, lines, or the gaussian's swell), drawn over the wave as it came by the amount heard (the
 * codec's MIX x AMOUNT); under it the codec's noise floor (CRUNCH x AMOUNT; flat for PCM) and the kernel the playback
 * weighs its four samples by (a box, a triangle, psx-spx's gaussian) */
static float pp_kernel(float t, float sm) /* the playback's weight on a sample t samples away, SMOOTH sm */
{
    float a = t < 0.0f ? -t : t, nr = a < 0.5f ? 1.0f : 0.0f, ln = a < 1.0f ? 1.0f - a : 0.0f, gs = 0.0f;
    int32_t gi = 0x1FF - (int32_t)(a * 256.0f + 0.5f);
    if (gi >= 0)
        gs = (float)PSX_GAUSS[gi] * (1.0f / 32768.0f);   /* (the four weights at the fraction: gauss[1FFh - 256|t|]) */
    return sm <= 0.5f ? nr + (ln - nr) * 2.0f * sm : ln + (gs - ln) * (2.0f * sm - 1.0f);
}
static void pp_draw_color(int32_t band, uint32_t now)
{
    const psx_bus_t *b = &set.psx;
    const psx_color_t *c = &set.pxc;
    float hz = c->rate == PSXR_GRIT ? b->crate : PSXC_RATE_HZ[psxr_rate(c->rate)], per = 88200.0f / hz;   /* (px a
                                          * codec sample: 2 at 44.1 kHz .. 11 at 8 kHz) */
    float ph = (float)(now % 6000u) / 6000.0f, amt = brk_clampf(c->amount, 0.0f, 1.0f);
    float mix = b->codec == CODEC_GRIT ? brk_clampf(b->cmix, 0.0f, 1.0f) * amt : 0.0f;
    float cr = c->adpcm ? brk_clampf(c->crunch, 0.0f, 1.0f) * amt : 0.0f, top = 1.0f;
    int32_t x, mid = 86, last = -1, k;
    uint32_t raw = psxr_raw(c->rate), seed;
    if (2.0f * per < 9.7f)
        top = 1.0f;                      /* (the wave's top partial, 9.7 px: under the rate's half, kept either way) */
    else
        top = raw ? 1.0f : 0.0f;         /* (over it: FILTERED takes it out first; RAW keeps it, aliased) */
    for (x = 8; x < 232; x++) {
        float t = (float)(x - 8), w = 0.66f * brk_sin_turns(t / 64.0f + ph) + 0.22f * brk_sin_turns(t / 9.7f + ph * 3.0f);
        float pos = t / per, fr = pos - (float)(int32_t)pos, y = 0.0f;
        int32_t i0 = (int32_t)pos, yy, j;
        for (j = -1; j <= 2; j++) {      /* the four codec samples around, weighed as SMOOTH plays them */
            float st = (float)(i0 + j) * per, sv = 0.66f * brk_sin_turns(st / 64.0f + ph) +
                       0.22f * top * brk_sin_turns(st / 9.7f + ph * 3.0f);
            seed = (uint32_t)(i0 + j) * 2654435761u + (now / 120u) * 40503u;
            sv += cr * 0.32f * ((float)((seed >> 16) & 255u) / 255.0f - 0.5f);   /* (the codec's error, CRUNCH) */
            y += sv * pp_kernel((float)j - fr, c->smooth);
        }
        y = w + (y - w) * mix;           /* (the amount heard over the wave as it came) */
        pp_px(x, mid - (int32_t)(w * 36.0f), A_DIM, band);
        yy = mid - (int32_t)(y * 36.0f);
        pp_span(x, last >= 0 ? last : yy, yy, A_HOT, band);
        pp_px(x, yy + 1, A_HOT, band);
        last = yy;
    }
    {                                    /* the codec's noise floor: grass rising with CRUNCH (PCM: flat) */
        int32_t y0 = 178, h0 = c->adpcm ? 3 + (int32_t)(cr * 30.0f) : 1;
        for (x = 8; x < 150; x++) {
            uint32_t r = (uint32_t)x * 2246822519u + (now / 90u) * 3266489917u;
            int32_t h = h0 * (int32_t)(160u + ((r >> 20) & 95u)) / 255, y;
            for (y = y0 - h; y <= y0; y++)
                pp_px(x, y, a_dith(&ar_floor1, 120 + (y0 - y) * 4, x, y), band);
        }
        for (x = 8; x < 150; x++)
            pp_px(x, y0 + 1, A_DIM, band);
    }
    {                                    /* the kernel: -2 .. 2 samples across 64 px, a box's height 40 */
        int32_t kx = 166, ky = 178, kl = -1;
        for (k = 0; k < 64; k++) {
            float t = ((float)k - 32.0f) / 16.0f, v = pp_kernel(t, c->smooth);
            int32_t yy = ky - (int32_t)(v * 40.0f);
            pp_span(kx + k, kl >= 0 ? kl : yy, yy, A_HOT, band);
            kl = yy;
        }
        for (k = 0; k < 64; k++)
            pp_px(kx + k, ky + 1, A_DIM, band);
    }
}

/* OUT: CONSOLE's roll-off (the low-pass and the DC blocker as heard, CONSOLE x AMOUNT) and its clip curve; the reverb's
 * input (INPUT's gain into its 16-bit maths, clipped flat at full scale, across what SEND x AMOUNT fills); the two
 * speakers, one dot when MONO */
static void pp_draw_out(int32_t band, uint32_t now)
{
    const psx_color_t *c = &set.pxc;
    float amt = brk_clampf(c->amount, 0.0f, 1.0f), a = brk_clampf(c->console, 0.0f, 1.0f) * amt;
    float fc = psx_console_hz(a), sd = brk_clampf(c->vsend, 0.0f, 1.0f) * amt, in = c->vinput;
    int32_t x, last = -1, k;
    for (x = 8; x < 150; x++) {          /* the roll-off: 20 Hz .. 20 kHz across, 0 .. -24 dB down (y 50 .. 114) */
        float f = 20.0f * brk_exp2f((float)(x - 8) * (9.966f / 141.0f)), w = f / fc, q = 1.0f + w * w;
        float re = 1.0f - a + a * (1.0f - w * w) / (q * q), im = -a * 2.0f * w / (q * q), m2 = re * re + im * im;
        float hw = f / 10.0f, hp2 = hw * hw / (1.0f + hw * hw), db, g;
        g = 1.0f - a + a * hp2;          /* (the DC blocker, blended in: its power, roughly) */
        db = 3.0103f * brk_log2f(m2 * g * g + 1e-9f);   /* (10 log10 of the power) */
        {
            int32_t yy = 50 + (int32_t)(-db * (64.0f / 24.0f));
            yy = yy > 114 ? 114 : yy;
            pp_px(x, 50, A_DIM, band);
            pp_span(x, last >= 0 ? last : yy, yy, A_HOT, band);
            pp_px(x, yy + 1, A_HOT, band);
            last = yy;
        }
    }
    for (k = 0; k < 3; k++) {            /* (100 Hz, 1 kHz, 10 kHz: 2.32, 5.64, 8.97 octaves over 20 Hz) */
        int32_t gx = 8 + (int32_t)((2.3219f + 3.3219f * (float)k) * (141.0f / 9.966f)), y;
        for (y = 52; y < 114; y += 3)
            pp_px(gx, y, A_BG1, band);
    }
    {                                    /* the clip: in -1.5 .. 1.5 across 64, out the same up 64 (y 50 .. 114) */
        int32_t cx = 166, cy = 82, cl = -1;
        for (k = 0; k < 64; k++) {
            float xi = ((float)k - 32.0f) / 21.33f, yo = psx_console_clip(xi, a);
            int32_t yy = cy - (int32_t)(yo * 21.33f), yd = cy - (int32_t)(xi * 21.33f);
            pp_px(cx + k, yd, A_DIM, band);
            pp_span(cx + k, cl >= 0 ? cl : yy, yy, A_HOT, band);
            cl = yy;
        }
    }
    {                                    /* the reverb's input: x in -1..1 across 44, out INPUT x, flat at full scale */
        int32_t bx = 8, by = 158, il = -1;
        for (k = 0; k < 45; k++) {
            float xi = ((float)k - 22.0f) / 22.0f, yo = brk_clampf(xi * in, -1.0f, 1.0f);
            int32_t yy = by - (int32_t)(yo * 22.0f), yd = by - (int32_t)(xi * 22.0f);
            uint16_t cc = (xi < 0.0f ? -xi : xi) <= sd + 0.02f ? A_HOT : A_DIM;   /* (lit: what SEND fills) */
            pp_px(bx + k, yd, A_BG1, band);
            pp_span(bx + k, il >= 0 ? il : yy, yy, cc, band);
            il = yy;
        }
    }
    {                                    /* the speakers: L and R, closing to one dot as MONO comes in */
        static float wd = 1.0f;
        float want = c->mono ? 0.0f : 1.0f;
        int32_t sp, cxs = 168, cy = 158;
        wd += (want - wd) * 0.35f;       /* (as the sound narrows: a few frames) */
        for (sp = 0; sp < 2; sp++) {
            int32_t ox = cxs + (int32_t)((sp ? 30.0f : -30.0f) * wd), dx, dy;
            for (dy = -7; dy <= 7; dy++)
                for (dx = -7; dx <= 7; dx++)
                    if (dx * dx + dy * dy <= 49)
                        pp_px(ox + dx, cy + dy, a_dith(&ar_lane, 150 + (49 - dx * dx - dy * dy) * 2 +
                              (int32_t)((now / 40u) % 8u), ox + dx, cy + dy), band);
        }
    }
}

/* PRESET: the four presets a row each, the one the stages came from lit (none once a stage is turned) */
static void pp_draw_preset(int32_t band)
{
    uint32_t k;
    for (k = 0; k < PSXP_N; k++) {
        int32_t y = 54 + (int32_t)k * 32, on = set.pxc.preset == k + 1u, r;
        for (r = y; r < y + 26; r++)
            if (r >= band && r < band + 60)
                cv_rect(16, r - band, 208, 1, on ? A_HOT : A_BG1);
        pp_text(120, y + 8, PSXP_NAME[k], on ? A_BG0 : A_DIM, band, 2);
    }
}

static void psxp_draw(uint32_t now)
{
    int32_t top, i;
    char gv[4][12];
    for (top = 0; top < 240; top += 60) {
        cv_begin(240, 60, A_BG0);
        aff_check();
        /* the header (AFFINE, controls phase 2): FX PSX, a message, the pattern to come or the colour's state; the
         * item shown, and FX's five parts as pips, PSX lit */
        if (top < 40) {
            pp_text(8, 8, "FX PSX", A_INK, top, 0);
            if (now < ui.msg_until)
                pp_text(232, 8, ui.msg[1], A_HOT, top, 1);
            else if (pat_next >= 0) {    /* (PRESETS playing: the pattern to come, until it lands) */
                char t[12] = "NEXT P";
                int_text((int32_t)pat_next + 1, t + 6);
                pp_text(232, 8, t, A_HOT, top, 1);
            }
            else if (pp_part == PP_COLOR && set.psx.codec != CODEC_GRIT)
                pp_text(232, 8, "GRIT OFF", A_DIM, top, 1);   /* (the colour is GRIT's: off, a turn of K2-K4) */
            else if (set.pxc.preset && (pp_part == PP_COLOR || pp_part == PP_OUT))
                pp_text(232, 8, PSXP_NAME[(set.pxc.preset - 1u) % PSXP_N], A_DIM, top, 1);
            else
                pp_text(232, 8, "SPU", A_DIM, top, 1);
            pp_text(8, 24, PP_NAME[pp_part % PP_N], A_HOT, top, 0);
            for (i = 0; i < 5; i++) {    /* (the pips: 8 x 8, 4 apart, right-aligned) */
                int32_t px = 232 - (5 - i) * 12 + 4, yy, xx;
                for (yy = 25; yy < 33; yy++)
                    for (xx = px; xx < px + 8; xx++)
                        pp_px(xx, yy, i == 1 + FXP_PSX ? A_HOT : A_DIM, top);
            }
        }
        switch (pp_part % PP_N) {
        case PP_PRESET: pp_draw_preset(top); break;
        case PP_VERB: pp_draw_verb(top, now); break;
        case PP_CODEC: pp_draw_codec(top, now); break;
        case PP_COLOR: pp_draw_color(top, now); break;
        case PP_OUT: pp_draw_out(top, now); break;
        case PP_LOFI: pp_draw_lofi(top); break;
        case PP_SKIP: pp_draw_skip(top, now); break;
        default: break;
        }
        if (top + 60 > AH_PANEL) {       /* the panel: the four knobs, as HOME's */
            cv_rect(0, AH_PANEL - top, 240, 1, A_DIM);
            for (i = 0; i < 4; i++) {
                int32_t gx = 8 + (i & 1) * 120, gy = 198 + (i >> 1) * 21;
                float g = psxp_norm(i);
                psxp_value(i, gv[i]);
                pp_text(gx, gy, psxp_name(i), A_DIM, top, 0);
                pp_text(gx + 106, gy, gv[i], A_INK, top, 1);
                if (g >= 0.0f)
                    home_bar(gx, gy + 13, 106, g, 0, top);
            }
        }
        cv_blit(0, (uint32_t)top);
    }
}
