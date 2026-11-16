/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH: the SNARE / HAT page (ENV). (Part of app.c's unity: included there in order.) */

/* ---- the SNARE / HAT page (ENV): ALGORITHM the voice, ENV again SOUND or SHAPE, PRESETS its presets */
static const char *const PERC_KNOB[PERC_VOICES][5][4] = {   /* (AFFINE, 2x: the foot's, DECAY and METAL/NZ short
                                          * there: snd_long) */
    {{"LEVEL", "TUNE", "DEC", "TONE"}, {"SNAP", "DRIVE", "PRESET", "DIRT"}, {"DROP", "TIME", "LIFT", "ROOM"},
     {"PRESET", "FX", "LEVEL", "DEC"}, {"MEMORY", "COPY", "PASTE", ""}},
    {{"LEVEL", "TUNE", "DEC", "NOISE"}, {"CUT", "DRIVE", "PRESET", "DIRT"}, {"", "", "LIFT", "ROOM"},
     {"PRESET", "FX", "LEVEL", "DEC"}, {"MEMORY", "COPY", "PASTE", ""}},
    {{"LEVEL", "TUNE", "DEC", "TONE"}, {"SPREAD", "DRIVE", "PRESET", "DIRT"}, {"", "", "LIFT", "ROOM"},
     {"PRESET", "FX", "LEVEL", "DEC"}, {"MEMORY", "COPY", "PASTE", ""}},
    {{"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""}}};
                                         /* (AFFINE, controls phase 2: the fifth, MEMORY: its memory 1-10 (PRESETS
                                          * until then), COPY and PASTE (SEL + ARP)) */   /* (LOAF BREAKER: the clap's; SHAPE's
                                          * K3 PRESET, the memory back to a built-in sound; the fourth, the MELODY's,
                                          * gone in 0.209; 0.225: SHAPE's K4 DIRT, MORE (DROP, TIME, LIFT, ROOM), and
                                          * BEGINNER's one page: PRESET, FX, LEVEL, TUNE; 0.229 DECAY for TUNE) */
static const char *const PPAGE_NAME[5] = {"SOUND", "SHAPE", "MORE", "PRESET", "MEMORY"};
#define PERC_ITEMS 4u                    /* (the drums' items: SOUND, SHAPE, MORE, MEMORY; ui.ppage 0..3) */
static uint32_t perc_pg(void) { return NOVICE ? 3u : ui.ppage % PERC_ITEMS == 3u ? 4u : ui.ppage % PERC_ITEMS; }
static const char *perc_kname(uint32_t v, uint32_t pg, int i) { return PERC_KNOB[v % PERC_VOICES][pg % 5u][i & 3]; }

/* LOAF BREAKER (11 Nov 2026): the drums' presets: the four built-in sounds of each, then FLEXOR's picks (the industrial
 * snares, chosen by ear on the listening pages, docs/sounds/PERC_PRESETS.md), each with the FX it was chosen with.
 * BEGINNER's FX (PX_FX) on top of the preset: DRY (no room), ROOM, CRUSH (5 bits), SMASH (twice the drive, the tail
 * lifted, a longer room), SMASH+BITS */
#define PFX_N 5u
enum { PFX_DRY, PFX_ROOM, PFX_CRUSH, PFX_SMASH, PFX_SMASHB };
static const char *const PFX_NAME[PFX_N] = {"DRY", "ROOM", "CRUSH", "SMASH", "SMASH+BITS"};
typedef struct {
    const char *name;
    uint8_t p[6];                        /* level, tune, decay, tone, snap, drive (0..100) */
    uint8_t x[5];                        /* PX_DROP .. PX_ROOM */
    uint8_t fx;                          /* the FX it was chosen with */
} ppre_t;
static const ppre_t PP_SN[] = {
    {"SHRAPNEL", {85, 64, 31, 80, 45, 86}, {74, 18, DIRT_FOLD, 35, 0}, PFX_ROOM},
    {"GRINDER", {85, 38, 50, 45, 50, 86}, {44, 55, DIRT_HARD, 55, 0}, PFX_ROOM},
    {"PISTON", {85, 34, 40, 35, 35, 68}, {77, 49, DIRT_HARD, 20, 0}, PFX_SMASHB},
    {"CONCRETE", {85, 19, 56, 10, 30, 45}, {30, 60, DIRT_SOFT, 60, 0}, PFX_SMASHB},
    {"BUNKER", {85, 29, 73, 10, 45, 45}, {30, 64, DIRT_HARD, 70, 0}, PFX_SMASHB},
    {"SHRAP CRUSH", {85, 64, 31, 80, 75, 58}, {53, 18, DIRT_FOLD, 20, 0}, PFX_SMASHB},
    {"GRIND CRUSH", {85, 38, 50, 45, 65, 58}, {30, 55, DIRT_HARD, 40, 0}, PFX_SMASHB},
    {"CONCR SMASH", {85, 19, 56, 10, 30, 45}, {30, 60, DIRT_SOFT, 60, 0}, PFX_SMASH},
    {"GABBER BITS", {85, 55, 45, 60, 75, 85}, {0, 0, DIRT_BITS, 0, 0}, PFX_DRY}};
static const ppre_t PP_CP[] = {
    {"BUNKER CLAP", {80, 20, 70, 30, 50, 60}, {0, 0, DIRT_HARD, 70, 0}, PFX_SMASH},
    {"FURNACE CLAP", {80, 30, 60, 35, 60, 45}, {0, 0, DIRT_FOLD, 50, 0}, PFX_SMASH}};
#define N_SN (sizeof PP_SN / sizeof PP_SN[0])
#define N_CP (sizeof PP_CP / sizeof PP_CP[0])
static uint32_t pp_nb(uint32_t v) { return PERC_PRESETS + (v == PV_SNARE ? N_SN : v == PV_CLAP ? N_CP : 0u); }
/* LOAF BREAKER (0.9.45, FLEXOR: the site's IMPORT page): a drum with its own sample has one PRESET more, the last, its
 * name: the sample (PX_RSV PX_SAMPLE) in place of the synthesis */
static uint32_t pp_n(uint32_t v) { return pp_nb(v) + (kit_has >> (v % 3u) & 1u); }
static int perc_smp(uint32_t v) { return perc_x(v)[PX_RSV] == PX_SAMPLE && kit_has >> (v % 3u) & 1u; }
static float perc_dms(uint32_t v, const perc_voice_t *pv)   /* (its decay, ms: the sample's fade, or its length) */
{
    const kit_ent_t *e = kit_ent(v);
    float f;
    if (!perc_smp(v) || !e)
        return perc_decay_ms(v, pv->decay);
    f = perc_sample_ms(pv->decay);
    return f > 0.0f ? f : (float)e->len * 1000.0f / (float)e->rate / brk_exp2f_c((pv->tune - 0.5f) * 2.0f);
}
static const ppre_t *pp_ext(uint32_t v, uint32_t i)   /* (i past the built-in four) */
{
    i -= PERC_PRESETS;
    return v == PV_SNARE ? &PP_SN[i % N_SN] : &PP_CP[i % N_CP];
}
static const char *pp_name(uint32_t v, uint32_t i)
{
    static char t[13];
    if (i >= pp_nb(v) || perc_x(v)[PX_RSV] == PX_SAMPLE) {   /* (its sample: its name) */
        kit_name(v, t);
        return t[0] ? t : "NO SAMPLE";
    }
    i %= pp_n(v);
    return i < PERC_PRESETS ? PERC_PRESET_NAME[v % PERC_VOICES][i] : pp_ext(v, i)->name;
}
static void pp_apply(uint32_t v, uint32_t i, int fx)   /* the drum's memory playing: preset i, with fx (-1: its own) */
{
    perc_voice_t *pv = perc_voice(v);
    uint8_t *x = perc_x(v);
    uint32_t j, own = PFX_DRY;
    i %= pp_n(v);
    x[PX_RSV] = 0u;
    if (i >= pp_nb(v)) {                 /* (0.9.45: its sample, as recorded: level, no fade, the tone open, dry) */
        static const float SP[6] = {0.85f, 0.5f, 1.0f, 0.5f, 0.5f, 0.0f};
        float *P[6] = {&pv->level, &pv->tune, &pv->decay, &pv->tone, &pv->snap, &pv->drive};
        for (j = 0; j < 6u; j++)
            *P[j] = SP[j];
        for (j = 0; j <= PX_ROOM; j++)
            x[j] = 0u;
        x[PX_RSV] = PX_SAMPLE;
    } else if (i < PERC_PRESETS) {
        perc_preset(pv, v, i);
        for (j = 0; j <= PX_ROOM; j++)
            x[j] = 0u;
    } else {
        const ppre_t *e = pp_ext(v, i);
        float *P[6] = {&pv->level, &pv->tune, &pv->decay, &pv->tone, &pv->snap, &pv->drive};
        for (j = 0; j < 6u; j++)
            *P[j] = (float)e->p[j] * 0.01f;
        for (j = 0; j <= PX_ROOM; j++)
            x[j] = e->x[j];
        own = e->fx;
    }
    x[PX_PRE] = (uint8_t)i;
    x[PX_FX] = (uint8_t)(fx < 0 ? own : (uint32_t)fx % PFX_N);
    switch (x[PX_FX]) {
    case PFX_ROOM:
        x[PX_ROOM] = 30u;
        break;
    case PFX_CRUSH:
        x[PX_DIRT] = DIRT_BITS;
        x[PX_ROOM] = 30u;
        break;
    case PFX_SMASH:
    case PFX_SMASHB:                     /* (twice the drive's gain: 1 + 15 d^2 doubled) */
        pv->drive = brk_clampf(brk_exp2f_c(0.5f * brk_log2f_c((1.0f + 30.0f * pv->drive * pv->drive) * (1.0f / 15.0f))), 0.0f, 1.0f);
        x[PX_LIFT] = (uint8_t)(x[PX_LIFT] + 35u > 100u ? 100u : x[PX_LIFT] + 35u);
        x[PX_ROOM] = 45u;
        if (x[PX_FX] == PFX_SMASHB)
            x[PX_DIRT] = DIRT_BITS;
        break;
    default:
        break;
    }
}

/* LOAF BREAKER: the snare's, the hat's and the clap's memories: SNARE 1..10, the one
 * playing kept as it is, the next as it was left (PRESETS on EDIT, SELECT on the drum's SEQ lane) */
static void perc_mem_turn(uint32_t v, int32_t d)
{
    uint8_t *s = perc_slot(v);
    char t[4];
    v %= PERC_VOICES;
    *perc_mem(v, *s) = *perc_voice(v);
    *s = (uint8_t)((*s % PERC_MEMS + (d > 0 ? 1u : PERC_MEMS - 1u)) % PERC_MEMS);
    *perc_voice(v) = *perc_mem(v, *s);
    int_text((int32_t)*s + 1, t);
    app_say(PERC_VOICE_NAME[v], t);
    ui.redraw |= RG_BLOCKS | RG_FOOT;
    changed();
}
/* MEMORY's COPY and PASTE (LOAF BREAKER 0.9.26, SEL + ARP on the drums until controls phase 2): a drum's sound and
 * extras copied, onto another memory of the same drum pasted */
static int8_t clip_pv = -1, clip_ps;
static perc_voice_t clip_pvoice;
static uint8_t clip_px[PX_N];
static void perc_copy(void)
{
    uint32_t v = ui.pvoice % PERC_VOICES;
    clip_pv = (int8_t)v;
    clip_ps = (int8_t)(*perc_slot(v) % PERC_MEMS);
    clip_pvoice = *perc_voice(v);
    memcpy(clip_px, perc_x(v), PX_N);
    say_int("COPIED", clip_ps + 1, "");
}
static void perc_paste(void)
{
    uint32_t v = ui.pvoice % PERC_VOICES;
    if (clip_pv != (int8_t)v) {
        app_say("PASTE", "COPY FIRST");
        return;
    }
    *perc_voice(v) = clip_pvoice;
    memcpy(perc_x(v), clip_px, PX_N);
    say_int("PASTED TO", (int32_t)(*perc_slot(v) % PERC_MEMS) + 1, "");
    changed();
}
static float *perc_param(int i)
{
    perc_voice_t *v = perc_voice(ui.pvoice);
    float *P[6] = {&v->level, &v->tune, &v->decay, &v->tone, &v->snap, &v->drive};
    uint32_t pg = perc_pg();
    if (pg == 3u)                        /* (BEGINNER: K3 LEVEL, K4 DECAY; 0.229, FLEXOR: TUNE out) */
        return i == 2 ? P[0] : i == 3 ? P[2] : 0;
    return pg == 0u ? P[i & 3] : pg == 1u && i < 2 ? P[4 + i] : 0;
}
static uint8_t *perc_xknob(int i)        /* (LOAF BREAKER: MORE's K1-K4: the drum's extras; the snare alone drops) */
{
    if (perc_pg() != 2u || (i < 2 && ui.pvoice != PV_SNARE))
        return 0;
    return &perc_x(ui.pvoice)[i == 0 ? PX_DROP : i == 1 ? PX_DTIME : i == 2 ? PX_LIFT : PX_ROOM];
}
static void perc_value(int i, char *t)
{
    float *p = perc_param(i);
    uint8_t *x = perc_x(ui.pvoice), *xk = perc_xknob(i);
    uint32_t pg = perc_pg(), v = ui.pvoice % PERC_VOICES;
    t[0] = 0;
    if ((pg == 1u && i == 2) || (pg == 3u && i == 0)) {   /* PRESET: the built-in sound it last put back (AFFINE, 2x:
                                          * its place, 5/13: the foot's 108 px hold no SHRAPNEL beside its name; the
                                          * page shows the name large, the header says it) */
        int_text((int32_t)(x[PX_PRE] % pp_n(v)) + 1, t);
        text_cat(t, "/");
        int_text((int32_t)pp_n(v), t + text_n(t));
        return;
    }
    if (pg == 3u && i == 1) {
        text_cat(t, PFX_NAME[x[PX_FX] % PFX_N]);
        return;
    }
    if (pg == 4u) {                      /* MEMORY: its number; the memory copied, from */
        if (i == 0)
            int_text((int32_t)(*perc_slot(v) % PERC_MEMS) + 1, t);
        else if (i == 2 && clip_pv == (int8_t)v)
            int_text(clip_ps + 1, t);
        else if (i == 2)
            text_cat(t, "-");
        return;
    }
    if (pg == 1u && i == 3) {
        text_cat(t, DIRT_NAME[x[PX_DIRT] % DIRT_N]);
        return;
    }
    if (xk && perc_smp(v) && i < 2) {    /* (0.9.45: DROP, TIME: the snare's synthesis, nothing on a sample) */
        text_cat(t, "-");
        return;
    }
    if (xk) {                            /* (MORE: OFF, or its value) */
        if (!*xk)
            text_cat(t, "OFF");
        else if (i == 0) {               /* (the drop: x1.7 .. x6, the body's start over its note) */
            text_cat(t, "X");
            tenths_text((int32_t)(17.0f + 0.43f * (float)*xk + 0.5f), t + 1);
        } else if (i == 1) {
            int_text((int32_t)(3.0f * brk_exp2f_c((float)*xk * (1.0f / 18.0f)) + 0.5f), t);
            text_cat(t, " MS");
        } else {
            int_text(*xk, t);
            text_cat(t, "%");
        }
        return;
    }
    if (perc_smp(v) && (pg == 0u || pg == 3u) && p) {   /* (0.9.45: the sample's TUNE, DECAY, TONE) */
        if (pg == 0u && i == 1) {        /* (semitones, +-12) */
            int32_t st = (int32_t)((*p - 0.5f) * 24.0f + (*p < 0.5f ? -0.5f : 0.5f));
            if (st > 0)
                text_cat(t, "+");
            int_text(st, t + (st > 0));
            text_cat(t, " ST");
            return;
        }
        if ((pg == 0u && i == 2) || (pg == 3u && i == 3)) {
            float ms = perc_sample_ms(*p);
            if (ms <= 0.0f)
                text_cat(t, "FULL");
            else {
                int_text((int32_t)(ms + 0.5f), t);
                text_cat(t, " MS");
            }
            return;
        }
        if (pg == 0u && i == 3) {        /* (left a low-pass, right a high-pass) */
            if (*p < 0.48f) {
                text_cat(t, "LP ");
                hz_text(400.0f * brk_exp2f_c(*p * 11.4f), t + 3);
            } else if (*p > 0.52f) {
                text_cat(t, "HP ");
                hz_text(20.0f * brk_exp2f_c((*p - 0.52f) * 15.0f), t + 3);
            } else
                text_cat(t, "OPEN");
            return;
        }
    }
    if (perc_smp(v) && ((pg == 1u && i == 0) || (pg == 2u && i < 2))) {   /* (SNAP, CUT, SPREAD, DROP, TIME: the
                                          * synthesis's, nothing on a sample) */
        text_cat(t, "-");
        return;
    }
    if (!p)
        return;
    if (pg == 0u && i == 1)
        hz_text(perc_tune_hz(v, *p), t);
    else if ((pg == 0u && i == 2) || (pg == 3u && i == 3)) {
        int_text((int32_t)(perc_decay_ms(v, *p) + 0.5f), t);
        text_cat(t, " MS");
    } else
        pct_text(*p, t);
}
static void perc_turn(int i, int32_t d)
{
    float *p = perc_param(i);
    uint8_t *x = perc_x(ui.pvoice), *xk = perc_xknob(i);
    uint32_t pg = perc_pg(), v = ui.pvoice % PERC_VOICES;
    char t[16];
    if ((pg == 1u && i == 2) || (pg == 3u && i < 2)) {   /* PRESET (no undo); BEGINNER's FX on it */
        perc_voice_t *pv = perc_voice(v);
        float lvl = pv->level, dec = pv->decay;
        uint32_t n = pp_n(v);
        if (i == 0 || pg == 1u)
            pp_apply(v, (x[PX_PRE] % n + (d > 0 ? 1u : n - 1u)) % n, -1);
        else {                           /* (FX: the preset again, with the next; its LEVEL and DECAY kept) */
            pp_apply(v, x[PX_PRE], (int)clamp((int32_t)x[PX_FX] + (d > 0 ? 1 : -1), 0, PFX_N - 1));
            pv->level = lvl;
            pv->decay = dec;
        }
        ui.ppreset[v] = x[PX_PRE];
        perc_value(i, t);
        if (!(i == 1 && pg == 3u)) {     /* (the header: the preset's name) */
            t[0] = 0;
            text_cat(t, pp_name(v, x[PX_PRE]));
        }
        app_say(i == 1 && pg == 3u ? "FX" : "PRESET", t);
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        changed();
        return;
    }
    if (pg == 4u) {                      /* MEMORY: K1 the memory, K2 copy it, K3 paste the copy onto it */
        if (i == 0)
            perc_mem_turn(v, d);
        else if (i == 1)
            perc_copy();
        else if (i == 2)
            perc_paste();
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        return;
    }
    if (pg == 1u && i == 3) {            /* DIRT: SOFT, HARD, FOLD, BITS */
        x[PX_DIRT] = (uint8_t)clamp((int32_t)x[PX_DIRT] + (d > 0 ? 1 : -1), 0, DIRT_N - 1);
        perc_value(i, t);
        app_say("DIRT", t);
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        changed();
        return;
    }
    if (xk) {
        *xk = (uint8_t)clamp((int32_t)*xk + d * 2, 0, 100);
        perc_value(i, t);
        app_say(snd_long(perc_kname(v, pg, i)), t);
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        changed();
        return;
    }
    if (!p)
        return;
    *p = knob_f(*p, d, 0.02f, 0.0f, 1.0f);
    perc_value(i, t);
    app_say(snd_long(perc_kname(v, pg, i)), t);
    ui.redraw |= RG_BLOCKS;
    changed();
}

static uint32_t pw_sig = 1u;
static void drum_view_turn(int32_t d)    /* GLO › SCREEN › VIEWS K1: the drums' view, RGB, WAVE, CLASSIC (EDIT held
                                          * until controls phase 2) */
{
    static const char *const N[3] = {"RGB", "WAVE", "CLASSIC"};
    int32_t v = clamp((ui.kclassic ? 2 : (int32_t)ui.kview) + (d > 0 ? 1 : -1), 0, 2);
    ui.kclassic = (uint8_t)(v == 2);
    ui.kview = (uint8_t)(v == 1);
    ui.pbig = (uint8_t)!ui.kclassic;
    pw_sig = 1u;
    app_say("DRUM VIEW", N[v]);
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}

/* LOAF BREAKER: the drums' big view (SELECT from BOOM: the snare, the hat, the clap, each in it; EDIT held: the WAVE, then
 * the classic view, as ever): the drum's hit drawn large over its level (its window ~1.4 times its decay, the decay
 * marked by an amber line), its name and memory, its preset (or its sample's name); the four sounds along the bottom,
 * the one edited lit (stopped: their keys 1-4 play them). (AFFINE, 2x: its knobs' names and values: the foot's, LOAF's
 * cards gone with them) */
#define PW_W 232u
static uint8_t pw_px[PW_W];
static int8_t pw_wave[PW_W];
static uint32_t pw_ms = 300u;   /* (pw_sig above: 0 at the start, a hit drawn at once) */
static uint8_t pw_rgb[PW_W][4];          /* (the RGB view: each column's lows, mids, highs and peak) */
static float perc_knob_f(int i)          /* the part's knob i, 0..1 (its card's bar) */
{
    float *p = perc_param(i);
    uint8_t *x = perc_x(ui.pvoice), *xk = perc_xknob(i);
    uint32_t pg = perc_pg(), n = pp_n(ui.pvoice % PERC_VOICES);
    if ((pg == 1u && i == 2) || (pg == 3u && i == 0))
        return (float)(x[PX_PRE] % n) / (float)(n - 1u);
    if (pg == 3u && i == 1)
        return (float)(x[PX_FX] % PFX_N) / (float)(PFX_N - 1u);
    if (pg == 1u && i == 3)
        return (float)(x[PX_DIRT] % DIRT_N) / (float)(DIRT_N - 1u);
    if (xk)
        return (float)*xk * 0.01f;
    return p ? *p : 0.0f;
}
static void perc_picture(const brk_status_t *st, int32_t top)
{
    const perc_voice_t *v = perc_voice(ui.pvoice);
    uint32_t i;
    char t[24], u[8];
    cv_oy = -top;
    if (ui.kview == 0u) {                /* LOAF BREAKER (0.231): the RGB view (rekordbox's colours, or SERATO's) */
        uint32_t pk = 1u;
        for (i = 0; i < PW_W; i++)
            pk = pw_rgb[i][3] > pk ? pw_rgb[i][3] : pk;
        for (i = 0; i < PW_W; i++) {
            const uint8_t *c = pw_rgb[i];
            uint32_t m = c[0] > c[1] ? c[0] : c[1], h = (uint32_t)c[3] * KV_H / pk, br, r, g, b;
            m = c[2] > m ? c[2] : m;
            if (!m || !h)
                continue;
            br = 120u + (uint32_t)c[3] * 135u / pk;
            r = (uint32_t)c[0] * 255u / m;
            g = (uint32_t)c[1] * 255u / m;
            b = (uint32_t)c[2] * 255u / m;
            r = r * r / 255u;
            g = g * g / 255u;
            b = b * b / 255u;
            cv_rect(4 + (int32_t)i, KV_MID - (int32_t)h, 1, 2 * (int32_t)h + 1,
                    set.wstyle ? serato_col(c[0], c[1], c[2], br) : RGB(r * br / 255u, g * br / 255u, b * br / 255u));
        }
        hit_picture(0, 0, PW_W, perc_dms(ui.pvoice, v), pw_ms);   /* (its decay's line over it) */
    } else
        hit_picture(pw_px, pw_wave, PW_W, perc_dms(ui.pvoice, v), pw_ms);
    t[0] = 0;                            /* SNARE 3, its preset (a sample: its name) */
    text_cat(t, PERC_VOICE_NAME[ui.pvoice % PERC_VOICES]);
    int_text((int32_t)*perc_slot(ui.pvoice) + 1, u);
    text_cat(t, " ");
    text_cat(t, u);
    snd_text(6, 4, t, 2, C_WHITE, 0, top);
    snd_text(234, 4, pp_name(ui.pvoice, perc_x(ui.pvoice)[PX_PRE]), 2, C_AMB, 1, top);
    drum_row(st, ui.einst, top);
    cv_oy = 0;
}
static void draw_perc_big(const brk_status_t *st)
{
    const perc_voice_t *v = perc_voice(ui.pvoice);
    uint32_t sig = sig_of(v, sizeof *v) ^ (ui.pvoice * 2654435761u) ^ (ui.kview + 1u) * 97u ^
                   (uint32_t)perc_smp(ui.pvoice) * 7919u ^ kit_has * 131u, top;
    if (sig != pw_sig) {                 /* (its hit again, only when it changed) */
        uint32_t pk = 0u;
        float d = perc_dms(ui.pvoice, v) * 1.4f;
        pw_sig = sig;
        pw_ms = (uint32_t)brk_clampf(d, 60.0f, 900.0f);   /* (AFFINE: EDIT ZOOM gone with the KICK menu's TOOLS: AUTO) */
        if (ui.kview == 0u)              /* (LOAF BREAKER: the RGB view) */
            perc_rgb(ui.pvoice, v, pw_rgb, PW_W, pw_ms);
        perc_shape(ui.pvoice, v, pw_px, PW_W, pw_ms, KV_H);
        perc_wave(ui.pvoice, v, pw_wave, PW_W, pw_ms, KV_H);
        for (top = 0; top < PW_W; top++)
            pk = pw_px[top] > pk ? pw_px[top] : pk;
        for (top = 0; pk && top < PW_W; top++) {   /* (as tall as it goes) */
            pw_px[top] = (uint8_t)((uint32_t)pw_px[top] * KV_H / pk);
            pw_wave[top] = (int8_t)((int32_t)pw_wave[top] * (int32_t)KV_H / (int32_t)pk);
        }
    }
    for (top = 0; top < 148u; top += 60u) {   /* (y 44..191, strips of 60 rows, as BOOM's) */
        uint32_t h = 148u - top < 60u ? 148u - top : 60u;
        cv_begin(240, h, C_BLACK);
        perc_picture(st, (int32_t)top);
        cv_blit(0, 44u + top);
    }
}

/* the SNARE / HAT page (EDIT): the voice's six settings as bars (the part's lit); then the three drums (LOAF BREAKER:
 * the classic view; the big one: draw_perc_big). AFFINE, 2x: three rows of two, each its name and its bar (the drum,
 * its memory and the part: the header's and the tabs') */
static void draw_perc(const brk_status_t *st)
{
    if (ui.pbig) {
        draw_perc_big(st);
        return;
    }
    const perc_voice_t *v = perc_voice(ui.pvoice);
    const float P[6] = {v->level, v->tune, v->decay, v->tone, v->snap, v->drive};
    uint32_t i;
    cv_begin(240, 52, C_BLACK);
    for (i = 0; i < 6u; i++) {           /* LEVEL TUNE, DECAY TONE (SOUND); SNAP DRIVE (SHAPE) */
        int32_t x = 6 + (int32_t)(i % 2u) * 118, y = 4 + (int32_t)(i / 2u) * 17;
        int lit = (i >= 4u) == (perc_pg() == 1u);
        snd_text(x, y, perc_kname(ui.pvoice, i / 4u, (int)(i % 4u)), 2, lit ? C_GRAY : C_DIM, 0, 0);
        cv_rect(x + 62, y + 3, 50, 4, C_BLOCK);
        cv_rect(x + 62, y + 3, (int32_t)(50.0f * P[i] + 0.5f), 4, lit ? C_HI : C_DIM);
    }
    cv_blit(0, 88);
    draw_drums(st);
}
