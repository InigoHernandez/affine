/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: HOME's picture (app/ui/aff.c) drawn off the device, band by band as the FM-1 draws it, into PPM frames:
 *   cc -std=c99 -O2 -Iapp -Iapp/dsp tests/look/home.c -lm -o build/look/home && build/look/home build/look */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "brk_math.h"
#include "brk.h"
static uint16_t fb[240 * 240];
/* fm1-base's canvas (device/gfx.c): 240 x 60, byte-swapped pixels, clipped to the band; each band copied into fb */
static uint16_t cv_px[240 * 60];
static int32_t cv_oy, cv_y0, cv_y1 = 60;
static uint32_t cv_w = 240u;
static int32_t g_band;                   /* the band being drawn: the canvas's row 0 is the screen's g_band */
static inline uint16_t swap16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
static void cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i;
    (void)w; (void)h;
    for (i = 0; i < 240u * 60u; i++)
        cv_px[i] = swap16(bg);
}
static void cv_blit(uint32_t x, uint32_t y)
{
    uint32_t i;
    (void)x;
    for (i = 0; i < 240u * 60u && y * 240u + i < 240u * 240u; i++)
        fb[y * 240u + i] = swap16(cv_px[i]);
}
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
typedef struct { uint8_t h; } felucca_font_t;   /* (fm1-base's faces, their line heights alone: affont.c's wrappers) */
static const felucca_font_t FONT_T = {12}, FONT_XS = {15}, FONT_B = {16};
#include "ui/affont.c"
#include "ui/aff.c"
static void frame(const aff_home_t *h, const char *path)
{
    FILE *f;
    int32_t i;
    memset(fb, 0xAA, sizeof fb);
    aff_tick(h);
    for (g_band = 0; g_band < 240; g_band += 60) {
        cv_begin(240, 60, A_BG0);
        aff_home_band(h, g_band);
        cv_blit(0, (uint32_t)g_band);
    }
    f = fopen(path, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void splash_frame(uint32_t el, const char *path)
{
    FILE *f;
    int32_t i;
    memset(fb, 0xAA, sizeof fb);
    for (g_band = 0; g_band < 240; g_band += 60) {
        cv_begin(240, 60, A_BG0);
        aff_title_band(g_band, 50000u + el, el, "DEV", "JUNGLE 175", "PRESS PLAY", "LOAF BREAKER + CHOPPAH");
        cv_blit(0, (uint32_t)g_band);
    }
    f = fopen(path, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    char path[512];
    aff_home_t h;
    int k;
    static const int32_t SEQ[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 6, 14, 15};
    memset(&h, 0, sizeof h);
    h.brk = "JUNGLE 175";
    h.bpm_x10 = 1750;
    h.pat = 0;
    h.playing = 1;
    h.g[0] = -0.45f; h.gvp[0] = "1.2K"; h.gl[0] = "FILTER";
    h.g[1] = 0.62f; h.gvp[1] = "6 BIT"; h.gl[1] = "CRUSH";
    h.g[2] = 0.48f; h.gvp[2] = "11K"; h.gl[2] = "REDUX";
    h.g[3] = 0.71f; h.gvp[3] = "SPACE"; h.gl[3] = "VERB";
    h.bip = 1u;
    /* the still: slice 7 playing */
    h.slice = 6; h.steps = 6; h.phase = 0.3f; h.bar = 3; h.beat = 2; h.t_ms = 2300; h.kick_ms = 0;
    snprintf(path, sizeof path, "%s/home.ppm", out);
    frame(&h, path);
    /* a pad held */
    h.pad = "SKIP";
    snprintf(path, sizeof path, "%s/home_pad.ppm", out);
    frame(&h, path);
    h.pad = 0;
    /* a tour, 25 frames a second: stopped (idle), playing (the pulse following a break's hits), then each effect's
     * picture in turn, named in the header */
    {
        static const struct { float t; const char *name; } SCN[] = {
            {0.0f, "STOPPED"}, {2.5f, "PLAYING"}, {6.5f, "CRUSH"}, {8.5f, "REDUX"}, {10.5f, "VERB"}, {12.5f, "SKIP"},
            {14.0f, "FILTER LP"}, {15.5f, "ROLL 1/16"}, {17.0f, "FREEZE"}, {18.5f, "REVERSE"}, {20.0f, "VINYL"},
            {21.5f, "PLAYING"}, {23.0f, ""}};
        static const float HIT[16] = {1.0f, 0.3f, 0.6f, 0.25f, 0.95f, 0.3f, 0.5f, 0.35f, 0.9f, 0.3f, 1.0f, 0.4f, 0.95f,
                                      0.3f, 0.6f, 0.5f};
        float t;
        int fr = 0;
        uint32_t sc = 0;
        memset(&h, 0, sizeof h);
        h.brk = "JUNGLE 175";
        h.bpm_x10 = 1750;
        h.gl[0] = "FILTER"; h.gl[1] = "CRUSH"; h.gl[2] = "REDUX"; h.gl[3] = "VERB";
        h.bip = 1u;
        h.tape = 1.0f;
        h.gate = 1.0f;
        for (t = 0.0f; t < 23.0f; t += 0.04f, fr++) {
            float step_s = 60.0f / 175.0f / 4.0f, ps;
            uint32_t st;
            while (sc + 1u < sizeof SCN / sizeof SCN[0] && t >= SCN[sc + 1u].t)
                sc++;
            h.t_ms = 1000u + (uint32_t)(t * 1000.0f);
            h.playing = sc >= 1u;
            h.crush = sc == 2u ? 0.9f : 0.0f;
            h.redux = sc == 3u ? 0.7f : 0.0f;
            h.send = sc == 4u ? 0.9f : 0.0f;
            h.skip = sc == 5u && ((int)(t * 6.0f) % 3 != 0);
            h.dj = sc == 6u ? -0.8f : 0.0f;
            h.pads = sc == 7u ? 1u << PAD_ROLL16 : sc == 8u ? 1u << PAD_FREEZE : sc == 9u ? 1u << PAD_REVERSE :
                     sc == 10u ? 1u << PAD_TAPESTOP : 0u;
            h.roll_div = sc == 7u ? 1u : 0u;
            h.tape = sc == 10u ? 1.0f - (t - SCN[10].t) / 1.5f : 1.0f;
            if (h.tape < 0.0f) h.tape = 0.0f;
            ps = h.playing ? (t - SCN[1].t) / step_s : 0.0f;
            st = (uint32_t)ps;
            h.slice = !h.playing ? -1 : (sc == 8u || sc == 7u) ? h.slice : (int32_t)(SEQ[st % 16u]);
            h.level = h.playing ? HIT[st % 16u] * (0.25f + 0.75f * expf(-(ps - (float)st) * 3.0f)) * 0.8f : 0.0f;
            h.bar = 1u + st / 16u;
            h.beat = 1u + (st % 16u) / 4u;
            h.msg0 = SCN[sc].name[0] ? SCN[sc].name : 0;
            h.msg1 = 0;
            h.g[1] = h.crush; h.gvp[1] = h.crush > 0.0f ? "3 BIT" : "OFF";
            h.g[2] = h.redux; h.gvp[2] = h.redux > 0.0f ? "1K" : "OFF";
            h.g[3] = h.send; h.gvp[3] = h.send > 0.0f ? "SPACE" : "OFF";
            h.g[0] = h.dj; h.gvp[0] = h.dj < 0.0f ? "400" : "OFF";
            snprintf(path, sizeof path, "%s/tour%04d.ppm", out, fr);
            frame(&h, path);
        }
    }
    {                                    /* the splash, 3.5 s at 25 frames a second */
        uint32_t el;
        int k = 0;
        an.ms = 0u;
        for (el = 0; el <= 3500u; el += 40u, k++) {
            snprintf(path, sizeof path, "%s/splash%03d.ppm", out, k);
            splash_frame(el, path);
        }
    }
    return 0;
}
