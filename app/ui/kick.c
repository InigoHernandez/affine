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
static const char *const BOOM_KNOB[BOOM_PARTS][4] = {{"PRESET", "TUNE", "DECAY", "LEVEL"},
                                                     {"PUNCH", "CLICK", "DRIVE", "TONE"}};
#define BOOM_NOTE_LO 23                  /* (TUNE: B0 .. F2 as MIDI notes, the SUB's 30..90 Hz) */
#define BOOM_NOTE_HI 41
static uint32_t boom_part(void) { return set.novice ? 0u : ui.kpage % BOOM_PARTS; }   /* (BEGINNER: BOOM alone) */
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
static void kick_open(void)              /* EDIT: BOOM's page */
{
    ui.page = PG_KICK;
    ui.einst = DR_KICK;
    ui.redraw |= RG_FOOT | RG_BLOCKS;
    page_say();
}
static void boom_part_turn(int32_t d)    /* ALGORITHM on BOOM: its parts, BOOM and SHAPE (EXPERT) */
{
    if (set.novice) {
        easy_no();
        return;
    }
    ui.kpage = (uint8_t)((ui.kpage % BOOM_PARTS + (d > 0 ? 1u : BOOM_PARTS - 1u)) % BOOM_PARTS);
    page_say();
    ui.redraw |= RG_HEAD | RG_BLOCKS | RG_FOOT;
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
    app_say(kick_knob_name(i), t);
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
                drum_px[i][c] = (uint8_t)(drum_px[i][c] * 30u / pk);
        }
    }
    cv_begin(240, 52, C_BLACK);
    for (i = 0; i < NDRUMS; i++) {
        int32_t x = 1 + (int32_t)i * 48;
        int sel = i == ui.einst, hit = (st->playing && st->bar64 >= 0 &&
            (drum_now[i][st->bar64 >> 4 & 3] >> (st->bar64 & 15) & 1u)) ||
            (ui.aud_d == i && plat_ms() - ui.aud_t < 150u);   /* (LOAF BREAKER: or just played on its key) */
        char kn[2] = {(char)('1' + i), 0};
        uint16_t ink = hit ? C_BLACK : sel ? C_HI : C_GRAY;
        cv_rect(x, 2, 46, 48, sel ? C_WHITE : C_LINE);
        cv_rect(x + 1, 3, 44, 46, hit ? C_BLOCK_HI : C_BLACK);
        cv_text(x + 5, 4, &FONT_T, DRUM_NAME[i], hit ? C_BLACK : sel ? C_HI : C_GRAY);
        if (!st->playing)                /* (its key, to play it: stopped) */
            cv_text(x + 42 - text_w(&FONT_T, kn), 37, &FONT_T, kn, hit ? C_BLACK : C_DIM);
        cv_rect(x + 5, 33, DW, 1, hit ? C_BLACK : C_LINE);   /* the centre line, the hit mirrored about it */
        for (c = 0; c < DW; c++)
            if (drum_px[i][c])
                cv_rect(x + 5 + (int32_t)c, 33 - drum_px[i][c] / 2, 1, drum_px[i][c] | 1, ink);
    }
    cv_blit(0, 140);
}

/* BOOM's page (y 44..191): its hit drawn large over its level (its window the SUB's decay and a third, the decay
 * marked in amber), its preset, its note; the four sounds along the bottom (stopped: keys 1-4 play them); under it the
 * part's knobs as cards (the drums' big view's look) */
#define KV_W 232u
static uint8_t kv_px[KV_W];              /* the hit's level, column by column */
static int8_t kv_wave[KV_W];             /* and its wave (the pitch falling: the cycles stretching) */
static uint32_t kv_sig = 1u, kv_t, kv_ms = 1000u;
static uint8_t kv_pend;                  /* (a change not drawn yet: main.c draws it 100 ms on) */
static void boom_picture(const brk_status_t *st, int32_t top)
{
    uint32_t i;
    float dms = kick_decay_ms(KL_SUB, set.kick.l[KL_SUB].decay);
    int32_t dx = 4 + (int32_t)(dms / (float)kv_ms * (float)KV_W);
    char t[24];
    cv_oy = -top;
    cv_rect(4, 50, KV_W, 1, C_LINE);     /* (the middle) */
    for (i = 0; i < KV_W; i++) {         /* the hit: its level (dim, mirrored), its wave over it */
        int32_t h = kv_px[i], w = kv_wave[i];
        if (h)
            cv_rect(4 + (int32_t)i, 50 - h, 1, 2 * h + 1, C_BLOCK);
        if (w > 0)
            cv_rect(4 + (int32_t)i, 50 - w, 1, w, C_HI);
        else if (w < 0)
            cv_rect(4 + (int32_t)i, 51, 1, -w, C_HI);
    }
    if (dx < 4 + (int32_t)KV_W) {        /* its DECAY, where it has gone (-40 dB) */
        int32_t y;
        for (y = 18; y < 84; y += 4)
            cv_rect(dx, y, 1, 2, C_AMB);
        int_text((int32_t)(dms + 0.5f), t);
        text_cat(t, " MS");
        cv_text(dx + 3 < 200 ? dx + 3 : dx - 3 - text_w(&FONT_XS, t), 72, &FONT_XS, t, C_AMB);
    }
    t[0] = 0;                            /* BOOM 808, its note and Hz */
    text_cat(t, "BOOM ");
    text_cat(t, boom_pre_name());
    cv_text(6, 2, &FONT_B, t, C_WHITE);
    note_name(boom_note() - 48, t);
    text_cat(t, "  ");
    hz_text(boom_hz(), t + text_n(t));
    cv_text(234 - text_w(&FONT_XS, t), 4, &FONT_XS, t, C_AMB);
    for (i = 0; i < NDRUMS; i++) {       /* the four sounds, BOOM lit (stopped: key i + 1 plays it) */
        int32_t x = 4 + (int32_t)i * 59;
        int sel = i == DR_KICK, hit = ui.aud_d == i && plat_ms() - ui.aud_t < 150u;
        cv_rect(x, 87, 55, 12, hit ? C_HI : sel ? C_BLOCK_HI : C_BLOCK);
        cv_text(x + 3, 88, &FONT_T, DRUM_NAME[i], hit ? C_BLACK : sel ? C_WHITE : C_GRAY);
        if (!st->playing) {
            char kn[2] = {(char)('1' + i), 0};
            cv_text(x + 52 - text_w(&FONT_T, kn), 88, &FONT_T, kn, hit ? C_BLACK : C_DIM);
        }
    }
    cv_oy = 0;
}
static void boom_cards(void)             /* the part's four knobs: its name, its value large, its bar */
{
    uint32_t i;
    char t[16];
    cv_begin(240, 48, C_BLACK);
    for (i = 0; i < 4u; i++) {
        int32_t x = 2 + (int32_t)i * 60;
        cv_rect(x, 0, 56, 46, C_BLOCK);
        cv_text(x + 4, 3, &FONT_XS, kick_knob_name((int)i), C_GRAY);
        kick_value((int)i, t);
        cv_text(x + 4, 16, text_w(&FONT_B, t) <= 50 ? &FONT_B : &FONT_XS, t, C_WHITE);
        cv_rect(x + 4, 38, 48, 3, C_BLACK);
        cv_rect(x + 4, 38, (int32_t)(48.0f * brk_clampf(kick_knob_f((int)i), 0.0f, 1.0f) + 0.5f), 3, C_HI);
    }
    cv_blit(0, 144);
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
        kick_shape(&set.kick, kv_px, KV_W, kv_ms, 34u);
        kick_wave(&set.kick, kv_wave, KV_W, kv_ms, 34u);
        for (top = 0; top < KV_W; top++)
            pk = kv_px[top] > pk ? kv_px[top] : pk;
        for (top = 0; pk && top < KV_W; top++) {   /* (as tall as it goes) */
            kv_px[top] = (uint8_t)((uint32_t)kv_px[top] * 34u / pk);
            kv_wave[top] = (int8_t)((int32_t)kv_wave[top] * 34 / (int32_t)pk);
        }
    }
    for (top = 0; top < 100u; top += 60u) {   /* (y 44..143, strips of 60 rows) */
        uint32_t h = 100u - top < 60u ? 100u - top : 60u;
        cv_begin(240, h, C_BLACK);
        boom_picture(st, (int32_t)top);
        cv_blit(0, 44u + top);
    }
    boom_cards();
}
