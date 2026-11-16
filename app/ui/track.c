/* SPDX-License-Identifier: MPL-2.0 */
/* LOAF BREAKER: ENV, where TAP TEMPO was: TRACKS (the patterns in an order, their loops, DROPs between them), PLAY
 * (a track from its start, or from an item: back to HOME as it starts) and DROP (the build's bars, the pattern after
 * the drop, the drop's break and crash). ENV again: the next part. The track plays from the main loop: as a pattern
 * comes into the last bar of its last loop, the next item is asked for (a pattern: in at the loop's end, pat_next; a
 * DROP: the build from the next bar, then the drop into the pattern after it). (Part of app.c's unity: included
 * there in order.) */

static void page_say(void);
static void break_ask(uint32_t i);      /* (main.c) */
#define NTPART 6u
/* (ENV's parts, ui.tpage: 0 TRACKS, 1 PLAY, 2 DROP, 4 BUILD; 3, LOAF's KNOB, gone; RESET is GLO's, RST_ON. AFFINE,
 * controls phase 2: their order and names in main.c's DM_PART) */
#define RST_ON (ui.page == PG_GLOBAL && ui.gpage == 5u)   /* (GLO RESET: drawn and turned by ENV's code here) */
/* RESET (ENV's last part): K1 what (a list: more to come), K4 turned right twice within 3 s: back to the start */
enum { RST_DROP, RST_BUILD, RST_KICK, RST_DRUMS, RST_MIX, RST_FX, RST_STEPS, RST_PADS, RST_KEYS, RST_TRACK, RST_SCREEN,
       RST_WIPE, RST_ALL, NRST };        /* (RST_WIPE: 0.263, FLEXOR; EVERYTHING last. AFFINE: KNOB, KICK TONE, ALL
                                          * KICKS gone with the hardcore kick; KICK is BOOM) */
static const char *const RST_NAME[NRST] = {"DROP", "BUILD", "BOOM", "DRUMS", "MIX", "FX CHAIN", "STEPS", "PADS", "KEYS",
                                           "TRACK", "SCREEN", "WIPE MEMORY", "EVERYTHING"};
static const char *const RST_WHAT[NRST] = {"4 BARS, NEXT, CRASH 100%", "SNARE ROLL, HIGH-PASS",
                                          "808, ITS LEVEL KEPT", "SNARE, HAT, CLAP", "LEVELS, SWING, COMP",
                                          "THE FOUR SLOTS", "1 HIT, ALWAYS, ON TIME", "THE BLACK KEYS",
                                          "THE WHITE KEYS", "FROM ITEM 1, BREAKS SAME", "SKIN, EFFECTS, ZOOM",
                                          "ALL SAVES ERASED", "ALL 64 PATTERNS TOO"};   /* (what each puts back: AFFINE,
                                          * a line at 2x under the list) */
static const char *const RST_SHORT[NRST] = {"DROP", "BUILD", "BOOM", "DRUMS", "MIX", "FX", "STEPS", "PADS", "KEYS",
                                           "TRACK", "SCREEN", "WIPE", "ALL"};   /* (the foot's WHAT, beside it) */
/* LOAF BREAKER (0.263, FLEXOR): WIPE MEMORY, before installing another firmware: every sector LOAF BREAKER saves in
 * (its objects: 0xB6000..0xDBFFF, 0xDE000..0xDFFFF; the panel's calibration, 0xDC000, every fm1-base firmware's, kept)
 * erased, one a frame (wipe_step, main.c), the settings left unreadable (a first start's, if it comes back); then no
 * more saving until the FM-1 restarts. The next firmware finds that memory blank */
#define WIPE_LO 0xB6000u
#define WIPE_HI 0xE0000u
static uint32_t wipe_at;                 /* (the next sector; 0 idle) */
static void wipe_start(void)
{
    if (playing_now())
        transport(0, 0);                 /* (stopped first: an erase stops the sound) */
    wipe_at = WIPE_LO;
    set_dirty = 0u;
}
static void wipe_step(void)              /* (main.c, each frame: a sector) */
{
    if (!wipe_at)
        return;
    if (wipe_at == 0xDC000u)
        wipe_at = 0xDE000u;              /* (the panel's pair kept) */
    if (wipe_at < WIPE_HI) {
        char t[20] = "", n[8];
        plat_data_erase(wipe_at);
        int_text((int32_t)((wipe_at - WIPE_LO) / 0x1000u + 1u), n);
        text_cat(t, n);
        text_cat(t, " / 40");
        app_say("WIPING", t);
        wipe_at += 0x1000u;
        return;
    }
    {
        uint32_t z = 0u;                 /* (the settings: too short to read, a first start's next time) */
        plat_store_save(OBJ_SETTINGS, &z, sizeof z);
    }
    wipe_at = 0u;
    wiped = 1u;
    app_say("MEMORY WIPED", "INSTALL THE OTHER FIRMWARE");
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
#define RST_TURNS 5u                     /* LOAF BREAKER: the clicks that reset (K2, K3 or K4), each within 2 s of the last */
#define RST_COOL 3000u                   /* LOAF BREAKER: after a reset, the RESET page's knobs at rest this long */
static int rst_cool(void) { return ui.rst_done && plat_ms() - ui.rst_done < RST_COOL; }
static int rst_live(void)
{
    return ui.rst_n && ui.rst_arm && plat_ms() - ui.rst_arm < 2000u;
}
#define RST_ASK_MS 5000u                 /* (LOAF BREAKER, 0.239: the clicks done, GLO pressed within this: the reset) */
static int rst_asking(void) { return ui.rst_ask && plat_ms() - ui.rst_ask < RST_ASK_MS; }
static void reset_pick(int32_t d)        /* RESET: the part (K4, 0.239; K1 or ALGORITHM before); the clicks start again */
{
    if (rst_cool())                      /* (LOAF BREAKER: just reset: nothing for 3 s) */
        return;
    ui.rst_sel = (uint8_t)clamp((int32_t)ui.rst_sel + (d > 0 ? 1 : -1), 0, NRST - 1);
    ui.rst_arm = 0u;
    ui.rst_n = 0u;
    ui.rst_ask = 0u;
    app_say("RESET", RST_NAME[ui.rst_sel % NRST]);
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
static void reset_do(uint32_t w);
static int rst_confirm(void)             /* (LOAF BREAKER, 0.239: GLO on RESET after the five clicks: the reset; 1 done) */
{
    if (!RST_ON || !rst_asking())
        return 0;
    ui.rst_ask = 0u;
    ui.rst_done = plat_ms() | 1u;
    if (ui.rst_sel % NRST == RST_ALL) {  /* (RESET ALL: its own screen says so) */
        reset_all();
        return 1;
    }
    if (ui.rst_sel % NRST == RST_WIPE) { /* (0.263: WIPE MEMORY, a sector a frame) */
        wipe_start();
        return 1;
    }
    reset_do(ui.rst_sel % NRST);
    app_say(RST_NAME[ui.rst_sel % NRST], "RESET: DONE");
    ui.redraw |= RG_BLOCKS | RG_FOOT;
    return 1;
}
static void reset_do(uint32_t w)
{
    if (w == RST_DROP) {
        set.drop_bars = 3u;
        set.drop_after = 0u;
        set.drop_brk = 0u;
        set.drop_crash = 100u;
    } else if (w == RST_BUILD) {
        set.drop_style = DS_SNARE;
        set.drop_sweep = 0u;
        set.drop_riser = 50u;
        set.drop_gap = 0u;
    } else if (w == RST_DRUMS) {         /* DRUMS: each drum's memory playing as it came (its preset) */
        uint32_t v;
        for (v = 0; v < PERC_VOICES; v++) {
            uint32_t m = *perc_slot(v) % PERC_MEMS;
            perc_preset(perc_mem(v, m), v, m % PERC_PRESETS);
            *perc_voice(v) = *perc_mem(v, m);
        }
    } else if (w == RST_MIX) {           /* MIX: the MIXER's four parts as they start */
        brk_fx_t f;
        perc_t d;
        brk_fx_defaults(&f);
        perc_defaults(&d);
        set.brk_level = 100u;
        set.kick.volume = 0.8f;
        set.perc.volume = d.volume;
        set.fx.wet = f.wet;
        set.swing = 50u;
        set.drums_chain = 0u;            /* (0.9.2: AROUND, FLEXOR) */
        set.comp = set.duck = set.duck_what = set.max_out = 0u;
        set.comp_ratio = 3u;
        set.comp_att = 59u;
        set.comp_rel = 55u;
        set.duck_att = 19u;
        set.duck_rel = 36u;
    } else if (w == RST_FX) {            /* FX CHAIN: its slots as they came (the MIXER's FX level kept) */
        float wet = set.fx.wet;
        uint32_t k;
        brk_fx_defaults(&set.fx);
        set.fx.wet = wet;
        for (k = 0; k < BRK_FX_SLOTS; k++) {
            brk_mbc_defaults(&set.mbc[k]);
            brk_fxx_defaults(&set.fxx[k], set.fx.slot[k].type);
        }
        pats.mem.fx_ok = 0u;
    } else if (w == RST_STEPS) {         /* STEPS: this pattern's drum steps plain (the steps themselves kept) */
        uint32_t k;
        for (k = 0; k < STEPB_N; k++)
            steps.b[set.pattern % NPAT][k] = 0u;
        for (k = 0; k < NDRUMS; k++)
            pats.hits[set.pattern % NPAT][k] = 0u;
    } else if (w == RST_PADS) {          /* PADS: the black keys' settings as they came */
        uint32_t k;
        brk_padset_defaults(&set.pads);
        set.half_mt_off = 0u;
        set.ovl_pad = 0u;
        set.rsv2[0] = 0u;                /* (0.9.45: every ARP key back to one effect, ovl_pad's) */
        arp_held = arp_lat = 0u;
        for (k = 0; k < 16u; k++)
            set.ovl_amt[k] = k == OV_DIVE || k == OV_RISE || k == OV_CRUSH || k == OV_DIST || k == OV_BOOM ? 70u : 100u;
        set.ovl_rate = 2u;
        set.ovl_sweep = 2u;
        set.ovl_latch = 0u;
    } else if (w == RST_KEYS) {          /* KEYS: each white key's loop and slice effect as they came */
        uint32_t k;
        for (k = 0; k < BRK_KEYS; k++) {
            brk_keyset_defaults(&set.keys[k]);
            brk_slicefx_defaults(&set.slx[k]);
        }
    } else if (w == RST_TRACK) {         /* TRACK: this track from its first item, each item's break SAME (its items
                                          * kept) */
        uint32_t k;
        pats.trk[set.trk % NTRACKS].rsv[0] = 0u;   /* (TRK_FROM, below) */
        for (k = 0; k < TRK_ITEMS; k++)
            steps.tb[set.trk % NTRACKS][k] = 0u;
    } else if (w == RST_SCREEN) {        /* SCREEN: the skin, the screen effects and their settings, WAVE ZOOM as they
                                          * start (GLO MORE, EFFECTS, FX SET) */
        set.skin = SKIN_NEW | SKIN_DEFAULT;   /* (LOAF BREAKER, 0.240: RED) */
        set.sfx_on = 0u;
        set.scrfx = set.sfx_kick = set.sfx_drop = set.sfx_len = 50u;
        set.wzoom = 3u;
        ui.redraw = RG_ALL;
    } else {                             /* AFFINE: BOOM: as it starts, 808, its level kept */
        float vol = set.kick.volume;
        kick_defaults(&set.kick);
        set.kick.volume = vol;
        set.kpre = 0u;
    }
    changed();
}
static const char *const DS_NAME[DS_N] = {"SNARE ROLL", "BOOM ROLL", "BOOM+SNARE", "STUTTER", "SWEEP ONLY", "RANDOM"};
static const char *const SWEEP_NAME[4] = {"HIGH-PASS", "LOW-PASS", "NONE", "RANDOM"};
static const char *const DS_SHORT[DS_N] = {"SNARE", "BOOM", "BOTH", "STUTTER", "SWEEP", "RAND"};   /* (AFFINE: the
                                          * foot's, beside STYLE and SWEEP at 2x; the header says the long ones) */
static const char *const SWEEP_SHORT[4] = {"HP", "LP", "OFF", "RAND"};
static uint32_t drop_rng = 0x2545F491u;
static uint32_t drop_rand(uint32_t n)    /* 0..n-1, by chance (the drops unlike each other) */
{
    drop_rng = drop_rng * 1664525u + 1013904223u + plat_ms();
    return (drop_rng >> 16) % n;
}
static const char *const TRK_END_NAME[3] = {"LOOP", "STOP", "HOLD"};

static track_t *trk_now(void) { return &pats.trk[set.trk % NTRACKS]; }
#define TRK_FROM (trk_now()->rsv[0])     /* LOAF BREAKER: the track's first item (TRACKS: SEL + PLAY; PLAY's FROM), saved
                                          * with it */
static uint8_t *trk_brk(uint32_t i) { return &steps.tb[set.trk % NTRACKS][i % TRK_ITEMS]; }   /* (the item's break;
                                          * AFFINE: LOAF BREAKER's item's kick gone) */
static int trk_is_pat(const track_t *t, uint32_t i)
{
    uint32_t v = t->it[i % TRK_ITEMS];
    return v != TI_END && v != TI_DROP && v != TI_GAP;
}
static void trk_brk_go(uint32_t i)       /* a pattern item coming: its own break, if it has one (at the bar's end) */
{
    uint32_t b = *trk_brk(i);
    if (b && b <= n_breaks && b - 1u != set.brk)
        break_ask(b - 1u);
}
static uint32_t trk_len(const track_t *t)   /* its items before the END */
{
    uint32_t n = 0u;
    while (n < TRK_ITEMS && t->it[n] != TI_END)
        n++;
    return n;
}
static uint32_t trk_bars(const track_t *t)   /* how long it plays, in bars (the drops' builds counted) */
{
    uint32_t i, b = 0u, n = trk_len(t), lb = pat_len() / 16u;
    for (i = 0; i < n; i++)
        b += t->it[i] == TI_DROP ? DROP_BARS[set.drop_bars % 5u] : t->it[i] == TI_GAP ? 0u : (uint32_t)t->rep[i] * lb;
    return b;
}
/* LOAF BREAKER TEST: a track playing, where it is in bars (done of total), and the bars left before its next DROP
 * (-1: none next); 0 when no track plays */
static int trk_where(const brk_status_t *st, uint32_t *done, uint32_t *total, int32_t *drop_in)
{
    const track_t *t = trk_now();
    uint32_t i, b = 0u, lb = pat_len() / 16u, pos = ui.trk_pos % TRK_ITEMS, n = trk_len(t), in, item;
    if (!ui.trk_on || !n)
        return 0;
    if (!lb)
        lb = 1u;
    for (i = 0; i < pos && i < n; i++)
        b += t->it[i] == TI_DROP ? DROP_BARS[set.drop_bars % 5u] : t->it[i] == TI_GAP ? 0u : (uint32_t)t->rep[i] * lb;
    item = t->it[pos] == TI_DROP ? DROP_BARS[set.drop_bars % 5u] : (uint32_t)t->rep[pos] * lb;
    in = t->it[pos] == TI_DROP ? 0u : (uint32_t)ui.trk_loops * lb + (st->step >= 0 ? (uint32_t)st->step / 16u : 0u);
    if (in > item)
        in = item;
    *done = b + in;
    *total = trk_bars(t);
    *drop_in = -1;
    for (i = pos + 1u; i < n && t->it[i] == TI_GAP; i++)
        ;
    if (i < n && t->it[i] == TI_DROP && t->it[pos] != TI_DROP)
        *drop_in = (int32_t)(item - in);
    return 1;
}
static int trk_pat_from(const track_t *t, uint32_t i, int wrap)   /* the first pattern item at i or after (wrap: from
                                          * the start again past the END), -1 none */
{
    uint32_t n = trk_len(t), k;
    for (k = 0; k < (wrap ? 2u * TRK_ITEMS : TRK_ITEMS); k++, i++) {
        if (i >= n) {
            if (!wrap || !n)
                return -1;
            i = 0u;
        }
        if (t->it[i] != TI_DROP && t->it[i] != TI_GAP)
            return (int)i;
    }
    return -1;
}

/* the drop asked for: a build into pattern to (its drums ready for the render), or (building) the drop at the
 * next bar */
static void drop_start_k(uint32_t to, int item);
static uint8_t b_stut;                   /* (0.9.26, FLEXOR: this build's STUTTER, a track's DROP item's K3: the break
                                          * rolled over the BUILD's style; kept in the item's break byte, a DROP's unused:
                                          * bit 0; bits 1-2 its K4, what the GAP keeps, b_gapm) */
static const char *const GAPM_NAME[4] = {"NOTHING", "BREAK", "DRUMS", "ECHOES"};
static void drop_start(uint32_t to) { drop_start_k(to, -1); }
static void drop_start_k(uint32_t to, int item)   /* item: a track's DROP item (its STUTTER, its GAP), -1 none */
{
    if (!playing_now()) {
        app_say("DROP", "PLAY FIRST");
        return;
    }
    if (build_p < 0.0f && !build_wait) {
        const pat_t *p;
        uint32_t d, b;
        char t[8] = "> P";
        ui.drop_pat = (uint8_t)(to % NPAT_ALL);   /* (LOAF BREAKER: 0..63; another group's: in at the drop) */
        drop_to = to / NPAT % NGRP == set.pgrp ? (uint8_t)(to % NPAT) : 0xFFu;
        if (drop_to == 0xFFu && grp_blocked(ui.drop_pat))
            ui.drop_pat = 0xFFu;
        b_stut = (uint8_t)(item >= 0 && *trk_brk((uint32_t)item) & 1u);
        b_gapm = (uint8_t)(item >= 0 ? *trk_brk((uint32_t)item) >> 1 & 3u : 0u);
        p = &pats.p[set.pattern % NPAT];
        if (drop_to < NPAT)
            p = &pats.p[drop_to];
        else if (ui.drop_pat < NPAT_ALL) {   /* (LOAF BREAKER: another group's: its drums read from its objects, the
                                          * canvas for room: it is drawn again before it is shown) */
            uint32_t sl = ui.drop_pat % NPAT, n = (sl + 1u) * (uint32_t)sizeof(pat_t);
            pat_t *tp = (pat_t *)(void *)cv_px;
            if (plat_store_head(OBJ_PG0 + ui.drop_pat / NPAT % NGRP, tp, n) == (int)n)
                p = &tp[sl];
            else {
                memset(&tp[0], 0, sizeof(pat_t));   /* (none saved: an empty pattern) */
                p = &tp[0];
            }
            dx_tp = p;                   /* (0.9.30: its drums' bars 3 and 4, OBJ_DX's) */
            dx_tg = (uint8_t)(ui.drop_pat / NPAT);
            dx_ts = (uint8_t)sl;
        }
        for (d = 0; d < NDRUMS; d++)
            for (b = 0; b < 4u; b++)
                drum_next[d][b] = (uint16_t)drum_bar(p, d, b % drum_bars(p, d));
        dx_tp = 0;
        drum_next_ok = 1u;
        build_bars = DROP_BARS[set.drop_bars % 5u];
        drop_gain = (float)set.drop_crash * 0.01f;
        b_style = (uint8_t)(set.drop_style < DS_RANDOM ? set.drop_style : drop_rand(DS_RANDOM));   /* (BUILD) */
        b_sweep = (uint8_t)(set.drop_sweep < 3u ? set.drop_sweep : drop_rand(3u));
        b_riser = (float)(set.drop_riser > 100u ? 100u : set.drop_riser) * 0.02f;
        b_gap = DROP_GAP[set.drop_gap % 5u];
        ui.build_said = plat_ms();
        ui.drop_early = ui.drop_asked = 0u;
        int_text((int32_t)ui.drop_pat + 1, t + 3);
        app_say(build_bars ? "BUILD NEXT BAR" : "DROP AT BAR END", t);
    } else if (build_p >= 0.0f) {
        app_say("DROP", "NEXT BAR");
        ui.drop_early = 1u;
    } else
        return;                          /* (asked already: it comes) */
    build_n++;
}

/* PLAY: the track from its first item (TRK_FROM; a DROP there: the pattern after it), from bar 1; HOME (home: 0, from
 * TRACKS: it stays) */
static void track_play(int home)
{
    const track_t *t = trk_now();
    int i = trk_pat_from(t, TRK_FROM < trk_len(t) ? TRK_FROM : 0u, 0);
    char n[8] = "TRACK ";
    if (i < 0) {
        app_say("TRACK", "EMPTY");
        return;
    }
    if (!pat_go(t->it[i] - 1u))          /* (LOAF BREAKER: another group's; refused: SAVE OR STOP) */
        return;
    pat_next = -1;
    pat_apply();
    trk_brk_go((uint32_t)i);             /* (its break) */
    if (playing_now())
        transport(1, 1);                 /* (from bar 1) */
    else {
        brk_reset();
        transport(1, 0);
    }
    ui.trk_on = 2u;
    ui.trk_pos = (uint8_t)i;
    ui.trk_loops = ui.trk_cnt = ui.trk_stop = 0u;
    ui.trk_drop = 0xFFu;
    ui.trk_prev = -1;
    if (home)
        ui.page = PG_HOME;
    ui.redraw = RG_ALL;
    int_text((int32_t)set.trk + 1, n + 6);
    app_say(n, "PLAYING");
}
static void track_stop(const char *why)
{
    ui.trk_on = 0u;
    ui.trk_drop = 0xFFu;
    app_say("TRACK", why);
    ui.redraw |= RG_HEAD | RG_BLOCKS;
}
/* the track's next item, as the last bar of the one playing begins (its last loop) */
static void track_next(void)
{
    const track_t *t = trk_now();
    uint32_t n = trk_len(t), i = ui.trk_pos + 1u;
    while (i < n && t->it[i] == TI_GAP)  /* (a gap left: played past) */
        i++;
    if (i >= n) {                        /* its end */
        if (t->end == 1u) {
            ui.trk_stop = 1u;            /* (at the loop's end) */
            return;
        }
        if (t->end == 2u) {
            track_stop("END  HELD");
            return;
        }
        i = 0u;
        while (i < n && t->it[i] == TI_GAP)
            i++;
        if (i >= n) {
            track_stop("EMPTY");
            return;
        }
    }
    if (t->it[i] == TI_DROP) {           /* a drop: the build, into the pattern after it */
        int j = trk_pat_from(t, i + 1u, t->end == 0u);
        if (j < 0) {
            ui.trk_pos = (uint8_t)i;
            return;
        }
        ui.trk_drop = (uint8_t)j;
        ui.trk_pos = (uint8_t)i;
        drop_start_k((uint32_t)t->it[j] - 1u, (int)i);
        return;
    }
    ui.trk_pos = (uint8_t)i;
    ui.trk_loops = 0u;
    if (t->it[i] - 1u != pat_cur() && !grp_blocked(t->it[i] - 1u))
        pat_next = (int8_t)(t->it[i] - 1u);
    trk_brk_go(i);                       /* (its break, from the same bar: break_poll swaps at its last 16th) */
    ui.redraw |= RG_HEAD | RG_BLOCKS;
}
/* the main loop, each frame: the loops counted (one as its last bar begins), the next item asked for */
static void track_poll(const brk_status_t *st)
{
    if (!ui.trk_on)
        return;
    if (!st->playing) {
        if (ui.trk_on == 1u)
            track_stop("STOPPED");
        return;
    }
    ui.trk_on = 1u;
    if (st->step < 0)
        return;
    if (st->step < ui.trk_prev) {        /* a new loop */
        ui.trk_cnt = 0u;
        if (ui.trk_stop) {
            transport(0, 0);
            track_stop("END");
            return;
        }
    }
    ui.trk_prev = (int16_t)st->step;
    if (build_p >= 0.0f || build_wait || ui.trk_drop != 0xFFu)
        return;                          /* (a drop on its way: the track waits for it) */
    {
        uint32_t len = pat_len(), last = len > 16u ? len - 16u : 0u;
        const track_t *t = trk_now();
        if ((uint32_t)st->step >= last && !ui.trk_cnt) {
            ui.trk_cnt = 1u;
            if (t->it[ui.trk_pos % TRK_ITEMS] == TI_DROP || ++ui.trk_loops >= t->rep[ui.trk_pos % TRK_ITEMS])
                track_next();
            ui.redraw |= RG_HEAD | (ui.page == PG_TRACK ? RG_BLOCKS : 0);
        }
    }
}
/* the break a drop brings in, + 1 (0: none): a track's, the pattern dropped into's own; else DROP's DROP BREAK */
static uint32_t trk_drop_brk(void)
{
    uint32_t b = ui.trk_on && ui.trk_drop != 0xFFu ? *trk_brk(ui.trk_drop) : 0u;
    if (!b || b > n_breaks)
        b = set.drop_brk <= n_breaks ? set.drop_brk : 0u;
    return b;
}
/* the drop came (the main loop, drop_n): a track's on from the pattern it went to */
static void track_dropped(void)
{
    if (ui.trk_drop == 0xFFu)
        return;
    ui.trk_pos = ui.trk_drop;
    ui.trk_drop = 0xFFu;
    ui.trk_loops = 0u;
    ui.trk_cnt = 0u;
    ui.trk_prev = -1;
}

/* ---- the page ---- */
static void trk_item_text(const track_t *t, uint32_t i, char *s, char *r)   /* "P3" "X4", "DROP" "", "END" "" */
{
    uint32_t v = t->it[i];
    s[0] = r[0] = 0;
    if (v == TI_END)
        text_cat(s, "END");
    else if (v == TI_DROP)
        text_cat(s, "DROP");
    else if (v == TI_GAP)
        text_cat(s, "EMPTY");
    else {
        s[0] = 'P';
        int_text((int32_t)v, s + 1);
        r[0] = 'X';
        int_text((int32_t)t->rep[i], r + 1);
    }
}
static void brk_word(uint32_t b, char *t)   /* AFFINE: a break's name, its tempo left out where it ends in one
                                          * ("OLDSKOO 160": "OLDSKOO"; the foot's, beside its name at 2x) */
{
    const char *s = break_name(b);
    uint32_t n = 0u, k;
    while (s[n] && n < 12u)
        n++;
    for (k = n; k > 0u && s[k - 1u] >= '0' && s[k - 1u] <= '9'; k--)
        ;
    if (k + 1u < n && k >= 4u && s[k - 1u] == ' ')
        n = k - 1u;                      /* (three letters at least, a space, two digits at least) */
    for (k = 0; k < n; k++)
        t[k] = s[k];
    t[n] = 0;
}
static void track_text(int i, char *t, int foot);
static void track_value(int i, char *t) { track_text(i, t, 1); }   /* (the foot's: main.c's knob_value) */
/* the knobs' values. foot: as the foot shows them beside their names (AFFINE, 2x: short, "HP", "OLDSKOO", "2"); else
 * as the header says them ("HIGH-PASS", "OLDSKOO 160", "TRACK 2") */
static void track_text(int i, char *t, int foot)
{
    const track_t *tr = trk_now();
    char r[6];
    t[0] = 0;
    if (RST_ON) {                        /* RESET (GLO): what, and K2-K4: the clicks */
        if (i == 3)                      /* (LOAF BREAKER, 0.239, FLEXOR: K4 what, K2 the clicks, then GLO) */
            text_cat(t, (foot ? RST_SHORT : RST_NAME)[ui.rst_sel % NRST]);
        else if (i == 1) {               /* (AFFINE: the turns as a count; GLO when they are done) */
            if (rst_cool() || rst_asking() || !rst_live())
                text_cat(t, rst_cool() ? "DONE" : rst_asking() ? "GLO" : "x5");
            else {
                int_text((int32_t)ui.rst_n, t);
                text_cat(t, "/5");
            }
        }
        return;
    }
    if (ui.tpage == 4u && ui.bitem) {    /* (AFFINE, controls phase 2: BUILD's TEMPO RISE item, PRESETS until then) */
        if (i == 0 && !set.drop_accel)
            text_cat(t, "OFF");
        else if (i == 0) {
            t[0] = '+';
            int_text(set.drop_accel, t + 1);
            text_cat(t, "%");
        }
        return;
    }
    if (ui.tpage == 4u) {                /* BUILD: its style, its filter, its noise, the gap before the drop */
        if (i == 0)
            text_cat(t, (foot ? DS_SHORT : DS_NAME)[set.drop_style % DS_N]);
        else if (i == 1)
            text_cat(t, (foot ? SWEEP_SHORT : SWEEP_NAME)[set.drop_sweep & 3u]);
        else if (i == 2) {
            if (!set.drop_riser)
                text_cat(t, "OFF");
            else {
                int_text(set.drop_riser * 2u, t);
                text_cat(t, "%");
            }
        } else {
            static const char *const G[5] = {"NONE", "1/16", "1/8", "1 BEAT", "1/2 BAR"};
            text_cat(t, G[set.drop_gap % 5u]);
        }
        return;
    }
    if (ui.tpage == 2u) {                /* DROP */
        if (i == 0) {
            uint32_t b = DROP_BARS[set.drop_bars % 5u];
            if (!b)
                text_cat(t, "NONE");
            else {
                int_text((int32_t)b, t);
                text_cat(t, b > 1u ? " BARS" : " BAR");
            }
        } else if (i == 1) {
            if (!set.drop_after)
                text_cat(t, "NEXT");
            else {
                t[0] = 'P';
                int_text(set.drop_after, t + 1);
            }
        } else if (i == 2) {
            if (!set.drop_brk || set.drop_brk > n_breaks)
                text_cat(t, "SAME");
            else if (foot)
                brk_word(set.drop_brk - 1u, t);
            else
                text_cat(t, break_name(set.drop_brk - 1u));
        } else if (set.drop_crash) {
            int_text(set.drop_crash, t);
            text_cat(t, "%");
        } else
            text_cat(t, "OFF");
        return;
    }
    if (ui.tpage == 1u) {                /* PLAY */
        if (i == 0) {
            if (!foot)                   /* (the foot: TRACK 2) */
                text_cat(t, "TRACK ");
            int_text((int32_t)set.trk + 1, t + text_n(t));
        } else if (i == 1)
            int_text((int32_t)TRK_FROM + 1, t);
        else if (i == 2)
            text_cat(t, TRK_END_NAME[tr->end % 3u]);
        return;
    }
    if (i == 0)                          /* TRACKS: K1 the track (AFFINE, controls phase 2: PRESETS until then; the
                                          * item: OCT) */
        int_text((int32_t)set.trk + 1, t);
    else if (i == 1)
        trk_item_text(tr, ui.trk_cur % TRK_ITEMS, t, r);
    else if (i == 2) {
        if (tr->it[ui.trk_cur % TRK_ITEMS] != TI_END && tr->it[ui.trk_cur % TRK_ITEMS] != TI_DROP &&
            tr->it[ui.trk_cur % TRK_ITEMS] != TI_GAP) {
            text_cat(t, "X");
            int_text(tr->rep[ui.trk_cur % TRK_ITEMS], t + 1);
        } else if (tr->it[ui.trk_cur % TRK_ITEMS] == TI_DROP)
            text_cat(t, *trk_brk(ui.trk_cur) & 1u ? "ON" : "OFF");
        else
            text_cat(t, "-");
    } else if (tr->it[ui.trk_cur % TRK_ITEMS] == TI_DROP)
        text_cat(t, foot && !(*trk_brk(ui.trk_cur) >> 1 & 3u) ? "NONE" : GAPM_NAME[*trk_brk(ui.trk_cur) >> 1 & 3u]);
    else if (!trk_is_pat(tr, ui.trk_cur))
        text_cat(t, "-");
    else if (!*trk_brk(ui.trk_cur) || *trk_brk(ui.trk_cur) > n_breaks)
        text_cat(t, "SAME");
    else if (foot)
        brk_word(*trk_brk(ui.trk_cur) - 1u, t);
    else
        text_cat(t, break_name(*trk_brk(ui.trk_cur) - 1u));
}
static void track_say(int i)             /* the knob turned, on the strip */
{
    static const char *const N[NTPART][4] = {{"TRACK", "PUT", "LOOPS", "BREAK"}, {"TRACK", "FROM ITEM", "AT THE END", ""},
                                        {"BUILD", "AFTER DROP", "DROP BREAK", "CRASH"}, {"", "", "", ""},
                                        {"BUILD STYLE", "SWEEP", "RISER", "GAP"}, {"RESET", "RESET", "RESET", "RESET"}};
    char t[20];
    track_text(i, t, 0);
    int drp = !ui.tpage && trk_now()->it[ui.trk_cur % TRK_ITEMS] == TI_DROP;
    app_say(drp && i == 2 ? "STUTTER" : drp && i == 3 ? "GAP KEEPS" : ui.tpage == 4u && ui.bitem ? "TEMPO RISE"
            : N[ui.tpage % NTPART][i & 3], t);
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
/* LOAF BREAKER (FLEXOR, 11 Nov 2026): BEGINNER's SONG keeps each part's essentials: TRACKS the item's pattern (K2)
 * and its loops (K3); PLAY the track (K1); DROP the build's length (K1); BUILD its style (K1). The rest as it was set */
static int trk_knob_ok(int i)            /* (0.9.22, FLEXOR: BEGINNER's SONG whole, as EXPERT's: every knob, ALGORITHM
                                          * the items, PRESETS' TEMPO RISE) */
{
    (void)i;
    return 1;
}
static void track_pick(int32_t d);
static void track_turn(int i, int32_t d)
{
    track_t *t = trk_now();
    uint32_t c = ui.trk_cur % TRK_ITEMS;
    int dir = d > 0 ? 1 : -1;
    if (RST_ON) {                        /* RESET (GLO): K1 (or ALGORITHM) what; K2, K3 or K4 turned RST_TURNS clicks:
                                          * done (then 3 s of nothing: the clicks past the last don't count) */
        if (rst_cool())
            return;
        if (i == 3)                      /* (LOAF BREAKER, 0.239, FLEXOR: K4 what; K2 alone the clicks, then GLO asked) */
            reset_pick(d);
        else if (i == 1) {
            uint32_t n = (uint32_t)(d < 0 ? -d : d);
            if (rst_asking())            /* (asked already: another turn, no harm) */
                return;
            if (!rst_live())
                ui.rst_n = 0u;
            ui.rst_n = (uint8_t)(ui.rst_n + n < RST_TURNS ? ui.rst_n + n : RST_TURNS);
            ui.rst_arm = plat_ms() | 1u;
            if (ui.rst_n >= RST_TURNS) {
                ui.rst_n = 0u;
                ui.rst_arm = 0u;
                ui.rst_ask = plat_ms() | 1u;
                app_say(RST_NAME[ui.rst_sel % NRST], "PRESS GLO TO RESET");
            } else {
                char t[16] = "TURN: ";
                int_text((int32_t)ui.rst_n, t + 6);
                text_cat(t, " / 5");
                app_say(RST_NAME[ui.rst_sel % NRST], t);
            }
        }
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        return;
    }
    if (!trk_knob_ok(i)) {
        easy_no();                       /* (BEGINNER: not this one) */
        return;
    }
    if (ui.tpage == 4u && ui.bitem) {    /* BUILD's TEMPO RISE: K1, the tempo up with the build (LOAF BREAKER TEST) */
        if (i == 0) {
            set.drop_accel = (uint8_t)clamp((int32_t)set.drop_accel + (dir > 0 ? 2 : -2), 0, 30);
            changed();
            track_say(0);
        }
        return;
    }
    if (ui.tpage == 4u) {                /* BUILD */
        if (i == 0)
            set.drop_style = (uint8_t)clamp((int32_t)set.drop_style + dir, 0, DS_N - 1);
        else if (i == 1)
            set.drop_sweep = (uint8_t)clamp((int32_t)set.drop_sweep + dir, 0, 3);
        else if (i == 2)
            set.drop_riser = (uint8_t)clamp((int32_t)set.drop_riser + d * 5, 0, 100);
        else
            set.drop_gap = (uint8_t)clamp((int32_t)set.drop_gap + dir, 0, 4);
        changed();
        track_say(i);
        return;
    }
    if (ui.tpage == 2u) {
        if (i == 0)
            set.drop_bars = (uint8_t)clamp((int32_t)set.drop_bars + dir, 0, 4);
        else if (i == 1)
            set.drop_after = (uint8_t)clamp((int32_t)set.drop_after + dir, 0, (int32_t)NPAT_ALL);
        else if (i == 2)
            set.drop_brk = (uint8_t)clamp((int32_t)set.drop_brk + dir, 0, (int32_t)n_breaks);
        else
            set.drop_crash = (uint8_t)clamp((int32_t)set.drop_crash + d * 5, 0, 100);
        changed();
    } else if (ui.tpage == 1u) {
        if (i == 0) {
            set.trk = (uint8_t)clamp((int32_t)set.trk + dir, 0, (int32_t)NTRACKS - 1);
            changed();
        } else if (i == 1) {
            uint32_t n = trk_len(t);
            TRK_FROM = (uint8_t)clamp((int32_t)TRK_FROM + dir, 0, n ? (int32_t)n - 1 : 0);
            set_dirty = 1u;
        } else if (i == 2) {
            t->end = (uint8_t)clamp((int32_t)t->end + dir, 0, 2);
            set_dirty = 1u;
        } else
            return;
    } else {
        if (i == 0) {                    /* (AFFINE, controls phase 2: K1 the track, as PLAY's) */
            track_pick(d);
            return;
        } else if (i == 1) {               /* END, EMPTY, DROP, P1..P64 (LOAF BREAKER: DROP just left of P1, the 64
                                          * past; EMPTY left of it, a case emptied: gone with one more turn left, or
                                          * ENV held) */
            static uint32_t del_t;       /* (0.9.26, FLEXOR: an item deleted, K2 left does nothing until the knob has
                                          * rested 0.7 s, each turn meanwhile counted again: the next item,
                                          * moved into its place, not deleted too by a hard turn) */
            if (dir < 0 && del_t && plat_ms() - del_t < 700u) {
                del_t = plat_ms() | 1u;
                return;
            }
            int32_t v = t->it[c] == TI_DROP ? 2 : t->it[c] == TI_GAP ? 1 :
                        t->it[c] == TI_END ? 0 : (int32_t)t->it[c] + 2;
            v = clamp(v + (t->it[c] == TI_END && dir > 0 ? 3 : dir), 0, (int32_t)NPAT_ALL + 2);   /* (the END: P1 at once) */
            if (v == 0 && trk_len(t) > c + 1u) {   /* (an END inside: the items after it moved up, the item gone) */
                uint32_t k;
                for (k = c; k + 1u < TRK_ITEMS; k++) {
                    t->it[k] = t->it[k + 1u];
                    t->rep[k] = t->rep[k + 1u];
                    *trk_brk(k) = *trk_brk(k + 1u);
                }
                t->it[TRK_ITEMS - 1u] = TI_END;
                *trk_brk(TRK_ITEMS - 1u) = 0u;
                if (TRK_FROM > c)        /* (the first item: where it was) */
                    TRK_FROM--;
                app_say("ITEM", "DELETED");
                del_t = plat_ms() | 1u;
                set_dirty = 1u;
                ui.redraw |= RG_BLOCKS | RG_FOOT;
                return;
            }
            if (t->it[c] == TI_END && v && c > trk_len(t))
                return;
            if ((v == 2) != (t->it[c] == TI_DROP))
                *trk_brk(c) = 0u;        /* (a DROP's byte its STUTTER, a pattern's its break: neither carried over) */
            t->it[c] = v == 2 ? TI_DROP : v == 1 ? TI_GAP : v == 0 ? TI_END : (uint8_t)(v - 2);
            if (!t->rep[c])
                t->rep[c] = 1u;
        } else if (i == 2) {
            if (t->it[c] == TI_DROP) {   /* (0.9.26, FLEXOR: a DROP's K3 its STUTTER, right ON, left OFF) */
                *trk_brk(c) = (uint8_t)((*trk_brk(c) & ~1u) | (dir > 0 ? 1u : 0u));
                set_dirty = 1u;
                track_say(2);
                return;
            }
            if (t->it[c] == TI_END || t->it[c] == TI_GAP)
                return;
            t->rep[c] = (uint8_t)clamp((int32_t)t->rep[c] + dir, 1, 16);
        } else if (t->it[c] == TI_DROP) {   /* (0.9.26, FLEXOR: a DROP's K4, what its GAP keeps: NOTHING, BREAK,
                                          * DRUMS, ECHOES; the GAP's length BUILD's K4) */
            *trk_brk(c) = (uint8_t)((*trk_brk(c) & 1u) |
                                    (uint32_t)clamp((int32_t)(*trk_brk(c) >> 1 & 3u) + dir, 0, 3) << 1);
            set_dirty = 1u;
            if (!set.drop_gap) {
                app_say("NO GAP YET", "SONG > BUILD: K4 GAP");
                ui.redraw |= RG_BLOCKS | RG_FOOT;
                return;
            }
        } else {                         /* (LOAF BREAKER: K4 the item's break; the END on PLAY's K3) */
            if (!trk_is_pat(t, c))
                return;
            *trk_brk(c) = (uint8_t)clamp((int32_t)*trk_brk(c) + dir, 0, (int32_t)n_breaks);
        }
        set_dirty = 1u;
    }
    track_say(i);
}
/* TRACKS' K1: the track (PRESETS until controls phase 2) */
static void track_pick(int32_t d)
{
    set.trk = (uint8_t)clamp((int32_t)set.trk + (d > 0 ? 1 : -1), 0, (int32_t)NTRACKS - 1);
    ui.trk_cur = 0u;
    changed();
    say_int("TRACK", (int32_t)set.trk + 1, "");
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
/* LOAF BREAKER: TRACKS, SEL + PLAY: the item edited is where PLAY starts the track (again on it: from the first) */
static void track_from_here(void)
{
    const track_t *t = trk_now();
    uint32_t c = ui.trk_cur % TRK_ITEMS;
    if (c >= trk_len(t)) {
        app_say("START", "PAST THE END");
        return;
    }
    TRK_FROM = (uint8_t)(TRK_FROM == c ? 0u : c);
    set_dirty = 1u;
    say_int("START AT ITEM", (int32_t)TRK_FROM + 1, "");
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
/* a white key on ENV. TRACKS: that pattern put at the item edited (its loops kept), then the next item; PLAY: that
 * track played (keys 1-8) */
static void track_key(uint32_t k)
{
    if (ui.tpage == 2u) {                /* AFFINE (controls phase 2): DROP, a white key: the drop into that pattern
                                          * (of the group in), its build from the next bar (SEL + ENV until then, into
                                          * AFTER) */
        drop_start(set.pgrp % NGRP * NPAT + k);
        return;
    }
    if (ui.tpage == 1u) {
        if (k < NTRACKS) {
            set.trk = (uint8_t)k;
            changed();
            track_play(1);
        }
        return;
    }
    if (ui.tpage == 0u) {
        track_t *t = trk_now();
        uint32_t c = ui.trk_cur % TRK_ITEMS;
        char s[8], r[6];
        if (c > trk_len(t))
            c = trk_len(t);
        if (t->it[c] == TI_END || t->it[c] == TI_DROP || t->it[c] == TI_GAP || !t->rep[c])
            t->rep[c] = 1u;
        t->it[c] = (uint8_t)(k + 1u);
        trk_item_text(t, c, s, r);
        say_int("ITEM", (int32_t)c + 1, "");
        app_say(ui.msg[0], s);
        if (c + 1u < TRK_ITEMS)
            ui.trk_cur = (uint8_t)(c + 1u);
        set_dirty = 1u;
        ui.redraw |= RG_BLOCKS | RG_FOOT;
    }
}
/* ENV held on TRACKS: the items after the one edited one place right, a gap after it (the one edited stays where it
 * is); the gap edited, for a white key (a pattern) or K2 (a DROP; K2 left: the gap gone again). Played past if left.
 * ENV held on a gap: it goes, the items on its sides closed up */
static void track_gap(void)
{
    track_t *t = trk_now();
    uint32_t n = trk_len(t), c = ui.trk_cur % TRK_ITEMS, k;
    if (c < n && t->it[c] == TI_GAP) {   /* (on a space: it goes, the items on each side close up, the cursor on the
                                          * one before) */
        for (k = c; k + 1u < TRK_ITEMS; k++) {
            t->it[k] = t->it[k + 1u];
            t->rep[k] = t->rep[k + 1u];
            *trk_brk(k) = *trk_brk(k + 1u);
        }
        t->it[TRK_ITEMS - 1u] = TI_END;
        *trk_brk(TRK_ITEMS - 1u) = 0u;
        if (ui.trk_on && ui.trk_pos > c)
            ui.trk_pos--;
        if (ui.trk_drop != 0xFFu && ui.trk_drop > c)
            ui.trk_drop--;
        if (TRK_FROM > c)
            TRK_FROM--;
        ui.trk_cur = (uint8_t)(c ? c - 1u : 0u);
        set_dirty = 1u;
        app_say("SPACE", "REMOVED");
        ui.redraw |= RG_BLOCKS | RG_FOOT;
        return;
    }
    if (c >= n) {
        app_say("SPACE", "NOTHING AFTER");
        return;
    }
    if (n >= TRK_ITEMS) {
        app_say("SPACE", "TRACK FULL");
        return;
    }
    for (k = n; k > c + 1u; k--) {       /* (the END too, once there is room for it) */
        t->it[k] = t->it[k - 1u];
        t->rep[k] = t->rep[k - 1u];
        *trk_brk(k) = *trk_brk(k - 1u);
    }
    t->it[c + 1u] = TI_GAP;
    t->rep[c + 1u] = 0u;
    *trk_brk(c + 1u) = 0u;
    ui.trk_cur = (uint8_t)(c + 1u);
    if (ui.trk_on && ui.trk_pos > c && ui.trk_pos + 1u < TRK_ITEMS)
        ui.trk_pos++;                    /* (a track playing: on where it was) */
    if (ui.trk_drop != 0xFFu && ui.trk_drop > c && ui.trk_drop + 1u < TRK_ITEMS)
        ui.trk_drop++;
    if (TRK_FROM > c && TRK_FROM + 1u < TRK_ITEMS)
        TRK_FROM++;
    set_dirty = 1u;
    say_int("SPACE AT", (int32_t)c + 2, "");
    app_say(ui.msg[0], "KEY: A PATTERN");
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}
static uint32_t track_keys_lit(void)     /* TRACKS: the item's pattern; PLAY: the tracks there are; DROP: the patterns */
{
    uint32_t m = 0u, i;
    if (ui.tpage == 2u)                  /* (DROP: every key a pattern to drop into, the one playing lit) */
        return 1u << set.pattern % NPAT;
    if (ui.tpage == 1u) {
        for (i = 0; i < NTRACKS; i++)
            if (trk_len(&pats.trk[i]))
                m |= 1u << i;
        return m;
    }
    if (ui.tpage == 0u) {
        uint32_t v = trk_now()->it[ui.trk_cur % TRK_ITEMS];
        return v && v <= NPAT_ALL && (v - 1u) / NPAT == set.pgrp ? 1u << (v - 1u) % NPAT : 0u;
    }
    return 0u;
}

/* the picture, y 44..191 (148 rows, drawn in strips no text crosses: seq.c's pic_draw; AFFINE: every text at 2x,
 * pic_text): TRACKS the track's items in four rows of eight (a pattern's number "3" and its loops "X4", a DROP lit
 * amber, the END), the item edited outlined, the one playing lit; above them the track, its length and what it does
 * at its end. PLAY: a radio. DROP and BUILD: the drop drawn (drop_picture). GLO's RESET: its list */
#define TK_GY 22                         /* the items' grid begins */
#define TK_CH 31                         /* a row of items */
/* LOAF BREAKER: ENV's PLAY, a radio: the stations (the tracks 1-8) on a lit dial, the needle gliding to the one K1
 * tunes, a big knob turned to it, the station's name and what it plays, PRESS PLAY blinking (ON AIR while it plays) and
 * the speaker's grille (AFFINE, 2x: the knob's TUNING and the track's FROM and END, the foot's, gone) */
static void radio_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a filled circle */
{
    int32_t y;
    for (y = -r; y <= r; y++) {
        int32_t w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r)
            w++;
        cv_rect(cx - w, cy + y, 2 * w + 1, 1, c);
    }
}
static void radio_picture(void)
{
    const track_t *t = trk_now();
    uint32_t i, n = trk_len(t), now = plat_ms();
    int32_t x, k;
    float a;
    char s[24];
    /* the dial: a lit window, a scale, the stations */
    cv_rect(6, 2, 228, 44, C_BLOCK);
    cv_rect(10, 6, 220, 36, RGB(40, 26, 12));
    for (x = 0; x <= 56; x++) {          /* (the scale's ticks, the long ones the stations) */
        int32_t px = 20 + x * 200 / 56, lng = !(x % 8);
        cv_rect(px, lng ? 23 : 27, 1, lng ? 12 : 5, lng ? C_AMB : RGB(120, 80, 40));
    }
    for (i = 0; i < NTRACKS; i++) {
        int32_t px = 20 + (int32_t)i * 200 / 7;
        s[0] = (char)('1' + i);
        s[1] = 0;
        pic_text(px - pic_tw(s) / 2, 10, s, i == set.trk % NTRACKS ? C_WHITE : trk_len(&pats.trk[i]) ? C_AMB
                 : RGB(110, 70, 35));
    }
    x = 20 + (int32_t)(ui.radio * 200.0f / 7.0f + 0.5f);
    cv_rect(x - 1, 7, 3, 34, C_HI);      /* the needle */
    cv_rect(x, 7, 1, 34, C_WHITE);
    /* the knob: a dark ring, its face, a line to where it points (-135..+135 degrees over the eight) */
    radio_disc(42, 100, 30, C_BLOCK);
    radio_disc(42, 100, 25, RGB(70, 70, 78));
    radio_disc(42, 100, 21, RGB(110, 110, 120));
    a = (-0.375f + 0.75f * ui.radio / 7.0f);   /* (turns from straight up) */
    for (k = 6; k <= 20; k++)
        cv_rect(42 + (int32_t)((float)k * brk_sin_turns(a)), 100 - (int32_t)((float)k * brk_sin_turns(a + 0.25f)), 3,
                3, C_HI);
    for (k = 0; k < 9; k++) {            /* (its marks around) */
        float b = -0.375f + 0.75f * (float)k / 8.0f;
        cv_rect(42 + (int32_t)(34.0f * brk_sin_turns(b)), 100 - (int32_t)(34.0f * brk_sin_turns(b + 0.25f)), 2, 2,
                C_GRAY);
    }
    /* the station: its name, how long, and PRESS PLAY or ON AIR blinking */
    s[0] = 0;
    text_cat(s, "TRACK ");
    int_text((int32_t)set.trk + 1, s + 6);
    pic_text(86, 64, s, C_WHITE);
    s[0] = 0;                            /* (its length; its items: TRACKS') */
    int_text((int32_t)trk_bars(t), s);
    text_cat(s, trk_bars(t) == 1u ? " BAR" : " BARS");
    pic_text(86, 82, n ? s : "EMPTY TRACK", C_GRAY);
    if (ui.trk_on)                       /* ON AIR, or PRESS PLAY blinking */
        pic_text(86, 100, (now / 400u) & 1u ? "ON AIR" : "", C_HI);
    else if (n)
        pic_text(86, 100, (now / 500u) & 1u ? "PRESS PLAY" : "", C_HI);
    for (k = 0; k < 4; k++)              /* the speaker's grille */
        for (x = 0; x < 24; x++)
            cv_rect(88 + x * 6, 124 + k * 6, 2, 2, RGB(60, 60, 66));
}
/* AFFINE, 2x: DROP and BUILD, one picture (the owner: little text, more picture): the build's bars (BUILD's roll in
 * them, its sweep and its riser over them), its gap, the drop (its crash) and the pattern after it (the drop's break
 * under it); over it one line: how to drop (DROP), the tempo's rise to it (BUILD, PRESETS). Each knob of both pages
 * changes it */
#define DP_XD 150                        /* the drop */
#define DP_Y0 26                         /* the bars' top, */
#define DP_YS 84                         /* where the sweep's zone ends, the roll's begins */
#define DP_Y1 142                        /* their bottom */
static void drop_roll(int32_t x0, int32_t x1, uint32_t bars, int32_t y, int32_t h, int32_t hw, uint16_t c)
{                                        /* a roll over the build: 4 a bar, then 8, 16, 32 over its last quarters */
    uint32_t u = 0u, all = bars * 128u;  /* (in 128ths of a bar) */
    while (u < all) {
        int32_t x = x0 + (int32_t)(u * (uint32_t)(x1 - x0) / all);
        cv_rect(x, y, hw, h, c);
        u += 32u >> (u * 4u / all);
    }
}
static void drop_picture(int build)
{
    uint32_t bars = DROP_BARS[set.drop_bars % 5u], k;
    int32_t gap = (int32_t)DROP_GAP[set.drop_gap % 5u] * 3, x0 = 6, xe = DP_XD - 4 - gap, x, y;
    uint32_t riser = set.drop_riser > 100u ? 100u : set.drop_riser;
    char s[16];
    /* the line over it */
    if (build) {
        pic_text(6, 4, "TEMPO RISE", C_GRAY);   /* (AFFINE, controls phase 2: BUILD's item; PRESETS before) */
        s[0] = 0;
        if (!set.drop_accel)
            text_cat(s, "OFF");
        else {
            s[0] = '+';
            int_text(set.drop_accel, s + 1);
            text_cat(s, "%");
        }
        pic_text(6 + pic_tw("TEMPO RISE") + 8, 4, s, set.drop_accel ? C_HI : C_DIM);
    } else
        pic_text(6, 4, "WHITE KEY: THE DROP", C_GRAY);   /* (controls phase 2: SEL + ENV before) */
    /* the build's bars (none: the bar playing ends into the drop, dim) */
    for (k = 0; k < (bars ? bars : 1u); k++) {
        int32_t bx = x0 + (int32_t)k * (xe - x0) / (int32_t)(bars ? bars : 1u);
        int32_t bw = x0 + (int32_t)(k + 1u) * (xe - x0) / (int32_t)(bars ? bars : 1u) - bx - 2;
        cv_rect(bx, DP_Y0, bw, DP_Y1 - DP_Y0, bars ? C_BLOCK : C_LINE);
        if (bars && bw >= 24)            /* (its beats) */
            for (x = 1; x < 4; x++)
                cv_rect(bx + bw * x / 4, DP_YS + 2, 1, DP_Y1 - DP_YS - 4, C_LINE);
    }
    if (bars) {
        /* the riser: a wedge growing to the drop, as high as its amount */
        for (x = x0; x < xe; x++) {
            int32_t h = (x - x0) * (DP_YS - DP_Y0 - 6) * (int32_t)riser / ((xe - x0) * 100);
            if (h > 0)
                cv_rect(x, DP_YS - 2 - h, 1, h, (x & 1) ? C_DIM : C_LINE);
        }
        /* the sweep: the filter's cutoff, up (HIGH-PASS) or down (LOW-PASS) */
        if (set.drop_sweep < 2u)
            for (x = x0; x < xe; x += 2) {
                int32_t f = (x - x0) * 64 / (xe - x0), d = f * f * (DP_YS - DP_Y0 - 14) / 4096;
                cv_rect(x, set.drop_sweep ? DP_Y0 + 6 + d : DP_YS - 8 - d, 2, 2, C_AMB);
            }
        else if (set.drop_sweep == 3u)   /* (RANDOM: one or the other, each build) */
            pic_text(x0 + (xe - x0 - pic_tw("?")) / 2, (DP_Y0 + DP_YS - PIC_TH) / 2, "?", C_AMB);
        /* the roll: the snare's, the boom's (each a lane), or the break stuttered */
        y = DP_YS + 6;
        if (set.drop_style == DS_SNARE || set.drop_style == DS_BOTH)
            drop_roll(x0, xe, bars, y, 18, 2, C_HI);
        if (set.drop_style == DS_KICK || set.drop_style == DS_BOTH)
            drop_roll(x0, xe, bars, y + 26, 18, 3, C_WHITE);
        if (set.drop_style == DS_STUTTER) {   /* (the break's pieces, shorter and shorter) */
            uint32_t u = 0u, all = bars * 128u;
            while (u < all) {
                uint32_t len = 64u >> (u * 4u / all);
                int32_t a = x0 + (int32_t)(u * (uint32_t)(xe - x0) / all);
                int32_t b = x0 + (int32_t)((u + len) * (uint32_t)(xe - x0) / all);
                cv_rect(a, y, b - a > 2 ? b - a - 1 : 1, 44, (u / len) & 1u ? C_AMB : C_HI);
                u += len;
            }
        }
        if (set.drop_style == DS_RANDOM)
            pic_text(x0 + (xe - x0 - pic_tw("?")) / 2, (DP_YS + DP_Y1 - PIC_TH) / 2, "?", C_AMB);
    }
    if (gap > 0)                         /* the gap: silence before the drop */
        for (x = xe + 2; x < DP_XD - 3; x += 3)
            cv_rect(x, (DP_Y0 + DP_Y1) / 2, 1, 1, C_DIM);
    /* the drop: a line, the crash bursting from it (as long as its amount; behind the pattern after it) */
    cv_rect(DP_XD - 1, DP_Y0 - 4, 2, DP_Y1 - DP_Y0 + 8, C_HI);
    if (set.drop_crash) {
        static const int8_t RAY[8][2] = {{7, 0}, {5, 5}, {0, 7}, {-5, 5}, {-7, 0}, {-5, -5}, {0, -7}, {5, -5}};
        int32_t len = 6 + (int32_t)(set.drop_crash > 100u ? 100u : set.drop_crash) * 20 / 100, r;   /* (in 7ths) */
        for (k = 0; k < 8u; k++)
            for (r = 4; r <= len; r += 3)
                cv_rect(DP_XD - 1 + RAY[k][0] * r / 7, DP_YS + RAY[k][1] * r / 7, 2, 2, r > len - 3 ? C_HI : C_WHITE);
    }
    /* the pattern after it (NEXT: the one after the one playing), its break under it (none: the same); its first beat
     * lit */
    {
        int32_t ax = DP_XD + 4, aw = 234 - ax;
        cv_rect(ax, DP_Y0, aw, DP_Y1 - DP_Y0, C_BLOCK);
        s[0] = 0;
        if (!set.drop_after)
            text_cat(s, "NEXT");
        else {
            s[0] = 'P';
            int_text(set.drop_after, s + 1);
        }
        pic_text(ax + (aw - pic_tw(s)) / 2, 46, s, C_WHITE);
        if (set.drop_brk && set.drop_brk <= n_breaks) {
            brk_word(set.drop_brk - 1u, s);
            pic_text(ax + (aw - pic_tw(s)) / 2, 64, s, C_AMB);
        }
        for (k = 0; k < 4u; k++)
            cv_rect(ax + 6 + (int32_t)k * (aw - 12) / 4, DP_Y1 - 14, (aw - 12) / 4 - 3, 8, k ? C_LINE : C_HI);
    }
}
/* AFFINE, 2x: GLO's RESET (drawn here, its code ENV's): the list (K4), five at a time; beside it the five turns of K2
 * as a meter filling; under it what the one picked puts back, in a line, and what to do next */
static void reset_picture(void)
{
    uint32_t w = ui.rst_sel % NRST, i;
    uint32_t on = rst_live() ? ui.rst_n : 0u, done = rst_cool(), ask = rst_asking();
    uint32_t first = w < 2u ? 0u : w + 3u > NRST ? NRST - 5u : w - 2u;   /* (the one picked kept in view) */
    for (i = first; i < first + 5u && i < NRST; i++) {
        int32_t y = 2 + (int32_t)(i - first) * 19;
        cv_rect(6, y, 144, 17, i == w ? C_HI : C_BLOCK);
        pic_text(12, y + 4, RST_NAME[i], i == w ? C_BLACK : C_GRAY);
    }
    if (first)                           /* (more above, more below) */
        cv_rect(153, 2, 2, 6, C_DIM);
    if (first + 5u < NRST)
        cv_rect(153, 87, 2, 6, C_DIM);
    for (i = 0; i < RST_TURNS; i++) {    /* (the turns so far: a meter, each bar taller) */
        int32_t h = 18 + (int32_t)i * 18;
        cv_rect(162 + (int32_t)i * 15, 93 - h, 12, h, done || ask || i < on ? C_HI : C_BLOCK);
    }
    pic_text(6, 104, w == RST_WIPE && wiped ? "WIPED: NOW INSTALL" : RST_WHAT[w],
             w == RST_WIPE && wiped ? C_HI : C_GRAY);
    cv_rect(6, 120, 228, 26, on || done || ask ? C_HI : C_BLOCK);
    pic_text(12, 128, done ? "DONE (WAIT 3 S)" : ask ? "PRESS GLO TO RESET" : on ? "KEEP TURNING K2"
             : "TURN K2 x5, THEN GLO", on || done || ask ? C_BLACK : C_GRAY);
}
static void track_picture(int32_t top)
{
    const track_t *t = trk_now();
    uint32_t i, n = trk_len(t);
    char s[20], r[8];
    cv_oy = -top;
    if (RST_ON)                          /* RESET (GLO): the list (K4), what it puts back, the clicks */
        reset_picture();
    else if (ui.tpage == 4u || ui.tpage == 2u)   /* BUILD, DROP: the drop */
        drop_picture(ui.tpage == 4u);
    else if (ui.tpage == 1u)             /* PLAY: the radio */
        radio_picture();
    if (RST_ON || ui.tpage) {
        cv_oy = 0;
        return;
    }
    s[0] = 0;
    text_cat(s, "TRACK ");
    int_text((int32_t)set.trk + 1, s + 6);
    pic_text(6, 4, s, C_WHITE);
    s[0] = 0;                            /* (where PLAY starts it: the flag on its item) */
    int_text((int32_t)trk_bars(t), s);
    text_cat(s, " BARS  ");
    text_cat(s, TRK_END_NAME[t->end % 3u]);
    pic_text(234 - pic_tw(s), 4, s, C_GRAY);
    for (i = 0; i < TRK_ITEMS; i++) {
        int32_t x = 1 + (int32_t)(i % 8u) * 30, y = TK_GY + (int32_t)(i / 8u) * TK_CH;
        int mark = i == ui.trk_cur;
        int now = ui.trk_on && i == ui.trk_pos;
        uint16_t bg = now ? C_HI : i < n ? (t->it[i] == TI_DROP ? C_AMB : C_BLOCK) : C_BLACK;
        if (i > n)
            cv_rect(x + 10, y + 13, 8, 1, C_LINE);   /* (past the END: nothing) */
        else if (i < n && t->it[i] == TI_GAP) {   /* (a gap: its outline, a +) */
            cv_rect(x, y, 28, 1, C_LINE);
            cv_rect(x, y + TK_CH - 4, 28, 1, C_LINE);
            cv_rect(x, y, 1, TK_CH - 3, C_LINE);
            cv_rect(x + 27, y, 1, TK_CH - 3, C_LINE);
            cv_rect(x + 10, y + 12, 9, 1, C_GRAY);
            cv_rect(x + 14, y + 8, 1, 9, C_GRAY);
        } else
            cv_rect(x, y, 28, TK_CH - 3, bg);
        if (mark) {                      /* (the item edited: a white frame) */
            cv_rect(x - 1, y - 1, 30, 1, C_WHITE);
            cv_rect(x - 1, y + TK_CH - 3, 30, 1, C_WHITE);
            cv_rect(x - 1, y - 1, 1, TK_CH - 1, C_WHITE);
            cv_rect(x + 28, y - 1, 1, TK_CH - 1, C_WHITE);
        }
        if (i == TRK_FROM && i < n) {    /* (LOAF BREAKER: the first item PLAY plays: a flag at its corner) */
            uint16_t fc = now ? C_BLACK : C_WHITE;
            cv_rect(x + 1, y + 1, 1, 7, fc);
            cv_rect(x + 2, y + 2, 1, 5, fc);
            cv_rect(x + 3, y + 3, 1, 3, fc);
            cv_rect(x + 4, y + 4, 1, 1, fc);
        }
        if (i > n || (i < n && t->it[i] == TI_GAP))
            continue;
        trk_item_text(t, i, s, r);
        {                                /* (AFFINE, 2x: two lines in its 28 rows, 4 above, 2 between, 2 under) */
            uint16_t c = now || t->it[i] == TI_DROP ? C_BLACK : i == n ? C_DIM : C_WHITE;
            char kk[6] = "";
            if (t->it[i] == TI_DROP && *trk_brk(i) & 1u)
                text_cat(kk, "S");       /* (0.9.26: its STUTTER; what its GAP keeps: B, D, E) */
            if (t->it[i] == TI_DROP && *trk_brk(i) >> 1 & 3u) {
                uint32_t kn = text_n(kk);
                kk[kn] = "BDE"[(*trk_brk(i) >> 1 & 3u) - 1u];
                kk[kn + 1u] = 0;
            }
            if (t->it[i] == TI_DROP && kk[0]) {   /* (LOAF BREAKER: S its STUTTER under it, what its GAP keeps) */
                pic_text(x + (28 - pic_tw("DRP")) / 2, y + 4, "DRP", c);
                pic_text(x + (28 - pic_tw(kk)) / 2, y + 16, kk, c);
            } else if (t->it[i] == TI_DROP)
                pic_text(x + (28 - pic_tw("DRP")) / 2, y + 9, "DRP", c);
            else {                       /* (the pattern's number alone: "12", its P left off, two apart in a row) */
                const char *pn = s[0] == 'P' ? s + 1 : s;
                pic_text(x + (28 - pic_tw(pn)) / 2, y + 4, pn, c);
                if (r[0]) {              /* (its loops, its own break: "X4 B3", the first ones left out where they
                                          * don't fit) */
                    char b[4] = "B";
                    const char *part[3];
                    uint16_t col[3];
                    uint32_t np = 0u, q, a = 0u;
                    int32_t w = 0, x0;
                    part[np] = r;
                    col[np++] = now ? C_BLACK : C_GRAY;
                    if (*trk_brk(i) && *trk_brk(i) <= n_breaks) {
                        int_text((int32_t)*trk_brk(i), b + 1);
                        part[np] = b;
                        col[np++] = now ? C_BLACK : C_AMB;
                    }
                    for (;;) {           /* (from the first that fits on) */
                        w = 0;
                        for (q = a; q < np; q++)
                            w += pic_tw(part[q]) + (q > a ? 4 : 0);
                        if (w <= 28 || a + 1u >= np)
                            break;
                        a++;
                    }
                    x0 = x + (28 - w) / 2;
                    for (q = a; q < np; q++) {
                        pic_text(x0, y + 16, part[q], col[q]);
                        x0 += pic_tw(part[q]) + 4;
                    }
                }
            }
        }
    }
    cv_oy = 0;
}
static void draw_track(void)
{
    pic_draw(track_picture);             /* (in strips: no text cut between two) */
}
