/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: BOOM (EDIT), the light kick: a clean sub with a pitch drop, for jungle's 808s (dsp/kick.c). One page, two
 * parts (ALGORITHM, EXPERT): BOOM, K1 PRESET (808, JUNGLE, DUB, TIGHT), K2 TUNE (the SUB's note), K3 DECAY (the SUB's,
 * in ms), K4 LEVEL; SHAPE, K1 PUNCH, K2 CLICK, K3 DRIVE (the kick's intensity), K4 TONE (the SUB's saturation).
 * PRESETS turns the preset too. LOAF BREAKER's KICK menu (its sections, LAB, KNOBS, STYLES, memories, views) is gone.
 * Here too: the drums' strip of the classic view (draw_drums). (Part of app.c's unity: included there in order.) */

#define BOOM_PRESETS 4u
static const uint8_t BOOM_PRE[BOOM_PRESETS] = {KICK_PRESET_808, KICK_PRESET_JUNGLE, KICK_PRESET_DUB, KICK_PRESET_TIGHT};
#define BOOM_PARTS 2u
static const char *const BOOM_PART[BOOM_PARTS] = {"BOOM", "SHAPE"};
static const char *const BOOM_KNOB[BOOM_PARTS][4] = {{"PRE", "TUNE", "DEC", "LEVEL"},   /* (AFFINE, 2x: the foot's,
                                          * PRESET and DECAY short there: snd_long) */
                                                     {"PUNCH", "CLICK", "DRIVE", "TONE"}};
#define BOOM_NOTE_LO 23                  /* (TUNE: B0 .. F2 as MIDI notes, the SUB's 30..90 Hz) */
#define BOOM_NOTE_HI 41
static uint32_t boom_part(void) { return NOVICE ? 0u : ui.kpage % BOOM_PARTS; }   /* (BEGINNER: BOOM alone) */
static const char *boom_pre_name(void) { return KICK_PRESET_NAME[BOOM_PRE[set.kpre % BOOM_PRESETS]]; }
static float boom_hz(void) { return kick_pitch_hz(KL_SUB, set.kick.l[KL_SUB].pitch); }
static int32_t boom_note(void)           /* the SUB's nearest note, MIDI */
{
    return (int32_t)(69.0f + 12.0f * brk_log2f_c(boom_hz() * (1.0f / 440.0f)) + 0.5f);
}
static void boom_note_set(int32_t n)
{
    float hz = 440.0f * brk_exp2f_c((float)(n - 69) * (1.0f / 12.0f));
    set.kick.l[KL_SUB].pitch = brk_clampf(brk_log2f_c(hz * (1.0f / 30.0f)) * (1.0f / 1.585f), 0.0f, 1.0f);
}
static void boom_pre_set(uint32_t n)     /* BOOM's preset n, its sound, the level kept (the editor's ED_ACT too) */
{
    float vol = set.kick.volume;
    set.kpre = (uint8_t)(n % BOOM_PRESETS);
    kick_preset(&set.kick, BOOM_PRE[set.kpre]);
    set.kick.volume = vol;
    app_say("BOOM", boom_pre_name());
    ui.redraw |= RG_BLOCKS | RG_FOOT;
    changed_kick();
}
static void boom_pre_turn(int32_t d)     /* K1 PRESET, PRESETS, SEQ's K2 on a BOOM lane: the next preset */
{
    boom_pre_set((uint32_t)clamp((int32_t)(set.kpre % BOOM_PRESETS) + (d > 0 ? 1 : -1), 0, BOOM_PRESETS - 1));
}
static const char *kick_knob_name(int i) { return BOOM_KNOB[boom_part()][i & 3]; }
static void kick_value(int i, char *t)
{
    const kick_t *k = &set.kick;
    t[0] = 0;
    switch (boom_part() * 4u + (uint32_t)(i & 3)) {
    case 0:
        text_cat(t, boom_pre_name());
        break;
    case 1:
        note_name(boom_note() - 48, t);  /* (seq.c's: 0 is C3, MIDI 48) */
        break;
    case 2:
        int_text((int32_t)(kick_decay_ms(KL_SUB, k->l[KL_SUB].decay) + 0.5f), t);
        text_cat(t, " MS");
        break;
    case 3:
        pct_text(k->volume, t);
        break;
    case 4:
        pct_text(k->l[KL_PUNCH].level, t);
        break;
    case 5:
        pct_text(k->l[KL_CLICK].level, t);
        break;
    case 6:
        pct_text(k->intensity, t);
        break;
    default:
        pct_text(k->l[KL_SUB].tone, t);
        break;
    }
}
static float *boom_param(int i)          /* the part's knob i's setting, 0..1 (BOOM's PRESET and TUNE: none) */
{
    kick_t *k = &set.kick;
    switch (boom_part() * 4u + (uint32_t)(i & 3)) {
    case 2:
        return &k->l[KL_SUB].decay;
    case 3:
        return &k->volume;
    case 4:
        return &k->l[KL_PUNCH].level;
    case 5:
        return &k->l[KL_CLICK].level;
    case 6:
        return &k->intensity;
    case 7:
        return &k->l[KL_SUB].tone;
    default:
        return 0;
    }
}
static float kick_knob_f(int i)          /* the part's knob i, 0..1 (its card's bar) */
{
    const float *p = boom_param(i);
    if (p)
        return *p;
    return (i & 3) ? (float)(boom_note() - BOOM_NOTE_LO) / (float)(BOOM_NOTE_HI - BOOM_NOTE_LO)
                   : (float)(set.kpre % BOOM_PRESETS) / (float)(BOOM_PRESETS - 1u);
}
static void kick_knob_turn(int i, int32_t d)
{
    float *p = boom_param(i);
    char t[16];
    if (!p && !(i & 3)) {                /* PRESET */
        boom_pre_turn(d);
        return;
    }
    if (p)
        *p = knob_f(*p, d, 0.02f, 0.0f, 1.0f);
    else                                 /* TUNE: a semitone a detent */
        boom_note_set(clamp(boom_note() + (d > 0 ? 1 : -1), BOOM_NOTE_LO, BOOM_NOTE_HI));
    kick_value(i, t);
    app_say(snd_long(kick_knob_name(i)), t);
    ui.redraw |= RG_BLOCKS | RG_FOOT;
    changed_kick();
}

/* EDIT's lower strip in the classic view (the drums' pages): the drums side by side, each its own waveform (one hit,
 * worked out again when its sound changes), the one edited outlined, lit on its hits */
#define DW 36u                           /* a drum's waveform: its columns */
static uint8_t drum_px[NDRUMS][DW];
static uint32_t drum_sig[NDRUMS] = {1u, 1u, 1u, 1u};
static uint32_t sig_of(const void *p, uint32_t n)
{
    uint32_t i, h = 2166136261u;
    for (i = 0; i < n; i++)
        h = (h ^ ((const uint8_t *)p)[i]) * 16777619u;
    return h;
}
static void draw_drums(const brk_status_t *st)
{
    uint32_t i, c;
    for (i = 0; i < NDRUMS; i++) {
        uint32_t sig = i ? sig_of(perc_voice(i - 1u), sizeof(perc_voice_t)) : sig_of(&set.kick, sizeof set.kick);
        if (sig != drum_sig[i]) {        /* its hit again, over a window that fits its decay, as tall as the box */
            uint32_t pk = 0u;
            drum_sig[i] = sig;
            if (i) {
                float d = perc_decay_ms(i - 1u, perc_voice(i - 1u)->decay) * 1.4f;
                perc_shape(i - 1u, perc_voice(i - 1u), drum_px[i], DW, (uint32_t)brk_clampf(d, 60.0f, 900.0f), 120u);
            } else
                kick_shape(&set.kick, drum_px[0], DW, 600u, 120u);
            for (c = 0; c < DW; c++)
                pk = drum_px[i][c] > pk ? drum_px[i][c] : pk;
            for (c = 0; c < DW && pk; c++)
                drum_px[i][c] = (uint8_t)(drum_px[i][c] * 28u / pk);
        }
    }
    cv_begin(240, 52, C_BLACK);
    for (i = 0; i < NDRUMS; i++) {       /* (AFFINE, 2x: a box 58 wide, its name over its hit, its key under it) */
        int32_t x = 1 + (int32_t)i * 60;
        int sel = i == ui.einst, hit = (st->playing && st->bar64 >= 0 &&
            (drum_now[i][st->bar64 >> 4 & 3] >> (st->bar64 & 15) & 1u)) ||
            (ui.aud_d == i && plat_ms() - ui.aud_t < 150u);   /* (LOAF BREAKER: or just played on its key) */
        char kn[2] = {(char)('1' + i), 0};
        uint16_t ink = hit ? C_BLACK : sel ? C_HI : C_GRAY;
        cv_rect(x, 2, 58, 48, sel ? C_WHITE : C_LINE);
        cv_rect(x + 1, 3, 56, 46, hit ? C_BLOCK_HI : C_BLACK);
        snd_text(x + 5, 7, DRUM_NAME[i], 2, hit ? C_BLACK : sel ? C_HI : C_GRAY, 0, 0);
        if (!st->playing)                /* (its key, to play it: stopped) */
            snd_text(x + 53, 37, kn, 2, hit ? C_BLACK : C_DIM, 1, 0);
        cv_rect(x + 5, 33, DW, 1, hit ? C_BLACK : C_LINE);   /* the centre line, the hit mirrored about it */
        for (c = 0; c < DW; c++)
            if (drum_px[i][c])
                cv_rect(x + 5 + (int32_t)c, 33 - drum_px[i][c] / 2, 1, drum_px[i][c] | 1, ink);
    }
    cv_blit(0, 140);
}

/* the four sounds along the bottom of BOOM's page and the drums' big view (the picture's rows 124..141; the screen's
 * 168..185, over the foot's): the one edited lit, the one just played on its key bright; stopped, its key (1-4) at
 * the right. Their names as the tabs have them (2x) */
#define DRUM_ROW_Y 124
static void drum_row(const brk_status_t *st, uint32_t ed, int32_t top)
{
    static const char *const TAG[NDRUMS] = {"BOOM", "SN", "HH", "CP"};
    uint32_t i;
    for (i = 0; i < NDRUMS; i++) {
        int32_t x = 1 + (int32_t)i * 60;
        int sel = i == ed, hit = ui.aud_d == i && plat_ms() - ui.aud_t < 150u;
        cv_rect(x, DRUM_ROW_Y, 58, 18, hit ? C_HI : sel ? C_BLOCK_HI : C_BLOCK);
        snd_text(x + 4, DRUM_ROW_Y + 4, TAG[i], 2, hit ? C_BLACK : sel ? C_WHITE : C_GRAY, 0, top);
        if (!st->playing) {
            char kn[2] = {(char)('1' + i), 0};
            snd_text(x + 55, DRUM_ROW_Y + 4, kn, 2, hit ? C_BLACK : C_DIM, 1, top);
        }
    }
}

/* BOOM's page (y 44..191; AFFINE, 2x): BOOM and its preset over its hit drawn large (its level dim, its wave over
 * it; its window the SUB's decay and a third, the decay marked by an amber line), the four sounds along the bottom
 * (stopped: keys 1-4 play them). Its knobs' names and values: the foot's (LOAF's cards gone with them) */
#define KV_W 232u
#define KV_MID 68                        /* the hit's centre line (the picture's rows: the screen's 44 on) */
#define KV_H 42u                         /* and its height either side */
static uint8_t kv_px[KV_W];              /* the hit's level, column by column */
static int8_t kv_wave[KV_W];             /* and its wave (the pitch falling: the cycles stretching) */
static uint32_t kv_sig = 1u, kv_t, kv_ms = 1000u;
static uint8_t kv_pend;                  /* (a change not drawn yet: main.c draws it 100 ms on) */
static void hit_picture(const uint8_t *px, const int8_t *wave, uint32_t n, float dms, uint32_t ms)
{                                        /* (BOOM's and the drums': the level, the wave, the decay's line; 0: none) */
    uint32_t i;
    int32_t dx = 4 + (int32_t)(dms / (float)ms * (float)n);
    cv_rect(4, KV_MID, (int32_t)n, 1, C_LINE);   /* (the middle) */
    for (i = 0; i < n; i++) {
        int32_t h = px ? px[i] : 0, w = wave ? wave[i] : 0;
        if (h)
            cv_rect(4 + (int32_t)i, KV_MID - h, 1, 2 * h + 1, C_BLOCK);
        if (w > 0)
            cv_rect(4 + (int32_t)i, KV_MID - w, 1, w, C_HI);
        else if (w < 0)
            cv_rect(4 + (int32_t)i, KV_MID + 1, 1, -w, C_HI);
    }
    if (dx < 4 + (int32_t)n) {           /* its DECAY, where it has gone (-40 dB) */
        int32_t y;
        for (y = KV_MID - (int32_t)KV_H - 6; y < KV_MID + (int32_t)KV_H + 8; y += 4)
            cv_rect(dx, y, 1, 2, C_AMB);
    }
}
static void boom_picture(const brk_status_t *st, int32_t top)
{
    cv_oy = -top;
    hit_picture(kv_px, kv_wave, KV_W, kick_decay_ms(KL_SUB, set.kick.l[KL_SUB].decay), kv_ms);
    snd_text(6, 4, "BOOM", 2, C_WHITE, 0, top);   /* BOOM, its preset */
    snd_text(234, 4, boom_pre_name(), 2, C_AMB, 1, top);
    drum_row(st, DR_KICK, top);
    cv_oy = 0;
}
static void draw_kick(const brk_status_t *st)
{
    uint32_t sig = sig_of(&set.kick, sizeof set.kick), top;
    if (sig != kv_sig && plat_ms() - kv_t < 100u)
        kv_pend = 1u;                    /* (a knob turning: the hit again at most every 100 ms, the last one drawn
                                          * then: main.c) */
    else if (sig != kv_sig) {            /* (its hit again, only when it changed) */
        uint32_t pk = 0u;
        kv_sig = sig;
        kv_t = plat_ms();
        kv_pend = 0u;
        kv_ms = (uint32_t)brk_clampf(kick_decay_ms(KL_SUB, set.kick.l[KL_SUB].decay) * 1.3f, 150.0f, 1600.0f);
        kick_wave(&set.kick, kv_wave, KV_W, kv_ms, KV_H);   /* (one pass: a column's level is its wave's size) */
        for (top = 0; top < KV_W; top++) {
            kv_px[top] = (uint8_t)(kv_wave[top] < 0 ? -kv_wave[top] : kv_wave[top]);
            pk = kv_px[top] > pk ? kv_px[top] : pk;
        }
        for (top = 0; pk && top < KV_W; top++) {   /* (as tall as it goes) */
            kv_px[top] = (uint8_t)((uint32_t)kv_px[top] * KV_H / pk);
            kv_wave[top] = (int8_t)((int32_t)kv_wave[top] * (int32_t)KV_H / (int32_t)pk);
        }
    }
    for (top = 0; top < 148u; top += 60u) {   /* (y 44..191, strips of 60 rows) */
        uint32_t h = 148u - top < 60u ? 148u - top : 60u;
        cv_begin(240, h, C_BLACK);
        boom_picture(st, (int32_t)top);
        cv_blit(0, 44u + top);
    }
}

/* ------------------------------------------------------------------------------ EDIT › BREAK (AFFINE) --- */
/* AFFINE (controls phase 2): EDIT's first part, the break itself (SYSTEM's PITCH part's code until then, SOUNDS ›
 * BREAK): K1 SPEED / KEEP LEN, K2 GRAIN, K3 SNAP, K4 the break's LEVEL; no items, so OCT is the pitch here as on
 * PERFORM. The pitch now, large, between the keys that move it (OCT- / OCT+: a semitone,
 * both: 0); K1 SPEED (the pitch makes a slice shorter or longer: the slice against its length, the tick) or KEEP LEN
 * (its length kept, in grains K2 long); SNAP (K3): what it is in this break, and where a key lands on a bar's 16ths */
static void brkp_body(const brk_status_t *st)
{
    char t[16];
    int32_t keep = pitch_keeps(), w, k, g = set.grain_ms ? set.grain_ms : 30, step = 0;
    float f = 1.0f, n = 0.0f;
    (void)st;
    cv_rect(4, 52, 50, 30, C_BLOCK);
    g_text(29, 62, "OCT-", 2, C_GRAY, 2);
    cv_rect(186, 52, 50, 30, C_BLOCK);
    g_text(211, 62, "OCT+", 2, C_GRAY, 2);
    t[0] = set.pitch > 0 ? '+' : 0;
    t[1] = 0;
    int_text(set.pitch, t + (set.pitch > 0));
    text_cat(t, " ST");
    g_text(120, 60, t, 3, set.pitch ? C_HI : C_WHITE, 2);
    g_seg(4, 96, 114, 22, "SPEED", !keep);
    g_seg(122, 96, 114, 22, "KEEP LEN", keep);
    for (k = 0; k < (set.pitch < 0 ? -set.pitch : set.pitch); k++)
        f *= set.pitch > 0 ? 0.94387f : 1.05946f;   /* (a semitone: 2^(1/12)) */
    w = (int32_t)(57.0f * f);
    w = w < 3 ? 3 : w > 112 ? 112 : w;
    cv_rect(6, 124, w, 12, keep ? C_LINE : C_AMB);
    for (k = 0; k < 57; k += g / 5 + 3)  /* (a grain 10-80 ms: 4-18 pixels) */
        cv_rect(124 + k, 124, 57 - k < g / 5 + 2 ? 57 - k : g / 5 + 2, 12, keep ? C_AMB : C_LINE);
    cv_rect(63, 121, 1, 18, C_GRAY);     /* (the slice's own length) */
    cv_rect(181, 121, 1, 18, C_GRAY);
    g_text(6, 148, "SNAP", 2, C_GRAY, 0);
    t[0] = 0;
    if (snap_index()) {
        n = SNAP_SL[snap_index()] * key_sps(brk_get_break());
        sixteenths_text(n, t);
        step = (int32_t)(n + 0.5f);
    } else
        text_cat(t, "OFF");
    g_text(234, 148, t, 2, snap_index() ? C_HI : C_DIM, 1);
    for (k = 0; k < 16; k++)
        cv_rect(8 + k * 14, 166, 12, 20, snap_index() && (step <= 1 || k % step == 0) ? C_HI : (k & 3) ? C_BLOCK : C_DIM);
}

static void brkp_value(int i, char *t)
{
    t[0] = 0;
    if (i == 0)
        text_cat(t, pitch_keeps() ? "KEEP LEN" : "SPEED");
    else if (i == 1) {
        int_text(set.grain_ms ? set.grain_ms : 30, t);
        text_cat(t, " MS");
    } else if (i == 2)
        text_cat(t, SNAP_SHORT[snap_index()]);
    else {                               /* (LOAF BREAKER: K4 the break's LEVEL, the MIXER's BREAK) */
        int_text(set.brk_level, t);
        text_cat(t, "%");
    }
}
static void brkp_turn(int i, int32_t d)
{
    char t[16];
    if (!glo_knob_ok(i)) {
        easy_no();
        return;
    }
    if (i == 0)
        set.pitch_keep = d > 0 ? 1u : 2u;   /* right KEEP LENGTH, left SPEED */
    else if (i == 1)
        set.grain_ms = (uint8_t)clamp((int32_t)(set.grain_ms ? set.grain_ms : 30u) + d * 5, 10, 80);
    else if (i == 2)
        set.snap = (uint8_t)(clamp((int32_t)snap_index() + (d > 0 ? 1 : -1), 0, SNAPS - 1) + 1);
    else
        set.brk_level = (uint8_t)clamp((int32_t)set.brk_level + d * 2, 0, 100);
    brkp_value(i, t);
    app_say(i == 0 ? "PITCH" : i == 1 ? "GRAIN" : i == 2 ? "SNAP" : "BREAK LEVEL", i == 0 && pitch_keeps()
            ? "KEEP LENGTH" : i == 2 ? SNAP_NAME[snap_index()] : t);
    changed();
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
static void draw_brkp(const brk_status_t *st)
{                                        /* (no waveform: rows 44-192 its own, as SYSTEM's were) */
    static const uint8_t TOP[4] = {44u, 92u, 140u, 192u};
    g_strips(TOP, 3u, brkp_body, st);
}
