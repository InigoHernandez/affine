/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH: the MIXER (LFO): the break, BOOM, the snare and hat, FX; swing and the drums' route. (Part of
 * app.c's unity: included there in order.) */

/* LOAF BREAKER: DUCK and COMP, MIX › MASTER and DYNAMICS (SEL + K1 and K3 too until AFFINE): f 0 DUCK (its amount,
 * attack, release, what it ducks), else COMP (its amount, ratio, attack, release) */
static uint8_t *dyn_byte(uint32_t f, int i)
{
    static uint8_t *const DUCKB[4] = {&set.duck, &set.duck_att, &set.duck_rel, &set.duck_what};
    static uint8_t *const COMPB[4] = {&set.comp, &set.comp_ratio, &set.comp_att, &set.comp_rel};
    return f ? COMPB[i & 3] : DUCKB[i & 3];
}
static void dyn_value(uint32_t f, int i, char *t)
{
    uint8_t v = *dyn_byte(f, i);
    t[0] = 0;
    if (i == 0) {                        /* the amount (COMP: its threshold) */
        if (!v)
            text_cat(t, "OFF");
        else if (f) {                    /* (AFFINE, 2x: "-36.0DB", to fit the foot beside COMP) */
            tenths_text(-(int32_t)v * 36 / 10, t);
            text_cat(t, "DB");
        } else {
            int_text(v, t);
            text_cat(t, "%");
        }
    } else if (!f) {
        if (i == 3)
            text_cat(t, v == 1u ? "+DRUMS" : "BREAK");
        else
            ms_text(i == 1 ? att_ms(v) : rel_ms(v), t);
    } else if (i == 1) {
        float r = COMP_RATIO[v & 7u];
        if (r < 2.0f)
            text_cat(t, "1.5");
        else
            int_text((int32_t)r, t);
        text_cat(t, ":1");
    } else
        ms_text(i == 2 ? att_ms(v) : rel_ms(v), t);
}
static void dyn_turn(uint32_t f, int i, int32_t d)
{
    static const char *const N[2][4] = {{"DUCK", "DUCK ATTACK", "DUCK RELEASE", "DUCK DUCKS"},
                                        {"COMP", "COMP RATIO", "COMP ATTACK", "COMP RELEASE"}};
    uint8_t *b = dyn_byte(f, i);
    char t[16];
    if (!f && i == 3)
        *b = (uint8_t)clamp((int32_t)*b + (d > 0 ? 1 : -1), 0, 1);
    else if (f && i == 1)
        *b = (uint8_t)clamp((int32_t)*b + (d > 0 ? 1 : -1), 0, 7);
    else
        *b = (uint8_t)clamp((int32_t)*b + d * 2, 0, 100);
    dyn_value(f, i, t);
    app_say(N[f != 0u][i & 3], t);
    changed();
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
/* the MIXER's knob i on half h, as the foot and the strip show it */
static void mix_value(uint32_t h, int i, char *t)
{
    t[0] = 0;
    if (h == 3u) {                       /* LOAF BREAKER: DYNAMICS: COMP's attack, release; DUCK's release, what it ducks */
        dyn_value(i < 2 ? 2u : 0u, i == 0 ? 2 : i == 1 ? 3 : i == 2 ? 2 : 3, t);
        return;
    }
    if (h == 2u) {                       /* LOAF BREAKER: MASTER: COMP, its RATIO, DUCK, MAX OUT */
        if (i < 3)
            dyn_value(i == 2 ? 0u : 2u, i == 2 ? 0 : i, t);
        else {
            uint32_t db = max_out_db();
            if (db)
                text_cat(t, "-");
            int_text((int32_t)db, t + (db ? 1 : 0));
            text_cat(t, " DB");
        }
        return;
    }
    if (h) {                             /* LOAF BREAKER: SENDS: FX, the drums' route (SWING: PATTERN's GROOVE) */
        if (i == 0)
            pct_text(set.fx.wet, t);
        else if (i == 1)
            text_cat(t, set.drums_chain ? "THRU FX" : "AROUND");   /* (AFFINE, 2x: as the strip's boxes) */
        else if (i == 2)
            text_cat(t, ARP_THRU[(set.ovl_pad >> 6) & 3u]);
        else
            text_cat(t, ROLL_THRU_NAME[set.pads.rsv[0] % 3u]);   /* (0.9.40: the rolls' THRU) */
        return;
    }
    if (i == 0) {
        int_text(set.brk_level, t);
        text_cat(t, "%");
    } else if (i < 3)                    /* (K4: the MELODY's, 0.200-0.208; the ARP's route in 0.9.29-0.9.30, SENDS'
                                          * alone since, FLEXOR) */
        pct_text(i == 1 ? set.kick.volume : set.perc.volume, t);
}

/* the MIXER's strip: four channels side by side (a fader, its value; BREAK and KICK with their level meters, FX with
 * the chain's slots; their values are in the foot), drawn as one 240 x 104 picture in two 52-row strips. GROOVE:
 * swing, the drums' route. AFFINE (2x): every word at 2x and whole in one strip; SENDS' columns as wide as their
 * choices; MASTER's ratio and DYNAMICS' times drawn (their curves), not written: the foot has the numbers */
#define MX_Y 18                          /* the faders and meters: from row 18, 80 rows (the values are in the foot) */
#define MX_H 80
static int32_t meter_px(float lv, int32_t h)   /* a level's height on an h-pixel meter: -48..0 dBFS */
{
    float db = lv > 0.0039f ? 6.0206f * brk_log2f(lv) : -48.0f;
    return (int32_t)(brk_clampf((db + 48.0f) * (1.0f / 48.0f), 0.0f, 1.0f) * (float)h + 0.5f);
}
static void mix_meter(int32_t x, float lv)
{
    int32_t h = meter_px(lv, MX_H);
    cv_rect(x, MX_Y, 5, MX_H, C_BLOCK);
    if (h)
        cv_rect(x, MX_Y + MX_H - h, 5, h, h >= MX_H - 2 ? C_WHITE : h >= MX_H * 5 / 6 ? C_AMB : C_HI);
}
static void mix_fader(int32_t x, float v, int on)   /* 0..1 from the bottom */
{
    int32_t h = (int32_t)(brk_clampf(v, 0.0f, 1.0f) * (float)MX_H + 0.5f);
    cv_rect(x, MX_Y, 14, MX_H, C_BLOCK);
    if (h)
        cv_rect(x + 2, MX_Y + MX_H - h, 10, h, on ? C_HI : C_DIM);
    cv_rect(x - 1, MX_Y + MX_H - h - 1, 16, 2, C_WHITE);   /* the cap */
}
/* AFFINE (2x): a word, its capitals' top on row ycap of the picture, written in the strip that holds it whole (top:
 * the strip's first row) at that strip's own rows (cv_oy aside: the text audit sees it where it lands); the layout
 * keeps every word inside one strip */
static void mx_text(int32_t x, int32_t ycap, int32_t top, const char *s, uint16_t c)
{
    int32_t oy = cv_oy, z = aff_fz(&FONT_XS);
    if (ycap < top || ycap + (int32_t)AFF_CAP_H * z > top + 52)
        return;
    cv_oy = 0;
    cv_text(x, ycap - top - aff_foff(&FONT_XS, z, 1), &FONT_XS, s, c);
    cv_oy = oy;
}
/* a choice, w x 24 at (x, y) of the picture, its word in the middle: lit when taken */
static void mx_box(int32_t x, int32_t y, int32_t w, const char *s, int on, int32_t top)
{
    cv_rect(x, y, w, 24, on ? C_BLOCK_HI : C_BLOCK);
    mx_text(x + (w - text_w(&FONT_XS, s)) / 2, y + 7, top, s, on ? C_BLACK : C_GRAY);
}
/* MASTER's RATIO: COMP's in / out curve, -48 .. 0 dB each way on 48 px (its threshold dotted; dim when it is off) */
static void mix_ratio(int32_t bx, int32_t by)
{
    float thr = -0.36f * (float)(set.comp > 100u ? 100u : set.comp), r = COMP_RATIO[set.comp_ratio & 7u];
    int32_t i, py = 0, tx = bx + (int32_t)(thr + 48.0f);
    cv_rect(bx, by, 48, 48, C_BLOCK);
    for (i = 0; i < 48; i += 2)          /* in = out */
        cv_rect(bx + i, by + 47 - i, 1, 1, C_LINE);
    if (set.comp)
        for (i = 0; i < 48; i += 3)
            cv_rect(tx, by + i, 1, 1, C_DIM);
    for (i = 0; i < 48; i++) {
        float in = -48.0f + (float)i * (48.0f / 47.0f), out = in > thr ? thr + (in - thr) / r : in;
        int32_t y = by + 47 - (int32_t)((out + 48.0f) * (47.0f / 48.0f) + 0.5f);
        if (i)
            cv_line(bx + i - 1, py, bx + i, y, set.comp ? C_HI : C_DIM);
        py = y;
    }
}
/* DYNAMICS: a gain over time, x0 .. x0 + w, 0 dB on row y0, down d px at most: COMP's (a hit: its reduction in over its
 * ATTACK, held, out over its RELEASE) or DUCK's (three kicks, each down at once, back over its RELEASE: a long one
 * pumps). The widths follow the knobs (their scale: 0 .. 100) */
static void mix_env(int32_t x0, int32_t w, int32_t y0, int32_t d, int duck, uint16_t c)
{
    int32_t i, py = y0, a = 2 + (int32_t)(set.comp_att > 100u ? 100u : set.comp_att) * 30 / 100;
    int32_t rc = 4 + (int32_t)(set.comp_rel > 100u ? 100u : set.comp_rel) * 48 / 100;
    int32_t rd = 4 + (int32_t)(set.duck_rel > 100u ? 100u : set.duck_rel) * 60 / 100;
    for (i = 0; i < w; i += 4)           /* 0 dB */
        cv_rect(x0 + i, y0, 2, 1, C_LINE);
    for (i = 0; i < w; i++) {
        float u, e = 0.0f;
        int32_t y;
        if (duck) {                      /* (a kick every 37 px, from 4) */
            int32_t k = i < 4 ? -1 : (i - 4) / 37;
            if (k >= 0) {
                u = (float)(i - 4 - k * 37) / (float)rd;
                e = u < 1.0f ? (1.0f - u) * (1.0f - u) : 0.0f;
                if (i - 4 == k * 37)
                    cv_rect(x0 + i, y0 - 4, 1, 3, C_GRAY);   /* (the kick) */
            }
        } else if (i >= 10 && i < 10 + a) {
            u = (float)(i - 10) / (float)a;
            e = 1.0f - (1.0f - u) * (1.0f - u);
        } else if (i >= 10 + a && i < 24 + a)
            e = 1.0f;
        else if (i >= 24 + a) {
            u = (float)(i - 24 - a) / (float)rc;
            e = u < 1.0f ? (1.0f - u) * (1.0f - u) : 0.0f;
        }
        y = y0 + (int32_t)(e * (float)d + 0.5f);
        if (y > y0 + 1)
            cv_rect(x0 + i, y0 + 1, 1, y - y0 - 1, C_LINE);
        if (i)
            cv_line(x0 + i - 1, py, x0 + i, y, c);
        py = y;
    }
}
static void mix_picture(int32_t top)
{
    static const char *const NAME[4][4] = {{"BREAK", "BOOM", "PERC", ""}, {"FX", "DRUMS", "ARP", "ROLL"},
                                           {"COMP", "RATIO", "DUCK", "MAX"}, {"COMP", "DUCK", "", ""}};
    static const int16_t COLX[4][4] = {{0, 60, 120, 180}, {0, 41, 114, 177}, {0, 60, 120, 180}, {0, 120, 0, 0}};
    uint32_t c, pg = ui.mpage % 4u;
    cv_oy = -top;                        /* (this strip's rows of the picture) */
    for (c = 0; c < (pg == 3u ? 2u : 4u); c++) {
        int32_t x = COLX[pg][c];
        if (c)
            cv_rect(x, 4, 1, 96, C_LINE);
        if (NAME[pg][c][0])
            mx_text(x + 6, 3, top, NAME[pg][c], C_GRAY);
    }
    if (pg == 3u) {                      /* DYNAMICS: COMP's reduction over a hit, DUCK's over the kicks; what DUCK
                                          * takes down (the break, the drums too) */
        float q = (float)(set.comp > 100u ? 100u : set.comp) * 0.01f * (1.0f - 1.0f / COMP_RATIO[set.comp_ratio & 7u]);
        cv_rect(4, 18, 110, 80, C_BLOCK);
        mix_env(5, 108, 24, set.comp ? 10 + (int32_t)(q * 60.0f) : 36, 0, set.comp ? C_HI : C_DIM);
        cv_rect(124, 18, 112, 50, C_BLOCK);
        mix_env(125, 110, 24, set.duck ? 6 + (int32_t)(set.duck > 100u ? 100u : set.duck) * 36 / 100 : 24, 1,
                set.duck ? C_HI : C_DIM);
        mx_box(123, 74, 56, "BREAK", set.duck > 0u, top);
        mx_box(181, 74, 56, "DRUMS", set.duck > 0u && set.duck_what == 1u, top);
        return;
    }
    if (pg == 2u) {                      /* MASTER: COMP and its reduction, its ratio, DUCK, MAX OUT */
        mix_fader(10, (float)set.comp * 0.01f, set.comp > 0u);
        mix_meter(30, set.comp ? brk_exp2f(-comp_gr * 0.16610f) : 0.0f);   /* (the gain it leaves: 0 dB full) */
        mix_ratio(66, 34);
        mix_fader(130, (float)set.duck * 0.01f, set.duck > 0u);
        mix_fader(190, (float)(12u - max_out_db()) * (1.0f / 12.0f), 1);
        return;
    }
    if (pg) {                            /* LOAF BREAKER: SENDS: FX, the chain's slots beside it; the drums' route */
        static const uint8_t CODE[3] = {1u, 2u, 0u};   /* (the ARP's route: BREAK, BOOM, THRU) */
        uint32_t j;
        mix_fader(8, set.fx.wet, set.fx.wet > 0.005f);
        for (c = 0; c < fx_nslots(); c++) {   /* (LOAF BREAKER: BEGINNER's two) */
            const brk_fxslot_t *sl = &set.fx.slot[c];
            int on = sl->on && sl->type != FXT_EMPTY;
            cv_rect(28, MX_Y + (int32_t)c * 21, 6, 17, on ? C_BLOCK_HI : sl->type == FXT_EMPTY ? C_BLACK : C_BLOCK);
            if (sl->type == FXT_EMPTY)
                cv_rect(28, MX_Y + (int32_t)c * 21 + 8, 6, 1, C_LINE);
        }
        mx_box(44, 18, 66, "THRU FX", set.drums_chain != 0u, top);   /* the drums' route, the one taken lit */
        mx_box(44, 46, 66, "AROUND", !set.drums_chain, top);
        for (j = 0; j < 3u; j++) {       /* the ARP's route; (0.9.40, FLEXOR) the three rolls' THRU */
            mx_box(118, 18 + (int32_t)j * 28, 56, ARP_THRU[CODE[j]], ((set.ovl_pad >> 6) & 3u) % 3u == CODE[j], top);
            mx_box(181, 18 + (int32_t)j * 28, 56, ROLL_THRU_NAME[j], set.pads.rsv[0] % 3u == j, top);
        }
        return;
    }
    mix_fader(10, (float)set.brk_level * 0.01f, set.brk_level > 0u);
    mix_meter(30, meter_brk);
    mix_fader(70, set.kick.volume, set.kick.volume > 0.0f);
    mix_meter(90, meter_kick);
    mix_fader(130, set.perc.volume, set.perc.volume > 0.0f);
    mix_meter(150, meter_perc);
}
static void draw_mixer(void)
{
    cv_begin(240, 52, C_BLACK);
    mix_picture(0);
    cv_oy = 0;
    cv_blit(0, 88);
    cv_begin(240, 52, C_BLACK);
    mix_picture(52);
    cv_oy = 0;
    cv_blit(0, 140);
}
