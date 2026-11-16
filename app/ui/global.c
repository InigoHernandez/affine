/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH: the GLOBAL page: MIDI clock, Bluetooth MIDI, pitch, snap, RESET ALL. (Part of app.c's unity: included
 * there in order.) */

static void reset_all(void);             /* (below, with the break's change) */
/* the GLOBAL page's knobs: clock in, clock out, Bluetooth, where MIDI goes */
static uint8_t *sfx_knob(int i)          /* LOAF BREAKER: GLO EFFECTS' K1-K4: AMOUNT, on a KICK, at the DROP, LENGTH */
{
    return i == 0 ? &set.scrfx : i == 1 ? &set.sfx_kick : i == 2 ? &set.sfx_drop : &set.sfx_len;
}
static void sfx_toggle(uint32_t e, int on)   /* LOAF BREAKER: GLO EFFECTS: effect e (SFX_*) on (1), off (0), or turned
                                          * over (-1) */
{
    uint8_t b = (uint8_t)(1u << (e - 1u));
    set.sfx_on = (uint8_t)(on < 0 ? set.sfx_on ^ b : on ? set.sfx_on | b : set.sfx_on & ~b);
    ui.sfx_cur = (uint8_t)e;
    changed();
    app_say(SFX_NAME[e], set.sfx_on & b ? "ON" : "OFF");
    ui.redraw = RG_ALL;
}
/* LOAF BREAKER (0.201, the pages by domain): GP_FILL, SWING and FILL, is SEQ's SETTINGS, this page's code shown
 * there. AFFINE (controls phase 2): GLO's parts MIDI (0), SETUP (2), SCREEN (its items EFFECTS 3, FX SET 4, VIEWS
 * GP_VIEW), RESET (5); the break's PITCH part (1) is EDIT's BREAK now (kick.c, PG_BREAK) */
#define GP_FILL 6u
#define GP_VIEW 7u
static void drum_view_turn(int32_t d);   /* (GLO › SCREEN › VIEWS K1: perc.c) */
static int tut_on(void);                 /* (LOAF BREAKER: the TUTORIAL, tutor.c: further on) */
static void tut_start(void);
static void tut_quit(void);
static void global_value(int i, char *t)
{
    t[0] = 0;
    if (ui.gpage == GP_FILL) {           /* (SEQ's SETTINGS: SWING; 0.9.31, FLEXOR: BREAK ROWS, FOLLOW) */
        if (i == 0) {
            int_text(set.swing, t);
            text_cat(t, "%");
        } else if (i == 1)
            text_cat(t, bk_seq() & 2u ? "16 STEPS" : "32 STEPS");
        else if (i == 2)
            text_cat(t, bk_seq() & 1u ? "OFF" : "ON");
        return;
    }
    if (ui.gpage == 4u) {                /* (FX SET: each 0..100 %) */
        pct_text((float)*sfx_knob(i) * 0.01f, t);
        return;
    }
    if (ui.gpage == GP_VIEW) {           /* (VIEWS: K1 the drums') */
        if (!i)
            text_cat(t, ui.kclassic ? "CLASSIC" : ui.kview ? "WAVE" : "RGB");
        return;
    }
    if (ui.gpage == 3u) {                /* (EFFECTS: K1 the one pointed at, K2 it on or off) */
        uint32_t e = ui.sfx_cur < SFX_CRT || ui.sfx_cur >= SFX_N ? SFX_CRT : ui.sfx_cur;
        if (i == 0)
            text_cat(t, SFX_NAME[e]);
        else if (i == 1)
            text_cat(t, SFX_ON(e) ? "ON" : "OFF");
        return;                          /* (K4 EDIT VIEW and PRESETS DRUM VIEW ZOOM until 0.9.28: KICK › KNOBS › TOOLS) */
    }
    if (ui.gpage == 2u) {                /* SETUP (FLEXOR, 0.213: the knobs in the screen's order, top to bottom): K1 SKIN,
                                          * K2 WAVE ZOOM, K3 the TUTORIAL (AFFINE: LOAF's MODE), K4 AUTO SAVE */
        if (i == 0)                      /* (LOAF BREAKER, 0.237: the names nine letters at most, to fit its column) */
            text_cat(t, SKIN_NAME[skin_idx()]);
        else if (i == 1)                 /* (WAVE ZOOM: HOME's SETTINGS since 0.9.2) */
            ;
        else if (i == 2)                 /* (AFFINE, 16 Nov 2026: the TUTORIAL running or not; one mode for everyone) */
            text_cat(t, tut_on() ? "ON" : "OFF");
        else
            text_cat(t, set.auto_save ? "ON" : "OFF");
        return;
    }
    text_cat(t, i == 0 ? (set.clk_in ? "FOLLOW" : "OFF") : i == 1 ? (set.clk_out ? "ON" : "OFF")
                : i == 2 && !NOVICE ? (ui.meter_on ? "ON" : "OFF")
                : "");                   /* (LOAF BREAKER: K3 the METER, EXPERT only; K4 BREAK TIME 0.256-0.9.1: HOME's
                                          * SETTINGS now) */
}

/* LOAF BREAKER (FLEXOR, 11 Nov 2026): BEGINNER's BREAK its LEVEL alone (K4; the pitch: OCT), GROOVE its SWING alone
 * (AFFINE: the TUTORIAL's alone) */
static int glo_knob_ok(int i)
{
    return !NOVICE || (ui.page == PG_BREAK ? i == 3 : ui.page != PG_GLOBAL || ui.gpage != GP_FILL || i < 3);
}
static void global_turn(int i, int32_t d)
{
    static const char *const NAME[4] = {"CLOCK IN", "CLOCK OUT", "BLUETOOTH", "BREAK TIME"};
    char t[16];
    if (!glo_knob_ok(i)) {
        easy_no();
        return;
    }
    if (ui.gpage == GP_FILL) {           /* LOAF BREAKER: SEQ's SETTINGS (GROOVE until 0.9.30): K1 SWING; 0.9.31, FLEXOR:
                                          * K2 BREAK ROWS, the FULL view's (right 32), K3 FOLLOW, the grid's bar the one
                                          * playing (right ON) */
        static const char *const SN[3] = {"SWING", "BREAK ROWS", "FOLLOW"};
        if (i == 0)
            set.swing = (uint8_t)clamp((int32_t)set.swing + d, 50, 75);
        else if (i < 3) {
            uint32_t b = i == 1 ? 2u : 1u;
            set.bkeys.bk[BK_SEQ] = (uint8_t)(0xD0u | (d > 0 ? bk_seq() & ~b : bk_seq() | b));
            if (brk_view())              /* (the FULL view's rows now) */
                set.brkv = (uint8_t)(BRKV_TAG | (bk_seq() & 2u ? 1u : 2u));
        } else
            return;
        set_dirty = 1u;
        global_value(i, t);
        app_say(SN[i], t);
        ui.redraw |= RG_HEAD | RG_BLOCKS | RG_FOOT;
        return;
    }
    if (ui.gpage == 3u) {                /* LOAF BREAKER: EFFECTS: K1 the effect, K2 it on (right) or off (left) */
        uint32_t e = ui.sfx_cur < SFX_CRT || ui.sfx_cur >= SFX_N ? SFX_CRT : ui.sfx_cur;
        if (i == 0) {
            ui.sfx_cur = (uint8_t)clamp((int32_t)e + (d > 0 ? 1 : -1), SFX_CRT, SFX_N - 1);
            app_say("EFFECT", SFX_NAME[ui.sfx_cur]);
            ui.redraw |= RG_BLOCKS | RG_FOOT;
        } else if (i == 1)
            sfx_toggle(e, d > 0);
                                         /* (K3 WAVE until 0.9.24: HOME's SETTINGS K2 alone; K4 EDIT VIEW until 0.9.28:
                                          * KICK › MORE › TOOLS, FLEXOR; neither named here since 0.9.30) */
        return;
    }
    if (ui.gpage == GP_VIEW) {
        if (!i)
            drum_view_turn(d);
        return;
    }
    if (ui.gpage == 4u) {                /* LOAF BREAKER: FX SET: K1 AMOUNT, K2 KICK, K3 DROP, K4 LENGTH, 5 % a detent */
        static const char *const SN[4] = {"EFFECT AMOUNT", "ON A BOOM", "AT THE DROP", "MOMENTS' LENGTH"};
        uint8_t *p = sfx_knob(i);
        *p = (uint8_t)clamp((int32_t)*p + d * 5, 0, 100);
        changed();
        global_value(i, t);
        app_say(SN[i & 3], t);
        ui.redraw = RG_ALL;
        return;
    }
    if (ui.gpage == 2u) {                /* SETUP: K1 the skin, the whole screen in its colours at once */
        if (i == 0) {
            set.skin = (uint8_t)(SKIN_NEW | (skin_idx() + (d > 0 ? 1u : NSKINS - 1u)) % NSKINS);
            changed();                   /* (settings_apply: skin_apply) */
            app_say("SKIN", SKIN_NAME[skin_idx()]);
            ui.redraw = RG_ALL;
        } else if (i == 1) {             /* (K2 WAVE ZOOM until 0.9.1: HOME's SETTINGS, FLEXOR) */
            app_say("WAVE ZOOM", "HOME > SETTINGS");
            return;
        } else if (i == 2) {             /* K3 the TUTORIAL: right starts it (from the step it was left at), left quits
                                          * it (AFFINE, 16 Nov 2026, the owner: one mode for everyone, LOAF BREAKER's
                                          * BEGINNER / EXPERT gone; GLO quits it too) */
            if (d > 0 && !tut_on())
                tut_start();
            else if (d < 0 && tut_on())
                tut_quit();
            ui.redraw |= RG_BLOCKS | RG_FOOT;
        } else {                         /* K4 AUTO SAVE: right on, left off */
            set.auto_save = d > 0 ? 1u : 0u;
            changed();
            app_say("AUTO SAVE", set.auto_save ? "ON" : "OFF");
            ui.redraw |= RG_BLOCKS | RG_FOOT;
        }
        return;
    }
    if (i == 0) {
        set.clk_in = d > 0;
        if (!set.clk_in) {               /* the tempo is the knob's again */
            ext.i = ext.n = 0u;
            ext.run = 0u;
            ext.bpm = ext.used = 0.0f;
        }
    } else if (i == 1)
        set.clk_out = d > 0;
    else if (i == 2 && !NOVICE) {        /* LOAF BREAKER: K3 the METER, EXPERT only (0.213; SETUP's before): right on,
                                          * left off (not saved): CPU, IMG, FR in the header */
        ui.meter_on = d > 0 ? 1u : 0u;
        app_say("METER", ui.meter_on ? "ON: CPU, IMG, FR" : "OFF");
        ui.redraw |= RG_HEAD | RG_FOOT;
        return;
    } else {                             /* (K4 BREAK TIME, 0.256-0.9.1: HOME's SETTINGS now, FLEXOR) */
        app_say("BREAK TIME", "HOME > SETTINGS");
        return;
    }
    global_value(i, t);
    app_say(NAME[i], t);
    changed();
}

/* ------------------------------------------------------------------------------------ AFFINE: the body at 2x --- */
/* AFFINE (the owner, 0.1 on the FM-1: "everything in the UI looks very small"): SYSTEM's parts and LIVE SETTINGS at
 * 2x, the least; the foot already says each knob's name and value, so the body shows pictures (what a setting does,
 * the choices, the ports) and few words. The body is drawn in strips (the canvas: 60 rows at most), the page's
 * drawing whole into each, cv_oy at the strip's first row: a rect across two strips is cut between them; a text goes
 * into the one strip that holds it (each placed inside one; the host's audit names one across two) */
static int32_t g_top;                    /* the strip being drawn: its first screen row */
static void g_text(int32_t x, int32_t y, const char *s, int32_t z, uint16_t c, int align)
{                                        /* s at the screen's (x, y), y its capitals' top, z times (2 the least); align
                                          * 0 left, 1 right (x its end), 2 centred on x */
    int32_t ly = y - g_top;
    if (align)
        x -= aff_text_w(s, z) / (align == 1 ? 1 : 2);
    if (ly + (int32_t)AFF_CAP_H * z <= 0 || ly >= (int32_t)cv_h)
        return;                          /* (another strip's) */
    aff_aud_add(x, ly, z, s);
    aff_text(x, y, s, z, c, 0, 0);       /* (cv_oy: into the strip's rows) */
}
static void g_strips(const uint8_t *top, uint32_t n, void (*draw)(const brk_status_t *), const brk_status_t *st)
{                                        /* the body in n strips from top[0] to top[n] */
    uint32_t b;
    for (b = 0; b < n; b++) {
        cv_begin(240, (uint32_t)(top[b + 1] - top[b]), C_BLACK);
        g_top = top[b];
        cv_oy = -g_top;
        draw(st);
        cv_oy = 0;
        cv_blit(0, top[b]);
    }
}
static void g_tri(int32_t x, int32_t y, int32_t h, uint16_t c)   /* a triangle pointing right: x, y its left top */
{
    int32_t r;
    for (r = 0; r < h; r++)
        cv_rect(x, y + r, (r < h / 2 ? r : h - 1 - r) + 1, 1, c);
}
static void g_seg(int32_t x, int32_t y, int32_t w, int32_t h, const char *s, int on)
{                                        /* one of a knob's choices: lit when it is the one */
    cv_rect(x, y, w, h, on ? C_HI : C_BLOCK);
    g_text(x + w / 2, y + (h - 10) / 2, s, 2, on ? C_BLACK : C_GRAY, 2);
}
static void g_row(int32_t y, const char *l, const char *v, uint16_t c)
{                                        /* a settings row, 26 rows from y: its name left, its value right, a rule */
    g_text(8, y + 8, l, 2, C_GRAY, 0);
    g_text(232, y + 8, v, 2, c, 1);
    cv_rect(4, y + 25, 232, 1, C_LINE);
}
static void g_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a frame 2 pixels thick, inside */
{
    cv_rect(x, y, w, 2, c);
    cv_rect(x, y + h - 2, w, 2, c);
    cv_rect(x, y, 2, h, c);
    cv_rect(x + w - 2, y, 2, h, c);
}

/* SYSTEM › MIDI: the settings a row each (where MIDI goes, the clock in and the tempo it gives, the clock out), then
 * the two ports: an arrow into the name lit while MIDI comes in, one out of it while it goes out (lit: in the last
 * 0.3 s; LOAF BREAKER, 0.259: no Bluetooth; the TRS jack takes MIDI in only) */
static void glo_midi(const brk_status_t *st)
{
    static const char *const PORT_NAME[2] = {"USB", "TRS"};
    char t[24], v[16];
    uint32_t p;
    g_row(44, "MIDI", "USB+TRS", C_WHITE);
    t[0] = 0;
    if (!set.clk_in)
        text_cat(t, "OFF");
    else if (!ext_active())
        text_cat(t, "WAITING");
    else {
        tenths_text((int32_t)(ext.bpm * 10.0f + 0.5f), v);
        text_cat(t, v);
        text_cat(t, " BPM");
    }
    g_row(70, "CLOCK IN", t, ext_active() ? C_HI : set.clk_in ? C_WHITE : C_DIM);
    if (ext_active()) {                  /* (the outside's transport: started, a play's triangle; stopped, a square) */
        int32_t x = 232 - aff_text_w(t, 2) - 16;
        if (ext.run)
            g_tri(x, 77, 11, C_HI);
        else
            cv_rect(x, 78, 9, 9, C_HI);
    }
    g_row(96, "CLOCK OUT", set.clk_out ? "TO USB" : "OFF",   /* (LOAF BREAKER, 0.259: USB; sent while playing) */
          set.clk_out && st->playing ? C_HI : set.clk_out ? C_WHITE : C_DIM);
    for (p = 0; p < 2u; p++) {
        uint32_t port = p ? (uint32_t)PORT_TRS : 0u;
        int32_t x = 4 + (int32_t)p * 118;
        int in = plat_midi_ago((int)port, 0) < 300u, out = !p && plat_midi_ago((int)port, 1) < 300u;
        cv_rect(x, 132, 114, 54, C_BLOCK);
        cv_rect(x + 8, 159, 14, 2, in ? C_HI : C_LINE);   /* in: into the name */
        g_tri(x + 22, 154, 11, in ? C_HI : C_LINE);
        g_text(x + 57, 155, PORT_NAME[p], 2, in || out ? C_WHITE : C_GRAY, 2);
        if (!p) {                        /* out: out of it */
            cv_rect(x + 80, 159, 14, 2, out ? C_HI : C_LINE);
            g_tri(x + 94, 154, 11, out ? C_HI : C_LINE);
        }
    }
}

/* SYSTEM › SETUP: the skins as little scenes in their colours, the one on framed (K1); the TUTORIAL off or running
 * (K3: AFFINE, LOAF's three modes before); AUTO SAVE (K4): a memory card, lit when on, and when it saves */
static void glo_setup(const brk_status_t *st)
{
    static const char *const TUT_SEG[2] = {"OFF", "TUTORIAL"};
    uint32_t s, m = tut_on() ? 1u : 0u;
    uint16_t mc = set.auto_save ? C_HI : C_LINE;
    (void)st;
    for (s = 0; s < NSKINS; s++) {       /* (its ground, a floor of its cards' colour under a rule, the hot gem, a
                                          * line of text and one of an amount) */
        const uint8_t (*c)[3] = SKIN_RGB[s];
        int32_t x = 8 + (int32_t)s * 46, y = 52;
        if (s == skin_idx())
            g_frame(x - 3, y - 3, 46, 38, C_WHITE);
        cv_rect(x, y, 40, 32, skin_rgb(c[7], 100u));
        cv_rect(x, y + 20, 40, 12, skin_rgb(c[5], 100u));
        cv_rect(x, y + 20, 40, 1, skin_rgb(c[0], 100u));
        cv_rect(x + 15, y + 5, 10, 10, skin_rgb(c[4], 100u));
        cv_rect(x + 4, y + 25, 14, 2, skin_rgb(c[6], 100u));
        cv_rect(x + 22, y + 25, 14, 2, skin_rgb(c[3], 100u));
    }
    for (s = 0; s < 2u; s++)             /* (left to right as the knob turns) */
        g_seg((int32_t)s * 121, 100, 118, 24, TUT_SEG[s], s == m);
    cv_rect(10, 148, 28, 38, mc);        /* the memory card: its body, the notch, the label, the contacts */
    cv_rect(32, 148, 6, 6, C_BLACK);
    cv_rect(15, 160, 18, 10, C_BLACK);
    for (s = 0; s < 4u; s++)
        cv_rect(14 + (int32_t)s * 6, 178, 3, 5, C_BLACK);
    g_text(50, 152, "AUTO SAVE", 2, set.auto_save ? C_WHITE : C_GRAY, 0);
    g_text(50, 170, "1 MIN AFTER A CHANGE", 2, C_DIM, 0);   /* (once stopped 20 s: main.c's AS_REST) */
}

/* LOAF BREAKER (0.9.30, FLEXOR): GLO EFFECTS' cards, two rows of four: each effect's card a small picture (a block, a
 * zigzag, a floor) with that effect done to it, its key's number on it, its name below, lit when on, K1's framed;
 * the eighth place the keys 1-7 that turn them on, lit as they are (AFFINE: the names at 2x, the cards a pixel
 * apart) */
static void glo_sfx(const brk_status_t *st)
{
    uint32_t f, cur = ui.sfx_cur < SFX_CRT || ui.sfx_cur >= SFX_N ? SFX_CRT : ui.sfx_cur;
    int32_t top = g_top;
    (void)st;
    for (f = SFX_CRT; f <= SFX_N; f++) {
        int32_t x = 1 + (int32_t)((f - 1u) % 4u) * 60, y = 45 + (int32_t)((f - 1u) / 4u) * 74, i, yy;
        char n[2] = {(char)('0' + f), 0};
        if (f == SFX_N) {                /* (the last place: a keyboard's seven keys, lit as their effects) */
            static const uint8_t BLK[5] = {0u, 1u, 3u, 4u, 5u};
            cv_rect(x, y, 59, 72, C_BLOCK);
            g_text(x + 29, y + 4, "KEYS", 2, C_GRAY, 2);
            for (i = 0; i < 7; i++)
                cv_rect(x + 2 + i * 8, y + 18, 7, 50, SFX_ON((uint32_t)i + 1u) ? C_HI : C_GRAY);
            for (i = 0; i < 5; i++)
                cv_rect(x + 7 + (int32_t)BLK[i] * 8, y + 18, 5, 28, C_BLACK);
            break;
        }
        cv_rect(x, y, 59, 54, C_LINE);   /* the picture: a sky, a floor, a block, a zigzag */
        cv_rect(x, y + 40, 59, 14, C_DIM);
        cv_rect(x + 30, y + 10, 14, 14, C_HI);   /* (right of the key's number) */
        for (i = 0; i < 59; i++) {
            yy = (i * 3) % 24;
            cv_rect(x + i, y + 22 + (yy < 12 ? yy : 24 - yy), 1, 2, C_WHITE);
        }
        for (yy = y; yy < y + 54; yy++) {   /* (the effect, on this strip's rows of it) */
            int32_t r = yy - top;
            uint16_t *p, tmp[59];
            if (r < 0 || r >= (int32_t)cv_h)
                continue;
            p = cv_px + (uint32_t)r * cv_w + (uint32_t)x;
            for (i = 58; i >= 0; i--) {
                uint32_t c = swap16(p[i]), h = ((uint32_t)i * 2654435761u) ^ ((uint32_t)yy * 40503u);
                h ^= h >> 13;
                if (f == SFX_CRT && (yy & 1))
                    c = (c >> 2) & 0x39E7u;
                else if (f == SFX_GRAIN && (h & 6u) == 0u)
                    c = h & 1u ? (c >> 1) & 0x7BEFu : c | 0x39E7u;
                else if (f == SFX_RGB)
                    c = (c & 0x07FFu) | (i >= 3 ? swap16(p[i - 3]) & 0xF800u : 0u);
                else if (f == SFX_SHAKE && i >= 5)
                    c = ((c >> 1) & 0x7BEFu) + ((swap16(p[i - 5]) >> 1) & 0x7BEFu);
                else if (f == SFX_STROBE)
                    c = ~c & 0xFFFFu;
                else if (f == SFX_8BIT)
                    c = swap16(p[i - i % 3]) & 0xC618u;
                tmp[i] = swap16(c);
            }
            if (f == SFX_VHS && ((yy - y) / 5) % 3 == 1) {   /* (a band in three slipped aside) */
                int32_t d = ((yy - y) / 15) & 1 ? 5 : -5;
                for (i = 0; i < 59; i++)
                    p[i] = tmp[(i - d + 59) % 59];
            } else
                for (i = 0; i < 59; i++)
                    p[i] = tmp[i];
            if (f == SFX_8BIT && (yy - y) % 3 && r > 0)
                for (i = 0; i < 59; i++)
                    p[i] = p[i - (int32_t)cv_w];   /* (3 x 3 blocks, the row above's) */
        }
        if (f == cur)
            g_frame(x, y, 59, 54, C_WHITE);
        cv_rect(x + 4, y + 4, 14, 14, C_BLACK);   /* its key */
        g_text(x + 11, y + 6, n, 2, SFX_ON(f) ? C_HI : C_WHITE, 2);
        cv_rect(x, y + 54, 59, 18, SFX_ON(f) ? C_HI : C_BLOCK);
        g_text(x + 29, y + 58, SFX_NAME[f], 2, SFX_ON(f) ? C_BLACK : C_GRAY, 2);
    }
}

/* SYSTEM › FX SET: the effects' four settings (the foot's values) as bars */
static void glo_sfxset(const brk_status_t *st)
{
    static const char *const L[4] = {"AMOUNT", "BOOM", "DROP", "LENGTH"};   /* (on a BOOM, at the DROP) */
    int32_t k, j;
    (void)st;
    for (k = 0; k < 4; k++) {
        int32_t y = 46 + k * 36, v = (int32_t)*sfx_knob((int)k);
        cv_rect(2, y, 236, 32, C_BLOCK);
        g_text(10, y + 11, L[k], 2, v ? C_GRAY : C_DIM, 0);
        cv_rect(84, y + 9, 146, 14, C_LINE);
        cv_rect(84, y + 9, v * 146 / 100, 14, C_AMB);
        for (j = 1; j < 4; j++)          /* (its quarters) */
            cv_rect(84 + j * 146 / 4, y + 9, 1, 14, C_BLOCK);
    }
}

/* SEQ's SETTINGS (LOAF BREAKER: GROOVE until 0.9.30; 0.9.31, FLEXOR: BREAK ROWS, FOLLOW): the bar's 16ths swung (K1);
 * the FULL view's break rows, one or two of 16 (K2); FOLLOW (K3): the grid's window on the bar playing, or kept */
static void glo_fill(const brk_status_t *st)
{
    uint32_t s, b, rows = bk_seq() & 2u ? 1u : 2u, shown;
    int32_t pos = st->playing && st->step >= 0 ? st->step % 32 : -1;
    g_text(6, 50, "SWING", 2, C_GRAY, 0);
    for (s = 0; s < 16u; s++) {          /* (the odd 16ths pushed later as the swing grows) */
        int32_t x = 6 + (int32_t)s * 14 + ((s & 1u) ? (int32_t)(set.swing - 50u) * 6 / 25 : 0);
        cv_rect(x, 66, 10, 22, (s & 3u) ? C_DIM : C_HI);
    }
    g_text(6, 98, "BREAK ROWS", 2, C_GRAY, 0);
    for (b = 0; b < rows; b++)
        for (s = 0; s < 16u; s++)
            cv_rect(8 + (int32_t)s * 14, 114 + (int32_t)b * 12, 13, 10, (s & 3u) ? C_DIM : C_GRAY);
    g_text(6, 146, "FOLLOW", 2, C_GRAY, 0);
    shown = !(bk_seq() & 1u) && pos >= 16 ? 1u : 0u;
    for (b = 0; b < 2u; b++)
        for (s = 0; s < 16u; s++)
            cv_rect(8 + (int32_t)(b * 116u + s * 7u), 166, 6, 18, pos == (int32_t)(b * 16u + s) ? C_HI : (s & 3u) ? C_DIM
                    : C_GRAY);
    g_frame(4 + (int32_t)shown * 116, 162, 118, 26, C_WHITE);
}

/* GLO › SCREEN › VIEWS (AFFINE, controls phase 2): the drums' view, the one in use lit */
static void glo_view(const brk_status_t *st)
{
    (void)st;
    g_text(8, 60, "DRUM VIEW", 2, C_GRAY, 0);
    g_seg(4, 100, 74, 30, "RGB", !ui.kclassic && !ui.kview);
    g_seg(83, 100, 74, 30, "WAVE", !ui.kclassic && ui.kview);
    g_seg(162, 100, 74, 30, "CLASSIC", ui.kclassic);
}
static void draw_global(const brk_status_t *st)
{                                        /* (LOAF BREAKER: no waveform on SYSTEM: rows 44-192 its own) */
    static const uint8_t TOP[4] = {44u, 92u, 140u, 192u};
    g_strips(TOP, 3u, ui.gpage == GP_FILL ? glo_fill : ui.gpage == 3u ? glo_sfx : ui.gpage == 4u ? glo_sfxset
             : ui.gpage == GP_VIEW ? glo_view : ui.gpage == 2u ? glo_setup : glo_midi, st);
}


/* LOAF BREAKER (0.9.2, FLEXOR): HOME's SETTINGS, the waveform's settings where it is played: K1 VIEW (the waveform or
 * the slices; HOME held still flips it), K2 WAVE TYPE (REKORDBOX, SERATO; SYSTEM EFFECTS' K3 until 0.9.24), K3 WAVE ZOOM (SYSTEM
 * SETUP's K2 until 0.9.1), K4 BREAK TIME (SYSTEM MIDI's K4 until 0.9.1). VIEW saved in bkeys.bk[BK_VIEW] */
static const char *const HSET_NAME[4] = {"VIEW", "GATE", "WAVE ZOOM", "BREAK TIME"};   /* (AFFINE: K2 the gate, HOME's
                                          * K1 before; LOAF's WAVE TYPE gone with its deck view) */
static void hset_value(int i, char *t)
{
    t[0] = 0;
    if (i == 1) {                        /* (AFFINE: the gate, how much of every slice sounds) */
        if (!set.gate || set.gate >= 100u)
            text_cat(t, "OFF");
        else {
            int_text(set.gate, t);
            text_cat(t, "%");
        }
        return;
    }
    text_cat(t, i == 0 ? (ui.blocks ? "SLICES" : "WAVEFORM")
                : i == 2 ? CDJ_ZNAME[set.wzoom % CDJ_ZN] : (set.bkeys.bk[BK_BHALF] & 1u ? "HALF" : "NORMAL"));
}
static void hset_turn(int i, int32_t d)
{
    char t[16];
    if (i == 0) {
        ui.blocks = d > 0 ? 1u : 0u;     /* (right the slices, left the waveform) */
        set.bkeys.bk[BK_VIEW] = ui.blocks;
    } else if (i == 1)
        set.gate = (uint8_t)clamp((int32_t)(set.gate ? set.gate : 100u) + d * 5, 5, 100);
    else if (i == 2)
        set.wzoom = (uint8_t)clamp((int32_t)(set.wzoom % CDJ_ZN) + (d > 0 ? 1 : -1), 0, CDJ_ZN - 1);
    else
        set.bkeys.bk[BK_BHALF] = d > 0 ? 1u : 0u;   /* (right HALF: a 115 break under a 230 kick at its own speed) */
    changed();
    hset_value(i, t);
    app_say(HSET_NAME[i & 3], t);
    ui.redraw |= RG_HEAD | RG_BLOCKS | RG_FOOT;
}
/* (AFFINE, 2x: the four as cards, K1-K4 left to right, a short name and a picture each; the foot has the values) VIEW:
 * a waveform and the slices, the one shown lit (HOME held flips it); GATE: how much of every slice sounds; ZOOM: two
 * bars of the break, the part the waveform shows framed; BREAK: the tempo's beats and the break's under them (HALF:
 * half as many), the break's tempo */
static void hset_body(const brk_status_t *st)
{
    static const char *const L[4] = {"VIEW", "GATE", "ZOOM", "BREAK"};
    static const uint8_t WV[12] = {22u, 14u, 8u, 4u, 12u, 6u, 18u, 10u, 6u, 3u, 14u, 8u};   /* (a break's peaks) */
    int32_t i, j, x, half = (int32_t)(set.bkeys.bk[BK_BHALF] & 1u), gate = set.gate && set.gate < 100u ? set.gate : 100,
            zw = (int32_t)CDJ_Z[set.wzoom % CDJ_ZN] * 2;
    char v[16];
    (void)st;
    for (i = 0; i < 4; i++) {
        cv_rect(1 + i * 60, 90, 58, 100, C_BLOCK);
        g_text(30 + i * 60, 95, L[i], 2, C_GRAY, 2);
    }
    for (j = 0; j < 24; j++)             /* VIEW */
        cv_rect(6 + j * 2, 127 - WV[j % 12] / 2, 1, WV[j % 12], ui.blocks ? C_LINE : C_HI);
    for (j = 0; j < 4; j++)
        cv_rect(6 + j * 12, 148, 11, 30, ui.blocks ? C_HI : C_LINE);
    for (j = 0; j < 4; j++) {            /* GATE: four slices, two a row */
        x = 66 + (j & 1) * 25;
        cv_rect(x, 116 + (j >> 1) * 34, 23, 26, C_LINE);
        cv_rect(x, 116 + (j >> 1) * 34, 23 * gate / 100 < 1 ? 1 : 23 * gate / 100, 26, C_AMB);
    }
    for (j = 0; j < 24; j++) {           /* ZOOM: 48 pixels two bars (CDJ_Z: twelfths of a bar) */
        x = 126 + j * 2;
        cv_rect(x, 145 - WV[(j + 5) % 12] / 2, 1, WV[(j + 5) % 12], x - 126 < zw ? C_HI : C_LINE);
    }
    for (j = 0; j < 3; j++)
        cv_rect(126 + j * 24 - (j == 2), 118, 1, 54, C_GRAY);
    g_frame(124, 116, zw + 4, 58, C_WHITE);
    for (j = 0; j < 4; j++)              /* BREAK: the tempo's beats, the break's */
        cv_rect(186 + j * 12, 114, 10, 10, C_GRAY);
    for (j = 0; j < (half ? 2 : 4); j++)
        cv_rect(186 + j * (half ? 24 : 12), 128, half ? 22 : 10, 10, C_HI);
    tenths_text((int32_t)set.tempo_x10 / (half ? 2 : 1), v);
    g_text(210, 152, v, 2, C_WHITE, 2);
    g_text(210, 168, "BPM", 2, C_GRAY, 2);
}
static void draw_hset(void)
{                                        /* (rows 44-88: the waveform, draw_wave's) */
    static const uint8_t TOP[3] = {88u, 140u, 192u};
    g_strips(TOP, 2u, hset_body, (const brk_status_t *)0);
}
