/* SPDX-License-Identifier: MPL-2.0 */
/* LOAF BREAKER: the TUTORIAL (FLEXOR, 10 Nov 2026): the firmware's basics learnt on the FM-1 itself, a step at a time.
 * Each step says one thing to do in the header (when no other message is up) and blinks the button or keys that do
 * it; when it is done (tut_done), "OK!" and the next: at once, or for a step with a result to hear (an effect, a
 * kick: its `wait`, FLEXOR 0.213) 5 s later (TUT_WAIT). The lessons
 * (HOME first: the PERFORM menu): the break, HOME, the effects, the pattern, the sounds (EDIT, keys 1-4 stopped; AFFINE:
 * LOAF BREAKER's lessons on its hardcore kick gone), the drums (SELECT, PRESET, FX), the FX page, the drop, SAVE.
 * Started from SYSTEM › SETUP K3 (right; left quits) and on a new FM-1 (TUTORIAL or PLAY); GLO quits it, SEL + GLO
 * skips a step. set.tut: 0 off, 1..TUT_N the step running (kept with SAVE), TUT_DONE finished. AFFINE (16 Nov 2026, the
 * owner: one mode for everyone): it runs in LOAF BREAKER's BEGINNER, its steps written for it (set.novice while it
 * runs, app.c NOVICE); the full AFFINE before and after it.
 * (Part of app.c's unity: included there in order, before screen.c, which draws the header.) */

enum { TC_HGEN = 100, TC_HSET, TC_HVIEW, TC_HBACK, TC_HHOLD };   /* (0.9.2, FLEXOR: HOME's tabs) */
enum { TC_HOME, TC_PLAY, TC_SLICES, TC_BRK, TC_PAD, TC_OVLPG, TC_OVLKEY, TC_EDIT, TC_STOP, TC_KEYS4,
       TC_SNPG, TC_PPRE, TC_PFX, TC_SEQ, TC_KICKS, TC_SNARE, TC_FXPG, TC_FXT, TC_DROP, TC_SAVE, TC_END };
#define TK_WHITE 1u                      /* (the keys a step blinks: the white ones, the black ones) */
#define TK_BLACK 2u
#define TK_DRUMS 4u                      /* (keys 1-4: BOOM, the snare, the hat, the clap, stopped) */
#define TK_KEY2 16u                      /* (key 2: the snare) */
#define TUT_LESSONS 10u
static const char *const TUT_LESSON[TUT_LESSONS] = {"THE BREAK", "EFFECTS", "THE SOUNDS", "THE DRUMS", "THE PATTERN",
                                                    "THE FX", "THE DROP", "SAVE", "DONE", "HOME"};
static const struct {
    uint8_t lesson, cond, keys, wait;    /* (wait: 1, its result to hear: 5 s before the next; 0: at once) */
    uint16_t btn;                        /* (the buttons it blinks: bit B_*) */
    const char *say;
} TUT[] = {                              /* (LOAF BREAKER, 0.235, FLEXOR: 23 steps (0.244: 25, the OVERLAY page; 0.246, FLEXOR: 24, the ARP key's step gone; 0.252: 26, THE PATTERN before THE KICK, the kick and the snare heard stopped on keys 1-4), BEGINNER as he chose it: its KICK's
                                          * pages, the drums' PRESET and FX by SELECT, the FX page; AFFINE: every
                                          * line fits the header's at 2x, 216 px, as tut_away's do) */
    {0u, TC_HOME, 0u, 0u, 1u << B_HOME, "PRESS HOME: PERFORM"},
    {0u, TC_PLAY, 0u, 1u, 1u << B_PLAY, "PRESS PLAY: THE BREAK"},
    {0u, TC_SLICES, TK_WHITE, 1u, 0u, "PLAY THE WHITE KEYS"},
    {0u, TC_BRK, 0u, 1u, 0u, "ALGORITHM: NEXT BREAK"},
    {9u, TC_HGEN, 0u, 0u, 1u << B_HOME, "HOME AGAIN: GENERATOR"},   /* (0.9.2, FLEXOR: HOME's tabs) */
    {9u, TC_HSET, 0u, 0u, 1u << B_HOME, "HOME AGAIN: SETTINGS"},
    {9u, TC_HVIEW, 0u, 0u, 0u, "KNOB 1 RIGHT: SLICES"},
    {9u, TC_HBACK, 0u, 1u, 1u << B_HOME, "HOME: PERFORM IN SLICES"},
    {9u, TC_HHOLD, 0u, 1u, 1u << B_HOME, "HOLD HOME: WAVEFORM"},
    {1u, TC_PAD, TK_BLACK, 1u, 0u, "HOLD A BLACK KEY"},
    {1u, TC_OVLPG, 0u, 0u, 1u << B_ARP, "PRESS ARP: EFFECTS"},   /* (0.244: the OVERLAY page back) */
    {1u, TC_OVLKEY, TK_WHITE, 1u, 0u, "HOLD TWO WHITE KEYS"},
    {4u, TC_SEQ, 0u, 0u, 1u << B_SEQ, "PRESS SEQ: THE PATTERN"},   /* (0.252, FLEXOR: THE PATTERN before THE
                                          * KICK: kicks in it first) */
    {4u, TC_KICKS, TK_WHITE, 1u, 0u, "WHITE KEYS: ADD 2 BOOMS"},
    {4u, TC_SNARE, TK_WHITE, 1u, 0u, "ALGORITHM: SNARE, ADD 1"},
    {2u, TC_EDIT, 0u, 0u, 1u << B_EDIT, "PRESS EDIT: BOOM"},
    {2u, TC_STOP, 0u, 0u, 1u << B_PLAY, "PRESS PLAY: STOP"},   /* (0.252, FLEXOR: heard stopped, keys 1-4) */
    {2u, TC_KEYS4, TK_DRUMS, 1u, 0u, "KEYS 1-4: THE DRUMS"},
    {3u, TC_SNPG, 0u, 0u, 0u, "TURN SELECT: THE SNARE"},
    {3u, TC_PPRE, TK_KEY2, 1u, 0u, "KNOB 1, KEY 2: PRESETS"},
    {3u, TC_PFX, TK_KEY2, 1u, 0u, "KNOB 2, KEY 2: ITS FX"},
    {5u, TC_FXPG, 0u, 0u, 1u << B_FX, "PLAY, THEN PRESS FX"},   /* (0.252: playing again) */
    {5u, TC_FXT, 0u, 1u, 0u, "TURN PRESETS: AN EFFECT"},
    {6u, TC_DROP, 0u, 1u, 1u << B_SEL | 1u << B_ENV, "HOLD SEL, PRESS ENV"},
    {7u, TC_SAVE, 0u, 0u, 1u << B_SAVE, "PRESS SAVE: ALL KEPT"},
    {8u, TC_END, 0u, 0u, 0u, "ENJOY! SEE THE MANUAL"},
};
#define TUT_N (sizeof TUT / sizeof TUT[0])
#define TUT_DONE 0xFEu
#define TUT_WAIT 5000u                   /* (FLEXOR: a step done with a result to hear, 5 s before the next) */
static uint32_t tut_ok;                  /* (a step done: when, |1; 0: none) */
static int tut_on(void) { return set.tut >= 1u && set.tut <= TUT_N; }
/* what the steps watch (common.c's counters, counted where it happens: main.c, seq.c), against the step's start */
static struct {
    uint32_t wk, padm, padn, lane, saves, home, t, fx, ovlk;
    uint8_t drop, brk, ppre, pfx;
} tut_at;
static uint32_t tut_fx_sig(void)        /* (the FX chain's first two slots: their effects, presets included) */
{
    uint32_t k, s = 0u;
    for (k = 0; k < 2u; k++)
        s = s << 8 | fx_sl(k)->type | mb_preset(k) << 4 | (uint32_t)eq_magic_is(k) << 6;
    return s;
}                                /* (at the step's start) */
static void tut_mark(void)
{
    tut_at.wk = tut_wk;
    tut_at.padm = tut_pads;
    tut_at.padn = tut_padn;
    tut_at.lane = tut_lane;
    tut_at.saves = tut_saves;
    tut_at.home = tut_home;
    tut_at.drop = drop_n;
    tut_at.brk = set.brk;
    tut_at.ppre = perc_x(PV_SNARE)[PX_PRE];
    tut_at.pfx = perc_x(PV_SNARE)[PX_FX];
    tut_at.fx = tut_fx_sig();
    tut_at.ovlk = tut_ovl;
    tut_at.t = plat_ms();
    tut_drs = 0u;
}
static void tut_mode(int on)             /* (AFFINE: LOAF BREAKER's BEGINNER while it runs, the full AFFINE after) */
{
    set.novice = on ? 1u : 0u;
    if (on) {
        ui.meter_on = 0u;                /* (the METER: not in it) */
        ui.fx_page = 0u;                 /* (the FX page: MAIN only) */
        if (ui.fx_focus >= FX_EASY_SLOTS)
            ui.fx_focus = 0u;            /* (the chain's two slots) */
    }
    changed();                           /* (fx_send: the chain's slots, two in it, all four after) */
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
static void tut_go(uint32_t step)        /* step 1..TUT_N (past the last: finished) */
{
    set.tut = (uint8_t)(step > TUT_N ? TUT_DONE : step);
    if (step > TUT_N) {
        set.auto_save = 1u;              /* (LOAF BREAKER, 0.250, FLEXOR: AUTO SAVE off for the tutorial, on after it) */
        tut_mode(0);
    }
    set_dirty = 1u;
    tut_ok = 0u;
    tut_mark();
    ui.redraw |= RG_HEAD;
}
static void tut_start(void)              /* SETUP K3 right: from where it was left, or the first step */
{
    set.auto_save = 0u;                  /* (0.250: off while it runs; on again at its end, or quit) */
    tut_go(set.tut >= 1u && set.tut <= TUT_N ? set.tut : 1u);
    tut_mode(1);
    app_say("TUTORIAL", "GLO QUIT, SEL+GLO SKIP");
}
/* LOAF BREAKER (FLEXOR, 11 Nov 2026, 0.229): a new FM-1 (or RESET ALL) asks first. set.tut TUT_ASK while it asks (after
 * the start screen); a knob or ALGORITHM moves the choice, PLAY (or white key 1-2) takes it. AFFINE (16 Nov 2026, the
 * owner): TUTORIAL or PLAY, one mode for everyone (LOAF's BEGINNER and EXPERT gone). The FM-1 and the web emulator only
 * (the simulator's scenarios start playing, LOAF_ASK set: it asks) */
#define TUT_ASK 0xFDu
static uint8_t ask_sel = 1u, ask_drawn = 0xFFu;
static const char *const ASK_NAME[2] = {"TUTORIAL", "PLAY"};
static const char *const ASK_TEXT[2] = {"26 GUIDED STEPS", "EVERYTHING, NOW"};   /* (TUT_N; a line each at 2x) */
static void tut_quit(void);
static void ask_pick(uint32_t c)
{
    set.tut = TUT_DONE;
    if (c == 0u) {                       /* (the TUTORIAL: AUTO SAVE off while it runs, on after it) */
        set.auto_save = 0u;
        set.tut = 0u;
        tut_go(1u);
        tut_mode(1);
        app_say("TUTORIAL", "GLO QUIT, SEL+GLO SKIP");
    } else {                             /* (PLAY: AUTO SAVE on, as LOAF's first start had it) */
        set.auto_save = 1u;
        tut_mode(0);
        app_say("PLAY", "TUTORIAL: SETUP K3");
    }
    set_dirty = 1u;
    changed();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = RG_ALL;
}
static void ask_draw(void)
{
    uint32_t c;
    if (ask_drawn == ask_sel)
        return;
    if (ask_drawn == 0xFFu)
        lcd_fill(0, 0, 240, 240, C_BLACK);
    ask_drawn = ask_sel;
    cv_begin(240, 60, C_BLACK);          /* the question (AFFINE: its lines at 2x, rows 32-41 and 46-55) */
    cv_text2((240 - text2_w(&FONT_B, "WELCOME")) / 2, 1, &FONT_B, "WELCOME", C_WHITE);
    cv_text((240 - text_w(&FONT_XS, "TURN A KNOB, PRESS PLAY")) / 2, 30, &FONT_XS, "TURN A KNOB, PRESS PLAY", C_AMB);
    cv_text((240 - text_w(&FONT_XS, "LATER: GLO, SETUP, KNOB 3")) / 2, 44, &FONT_XS, "LATER: GLO, SETUP, KNOB 3", C_DIM);
    cv_blit(0, 0);
    for (c = 0; c < 2u; c++) {           /* the two, one a strip: the name, a line under it (AFFINE: 2x, centred in
                                          * the card; the strips at rows 76 and 146) */
        int on = c == ask_sel;
        char n[16];
        cv_begin(240, 60, C_BLACK);
        cv_rect(12, 3, 216, 50, on ? C_HI : C_BLOCK);
        n[0] = (char)('1' + c);
        n[1] = 0;
        cv_text(24, 12, &FONT_B, n, on ? C_BLACK : C_DIM);
        cv_text(44, 12, &FONT_B, ASK_NAME[c], on ? C_BLACK : C_WHITE);
        cv_text(44, 29, &FONT_XS, ASK_TEXT[c], on ? C_BLACK : C_GRAY);
        cv_blit(0, 76u + c * 70u);
    }
}
static void ask_input(uint32_t kdn, uint32_t bdn, int32_t d)
{
    uint32_t i;
    if (d)
        ask_sel = (uint8_t)clamp((int32_t)ask_sel + (d > 0 ? 1 : -1), 0, 1);
    for (i = 0; i < 2u; i++)
        if (kdn >> WK[i] & 1u) {
            ask_pick(i);
            return;
        }
    if (bdn & 1u << B_PLAY)
        ask_pick(ask_sel);
}
static void tut_quit(void)
{
    set.tut = TUT_DONE;
    set.auto_save = 1u;                  /* (0.250: on after the tutorial) */
    tut_mode(0);
    set_dirty = 1u;
    app_say("TUTORIAL", "QUIT (SETUP K3: AGAIN)");
    ui.redraw |= RG_HEAD | RG_BLOCKS | RG_FOOT;
}
static int tut_done(uint32_t c, const brk_status_t *st)
{
    switch (c) {
    case TC_HGEN:                        /* (0.9.2: HOME's tabs, PERFORM, GENERATOR, SETTINGS, the VIEW) */
        return ui.page == PG_GEN;
    case TC_HSET:
        return ui.page == PG_HSET;
    case TC_HVIEW:
        return ui.page == PG_HSET && ui.blocks;
    case TC_HBACK:
        return ui.page == PG_HOME;
    case TC_HHOLD:
        return ui.page == PG_HOME && !ui.blocks;
    case TC_HOME:                        /* (HOME pressed, PERFORM: on it already, it stays: tut_home_stay) */
        return tut_home != tut_at.home && ui.page == PG_HOME;
    case TC_PLAY:
        return st->playing;
    case TC_SLICES:
        return tut_wk - tut_at.wk >= 4u;
    case TC_BRK:
        return set.brk != tut_at.brk;
    case TC_PAD:
        return tut_padn != tut_at.padn;
    case TC_OVLPG:
        return ui.page == PG_OVL;
    case TC_OVLKEY:                      /* (two of its effects played) */
        return tut_ovl - tut_at.ovlk >= 2u;
    case TC_EDIT:
        return ui.page == PG_KICK;
    case TC_STOP:
        return !st->playing;
    case TC_KEYS4:                       /* (the four drums each played once, stopped) */
        return (tut_drs & 15u) == 15u;
    case TC_SNPG:
        return ui.page == PG_PERC && ui.einst == DR_SNARE;
    case TC_PPRE:                        /* (0.252: and key 2 after it, heard) */
        return perc_x(PV_SNARE)[PX_PRE] != tut_at.ppre && (tut_heard & 2u);
    case TC_PFX:
        return perc_x(PV_SNARE)[PX_FX] != tut_at.pfx && (tut_heard & 2u);
    case TC_FXPG:                        /* (0.252: playing again) */
        return ui.page == PG_FX && st->playing;
    case TC_FXT:
        return tut_fx_sig() != tut_at.fx;
    case TC_SEQ:
        return ui.page == PG_SEQ;
    case TC_KICKS:
        return ui.page == PG_SEQ && sl_kick(ui.seq_lane) && tut_lane - tut_at.lane >= 2u;
    case TC_SNARE:
        return ui.page == PG_SEQ && sl_drum(ui.seq_lane) == DR_SNARE && tut_lane != tut_at.lane;
    case TC_DROP:
        return drop_n != tut_at.drop;
    case TC_SAVE:
        return tut_saves != tut_at.saves;
    default:                             /* (the last: read, then gone) */
        return plat_ms() - tut_at.t > 6000u;
    }
}
/* LOAF BREAKER (0.248, FLEXOR): off the step's page, the header says how to go back there instead (and blinks that
 * button); NULL: on the way */
static const char *tut_away(uint32_t c, uint16_t *btn)
{
    int home = ui.page == PG_HOME || ui.page == PG_GEN, kick = ui.page == PG_KICK;
    *btn = 0u;
    switch (c) {
    case TC_HVIEW:                       /* (0.9.2: off HOME's SETTINGS) */
        if (ui.page != PG_HSET) {
            *btn = 1u << B_HOME;
            return "HOME: BACK TO SETTINGS";
        }
        return 0;
    case TC_HHOLD:
        if (ui.page != PG_HOME) {
            *btn = 1u << B_HOME;
            return "HOME: BACK TO PERFORM";
        }
        return 0;
    case TC_SLICES:
    case TC_BRK:
        if (!home) {
            *btn = 1u << B_HOME;
            return "HOME: BACK TO THE BREAK";
        }
        return 0;
    case TC_OVLKEY:
        if (ui.page != PG_OVL || ui.opage) {
            *btn = 1u << B_ARP;
            return "ARP: BACK TO EFFECTS";
        }
        return 0;
    case TC_KEYS4:
        if (!kick) {
            *btn = 1u << B_EDIT;
            return "PRESS EDIT: BACK TO BOOM";
        }
        return 0;
    case TC_SNPG:
        if (!kick && ui.page != PG_PERC) {
            *btn = 1u << B_EDIT;
            return "PRESS EDIT: BACK TO BOOM";
        }
        return 0;
    case TC_PPRE:
    case TC_PFX:
        if (ui.page == PG_PERC && ui.einst == DR_SNARE)
            return 0;
        if (kick || ui.page == PG_PERC)
            return "SELECT: BACK TO SNARE";
        *btn = 1u << B_EDIT;
        return "EDIT, THEN SELECT: SNARE";
    case TC_KICKS:
    case TC_SNARE:
        if (ui.page != PG_SEQ) {
            *btn = 1u << B_SEQ;
            return "SEQ: BACK TO PATTERN";
        }
        if (c == TC_KICKS && !sl_kick(ui.seq_lane))
            return "ALGORITHM: THE BOOM LANE";
        return 0;
    case TC_FXT:
        if (ui.page != PG_FX) {
            *btn = 1u << B_FX;
            return "FX: BACK TO EFFECTS";
        }
        return 0;
    default:
        return 0;
    }
}
/* LOAF BREAKER (0.249, FLEXOR: "the tutorial must flow"): the page buttons a step lets through: its own, the way back
 * (tut_away), the next step's once this one is done; GLO (quit, SEL + GLO skip), PLAY, SAVE, REC are never held back */
#define TUT_NAV (1u << B_HOME | 1u << B_FX | 1u << B_EDIT | 1u << B_LFO | 1u << B_SEQ | 1u << B_ENV | 1u << B_ARP)
static uint32_t tut_btn_ok(void)
{
    uint32_t s = set.tut - 1u, m = TUT[s].btn;
    uint16_t bt;
    tut_away(TUT[s].cond, &bt);
    m |= bt;
    if (tut_ok && s + 1u < TUT_N)
        m |= TUT[s + 1u].btn;
    return m;
}
static uint32_t tut_left(void)           /* a step done: the seconds before the next (1..5) */
{
    uint32_t e = plat_ms() - tut_ok;
    return e >= TUT_WAIT ? 0u : (TUT_WAIT - e + 999u) / 1000u;
}
static void tut_frame(const brk_status_t *st)   /* each frame: the step done, 5 s, the next */
{
    static uint32_t left;
    if (!tut_on())
        return;
    if (tut_ok) {
        uint32_t l = tut_left();
        if (!l)
            tut_go(set.tut + 1u);
        else if (l != left)
            ui.redraw |= RG_HEAD;        /* (the header's countdown) */
        left = l;
        return;
    }
    {                                    /* (off the step's page, or back: the header says so) */
        static const char *was;
        uint16_t bt;
        const char *w = tut_away(TUT[set.tut - 1u].cond, &bt);
        if (w != was)
            ui.redraw |= RG_HEAD;
        was = w;
    }
    if (tut_done(TUT[set.tut - 1u].cond, st)) {
        if (!TUT[set.tut - 1u].wait) {   /* (nothing to hear: "OK!" and the next at once) */
            if (TUT[set.tut - 1u].cond != TC_END)
                app_say("TUTORIAL", "OK!");
            tut_go(set.tut + 1u);
            return;
        }
        tut_ok = plat_ms() | 1u;
        left = 0u;
        ui.redraw |= RG_HEAD;
    }
}
/* HOME on PERFORM already, the TUTORIAL's HOME step: it stays there (not GENERATOR) */
static int tut_home_stay(void) { return tut_on() && !tut_ok && TUT[set.tut - 1u].cond == TC_HOME; }
static void tut_skip(void)               /* SEL + GLO */
{
    app_say("TUTORIAL", "STEP SKIPPED");
    tut_go(set.tut + 1u);
}
static void tut_head(char *a, char *n, char *b)   /* the header's: the lesson ("THE BREAK"), the step ("3/26"), what
                                          * to do (AFFINE: three parts, the lines at 2x) */
{
    char k[8];
    uint32_t s = set.tut - 1u;
    a[0] = b[0] = 0;
    int_text((int32_t)set.tut, n);
    text_cat(n, "/");
    int_text((int32_t)TUT_N, k);
    text_cat(n, k);
    text_cat(a, TUT_LESSON[TUT[s].lesson % TUT_LESSONS]);
    if (tut_ok) {                        /* (done: "OK! NEXT IN 4 S") */
        text_cat(b, set.tut + 1u < TUT_N ? "OK! NEXT IN " : "OK! ALL DONE ");
        int_text((int32_t)tut_left(), k);
        text_cat(b, k);
        text_cat(b, " S");
        return;
    }
    {
        uint16_t bt;
        const char *w = tut_away(TUT[s].cond, &bt);
        text_cat(b, w ? w : TUT[s].say);   /* (off the step's page: the way back) */
    }
}
static void tut_leds(uint32_t *b, uint32_t *k, int playing)   /* the step's buttons and keys blinking */
{
    uint32_t s, i;
    if (!tut_on() || tut_ok || !((plat_ms() / 300u) & 1u))
        return;
    s = set.tut - 1u;
    {
        uint16_t bt;
        if (tut_away(TUT[s].cond, &bt)) { /* (off the step's page: the button back, nothing else) */
            *b |= bt;
            return;
        }
    }
    *b |= TUT[s].btn;
    if ((TUT[s].cond == TC_DROP || TUT[s].cond == TC_FXPG || TUT[s].cond == TC_FXT) && !playing)   /* (the drop, the
                                          * FX: PLAY first) */
        *b |= 1u << B_PLAY;
    for (i = 0; i < 16u && TUT[s].keys & TK_WHITE; i++)
        *k |= 1u << WK[i];
    for (i = 0; i < 4u && TUT[s].keys & TK_DRUMS; i++)
        *k |= 1u << WK[i];
    if (TUT[s].keys & TK_KEY2)
        *k |= 1u << WK[1];
    for (i = 0; i < 11u && TUT[s].keys & TK_BLACK; i++)
        *k |= 1u << BK[i];
}
