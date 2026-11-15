/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: the PSX page, EFFECTS' fifth tab (SELECT past the slots: ui.fx_focus == BRK_FX_SLOTS), the PSX bus's settings
 * (psx.h; HOME's knobs keep the amounts: FILTER, CRUSH, REDUX, VERB). Four parts on ALGORITHM:
 *   VERB   TYPE (the nine), SIZE, TONE, DEPTH; the picture: the preset's own early echoes (its combs, from its
 *          registers) and its tail, the sound going in lit
 *   CODEC  MODE (OFF, GRIT, XA), RATE, BITS, MIX; the picture: a wave as the codec's steps hold it
 *   LOFI   CRUSH's MODE and MASK, REDUX's MODE and JITTER; the picture: the bits' staircase
 *   SKIP   LENGTH, CHANCE, SEEK, MIX; the picture: the disc, the slipping sector lit
 * Drawn whole in the POLYGON look (app/ui/aff.c), as HOME. Part of app.c's unity, after aff.c. */

#define FX_PSX() (ui.page == PG_FX && ui.fx_focus == BRK_FX_SLOTS)
enum { PP_VERB, PP_CODEC, PP_LOFI, PP_SKIP, PP_N };
static const char *const PP_NAME[PP_N] = {"VERB", "CODEC", "LOFI", "SKIP"};
static uint8_t pp_part;                  /* the part shown (not saved) */
static const char *const VERB_SHORT[PSXV_N] = {"ROOM", "STD S", "STD M", "STD L", "HALL", "HALF", "SPACE", "CHAOS",
                                               "DELAY"};   /* (the presets' names short: HOME's picture, this page) */
static const float GRIT_RATES[6] = {11025.0f, 16000.0f, 22050.0f, 32000.0f, 37800.0f, 44100.0f};

static const char *psxp_name(int i)
{
    static const char *const N[PP_N][4] = {{"TYPE", "SIZE", "TONE", "DEPTH"}, {"MODE", "RATE", "BITS", "MIX"},
                                           {"CRUSH", "MASK", "REDUX", "JITTER"}, {"LENGTH", "CHANCE", "SEEK", "MIX"}};
    return N[pp_part % PP_N][i & 3];
}

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
    case PP_VERB:
        if (i == 0)
            text_cat(t, VERB_SHORT[b->vtype % PSXV_N]);
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
    case PP_VERB: return i == 0 ? -1.0f : i == 1 ? b->vsize : i == 2 ? b->vtone : b->vdepth;
    case PP_CODEC: return i == 3 ? b->cmix : -1.0f;
    case PP_LOFI: return i == 1 ? b->cmask : i == 3 ? b->rjit : -1.0f;
    default: return i == 0 ? -1.0f : i == 1 ? b->skchance : i == 2 ? b->skseek : b->skmix;
    }
}

static void psxp_turn(int i, int32_t d)
{
    psx_bus_t *b = &set.psx;
    char t[16];
    int s = d > 0 ? 1 : -1;
    switch (pp_part % PP_N) {
    case PP_VERB:
        if (i == 0)
            b->vtype = (uint8_t)clamp((int32_t)b->vtype + s, 0, PSXV_N - 1);
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

static void psxp_part_turn(int32_t d)    /* ALGORITHM on the PSX page: its parts */
{
    pp_part = (uint8_t)((pp_part + (d > 0 ? 1u : PP_N - 1u)) % PP_N);
    app_say("PSX", PP_NAME[pp_part]);
}

/* ---------------------------------------------------------------------------------------------- the picture --- */
/* the stage: rows 44..186 between the header and the panel */
#define PP_TOP 44
#define PP_BOT 186
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
        aff_text(8, base + 4, "TAIL", 2, A_DIM, band, 0);
        aff_text(232, base + 4, t, 2, A_INK, band, 1);
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

static void psxp_draw(uint32_t now)
{
    int32_t top, i;
    char gv[4][12];
    for (top = 0; top < 240; top += 60) {
        cv_begin(240, 60, A_BG0);
        aff_check();
        /* the header: PSX and its parts, the one shown lit */
        if (top < 40) {
            int32_t x = 8;
            aff_text(8, 8, "PSX", 2, A_INK, top, 0);
            if (now < ui.msg_until) {
                aff_text(232, 8, ui.msg[1], 2, A_HOT, top, 1);
            } else
                aff_text(232, 8, "SPU", 2, A_DIM, top, 1);
            for (i = 0; i < PP_N; i++) {
                x = aff_text(x, 24, PP_NAME[i], 2, i == pp_part ? A_HOT : A_DIM, top, 0) + 10;
            }
        }
        switch (pp_part % PP_N) {
        case PP_VERB: pp_draw_verb(top, now); break;
        case PP_CODEC: pp_draw_codec(top, now); break;
        case PP_LOFI: pp_draw_lofi(top); break;
        default: pp_draw_skip(top, now); break;
        }
        if (top + 60 > AH_PANEL) {       /* the panel: the four knobs, as HOME's */
            cv_rect(0, AH_PANEL - top, 240, 1, A_DIM);
            for (i = 0; i < 4; i++) {
                int32_t gx = 8 + (i & 1) * 120, gy = 198 + (i >> 1) * 21;
                float g = psxp_norm(i);
                psxp_value(i, gv[i]);
                aff_text(gx, gy, psxp_name(i), 2, A_DIM, top, 0);
                aff_text(gx + 106, gy, gv[i], 2, A_INK, top, 1);
                if (g >= 0.0f)
                    home_bar(gx, gy + 13, 106, g, 0, top);
            }
        }
        cv_blit(0, (uint32_t)top);
    }
}
