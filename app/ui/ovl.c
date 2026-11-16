/* SPDX-License-Identifier: MPL-2.0 */
/* CHOPPAH: the OVERLAY page (ARP, PRD B-52; LOAF BREAKER: gone 0.1xx-0.243 for FILL, back in 0.244, FLEXOR: "the screen
 * from before with all the FX"). Each white key an effect over everything that plays (dsp/ovl.c), on while held, any
 * together; with LATCH a press switches its effect on and off (and latched ones stay on on the other pages). K1 the
 * amount of the effect touched last, K2 the rate of the ones on the beat, K3 the sweeps' length, K4 LATCH. The ARP
 * black key shares the effect touched last (0.9.27, ovl_last), and the white keys of the effects on are lit (seq.c
 * keys_lit). (Part of app.c's unity: included there in order.) */

static uint32_t ovl_held, ovl_latched;   /* bit OV_*: held down on this page; switched on (LATCH) */
#define ovl_last (set.ovl_pad & 15u)     /* the effect touched last: K1 its amount (0.9.27, FLEXOR: the ARP key's own, one
                                          * effect for both: touched on the page, the key plays it; turned on the key's K1,
                                          * the page tunes it) */

static uint32_t ovl_pad_bits(void)       /* the ARP keys' effects on (0.9.45: each key its own) */
{
    uint32_t j, m = 0u;
    for (j = 0; j < 11u; j++)
        if (arpk_on(j))
            m |= 1u << arpk_fx(j);
    if (arp_rec && !(set.ovl_pad & 32u)) /* (AFFINE: an ARP key recorded in the pattern) */
        m |= 1u << arpk_fx(arpk_focus());
    return m;
}
static void ovl_push(void)
{
    ovl_set(ovl_held | ovl_latched | ovl_pad_bits(), set.ovl_amt, set.ovl_rate, set.ovl_sweep);
}
static void ovl_pad_key(uint32_t j, int down)   /* GLOUPI BREAKS: the ARP black key j (0..10) down or up */
{
    uint16_t b = (uint16_t)(1u << (j % 11u));
    if (down) {                          /* (0.9.45: the key touched last: its knobs; the OVERLAY page's K1 its effect) */
        arp_foc = (uint8_t)(j % 11u);
        set.ovl_pad = (uint8_t)((set.ovl_pad & ~15u) | arpk_fx(j));
    }
    if (arpk_latch(j)) {                 /* LOAF BREAKER: K4 LATCH on: each press turns it on, or off */
        if (down)
            arp_lat ^= b;
        arp_held &= (uint16_t)~b;
    } else {                             /* (off: on while held) */
        arp_held = (uint16_t)(down ? arp_held | b : arp_held & ~b);
        arp_lat &= (uint16_t)~b;
    }
    ovl_push();
    ui.redraw |= RG_BLOCKS | RG_FOOT;
}

static void ovl_keys(uint32_t keys, uint32_t kdn)   /* every input: the white keys on the page, then the engine */
{
    uint32_t i, held = 0u;
    if (ui.page == PG_OVL)               /* (ARP EFFECTS: the keys) */
        for (i = 0; i < 16u; i++) {
            if (kdn >> WK[i] & 1u) {
                arpk_set_fx(arpk_focus(), i);   /* (0.9.45: the ARP key touched last plays it) */
                set.ovl_pad = (uint8_t)((set.ovl_pad & ~15u) | i);
                set_dirty = 1u;
                tut_ovl++;               /* (LOAF BREAKER: the TUTORIAL's count) */
                if (set.ovl_latch)
                    ovl_latched ^= 1u << i;
                app_say(OVL_NAME[i], set.ovl_latch ? (ovl_latched >> i & 1u ? "ON" : "OFF") : "");
                ui.redraw |= RG_BLOCKS | RG_FOOT;   /* (K1: its amount) */
            }
            if (!set.ovl_latch && keys >> WK[i] & 1u)
                held |= 1u << i;
        }
    if (held != ovl_held)
        ui.redraw |= RG_BLOCKS;
    ovl_held = held;
    ovl_push();
}
/* (LOAF BREAKER's BLACK KEYS tab, the layout edited on ARP: gone with CUSTOM, AFFINE 16 Nov 2026, the owner: the rows
 * AFFINE and PUNCH, FX + OCT- and OCT+) */
static void bk_release_all(void)         /* (the layout changes: whatever a key had on lets go) */
{
    uint32_t i, keys = plat_keys();
    for (i = 0; i < BRK_NPADS; i++)
        brk_pad((int)i, 0);
    for (i = 0; i < 11u; i++)            /* (AFFINE: a key held through it: silent until let go) */
        if (keys >> BK[i] & 1u)
            bk_mute |= (uint16_t)(1u << i);
    kil_mom = 0u;
    roll_held = 0xFFu;
    roll_sel = 0u;
    kroll = 0u;
    half_lat = 0u;
    pad_latched = 0u;
    arp_held = arp_lat = 0u;
    ovl_push();
    kick_hold = 0u;
    ui.pad_focus = -1;
    ui.kil_focus = 0u;
}
static void ovl_value(int i, char *t)
{
    t[0] = 0;
    if (i == 0) {
        int_text(set.ovl_amt[ovl_last & 15u], t);
        text_cat(t, " %");
    } else if (i == 1)
        text_cat(t, OVL_RATE_NAME[set.ovl_rate & 3u]);
    else if (i == 2)
        text_cat(t, OVL_SWEEP_NAME[set.ovl_sweep & 3u]);
    else
        text_cat(t, set.ovl_latch ? "ON" : "OFF");
}
static void ovl_turn(int i, int32_t d)
{
    char t[16];
    if (i == 0) {
        uint8_t *a = &set.ovl_amt[ovl_last & 15u];
        *a = (uint8_t)clamp((int32_t)*a + d * 2, 0, 100);
    } else if (i == 1)
        set.ovl_rate = (uint8_t)clamp((int32_t)set.ovl_rate + (d > 0 ? 1 : -1), 0, 3);
    else if (i == 2)
        set.ovl_sweep = (uint8_t)clamp((int32_t)set.ovl_sweep + (d > 0 ? 1 : -1), 0, 3);
    else {
        set.ovl_latch = d > 0 ? 1u : 0u;
        if (!set.ovl_latch)
            ovl_latched = 0u;            /* (back to held: what was latched lets go) */
    }
    changed();
    ovl_push();
    ovl_value(i, t);
    app_say(i == 0 ? OVL_NAME[ovl_last & 15u] : i == 1 ? "RATE" : i == 2 ? "SWEEP" : "LATCH", t);
    ui.redraw |= RG_BLOCKS;
}

/* the OVERLAY page's strip: the sixteen effects in key order, a row each (2x): 1-8 on the left, 9-16 on the right,
 * its key, its name, a bar of its amount; one sounding lit, one latched outlined amber, the one K1 tunes outlined
 * white. (AFFINE, 2x: LOAF's four by four cards held no 2x name beside its key) */
static void draw_ovl(void)
{
    uint32_t band, i, on = ovl_sounding() | ovl_held | ovl_latched | ovl_pad_bits();
    char t[4];
    for (band = 0; band < 2u; band++) {  /* (rows 88..139: keys 1-4 and 9-12; 140..191: 5-8 and 13-16) */
        cv_begin(240, 52, C_BLACK);
        for (i = 0; i < 16u; i++) {
            int32_t x = 1 + (int32_t)(i / 8u) * 120, y = (int32_t)(i % 8u) * 13 - (int32_t)band * 52;
            int lit = on >> i & 1u, lat = ovl_latched >> i & 1u, sel = i == ovl_last, ol = !lit && (lat || sel);
            if (y < 0 || y >= 52)
                continue;
            if (ol)
                cv_rect(x, y, 118, 12, lat ? C_AMB : C_WHITE);
            cv_rect(x + ol, y + ol, 118 - 2 * ol, 12 - 2 * ol, lit ? C_HI : C_BLOCK);
            t[0] = 0;
            int_text((int32_t)i + 1, t);
            snd_text(x + 21, y + 1, t, 2, lit ? C_BLACK : C_DIM, 1, 0);                        /* its key */
            snd_text(x + 27, y + 1, OVL_NAME[i], 2, lit ? C_BLACK : sel ? C_HI : C_GRAY, 0, 0);
            cv_rect(x + 94, y + 4, 20, 4, lit ? C_BLOCK_HI : C_LINE);                         /* its amount */
            cv_rect(x + 94, y + 4, (int32_t)set.ovl_amt[i] * 20 / 100, 4, lit ? C_BLACK : lat ? C_AMB : C_GRAY);
        }
        cv_blit(0, 88u + band * 52u);
    }
}
