/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH: the start screen, the header, the break's waveform and the slice blocks. (Part of app.c's unity:
 * included there in order.) */

#define INTRO_STEP 140u                  /* (AFFINE: the splash's 2.24 s; LOAF's blocks dropped in at 55 ms) */
#define INTRO_HOLD 1000u                 /* ms the finished screen stays before the slicer opens */

/* the mark: sixteen bars like a break's hits, two of them chopped out of line */
static const uint8_t MARK_H[16] = {30, 12, 26, 10, 34, 14, 11, 20, 12, 22, 28, 24, 34, 10, 13, 22};
#include "pp_wave.h"                     /* (LOAF BREAKER: PRESS PLAY's picture, tools/make_pp_wave.py) */
static void mark(int32_t y0, uint32_t shown, const uint8_t (*px)[BLK_W])
{
    uint32_t k, c;
    for (k = 0; k < 16u && k < shown; k++) {
        int32_t x = 8 + (int32_t)k * 14, lift = (k == 5u || k == 11u) ? -10 : 0;
        uint16_t col = (k == 5u || k == 11u) ? C_WHITE : C_BLOCK_HI;
        if (px) {                        /* the loaded break: each slice's own shape */
            for (c = 0; c < 12u; c++) {
                int32_t h = px[k][c * BLK_W / 12u] * 2 + 1;
                cv_rect(x + (int32_t)c, y0 + lift + 20 - h / 2, 1, h, col);
            }
        } else                           /* the splash, before any break is in: the drawn mark */
            cv_rect(x, y0 + lift + 20 - MARK_H[k] / 2, 12, MARK_H[k], col);
    }
}

/* the platform's own screens' faces (APP_SYS_TEXT, device/sys_text.c): the body FONT_XS, a heading FONT_B, the big
 * one FONT_B doubled (the device's: the simulator draws none of them) */
#ifndef PLAT_HOST
static int32_t app_sys_h(int face)
{
    return face == SYS_BIG ? 2 * (int32_t)FONT_B.h : face == SYS_B ? FONT_B.h : FONT_XS.h;
}
static int32_t app_sys_w(int face, const char *s)
{
    return face == SYS_BIG ? text2_w(&FONT_B, s) : text_w(face == SYS_B ? &FONT_B : &FONT_XS, s);
}
static void app_sys_text(int32_t x, int32_t y, int face, const char *s, uint16_t c)
{
    if (face == SYS_BIG)
        cv_text2(x, y, &FONT_B, s, c);
    else
        cv_text(x, y, face == SYS_B ? &FONT_B : &FONT_XS, s, c);
}
#endif

static void title(int32_t y)             /* the name and the tagline, centred, into the current strip */
{
    cv_text2((240 - text2_w(&FONT_B, APP_NAME)) / 2, y, &FONT_B, APP_NAME, C_WHITE);
#if CH_TAGLINE
    cv_text((240 - text_w(&FONT_XS, "AMEN CHOPPER  FOR THE FM-1")) / 2, y + 36, &FONT_XS,
            "AMEN CHOPPER  FOR THE FM-1", C_HI);
#endif
}

/* the version, as the screens show it: "V0.7" (a release), "DEV" (any other build) */
static void version_text(char *t)
{
    t[0] = 0;                            /* (GLOUPI BREAKS: the number alone, no "V") */
    text_cat(t, CH_VERSION);
}


/* GLOUPI BREAKS: a grey picture (tools/make_egg.py: a byte a run, bits 0-3 the grey, 4-7 the length less one) at
 * x0, y0 on the screen, w x h, in strips of 60 rows, each the screen's width (its sides black) */
static int32_t gl_amt;                   /* LOAF BREAKER TEST: the start-up glitch: how far rows jump (0: none), its seed */
static uint32_t gl_seed;
static int32_t gl_band = -1;             /* (LOAF BREAKER: VHS: the tracking band's top row, -1 none) */
static int32_t gl_shift(int32_t row)     /* a band of rows: shifted, or not (a third of the bands) */
{
    uint32_t h = ((uint32_t)row / 5u + gl_seed) * 2654435761u;
    if (!gl_amt || (h >> 28) % 3u)
        return 0;
    return (int32_t)((h >> 8) % (uint32_t)(2 * gl_amt + 1)) - gl_amt;
}
static void grey_draw(const uint8_t *rle, uint32_t len, const uint16_t *pal, int32_t w, int32_t h, int32_t x0,
                      int32_t y0)
{
    int32_t r, sh;
    for (r = 0; r < h; r += sh) {
        uint32_t i;
        int32_t x = 0, row = 0;
        sh = h - r < 60 ? h - r : 60;
        cv_begin(240, (uint32_t)sh, C_BLACK);
        for (i = 0; i < len && row < r + sh; i++) {
            uint32_t b = rle[i], c = b & 15u;
            int32_t n = (int32_t)(b >> 4) + 1;
            if (c && row >= r) {
                int32_t g = gl_shift(row);   /* (the glitch: the band shifted, its light greyed) */
                cv_rect(x0 + x + g, row - r, n, 1, g ? (g & 1 ? RGB(150, 150, 156) : pal[c]) : pal[c]);
            }
            x += n;
            if (x >= w) {
                x = 0;
                row++;
            }
        }
        if (gl_amt) {                    /* (LOAF BREAKER: VHS: the tracking band rolling down, a smear of grey
                                          * noise, the row above it dark; specks here and there) */
            int32_t k;
            for (k = 0; k < sh; k++) {
                int32_t row = r + k, in = row >= gl_band && row < gl_band + 7;
                uint32_t hsh = ((uint32_t)row * 2246822519u) ^ (gl_seed * 3266489917u), m;
                if (row == gl_band - 1)
                    cv_rect(0, k, 240, 1, C_BLACK);
                for (m = 0; m < (in ? 26u : 2u); m++) {
                    uint32_t q = (hsh += 0x9E3779B9u) * 2654435761u, lv = 90u + (q >> 25);
                    cv_rect((int32_t)((q >> 8) % 240u), k, in ? 2 + (int32_t)((q >> 4) & 7u) : 1, 1,
                            RGB(lv, lv, lv + 6u));
                }
            }
        }
        cv_blit(0, (uint32_t)(y0 + r));
    }
}



/* fm1-base's first screen at power-on, while the breaks are decoded (APP_SPLASH): the mark and the name (LOAF
 * BREAKER: its picture, nearly the whole screen, the version at the top right; held 1.5 s, app_frame) */
static void aff_title(uint32_t now, uint32_t el, const char *top, const char *l1, const char *l2, const char *l3);
static uint32_t splash_t;                /* when it was drawn (0: gone) */
static void splash_glitch(uint32_t age)  /* LOAF BREAKER TEST: the start-up picture, its rows jumping, settling by 700 ms */
{
#if CH_LOGO
    gl_amt = age < 700u ? (int32_t)((700u - age) * 24u / 700u) + 1 : 0;
    gl_seed = age / 40u;
    sfx_splash_d = (uint8_t)(2 + gl_amt / 4);   /* (LOAF BREAKER: its red off, the more as it jumps) */
    gl_band = age < 700u ? (int32_t)(age * (uint32_t)SPLASH_H / 560u) - 8 : -1;   /* (rolling down the picture) */
    grey_draw(SPLASH_RLE, sizeof SPLASH_RLE, SPLASH_PAL, SPLASH_W, SPLASH_H, (240 - SPLASH_W) / 2, 240 - SPLASH_H);
    gl_amt = 0;
    gl_band = -1;
    sfx_splash_d = 0u;
#else
    (void)age;
#endif
}
void app_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
#if CH_LOGO
    sfx_splash_d = 2u;                   /* (LOAF BREAKER: the picture's red two pixels off) */
    grey_draw(SPLASH_RLE, sizeof SPLASH_RLE, SPLASH_PAL, SPLASH_W, SPLASH_H, (240 - SPLASH_W) / 2, 240 - SPLASH_H);
    sfx_splash_d = 0u;
    {
        char t[24];                     /* (a TEST's "0.268 TEST 12" too) */
        version_text(t);
        cv_begin(240, 240 - SPLASH_H, C_BLACK);
        cv_text(234 - text_w(&FONT_T, t), 1, &FONT_T, t, C_GRAY);
        cv_blit(0, 0);
    }
    splash_t = plat_ms() | 1u;
#else
    {                                    /* AFFINE: its title, the version above it */
        char t[24];
        version_text(t);
        aff_title(plat_ms(), 0u, t, "", "", "");   /* (the splash's first frame: the start screen goes on from it) */
    }
#endif
}

static void intro_draw(uint32_t now)      /* AFFINE: the start screen, its title moving (the floor, the gem) */
{
    static uint32_t drawn_t;
    char t[32];
    const brk_break_t *b = brk_get_break();
    if (ui.intro_drawn && now - drawn_t < 33u && !(ui.redraw & RG_HEAD))
        return;
    drawn_t = now;
    ui.intro_drawn = 1u;
    version_text(t);
    aff_title(now, now - ui.intro_t0, t, b ? b->name : "", "PRESS PLAY", "BUILT ON LOAF BREAKER AND CHOPPAH");
    ui.redraw = 0u;
}

/* RESET ALL's screen: a brain seen from the side, an ice pick in through the eye socket, the frontal lobe gone grey,
 * a flatline under it, LOBOTOMIZED. Drawn whole, in four strips of 60 rows, from shapes (no picture in flash) */
#define LB_PINK RGB(255, 140, 160)
#define LB_FOLD RGB(190, 70, 100)
#define LB_EDGE RGB(120, 30, 60)
#define LB_DEAD RGB(96, 96, 104)
static int lobo_in(int32_t x, int32_t y)  /* inside the brain: the cerebrum (flat-bottomed), the cerebellum, the stem */
{
    float dx = (float)(x - 126) * (1.0f / 92.0f), dy = (float)(y - 88) * (1.0f / 60.0f);
    float cx = (float)(x - 176) * (1.0f / 36.0f), cy = (float)(y - 132) * (1.0f / 19.0f);
    if (dx * dx + dy * dy < 1.0f && dy < 0.66f)
        return 1;
    if (cx * cx + cy * cy < 1.0f)
        return 1;
    return x >= 146 && x < 162 && y >= 120 && y < 168;
}
static void draw_lobotomy(void)          /* AFFINE: RESET ALL done: the title, everything cleared */
{
    aff_title(plat_ms(), 5000u, "", "ALL CLEARED", "", "");   /* (AFFINE: its 2x line alone; "AS A NEW FM-1" was
                                          * aff_title's 1x line) */
}

/* the start screen's lights: a comet runs through the buttons as the break drops in (OCT- and OCT+, the top row left
 * to right, the bottom row back), a tail of mid-lit buttons behind it and the ones it passed glowing; then all flash
 * as the mark completes, and PLAY blinks green with PRESS PLAY. Returns 0 once the start screen is over */
/* the buttons' comet's way round: OCT- and OCT+, the top row left to right, the bottom row back (the demo's too) */
static const uint8_t PATH[NB] = {B_OCTDN, B_OCTUP, B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO,
                                 B_REC, B_PLAY, B_SEQ, B_ARP, B_SAVE, B_HOME};
static int intro_leds(uint32_t now)
{
    uint32_t t = now - ui.intro_t0, run = 16u * INTRO_STEP, b = 0u, mid = 0u, dim = 0u, i;
    if (!ui.intro)
        return 0;
    if (t < run) {                       /* the comet */
        uint32_t head = t * (NB + 2u) / run;   /* (+2: its tail runs out too) */
        for (i = 0; i < NB; i++)
            if (i == head)
                b |= 1u << PATH[i];
            else if (i + 2u >= head && i < head)
                mid |= 1u << PATH[i];
            else if (i < head)
                dim |= 1u << PATH[i];
    } else if (t < run + 160u)           /* the mark complete: all flash */
        b = ((1u << NB) - 1u) | LED_PLAY_GREEN;
    else {                               /* PRESS PLAY: PLAY blinks green, the rest glow */
        dim = ((1u << NB) - 1u) & ~(1u << B_PLAY);
        if ((t - run - 160u) % 500u < 250u)
            b = LED_PLAY_GREEN;
    }
    plat_lights(b, 0u, dim, 0u, 1);
    plat_lights_mid(mid, 0u);
    return 1;
}

static void intro_end(void)
{
    ui.intro = 0u;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = RG_ALL;
}

/* the header's level meter: what leaves, left and right, -48..0 dBFS in 18 steps of 2.7 dB; amber from -8 dB, the
 * top white near full scale */
static void draw_meter(void)
{
    float lv[2];
    uint32_t c;
    int32_t h;
    lv[0] = meter_l;
    lv[1] = meter_r;
    cv_begin(8, 20, C_BLACK);
    for (c = 0; c < 2u; c++) {
        for (h = 0; h < 18; h++)         /* the steps it is above: -48 dB + 2.67 dB each (2^(dB / 6.02)) */
            if (lv[c] < brk_exp2f(((float)h * (48.0f / 18.0f) - 48.0f) * (1.0f / 6.0206f)))
                break;
        cv_rect((int32_t)c * 4, 1, 3, 18, C_BLOCK);
        if (h)
            cv_rect((int32_t)c * 4, 19 - h, 3, h, h >= 18 ? C_WHITE : h >= 15 ? C_AMB : C_HI);
    }
    cv_blit(ui.meter_x, 4);
}

/* the header: the break and the tempo, a play / stop mark; or a message for a moment */
/* the battery (CHOPPAH 1.0's, plat_battery): three bars from the foot, red at one or none, a USB host powering the
 * FM-1 shows a bolt instead. 9 wide, rows 4..20 (the header's canvas); -1 (not read yet): nothing */
#define BATT_W 9                         /* the body's width (the nub 3 wide on top of it) */
#define BATT_LOW RGB(255, 104, 72)       /* one bar or none: red */
static void draw_batt(int32_t x, int b)
{
    uint16_t c = b >= 0 && b <= 1 ? BATT_LOW : C_DIM;
    int32_t i;
    if (b < 0)
        return;
    cv_rect(x + 3, 4, 3, 2, c);           /* the nub */
    cv_rect(x, 6, BATT_W, 14, c);         /* the body's frame */
    cv_rect(x + 1, 7, BATT_W - 2, 12, C_BLACK);
    if (b == 4) {                        /* a USB host powers the FM-1: a bolt */
        cv_line(x + 5, 7, x + 2, 12, C_HI);
        cv_line(x + 2, 12, x + 6, 12, C_HI);
        cv_line(x + 6, 12, x + 3, 18, C_HI);
        return;
    }
    for (i = 0; i < b && i < 3; i++)      /* the bars from the foot up */
        cv_rect(x + 2, 15 - i * 4, BATT_W - 4, 3, b <= 1 ? BATT_LOW : C_HI);
}

/* AFFINE: the header's text at 2x (FONT_B): one line with its capitals in rows 9-18 (y 6), or two, rows 2-11 (HEAD_Y1)
 * and 14-23 (HEAD_Y2); from HEAD_X to HEAD_R, the battery right of it */
#define HEAD_X 8
#define HEAD_R 224
#define HEAD_Y1 (-1)
#define HEAD_Y2 11
/* how many characters of s fit in w pixels: cut after a word (words) when one fits, else after a character */
static int32_t head_fit(const char *s, int32_t w, int words)
{
    char t[24];
    int32_t i, fit = 0, wfit = 0;
    for (i = 0; i < 23 && s[i]; i++) {
        t[i] = s[i];
        t[i + 1] = 0;
        if (text_w(&FONT_B, t) > w)
            break;
        fit = i + 1;
        if (s[i] != ' ' && (s[i + 1] == ' ' || !s[i + 1]))
            wfit = i + 1;
    }
    return words && wfit ? wfit : fit;
}
static int32_t head_put(int32_t x, int32_t y, const char *s, int32_t n, uint16_t c)   /* s's first n characters */
{
    char t[24];
    int32_t i;
    for (i = 0; i < n && i < 23 && s[i]; i++)
        t[i] = s[i];
    t[i] = 0;
    return t[0] ? cv_text(x, y, &FONT_B, t, c) : x;
}
/* app_say's two parts: on one line when they fit, else a on the first, b on the second (b too long for a line: its
 * first words beside a); never past the battery */
static void head_msg(const char *a, const char *b)
{
    int32_t w = HEAD_R - HEAD_X, x = HEAD_X, n = 0;
    if (text_w(&FONT_B, a) + 10 + text_w(&FONT_B, b) <= w || !b[0]) {
        x = a[0] ? head_put(HEAD_X, 6, a, head_fit(a, w, 1), C_WHITE) + 8 : HEAD_X;
        if (b[0] && x < HEAD_R)
            head_put(x, 6, b, head_fit(b, HEAD_R - x, 1), C_HI);
        return;
    }
    if (a[0])
        x = head_put(HEAD_X, HEAD_Y1, a, head_fit(a, w, 1), C_WHITE) + 8;
    if (!a[0] || text_w(&FONT_B, b) > w) {
        n = x < HEAD_R ? head_fit(b, HEAD_R - x, 1) : 0;
        if (n && b[n] && b[n] != ' ' && x > HEAD_X)
            n = 0;                       /* (no whole word beside a: all of b below) */
        head_put(x, HEAD_Y1, b, n, C_HI);
        while (b[n] == ' ')
            n++;
    }
    head_put(HEAD_X, HEAD_Y2, b + n, head_fit(b + n, w, 1), C_HI);
}

static void draw_head(const brk_status_t *st, const brk_break_t *b, uint32_t now)
{
    char t[24];
    cv_begin(240, 28, C_BLACK);
    ui.meter_x = 0;
    if (now < ui.msg_until)
        head_msg(ui.msg[0], ui.msg[1]);
    else if (tut_on()) {                 /* LOAF BREAKER: the TUTORIAL: its lesson and step, what to do (AFFINE: 2x) */
        char a[24], n[8], w[40];
        tut_head(a, n, w);
        cv_text(HEAD_R - text_w(&FONT_B, n), HEAD_Y1, &FONT_B, n, C_GRAY);   /* "3/26" */
        head_put(HEAD_X, HEAD_Y1, a, head_fit(a, HEAD_R - HEAD_X - text_w(&FONT_B, n) - 10, 1), C_AMB);
        head_put(HEAD_X, HEAD_Y2, w, head_fit(w, HEAD_R - HEAD_X, 1), tut_ok ? C_HI : C_WHITE);   /* (done: "OK! NEXT IN 4 S") */
    } else {
        int32_t x, r, room;
        char p[8], tr[8];
        if (ui.rec) {                    /* REC: a dot (AFFINE: in place of the mark, the word gone at 2x) */
            cv_rect(10, 9, 6, 10, C_HI);
            cv_rect(9, 10, 8, 8, C_HI);
            cv_rect(8, 11, 10, 6, C_HI);
        } else if (st->playing) {        /* a play triangle, or a stop square */
            int32_t k;
            for (k = 0; k < 6; k++)
                cv_rect(8 + k, 8 + k, 1, 13 - 2 * k, C_HI);
        } else
            cv_rect(8, 9, 10, 10, C_DIM);
        tenths_text(set.tempo_x10, t);   /* the tempo at the right (AFFINE: "BPM" gone, no room at 2x; TEST: white on
                                          * each beat) */
        r = HEAD_R - text_w(&FONT_B, t);
        cv_text(r, 6, &FONT_B, t, st->playing && st->bar16 >= 0 && !(st->bar16 & 3) ? C_WHITE : C_HI);
        if (ext_active()) {              /* (the clock from outside: EXT before it) */
            r -= 8 + text_w(&FONT_B, "EXT");
            cv_text(r, 6, &FONT_B, "EXT", C_AMB);
        }
        p[0] = 'P';                      /* the pattern (> one queued), a track playing ("T1"): as far as room */
        int_text((int32_t)pat_cur() + 1, p + 1);
        if (pat_next >= 0)
            text_cat(p, ">");
        tr[0] = 0;
        if (ui.trk_on) {
            tr[0] = 'T';
            int_text((int32_t)set.trk + 1, tr + 1);
        }
        room = r - 10 - 24 - 10 - text_w(&FONT_B, p);   /* (the break's name: cut to leave its pattern room) */
        {
            const char *nm = b ? break_name(set.brk) : "NO BREAK";
            x = head_put(24, 6, nm, head_fit(nm, room, 1), brk_next >= 0 ? C_AMB : C_WHITE);   /* (amber: to come;
                                          * cut after a word: "OLDSKOO", never "OLDSKOO 16") */
        }
        x = cv_text(x + 8, 6, &FONT_B, p, pat_next >= 0 ? C_AMB : C_GRAY);
        if (tr[0] && x + 6 + text_w(&FONT_B, tr) <= r - 10)
            x = cv_text(x + 6, 6, &FONT_B, tr, C_HI);
        if (x + 6 <= r - 16)             /* the level meter, left of the tempo, when there is room */
            ui.meter_x = (int16_t)(r - 14);
    }
    draw_batt(229, plat_battery());      /* LOAF BREAKER: the battery (CHOPPAH 1.0's), always at the right end */
    cv_rect(0, 27, 240, 1, now - ui.kick_t < 90u ? C_HI : C_LINE);   /* (TEST: lit on each kick) */
    if (st->playing && st->bar16 >= 0)   /* (TEST: the bar's beats, four ticks under the header, the one now lit) */
        cv_rect(2 + (st->bar16 >> 2) * 60, 26, 56, 1, C_GRAY);
    cv_blit(0, 0);
}

/* the whole break: its waveform, the slice markers, the slice sounding lit, the playhead (44 rows, under the tab
 * bar) */
static void draw_wave(const brk_status_t *st, const brk_break_t *b)
{
    uint32_t i;
    cv_begin(240, 44, C_BLACK);
    if (b && b->len) {
        int32_t lit_a = -1, lit_z = -1;
        if (st->slice >= 0) {
            lit_a = (int32_t)(b->slice[st->slice] * 240u / b->len);
            lit_z = (int32_t)(b->slice[st->slice + 1] * 240u / b->len);
            cv_rect(lit_a, 1, lit_z - lit_a, 42, C_BLOCK);
            if (set.gate && set.gate < 100u && st->key < 0)   /* gated: only the part that sounds lit */
                lit_z = lit_a + ((lit_z - lit_a) * (int32_t)set.gate + 99) / 100;
        }
        for (i = 0; i < 240u; i++) {
            int32_t h = wave_px[i] * 5 / 6;
            cv_rect((int32_t)i, 22 - h, 1, 2 * h + 1, (int32_t)i >= lit_a && (int32_t)i < lit_z ? C_HI : C_DIM);
        }
        for (i = 1; i < b->nslices; i++)
            cv_rect((int32_t)(b->slice[i] * 240u / b->len), 0, 1, 5, C_GRAY);
        if (st->slice >= 0)
            cv_rect((int32_t)((uint32_t)st->pos * 240u / b->len), 0, 1, 44, C_WHITE);
    }
    cv_blit(0, 44);
}

static void draw_blocks(const brk_status_t *st, uint32_t keys)
{
    uint32_t row, k, c;
    char t[4];
    for (row = 0; row < 2u; row++) {
        cv_begin(240, 52, C_BLACK);
        for (k = row * 8u; k < row * 8u + 8u; k++) {
            int32_t x = 1 + (int32_t)(k % 8u) * 30, held = (keys >> WK[k]) & 1u;
            int on = ui.rec && !st->playing ? k == rec_step % 16u   /* LOAF BREAKER: REC stopped, the step the
                                          * next key writes (its half: REC STEP's popup) */
                                            : st->slice == (int32_t)k, have = k < blk_n;
            uint16_t ground = on ? C_BLOCK_HI : C_BLOCK, ink = on ? C_BLACK : have ? C_GRAY : C_LINE;
            cv_rect(x, 0, 28, 50, held ? C_WHITE : ground);
            if (held)
                cv_rect(x + 2, 2, 24, 46, ground);
            if (have)
                for (c = 0; c < BLK_W - 4u; c++) {   /* the slice's shape, 22 columns of its 26 (AFFINE: centred
                                          * lower, the tag's row above it) */
                    int32_t h = blk_px[k][c * BLK_W / (BLK_W - 4u)];
                    cv_rect(x + 3 + (int32_t)c, 22 - h / 2, 1, h + 1, ink);
                }
            int_text((int32_t)k + 1, t);
            cv_text(x + 3, 34, &FONT_T, t, on ? C_BLACK : have ? C_AMB : C_LINE);   /* (AFFINE: 2x, the bottom row) */
            if (have && key_custom((int)k))  /* a loop of its own: a dot */
                cv_rect(x + 21, 38, 4, 4, on ? C_BLACK : C_HI);
            if (have && (set.slx[k].type || set.slx[k].level < 100u))   /* an effect of its own: its tag (AFFINE: on
                                          * the top row, no room beside a 2x number) */
                cv_text(x + 3, 0, &FONT_XS, set.slx[k].type ? SLX_TAG[set.slx[k].type] : "LV", on ? C_BLACK : C_HI);
        }
        cv_blit(0, 88u + row * 52u);
    }
}
