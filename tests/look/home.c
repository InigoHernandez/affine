/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: HOME's picture (app/ui/aff.c) drawn off the device, band by band as the FM-1 draws it, into PPM frames:
 *   cc -std=c99 -O2 -Iapp -Iapp/dsp tests/look/home.c -lm -o build/look/home && build/look/home build/look */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "brk_math.h"
static uint16_t fb[240 * 240];
static int32_t g_band;                   /* the band being drawn: the canvas's row 0 is the screen's g_band */
static void cv_pset(int32_t x, int32_t y, uint16_t c)
{
    y += g_band;
    if (x >= 0 && x < 240 && y >= g_band && y < g_band + 60 && y < 240)
        fb[y * 240 + x] = c;
}
static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            cv_pset(i, j, c);
}
#include "ui/aff.c"
static void frame(const aff_home_t *h, const char *path)
{
    FILE *f;
    int32_t i;
    memset(fb, 0xAA, sizeof fb);         /* (a grey: a pixel left undrawn shows) */
    for (g_band = 0; g_band < 240; g_band += 60)
        aff_home_band(h, g_band);
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
    /* two bars at 175 BPM, 30 frames a second: the GIF's */
    for (k = 0; k < 82; k++) {
        float ms = (float)k * 1000.0f / 30.0f, step_ms = 60000.0f / 175.0f / 4.0f;
        uint32_t st = (uint32_t)(ms / step_ms);
        h.t_ms = 2000u + (uint32_t)ms;
        h.steps = st;
        h.phase = (ms - (float)st * step_ms) / step_ms;
        h.slice = SEQ[st % 16u];
        h.kick_ms = 2000u + (uint32_t)((float)st * step_ms);
        h.bar = 1u + st / 16u;
        h.beat = 1u + (st % 16u) / 4u;
        h.g[1] = 0.3f + 0.4f * brk_sin_turns(ms / 4000.0f);
        snprintf(path, sizeof path, "%s/anim%03d.ppm", out, k);
        frame(&h, path);
    }
    return 0;
}
