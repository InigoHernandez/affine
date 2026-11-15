/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: what the PSX effects cost against the FX chain LOAF BREAKER already runs on the FM-1 (host time, the same
 * compiler and flags): a minute of audio through each, in 256-frame blocks. The device's own figures come from its
 * meter; this says which is heavier.   psx_bench */
#include <stdio.h>
#include <time.h>
#include "brk_fx.h"
#include "psx.h"
#define N 256u
#define BLOCKS (44100u * 60u / N)
static int16_t work[PSXV_WORDS];
static uint8_t ring[2 * 9216];
static uint8_t dly[32768];
static float l[N], r[N], wl[N], wr[N], in[N], zero[N];
static uint32_t rnd = 1u;
static void fill(void) { uint32_t i; for (i = 0; i < N; i++) { rnd = rnd * 1664525u + 1013904223u; l[i] = r[i] = in[i] = (float)(int32_t)rnd * (0.3f / 2147483648.0f); } }
static double secs(clock_t a) { return (double)(clock() - a) / CLOCKS_PER_SEC; }
int main(void)
{
    uint32_t b;
    clock_t t;
    double base;
    brk_fx_t p;
    brk_fx_init();
    brk_fx_delay_buffer(dly, 32768u);
    brk_fx_defaults(&p);
    for (b = 0; b < BRK_FX_SLOTS; b++) p.slot[b].type = FXT_EMPTY;
    brk_fx_set(&p);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); brk_fx_process(in, 0, 0, 0, 0, l, r, N, 0); } base = secs(t);
    printf("  chain, empty         %6.1f ms a minute (the reference below is that much less)\n", base * 1e3);
    brk_fx_slot_defaults(&p.slot[0], FXT_DELAY); p.slot[0].on = 1; p.slot[0].mix = 0.5f; brk_fx_set(&p);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); brk_fx_process(in, 0, 0, 0, 0, l, r, N, 0); }
    printf("  LOAF's DELAY         %6.1f ms   (runs on the FM-1; LOAF's plate, gone, measured 38-54 here)\n",
           (secs(t) - base) * 1e3);
    brk_fx_slot_defaults(&p.slot[0], FXT_CRUNCH); p.slot[0].on = 1; brk_fx_set(&p);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); brk_fx_process(in, 0, 0, 0, 0, l, r, N, 0); }
    printf("  LOAF's CRUNCH        %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_verb_init(work, PSXV_WORDS); psx_skip_init(ring, 9216u); psx_init();
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); } base = secs(t);
    psx_verb_set(PSXV_SPACE_ECHO, 0.8f, 1.0f, 0.3f, 0.0f);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_verb_process(l, r, wl, wr, N); }
    printf("  SPU VERB             %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_crush_set(6.0f, CRUSH_DITHER, 0.3f);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_crush_process(l, r, N, 1.0f); }
    printf("  CRUSH                %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_redux_set(4000.0f, REDUX_SOFT, 0.0f);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_redux_process(l, r, N, 1.0f); }
    printf("  REDUX                %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_grit_set(22050.0f, 4u, 0u);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_grit_process(l, r, N, 1.0f); }
    printf("  SPU GRIT 22 kHz      %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_grit_set(44100.0f, 4u, 0u);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_grit_process(l, r, N, 1.0f); }
    printf("  SPU GRIT 44 kHz      %6.1f ms   (its heaviest)\n", (secs(t) - base) * 1e3);
    psx_xa_set(0u, 0u, 1u);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_xa_process(l, r, N, 1.0f); }
    printf("  XA 37.8 kHz stereo   %6.1f ms\n", (secs(t) - base) * 1e3);
    psx_skip_set(2u, 0.5f, 0.5f);
    t = clock(); for (b = 0; b < BLOCKS; b++) { fill(); psx_skip_process(l, r, N, (float)b * 0.08f, 15120.0f, 1.0f); }
    printf("  SKIP                 %6.1f ms\n", (secs(t) - base) * 1e3);
    (void)zero;
    return 0;
}
