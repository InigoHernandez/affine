/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: what HOME's picture (app/ui/aff.c) costs to draw, counted as MONO2K counts its screens: instructions a frame
 * on this Mac, built with the FM-1's flags, read from the kernel (proc_pid_rusage ri_instructions). The canvas is
 * fm1-base's (device/gfx.c: 240 x 60, byte-swapped pixels, clipped to the band). The yardstick: one FM-1 core is about
 * Felucca's heaviest patch, ~1570 Mac instructions a sample, 69 M a second; HOME draws up to 30 frames a second.
 *   cc -std=c99 -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -Iapp -Iapp/dsp tests/look/bench.c -lm \
 *      -o build/look/bench && build/look/bench */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#include "brk_math.h"
#include "brk.h"
static uint16_t cv_px[240 * 60];
static int32_t cv_oy, cv_y0, cv_y1 = 60;
static uint32_t cv_w = 240u;
static inline uint16_t swap16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
static void cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i;
    (void)w; (void)h;
    for (i = 0; i < 240u * 60u; i++)
        cv_px[i] = swap16(bg);
}
static void cv_blit(uint32_t x, uint32_t y) { (void)x; (void)y; }
static inline void cv_pset(int32_t x, int32_t y, uint16_t c)
{
    y += cv_oy;
    if ((uint32_t)x < 240u && y >= cv_y0 && y < cv_y1)
        cv_px[(uint32_t)y * 240u + (uint32_t)x] = swap16(c);
}
static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h + cv_oy, i;
    uint16_t sc = swap16(c);
    y += cv_oy;
    if (x < 0) x = 0;
    if (y < cv_y0) y = cv_y0;
    if (x1 > 240) x1 = 240;
    if (y1 > cv_y1) y1 = cv_y1;
    for (; y < y1; y++)
        for (i = x; i < x1; i++)
            cv_px[(uint32_t)y * 240u + (uint32_t)i] = sc;
}
typedef struct { uint8_t h; } felucca_font_t;
static const felucca_font_t FONT_T = {12}, FONT_XS = {15}, FONT_B = {16};
#include "ui/affont.c"
#include "ui/aff.c"

static uint64_t instr(void)
{
    struct rusage_info_v4 ri;
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
    return ri.ri_instructions;
}
static volatile uint32_t sink;
static uint64_t frame_cost(aff_home_t *h)   /* (50 frames between two reads of the counter: a read costs ~20 k) */
{
    uint64_t best = ~0ull;
    int r, n;
    for (r = 0; r < 3; r++) {
        uint64_t a, b;
        a = instr();
        for (n = 0; n < 50; n++) {
            int32_t t;
            h->t_ms += 33u;
            aff_tick(h);
            for (t = 0; t < 240; t += 60) {
                cv_begin(240, 60, A_BG0);
                aff_home_band(h, t);
                cv_blit(0, (uint32_t)t);
                sink += cv_px[(n * 97 + t) % (240 * 60)];
            }
        }
        b = instr();
        if ((b - a) / 50u < best)
            best = (b - a) / 50u;
    }
    return best;
}
static uint64_t title_cost(uint32_t el)
{
    uint64_t a = instr(), b;
    int n;
    for (n = 0; n < 20; n++) {
        aff_title(4000u + el, el, "0.1", "OLDSKOO 160", "PRESS PLAY", "LOAF BREAKER + CHOPPAH");
        sink += cv_px[n * 31];
    }
    b = instr();
    return (b - a) / 20u;
}

int main(void)
{
    static const struct { const char *name; float crush, redux, send, dj; uint32_t pads; uint8_t roll, skip; } S[] = {
        {"PLAYING", 0, 0, 0, 0, 0, 0, 0}, {"CRUSH", 0.9f, 0, 0, 0, 0, 0, 0}, {"REDUX", 0, 0.7f, 0, 0, 0, 0, 0},
        {"VERB", 0, 0, 0.9f, 0, 0, 0, 0}, {"SKIP", 0, 0, 0, 0, 0, 0, 1}, {"FILTER", 0, 0, 0, -0.8f, 0, 0, 0},
        {"ROLL 1/16", 0, 0, 0, 0, 1u << PAD_ROLL16, 1, 0}, {"FREEZE", 0, 0, 0, 0, 1u << PAD_FREEZE, 0, 0}};
    aff_home_t h;
    uint64_t base = 0, c;
    uint32_t i;
    {   /* (the counter's own cost) */
        uint64_t a = instr(), b = instr();
        base = b - a;
    }
    memset(&h, 0, sizeof h);
    h.pat_next = -1;
    h.brk = "OLDSKOO 160"; h.bpm_x10 = 1700; h.playing = 1; h.tape = 1.0f; h.gate = 1.0f; h.t_ms = 1000u;
    h.gl[0] = "FILTER"; h.gl[1] = "CRUSH"; h.gl[2] = "REDUX"; h.gl[3] = "VERB";
    h.gvp[0] = h.gvp[1] = h.gvp[2] = h.gvp[3] = "OFF";
    h.g[0] = h.g[1] = h.g[2] = h.g[3] = 0.0f;
    h.bip = 1u;
    printf("HOME, a frame (Mac instructions with the FM-1's flags; one FM-1 core ~69 M a second)\n");
    for (i = 0; i < sizeof S / sizeof S[0]; i++) {
        h.crush = S[i].crush; h.redux = S[i].redux; h.send = S[i].send; h.dj = S[i].dj; h.pads = S[i].pads;
        h.roll_div = S[i].roll; h.skip = S[i].skip; h.slice = (int32_t)(i * 3u % 16u); h.level = 0.6f;
        c = frame_cost(&h);
        printf("  %-10s %8.2f M   at 30 fps: %5.1f %% of a core\n", S[i].name, (double)c / 1e6, (double)c * 30.0 / 69e6 * 100.0);
    }
    {   /* the parts of a frame, PLAYING (50 frames each) */
        int32_t t, n, y, i2;
        uint64_t a;
#define PART(name, body) a = instr(); for (n = 0; n < 50; n++) for (t = 0; t < 240; t += 60) { body; sink += cv_px[(n * 13 + t) % 14400]; } \
        printf("    %-12s %7.1f k\n", name, (double)(instr() - a) / 50.0 / 1000.0);
        memset(&h, 0, sizeof h);
        h.pat_next = -1;
        h.brk = "OLDSKOO 160"; h.bpm_x10 = 1700; h.playing = 1; h.tape = 1.0f; h.gate = 1.0f; h.t_ms = 1000u; h.slice = 5;
        h.gl[0] = "FILTER"; h.gl[1] = "CRUSH"; h.gl[2] = "REDUX"; h.gl[3] = "VERB";
        h.gvp[0] = h.gvp[1] = h.gvp[2] = h.gvp[3] = "OFF"; h.bip = 1u;
        aff_tick(&h);
        printf("  parts of a frame:\n");
        PART("cv_begin", cv_begin(240, 60, A_BG0))
        PART("sky", for (y = t; y < t + 60 && y < AH_HORIZ; y++) { a_pat_t p; a_pat(&p, &ar_sky, y * 256 / AH_HORIZ, y, 0, 5); a_span(0, 240, y - t, &p, 1, 0); })
        PART("floor", home_floor(&h, t))
        PART("gem", home_gem(&h, t))
        PART("cells", for (i2 = 0; i2 < 4; i2++) aff_knob_cell(8 + (i2 & 1) * 120, 198 + (i2 >> 1) * 21, 106, h.gl[i2], h.gvp[i2], h.g[i2], i2 == 0, A_DIM, t))
        PART("header", { aff_text(8, 8, "OLDSKOO 160", 2, A_INK, t, 0); aff_text(232, 8, "170", 2, A_INK, t, 1); aff_text(8, 24, "A01", 2, A_DIM, t, 0); aff_text(120, 24, "SLICE 06", 2, A_HOT, t, 2); aff_text(232, 24, "3.4", 2, A_DIM, t, 1); })
        PART("whole band", aff_home_band(&h, t))
    }
    c = title_cost(600u);
    printf("  %-10s %8.2f M\n", "TITLE 0.6s", (double)c / 1e6);
    c = title_cost(3000u);
    printf("  %-10s %8.2f M\n", "TITLE 3s", (double)c / 1e6);
    return (int)(sink & 0u);
}
