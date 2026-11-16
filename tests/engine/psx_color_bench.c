/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: what the colour stage (GRIT's path, app/dsp/psx.c) costs, counted as MONO2K counts: Mac instructions a sample
 * (stereo, 44.1 kHz), built with the FM-1's flags, read from the kernel (proc_pid_rusage ri_instructions). The
 * yardstick: one FM-1 core is about 69 M Mac instructions a second, ~1570 a sample for the whole render; JieLi code
 * runs 0.9-1.5x the Mac's count. The input: noise and a chord at -10 dBFS (the encoder costs the same on anything).
 *   cc -std=c99 -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -Iapp/dsp tests/engine/psx_color_bench.c \
 *      app/dsp/psx.c -lm -o build/engine/psx_color_bench && build/engine/psx_color_bench
 * (-DPSX_OLD: only what an older psx.c has, GRIT's psx_grit_set and XA: the "before" column) */
#include <libproc.h>
#include <math.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include "psx.h"
#define N 256u
#define BLOCKS 1723u                     /* 10 s */
static float l[N], r[N];
static uint32_t rnd = 1u, t0;
static uint64_t instr(void)
{
    struct rusage_info_v4 ri;
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
    return ri.ri_instructions;
}
static void fill(void)
{
    uint32_t i;
    for (i = 0; i < N; i++, t0++) {
        float t = (float)t0 * (1.0f / 44100.0f);
        rnd = rnd * 1664525u + 1013904223u;
        l[i] = 0.1f * sinf(6.2831853f * 220.0f * t) + 0.1f * sinf(6.2831853f * 329.6f * t) +
               (float)(int32_t)rnd * (0.08f / 2147483648.0f);
        r[i] = l[i] * 0.9f;
    }
}
static double fill_cost;
typedef void (*setup_fn)(void);
static double per_sample(setup_fn setup, int xa, float mix)
{
    uint32_t b;
    uint64_t a;
    psx_init();
    setup();
    for (b = 0; b < 40u; b++) { fill(); if (xa) psx_xa_process(l, r, N, mix); else psx_grit_process(l, r, N, mix); }
    a = instr();
    for (b = 0; b < BLOCKS; b++) { fill(); if (xa) psx_xa_process(l, r, N, mix); else psx_grit_process(l, r, N, mix); }
    return (double)(instr() - a) / (BLOCKS * N) - fill_cost;
}
static void row(const char *name, setup_fn s, int xa, float mix)
{
    double c = per_sample(s, xa, mix);
    printf("  %-44s %6.0f   %4.1f %% of a core\n", name, c, 100.0 * c / 1570.0);
}
static void g22(void) { psx_grit_set(22050.0f, 4u, 0u); }
static void g11(void) { psx_grit_set(11025.0f, 4u, 0u); }
static void g44(void) { psx_grit_set(44100.0f, 4u, 0u); }
static void g32(void) { psx_grit_set(32000.0f, 4u, 0u); }
static void xa37(void) { psx_xa_set(0u, 0u, 1u); }
#ifndef PSX_OLD
static void r44f(void) { psx_color_set(PSXC_RATE_44K, 0u, 0u, 0.0f, 1.0f); }
static void r22f(void) { psx_color_set(PSXC_RATE_22K, 0u, 0u, 0.0f, 1.0f); }
static void r11f(void) { psx_color_set(PSXC_RATE_11K, 0u, 0u, 0.0f, 1.0f); }
static void r8f(void) { psx_color_set(PSXC_RATE_8K, 0u, 0u, 0.0f, 1.0f); }
static void r22r(void) { psx_color_set(PSXC_RATE_22K, 1u, 0u, 0.0f, 1.0f); }
static void r11r(void) { psx_color_set(PSXC_RATE_11K, 1u, 0u, 0.0f, 1.0f); }
static void r8r(void) { psx_color_set(PSXC_RATE_8K, 1u, 0u, 0.0f, 1.0f); }
static void a44(void) { psx_color_set(PSXC_RATE_44K, 0u, 1u, 0.0f, 1.0f); }
static void a22(void) { psx_color_set(PSXC_RATE_22K, 0u, 1u, 0.0f, 1.0f); }
static void a11(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.0f, 1.0f); }
static void a8(void) { psx_color_set(PSXC_RATE_8K, 0u, 1u, 0.0f, 1.0f); }
static void a11c5(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.5f, 1.0f); }
static void a11c10(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 1.0f, 1.0f); }
static void ps1(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.25f, 1.0f); }
static void s0(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.0f, 0.0f); }
static void s50(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.0f, 0.5f); }
static void s75(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.0f, 0.75f); }
static void s25(void) { psx_color_set(PSXC_RATE_11K, 0u, 1u, 0.0f, 0.25f); }
#endif
static void none(void) { }

int main(void)
{
    uint32_t b;
    uint64_t a = instr();
    for (b = 0; b < BLOCKS; b++) fill();
    fill_cost = (double)(instr() - a) / (BLOCKS * N);
    (void)none;
    printf("Mac instructions a sample (stereo), the FM-1's flags; a core ~1570 a sample\n");
    row("GRIT 22.05 kHz (the default)", g22, 0, 1.0f);
    row("GRIT 11.025 kHz", g11, 0, 1.0f);
    row("GRIT 44.1 kHz (its heaviest)", g44, 0, 1.0f);
    row("GRIT 32 kHz", g32, 0, 1.0f);
    row("XA 37.8 kHz stereo (unchanged)", xa37, 1, 1.0f);
#ifndef PSX_OLD
    row("RATE 44.1 kHz FILTERED, ADPCM off", r44f, 0, 1.0f);
    row("RATE 22.05 kHz FILTERED, ADPCM off", r22f, 0, 1.0f);
    row("RATE 11.025 kHz FILTERED, ADPCM off", r11f, 0, 1.0f);
    row("RATE 8 kHz FILTERED, ADPCM off", r8f, 0, 1.0f);
    row("RATE 22.05 kHz RAW, ADPCM off", r22r, 0, 1.0f);
    row("RATE 11.025 kHz RAW, ADPCM off", r11r, 0, 1.0f);
    row("RATE 8 kHz RAW, ADPCM off", r8r, 0, 1.0f);
    row("ADPCM 44.1 kHz, CRUNCH 0", a44, 0, 1.0f);
    row("ADPCM 22.05 kHz, CRUNCH 0", a22, 0, 1.0f);
    row("ADPCM 11.025 kHz, CRUNCH 0 (SMOOTH 1)", a11, 0, 1.0f);
    row("ADPCM 8 kHz, CRUNCH 0", a8, 0, 1.0f);
    row("ADPCM 11.025 kHz, CRUNCH 0.5", a11c5, 0, 1.0f);
    row("ADPCM 11.025 kHz, CRUNCH 1", a11c10, 0, 1.0f);
    row("ADPCM 11.025 kHz, mix 50 % (the dry delayed)", a11, 0, 0.5f);
    row("ADPCM 11.025 kHz, SMOOTH 0 (nearest)", s0, 0, 1.0f);
    row("ADPCM 11.025 kHz, SMOOTH 0.25 (nearest/linear)", s25, 0, 1.0f);
    row("ADPCM 11.025 kHz, SMOOTH 0.5 (linear)", s50, 0, 1.0f);
    row("ADPCM 11.025 kHz, SMOOTH 0.75 (linear/gaussian)", s75, 0, 1.0f);
    row("the full PS1 (11.025k FILTERED, ADPCM, CRUNCH .25)", ps1, 0, 1.0f);
#endif
    return 0;
}
