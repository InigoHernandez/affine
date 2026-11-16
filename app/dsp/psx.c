/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE's PS1-era effects (psx.h). AFFINE's own code; the hardware's documented numbers come from psx-spx
 * (https://psx-spx.consoledev.net/: "SPU Reverb Examples", "Reverb Buffer Resampling", "CDROM XA-ADPCM",
 * "4-Point Gaussian Interpolation"). */
#include "psx.h"
#include "brk_math.h"

static inline int32_t sat16(int32_t x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }
static inline int32_t vmul(int32_t a, int32_t v) { return (a * v) >> 15; }   /* a volume times a sample, /8000h */
static inline int32_t f2s(float x)
{
    x *= 32767.0f;
    return x >= 32767.0f ? 32767 : x <= -32768.0f ? -32768 : (int32_t)(x < 0.0f ? x - 0.5f : x + 0.5f);
}
#define S2F (1.0f / 32768.0f)

/* =============================================================================================== SPU VERB --- */
const char *const PSXV_NAME[PSXV_N] = {"ROOM", "STUDIO S", "STUDIO M", "STUDIO L", "HALL", "HALF ECHO",
                                       "SPACE ECHO", "CHAOS", "DELAY"};
/* the presets' registers 1F801DC0h..1F801DFEh, as psx-spx lists them ("SPU Reverb Examples"), in this order: */
enum { R_DAPF1, R_DAPF2, R_VIIR, R_VCOMB1, R_VCOMB2, R_VCOMB3, R_VCOMB4, R_VWALL,
       R_VAPF1, R_VAPF2, R_MLSAME, R_MRSAME, R_MLCOMB1, R_MRCOMB1, R_MLCOMB2, R_MRCOMB2,
       R_DLSAME, R_DRSAME, R_MLDIFF, R_MRDIFF, R_MLCOMB3, R_MRCOMB3, R_MLCOMB4, R_MRCOMB4,
       R_DLDIFF, R_DRDIFF, R_MLAPF1, R_MRAPF1, R_MLAPF2, R_MRAPF2, R_VLIN, R_VRIN, R_N };
static const uint16_t PRESET[PSXV_N][R_N] = {
    {0x007D,0x005B,0x6D80,0x54B8,0xBED0,0x0000,0x0000,0xBA80, 0x5800,0x5300,0x04D6,0x0333,0x03F0,0x0227,0x0374,0x01EF,
     0x0334,0x01B5,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x01B4,0x0136,0x00B8,0x005C,0x8000,0x8000},
    {0x0033,0x0025,0x70F0,0x4FA8,0xBCE0,0x4410,0xC0F0,0x9C00, 0x5280,0x4EC0,0x03E4,0x031B,0x03A4,0x02AF,0x0372,0x0266,
     0x031C,0x025D,0x025C,0x018E,0x022F,0x0135,0x01D2,0x00B7, 0x018F,0x00B5,0x00B4,0x0080,0x004C,0x0026,0x8000,0x8000},
    {0x00B1,0x007F,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0xB4C0, 0x5280,0x4EC0,0x0904,0x076B,0x0824,0x065F,0x07A2,0x0616,
     0x076C,0x05ED,0x05EC,0x042E,0x050F,0x0305,0x0462,0x02B7, 0x042F,0x0265,0x0264,0x01B2,0x0100,0x0080,0x8000,0x8000},
    {0x00E3,0x00A9,0x6F60,0x4FA8,0xBCE0,0x4510,0xBEF0,0xA680, 0x5680,0x52C0,0x0DFB,0x0B58,0x0D09,0x0A3C,0x0BD9,0x0973,
     0x0B59,0x08DA,0x08D9,0x05E9,0x07EC,0x04B0,0x06EF,0x03D2, 0x05EA,0x031D,0x031C,0x0238,0x0154,0x00AA,0x8000,0x8000},
    {0x01A5,0x0139,0x6000,0x5000,0x4C00,0xB800,0xBC00,0xC000, 0x6000,0x5C00,0x15BA,0x11BB,0x14C2,0x10BD,0x11BC,0x0DC1,
     0x11C0,0x0DC3,0x0DC0,0x09C1,0x0BC4,0x07C1,0x0A00,0x06CD, 0x09C2,0x05C1,0x05C0,0x041A,0x0274,0x013A,0x8000,0x8000},
    {0x0017,0x0013,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0x8500, 0x5F80,0x54C0,0x0371,0x02AF,0x02E5,0x01DF,0x02B0,0x01D7,
     0x0358,0x026A,0x01D6,0x011E,0x012D,0x00B1,0x011F,0x0059, 0x01A0,0x00E3,0x0058,0x0040,0x0028,0x0014,0x8000,0x8000},
    {0x033D,0x0231,0x7E00,0x5000,0xB400,0xB000,0x4C00,0xB000, 0x6000,0x5400,0x1ED6,0x1A31,0x1D14,0x183B,0x1BC2,0x16B2,
     0x1A32,0x15EF,0x15EE,0x1055,0x1334,0x0F2D,0x11F6,0x0C5D, 0x1056,0x0AE1,0x0AE0,0x07A2,0x0464,0x0232,0x8000,0x8000},
    {0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x8100, 0x0000,0x0000,0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,
     0x1005,0x0005,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000},
    {0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,
     0x1005,0x0005,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000, 0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000},
};
/* the 39-tap filter the hardware resamples the reverb's input and output with (psx-spx, "Reverb Buffer Resampling"):
 * a half-band, so only the even taps and the centre (4000h) are non-zero; the even ones here, k = 0, 2 .. 38 */
static const int16_t RS_EVEN[20] = {-0x0001, 0x0002, -0x000A, 0x0023, -0x0067, 0x010A, -0x0268, 0x0534, -0x0B90, 0x2806,
                                     0x2806, -0x0B90, 0x0534, -0x0268, 0x010A, -0x0067, 0x0023, -0x000A, 0x0002, -0x0001};
#define RS_C 0x4000                      /* the centre tap, k = 19 */

static volatile uint32_t p_vtype;
static volatile float p_vdepth = 0.75f, p_vsize = 1.0f, p_vtone, p_vfreeze, p_vin = 1.0f;
static struct {
    int16_t *wk;
    uint32_t msk, cur;                   /* the work area, its mask, the buffer address (cells) */
    uint32_t type;                       /* the preset in use (a change clears the area: the hardware's garbage avoided) */
    uint32_t xfade;                      /* 1: the wet faded out for a new type or a leap of SIZE (cleared and switched at
                                          * the next block) */
    float in_s, eo;                      /* INPUT eased; the last block's output */
    uint32_t hush;                       /* blocks with nothing in or out (a lap of the area: it holds nothing; a change
                                          * of type or SIZE then needs no care) */
    int32_t off[R_N];                    /* the addresses in cells (the presets' x 4, scaled by SIZE) */
    uint32_t wlast[8];                   /* (SIZE gliding: each write head's last cell written, VW's order, */
    int32_t wv[8];                       /* its last sample, */
    float wpf[8];                        /* its place at the last tick, cells from the address then) */
    uint32_t glided;                     /* 1: the last block glided (the heads are the last tick's) */
    int32_t v[R_N];                      /* this block's volumes (signed 16-bit) */
    float size_s, size0, depth_s, tone_z[2];   /* SIZE eased (size0: where the block starts), DEPTH, TONE's one-pole */
    uint32_t size_end;                   /* 1: size_s is SIZE's own (a glide ends with this block) */
    int16_t in_h[2][78];                 /* the input resampler's history, 44.1 kHz, doubled (no wrap in the loop) */
    uint32_t in_i;
    int16_t out_h[2][40];                /* the output's, 22.05 kHz, doubled */
    uint32_t out_i;
    uint32_t odd;                        /* 44.1 kHz phase: a reverb tick on every other sample */
    int32_t held[2];                     /* the odd phase's output: the centre tap alone */
} vb;

void psx_verb_init(int16_t *work, uint32_t words)
{
    uint32_t i;
    vb.wk = work;
    vb.msk = words - 1u;
    for (i = 0; i <= vb.msk; i++)
        work[i] = 0;
    vb.cur = 0;
    vb.type = PSXV_N;                    /* (set on the first block) */
    vb.size_s = 1.0f;
    vb.in_s = 1.0f;
    vb.hush = 1000u;
}

void psx_verb_set(uint32_t type, float depth, float size, float tone, float freeze)
{
    p_vtype = type < PSXV_N ? type : 0u;
    p_vdepth = brk_clampf(depth, 0.0f, 1.0f);
    p_vsize = brk_clampf(size, 0.0f, 4.0f);
    p_vtone = brk_clampf(tone, 0.0f, 1.0f);
    p_vfreeze = brk_clampf(freeze, 0.0f, 1.0f);
}
void psx_verb_input(float gain) { p_vin = brk_clampf(gain, 0.25f, 4.0f); }

/* the addresses at SIZE scale sz (the presets' x 4 cells): dAPF1, dAPF2 and mLSAME..mRAPF2 (dLSAME..dRDIFF sit in that
 * range) */
static inline void verb_offs(const uint16_t *r, float sz)
{
    uint32_t i;
    vb.off[R_DAPF1] = (int32_t)((float)(r[R_DAPF1] * 4u) * sz + 0.5f);
    vb.off[R_DAPF2] = (int32_t)((float)(r[R_DAPF2] * 4u) * sz + 0.5f);
    for (i = R_MLSAME; i <= R_MRAPF2; i++)
        vb.off[i] = (int32_t)((float)(r[i] * 4u) * sz + 0.5f);
}

/* SIZE's scale for type t: 0..1 a quarter .. the hardware's; past 1 as far as the area allows (small rooms can grow).
 * span: the cells its farthest address reaches at scale 1 */
static float verb_scale(uint32_t t, float *span)
{
    uint32_t i, maxo = 0;
    float s = p_vsize <= 1.0f ? 0.25f + 0.75f * p_vsize : p_vsize;
    for (i = R_MLSAME; i <= R_MRAPF2; i++)
        if (PRESET[t][i] > maxo)
            maxo = PRESET[t][i];
    *span = (float)(maxo * 4u + PRESET[t][R_DAPF1] * 4u);
    if (maxo && s * *span > (float)(vb.msk - 2u))
        s = (float)(vb.msk - 2u) / *span;
    return s;
}

/* the block's addresses and volumes; 1 back when the wet is to fade out. A new type, or a leap of SIZE (a preset's),
 * while it sounds: this block fades the old one out, the next clears the area (its old reflections would read as
 * noise) and starts the new one from silence; silent, at once. A small change of SIZE (a knob's) glides: the addresses
 * move every tick (verb_tick_glide), the farthest 24 cells a block at most (a fifth of a cell a tick: the repeats bend
 * three semitones at most, a tape's pitch bend; faster, the linear reads alias; they jumped once a block before:
 * clicks) */
BRK_COLD static uint32_t verb_block(void)
{
    uint32_t t = p_vtype, i, out = 0u, live = vb.hush < 270u;   /* (a lap: 32768 ticks, 256 blocks) */
    float span, s = verb_scale(t, &span), d = s - vb.size_s, fr = p_vfreeze, k, lim = 24.0f / span;
    if (t != vb.type || (live && (d > 0.25f || d < -0.25f))) {
        if (vb.type < PSXV_N && !vb.xfade && live) {
            vb.xfade = 1u;               /* (sounding: out first, as it is) */
            t = vb.type;
            s = vb.size_s;
            out = 1u;
        } else {
            for (i = 0; i <= vb.msk; i++)
                vb.wk[i] = 0;
            for (i = 0; i < 40u; i++)    /* (and the old tail still in the output's resampler) */
                vb.out_h[0][i] = vb.out_h[1][i] = 0;
            vb.held[0] = vb.held[1] = 0;
            if (vb.xfade)
                vb.depth_s = 0.0f;       /* (faded out: the new one fades in from nothing) */
            vb.xfade = 0u;
            vb.type = t;
            vb.size_s = s;
        }
    }
    vb.size0 = vb.size_s;
    d = s - vb.size_s;
    if (!live || (d < 1e-5f && d > -1e-5f))
        vb.size_s = s;                   /* (silent, or there: no glide) */
    else if (d * 0.2f > lim || d * 0.2f < -lim)
        vb.size_s += d > 0.0f ? lim : -lim;
    else
        vb.size_s = d < 4e-5f && d > -4e-5f ? s : vb.size_s + d * 0.2f;   /* (the last step lands on it) */
    vb.size_end = vb.size_s == s;        /* (a glide's last block: it ends on whole cells, as the next one runs) */
    verb_offs(PRESET[t], vb.size0);      /* (the ticks move them on to size_s when it glides) */
    for (i = 0; i < R_N; i++)
        vb.v[i] = (int32_t)(int16_t)PRESET[t][i];
    /* FREEZE: the reflections' filter opened and their wall turned to almost one, the input closed */
    if (fr > 0.0f) {
        int32_t w = vb.v[R_VWALL];
        float wt = w < 0 ? -32512.0f : 32512.0f;   /* 7F00h: Chaos Echo's way, a hair under forever */
        vb.v[R_VIIR] = (int32_t)((float)vb.v[R_VIIR] + (32767.0f - (float)vb.v[R_VIIR]) * fr);
        vb.v[R_VWALL] = (int32_t)((float)w + (wt - (float)w) * fr);
    }
    k = 1.0f - fr;
    vb.v[R_VLIN] = (int32_t)((float)vb.v[R_VLIN] * k);   /* (-8000h: the presets invert the input, as measured) */
    vb.v[R_VRIN] = (int32_t)((float)vb.v[R_VRIN] * k);
    return out;
}

/* one 22.05 kHz tick: L then R (the hardware alternates them at 44.1 kHz, the same buffer address) */
static inline void verb_tick(int32_t lin, int32_t rin, int32_t *lo, int32_t *ro)
{
    int16_t *w = vb.wk;
    uint32_t c = vb.cur, m = vb.msk;
    const int32_t *o = vb.off, *v = vb.v;
#define AT(x) w[(c + (uint32_t)(x)) & m]
    int32_t l = vmul(lin, v[R_VLIN]), r = vmul(rin, v[R_VRIN]), a, b, t;
    /* same side reflection, then different side: a one-pole (vIIR) of the input plus the far wall (vWALL) */
    a = AT(o[R_MLSAME] - 1);
    AT(o[R_MLSAME]) = (int16_t)sat16(vmul(sat16(l + vmul(AT(o[R_DLSAME]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = AT(o[R_MRSAME] - 1);
    AT(o[R_MRSAME]) = (int16_t)sat16(vmul(sat16(r + vmul(AT(o[R_DRSAME]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = AT(o[R_MLDIFF] - 1);
    AT(o[R_MLDIFF]) = (int16_t)sat16(vmul(sat16(l + vmul(AT(o[R_DRDIFF]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = AT(o[R_MRDIFF] - 1);
    AT(o[R_MRDIFF]) = (int16_t)sat16(vmul(sat16(r + vmul(AT(o[R_DLDIFF]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    /* early echo: four combs read back */
    a = sat16(vmul(AT(o[R_MLCOMB1]), v[R_VCOMB1]) + vmul(AT(o[R_MLCOMB2]), v[R_VCOMB2]) +
              vmul(AT(o[R_MLCOMB3]), v[R_VCOMB3]) + vmul(AT(o[R_MLCOMB4]), v[R_VCOMB4]));
    b = sat16(vmul(AT(o[R_MRCOMB1]), v[R_VCOMB1]) + vmul(AT(o[R_MRCOMB2]), v[R_VCOMB2]) +
              vmul(AT(o[R_MRCOMB3]), v[R_VCOMB3]) + vmul(AT(o[R_MRCOMB4]), v[R_VCOMB4]));
    /* late reverb: two all-passes */
    t = AT(o[R_MLAPF1] - o[R_DAPF1]); a = sat16(a - vmul(t, v[R_VAPF1])); AT(o[R_MLAPF1]) = (int16_t)a;
    a = sat16(vmul(a, v[R_VAPF1]) + t);
    t = AT(o[R_MRAPF1] - o[R_DAPF1]); b = sat16(b - vmul(t, v[R_VAPF1])); AT(o[R_MRAPF1]) = (int16_t)b;
    b = sat16(vmul(b, v[R_VAPF1]) + t);
    t = AT(o[R_MLAPF2] - o[R_DAPF2]); a = sat16(a - vmul(t, v[R_VAPF2])); AT(o[R_MLAPF2]) = (int16_t)a;
    a = sat16(vmul(a, v[R_VAPF2]) + t);
    t = AT(o[R_MRAPF2] - o[R_DAPF2]); b = sat16(b - vmul(t, v[R_VAPF2])); AT(o[R_MRAPF2]) = (int16_t)b;
    b = sat16(vmul(b, v[R_VAPF2]) + t);
#undef AT
    vb.cur = (c + 1u) & m;
    *lo = a;
    *ro = b;
}

/* a tick while SIZE glides: verb_tick's, its addresses fractional (of: cells). The reads fall between two cells
 * (linear); each write head moves as a tape's would, writing every cell it passes (0, 1 or 2 a tick: SIZE shrinking or
 * growing) with the sample between its last and this one where the cell falls (whole cells skipped or landed on twice
 * before: a sample from the area's last lap, or one lost). So the addresses move a little every tick, a pitch bend
 * without steps. The heads' IIR reads their own last sample */
static const uint8_t VW[8] = {R_MLSAME, R_MRSAME, R_MLDIFF, R_MRDIFF, R_MLAPF1, R_MRAPF1, R_MLAPF2, R_MRAPF2};
static inline int32_t verb_atf(const int16_t *w, uint32_t c, uint32_t m, float x)
{
    int32_t i = (int32_t)x, a = w[(c + (uint32_t)i) & m], b = w[(c + (uint32_t)i + 1u) & m];
    return a + (int32_t)((float)(b - a) * (x - (float)i));
}
static void verb_tick_glide(int32_t lin, int32_t rin, int32_t *lo, int32_t *ro, const float *of, const float *winv)
{
    int16_t *w = vb.wk;
    uint32_t c = vb.cur, m = vb.msk, j;
    const int32_t *v = vb.v;
    int32_t val[8];
#define AF(x) verb_atf(w, c, m, (x))
    int32_t l = vmul(lin, v[R_VLIN]), r = vmul(rin, v[R_VRIN]), a, b, t;
    a = vb.wv[0];
    val[0] = sat16(vmul(sat16(l + vmul(AF(of[R_DLSAME]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = vb.wv[1];
    val[1] = sat16(vmul(sat16(r + vmul(AF(of[R_DRSAME]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = vb.wv[2];
    val[2] = sat16(vmul(sat16(l + vmul(AF(of[R_DRDIFF]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = vb.wv[3];
    val[3] = sat16(vmul(sat16(r + vmul(AF(of[R_DLDIFF]), v[R_VWALL]) - a), v[R_VIIR]) + a);
    a = sat16(vmul(AF(of[R_MLCOMB1]), v[R_VCOMB1]) + vmul(AF(of[R_MLCOMB2]), v[R_VCOMB2]) +
              vmul(AF(of[R_MLCOMB3]), v[R_VCOMB3]) + vmul(AF(of[R_MLCOMB4]), v[R_VCOMB4]));
    b = sat16(vmul(AF(of[R_MRCOMB1]), v[R_VCOMB1]) + vmul(AF(of[R_MRCOMB2]), v[R_VCOMB2]) +
              vmul(AF(of[R_MRCOMB3]), v[R_VCOMB3]) + vmul(AF(of[R_MRCOMB4]), v[R_VCOMB4]));
    t = AF(of[R_MLAPF1] - of[R_DAPF1]); a = sat16(a - vmul(t, v[R_VAPF1])); val[4] = a;
    a = sat16(vmul(a, v[R_VAPF1]) + t);
    t = AF(of[R_MRAPF1] - of[R_DAPF1]); b = sat16(b - vmul(t, v[R_VAPF1])); val[5] = b;
    b = sat16(vmul(b, v[R_VAPF1]) + t);
    t = AF(of[R_MLAPF2] - of[R_DAPF2]); a = sat16(a - vmul(t, v[R_VAPF2])); val[6] = a;
    a = sat16(vmul(a, v[R_VAPF2]) + t);
    t = AF(of[R_MRAPF2] - of[R_DAPF2]); b = sat16(b - vmul(t, v[R_VAPF2])); val[7] = b;
    b = sat16(vmul(b, v[R_VAPF2]) + t);
#undef AF
    for (j = 0; j < 8u; j++) {           /* the heads: every cell passed since the last tick, in order */
        float o = of[VW[j]], pr = vb.wpf[j] - 1.0f, inv = winv[j];   /* (1 / the cells it moves a tick) */
        int32_t kt = (int32_t)(o + 0.5f), x0 = vb.wv[j], dx = val[j] - x0;
        uint32_t at = (c + (uint32_t)kt) & m, cnt = (at - vb.wlast[j]) & m, q;
        if (cnt > 3u)
            cnt = 1u;                    /* (never: the heads move a cell a tick at most) */
        for (q = 1u; q <= cnt; q++) {
            float fr = ((float)(kt - (int32_t)(cnt - q)) - pr) * inv;
            fr = fr < 0.0f ? 0.0f : fr > 1.0f ? 1.0f : fr;
            w[(at - (cnt - q)) & m] = (int16_t)(x0 + (int32_t)((float)dx * fr));
        }
        if (cnt)
            vb.wlast[j] = at;
        vb.wv[j] = val[j];
        vb.wpf[j] = o;
    }
    vb.cur = (c + 1u) & m;
    *lo = a;
    *ro = b;
}

/* the reverb over n samples: in = (src x gain + vsend) x INPUT (each side; vsend mono, 0 none; gain moves from g0 to
 * g1 over the block, INPUT from where it was), out written (add 0) or added (add 1: in place over src is fine, each
 * sample read before it is written); the input's and the output's sums of magnitudes back (so the bus knows when it
 * may rest) */
static void verb_core(const float *srcl, const float *srcr, float g0, float g1, const float *vsend, float *outl,
                      float *outr, uint32_t n, int add, float *ein, float *eout)
{
    uint32_t i, k, ch;
    float tc = brk_tan_pi(1500.0f * brk_exp2f(3.5f * (1.0f - p_vtone)) / 44100.0f), g = tc / (1.0f + tc);
    float d0 = vb.depth_s, d1 = p_vdepth, dd, ei = 0.0f, eo = 0.0f, rn = 1.0f / (float)(n ? n : 1u);
    float gain = g0, dg = (g1 - g0) * rn, in = vb.in_s, din = (p_vin - vb.in_s) * rn, of[R_N], dof[R_N], winv[8];
    uint32_t gl;
    const uint16_t *pr;
    if (verb_block())
        d1 = 0.0f;                       /* (a new type: the old one out first) */
    pr = PRESET[vb.type];                /* (the type sounding this block: the old one while it fades out) */
    gl = vb.size_s != vb.size0;
    if (gl) {                            /* SIZE gliding: each address from where it is to where the block ends, a step a
                                          * tick (whole cells where the glide starts and ends: the still ticks' own) */
        if (!vb.glided)
            for (k = 0; k < 8u; k++) {   /* (the heads where the last tick left them) */
                vb.wlast[k] = (vb.cur - 1u + (uint32_t)vb.off[VW[k]]) & vb.msk;
                vb.wv[k] = vb.wk[vb.wlast[k]];
                vb.wpf[k] = (float)vb.off[VW[k]];
            }
        for (k = 0; k < R_N; k++) {
            float a0, a1;
            if (k != R_DAPF1 && k != R_DAPF2 && (k < R_MLSAME || k > R_MRAPF2))
                continue;
            a0 = vb.glided ? (float)(pr[k] * 4u) * vb.size0 : (float)vb.off[k];
            a1 = (float)(pr[k] * 4u) * vb.size_s;
            if (vb.size_end)
                a1 = (float)(int32_t)(a1 + 0.5f);
            of[k] = a0;
            dof[k] = (a1 - a0) * rn * 2.0f;   /* (a tick every other sample) */
        }
        for (k = 0; k < 8u; k++)
            winv[k] = 1.0f / (1.0f + dof[VW[k]]);
    }
    vb.glided = gl && !vb.size_end;      /* (a glide that ended on whole cells: the next one starts from them) */
    vb.depth_s = d1;
    vb.in_s = p_vin;
    dd = (d1 - d0) * rn;
    for (i = 0; i < n; i++) {
        float x[2], v = vsend ? vsend[i] : 0.0f, ol, or_;
        int32_t y[2];
        gain += dg;
        in += din;
        x[0] = (srcl[i] * gain + v) * in;
        x[1] = (srcr[i] * gain + v) * in;
        ei += brk_absf(x[0]) + brk_absf(x[1]);
        for (ch = 0; ch < 2u; ch++) {    /* TONE: a one-pole low-pass (AFFINE's; open at 0) */
            if (p_vtone > 0.0f) {
                vb.tone_z[ch] += g * (x[ch] - vb.tone_z[ch]);
                x[ch] = vb.tone_z[ch];
            }
            vb.in_h[ch][vb.in_i] = vb.in_h[ch][vb.in_i + 39u] = (int16_t)f2s(x[ch]);
        }
        vb.in_i = vb.in_i + 1u >= 39u ? 0u : vb.in_i + 1u;
        if (!vb.odd) {
            int32_t rin[2];

            for (ch = 0; ch < 2u; ch++) {   /* down to 22.05 kHz: the 39 taps over the last 39 samples */
                const int16_t *h = &vb.in_h[ch][vb.in_i];   /* h[0] the oldest .. h[38] the newest */
                int32_t acc = RS_C * h[19];
                for (k = 0; k < 20u; k++)
                    acc += RS_EVEN[k] * h[2u * k];
                rin[ch] = sat16(acc >> 15);
            }
            if (gl) {                    /* SIZE gliding: the addresses moved on, a little each tick */
                of[R_DAPF1] += dof[R_DAPF1];
                of[R_DAPF2] += dof[R_DAPF2];
                for (k = R_MLSAME; k <= R_MRAPF2; k++)
                    of[k] += dof[k];
                verb_tick_glide(rin[0], rin[1], &y[0], &y[1], of, winv);
            } else {
                verb_tick(rin[0], rin[1], &y[0], &y[1]);
            }
            for (ch = 0; ch < 2u; ch++) {   /* up to 44.1 kHz: the even taps here, the centre on the odd sample */
                const int16_t *h;
                int32_t acc = 0;
                vb.out_h[ch][vb.out_i] = vb.out_h[ch][vb.out_i + 20u] = (int16_t)y[ch];
                h = &vb.out_h[ch][vb.out_i + 1u];   /* h[0] the oldest .. h[19] the newest */
                for (k = 0; k < 20u; k++)
                    acc += RS_EVEN[k] * h[k];
                y[ch] = sat16(acc >> 14);
                vb.held[ch] = h[10];     /* (the odd phase: 2 x 4000h / 8000h x the sample 9.5 ticks back) */
            }
            vb.out_i = vb.out_i + 1u >= 20u ? 0u : vb.out_i + 1u;
        } else {
            y[0] = vb.held[0];
            y[1] = vb.held[1];
        }
        vb.odd ^= 1u;
        d0 += dd;
        ol = (float)y[0] * S2F * d0;
        or_ = (float)y[1] * S2F * d0;
        eo += brk_absf(ol) + brk_absf(or_);
        if (add) {
            outl[i] += ol;
            outr[i] += or_;
        } else {
            outl[i] = ol;
            outr[i] = or_;
        }
    }
    *ein = ei;
    *eout = eo;
    vb.eo = eo;
    vb.hush = ei > 1e-4f || eo > 1e-4f ? 0u : vb.hush < 1000u ? vb.hush + 1u : vb.hush;
}

void psx_verb_process(const float *inl, const float *inr, float *outl, float *outr, uint32_t n)
{
    float ei, eo;
    uint32_t i;
    if (!vb.wk) {
        for (i = 0; i < n; i++)
            outl[i] = outr[i] = 0.0f;
        return;
    }
    verb_core(inl, inr, 1.0f, 1.0f, 0, outl, outr, n, 0, &ei, &eo);
}

/* ======================================================================================= SPU GRIT and XA --- */
/* the predictors (psx-spx, "XA-ADPCM": pos_xa_adpcm_table, neg_xa_adpcm_table; SPU-ADPCM has five, XA four) */
static const int16_t PRED[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
/* the interpolator: the SPU's four-point "gaussian" table, its 512 values as psx-spx lists them ("Sound Processing
 * Unit (SPU)", "4-Point Gaussian Interpolation": documented numbers, copied from its text). With i the 8-bit fraction
 * between the older and the old of four samples: out = gauss[0FFh - i] x oldest + gauss[1FFh - i] x older +
 * gauss[100h + i] x old + gauss[000h + i] x new, each product SAR 15. Each four sum to 7F7Fh..7F81h, never past 8000h
 * (psx-spx). AFFINE computed a four-term Nuttall window here before (within 55 of 59B3h everywhere: the sound moved
 * 56-61 dB under the signal) */
const int16_t PSX_GAUSS[512] = {
    -0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,
    0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x001,0x001,0x001,0x001,0x002,0x002,0x002,0x003,0x003,
    0x003,0x004,0x004,0x005,0x005,0x006,0x007,0x007,0x008,0x009,0x009,0x00A,0x00B,0x00C,0x00D,0x00E,
    0x00F,0x010,0x011,0x012,0x013,0x015,0x016,0x018,0x019,0x01B,0x01C,0x01E,0x020,0x021,0x023,0x025,
    0x027,0x029,0x02C,0x02E,0x030,0x033,0x035,0x038,0x03A,0x03D,0x040,0x043,0x046,0x049,0x04D,0x050,
    0x054,0x057,0x05B,0x05F,0x063,0x067,0x06B,0x06F,0x074,0x078,0x07D,0x082,0x087,0x08C,0x091,0x096,
    0x09C,0x0A1,0x0A7,0x0AD,0x0B3,0x0BA,0x0C0,0x0C7,0x0CD,0x0D4,0x0DB,0x0E3,0x0EA,0x0F2,0x0FA,0x101,
    0x10A,0x112,0x11B,0x123,0x12C,0x135,0x13F,0x148,0x152,0x15C,0x166,0x171,0x17B,0x186,0x191,0x19C,
    0x1A8,0x1B4,0x1C0,0x1CC,0x1D9,0x1E5,0x1F2,0x200,0x20D,0x21B,0x229,0x237,0x246,0x255,0x264,0x273,
    0x283,0x293,0x2A3,0x2B4,0x2C4,0x2D6,0x2E7,0x2F9,0x30B,0x31D,0x330,0x343,0x356,0x36A,0x37E,0x392,
    0x3A7,0x3BC,0x3D1,0x3E7,0x3FC,0x413,0x42A,0x441,0x458,0x470,0x488,0x4A0,0x4B9,0x4D2,0x4EC,0x506,
    0x520,0x53B,0x556,0x572,0x58E,0x5AA,0x5C7,0x5E4,0x601,0x61F,0x63E,0x65C,0x67C,0x69B,0x6BB,0x6DC,
    0x6FD,0x71E,0x740,0x762,0x784,0x7A7,0x7CB,0x7EF,0x813,0x838,0x85D,0x883,0x8A9,0x8D0,0x8F7,0x91E,
    0x946,0x96F,0x998,0x9C1,0x9EB,0xA16,0xA40,0xA6C,0xA98,0xAC4,0xAF1,0xB1E,0xB4C,0xB7A,0xBA9,0xBD8,
    0xC07,0xC38,0xC68,0xC99,0xCCB,0xCFD,0xD30,0xD63,0xD97,0xDCB,0xE00,0xE35,0xE6B,0xEA1,0xED7,0xF0F,
    0xF46,0xF7F,0xFB7,0xFF1,0x102A,0x1065,0x109F,0x10DB,0x1116,0x1153,0x118F,0x11CD,0x120B,0x1249,0x1288,0x12C7,
    0x1307,0x1347,0x1388,0x13C9,0x140B,0x144D,0x1490,0x14D4,0x1517,0x155C,0x15A0,0x15E6,0x162C,0x1672,0x16B9,0x1700,
    0x1747,0x1790,0x17D8,0x1821,0x186B,0x18B5,0x1900,0x194B,0x1996,0x19E2,0x1A2E,0x1A7B,0x1AC8,0x1B16,0x1B64,0x1BB3,
    0x1C02,0x1C51,0x1CA1,0x1CF1,0x1D42,0x1D93,0x1DE5,0x1E37,0x1E89,0x1EDC,0x1F2F,0x1F82,0x1FD6,0x202A,0x207F,0x20D4,
    0x2129,0x217F,0x21D5,0x222C,0x2282,0x22DA,0x2331,0x2389,0x23E1,0x2439,0x2492,0x24EB,0x2545,0x259E,0x25F8,0x2653,
    0x26AD,0x2708,0x2763,0x27BE,0x281A,0x2876,0x28D2,0x292E,0x298B,0x29E7,0x2A44,0x2AA1,0x2AFF,0x2B5C,0x2BBA,0x2C18,
    0x2C76,0x2CD4,0x2D33,0x2D91,0x2DF0,0x2E4F,0x2EAE,0x2F0D,0x2F6C,0x2FCC,0x302B,0x308B,0x30EA,0x314A,0x31AA,0x3209,
    0x3269,0x32C9,0x3329,0x3389,0x33E9,0x3449,0x34A9,0x3509,0x3569,0x35C9,0x3629,0x3689,0x36E8,0x3748,0x37A8,0x3807,
    0x3867,0x38C6,0x3926,0x3985,0x39E4,0x3A43,0x3AA2,0x3B00,0x3B5F,0x3BBD,0x3C1B,0x3C79,0x3CD7,0x3D35,0x3D92,0x3DEF,
    0x3E4C,0x3EA9,0x3F05,0x3F62,0x3FBD,0x4019,0x4074,0x40D0,0x412A,0x4185,0x41DF,0x4239,0x4292,0x42EB,0x4344,0x439C,
    0x43F4,0x444C,0x44A3,0x44FA,0x4550,0x45A6,0x45FC,0x4651,0x46A6,0x46FA,0x474E,0x47A1,0x47F4,0x4846,0x4898,0x48E9,
    0x493A,0x498A,0x49D9,0x4A29,0x4A77,0x4AC5,0x4B13,0x4B5F,0x4BAC,0x4BF7,0x4C42,0x4C8D,0x4CD7,0x4D20,0x4D68,0x4DB0,
    0x4DF7,0x4E3E,0x4E84,0x4EC9,0x4F0E,0x4F52,0x4F95,0x4FD7,0x5019,0x505A,0x509A,0x50DA,0x5118,0x5156,0x5194,0x51D0,
    0x520C,0x5247,0x5281,0x52BA,0x52F3,0x532A,0x5361,0x5397,0x53CC,0x5401,0x5434,0x5467,0x5499,0x54CA,0x54FA,0x5529,
    0x5558,0x5585,0x55B2,0x55DE,0x5609,0x5632,0x565B,0x5684,0x56AB,0x56D1,0x56F6,0x571B,0x573E,0x5761,0x5782,0x57A3,
    0x57C3,0x57E2,0x57FF,0x581C,0x5838,0x5853,0x586D,0x5886,0x589E,0x58B5,0x58CB,0x58E0,0x58F4,0x5907,0x5919,0x592A,
    0x593A,0x5949,0x5958,0x5965,0x5971,0x597C,0x5986,0x598F,0x5997,0x599E,0x59A4,0x59A9,0x59AD,0x59B0,0x59B2,0x59B3,
};

/* ---- the encoder (SPU-ADPCM and XA-ADPCM): one unit of 28 samples encoded and decoded at once ---- */
/* the encoder's choices for a unit. CRUNCH (the colour stage's, AFFINE's) spoils them; at rest (ENC_CLEAN) they are
 * the encoder as GRIT has always had it: every predictor ranked, the two best simulated, the finest shift that holds
 * the residual's peak */
typedef struct {
    uint32_t nf;                         /* predictors it may use, 0..nf - 1 (5: SPU-ADPCM's all; CRUNCH: down to 2) */
    uint32_t sim;                        /* 1: the two best simulated on the decoder's history; 0: the first ranked */
    int32_t bias;                        /* shifts coarser than the peak needs (each: twice the step, +6 dB of grain) */
    int32_t shmax;                       /* the finest shift allowed (12: a step of one LSB; less: quiet tails grain) */
    int32_t trunc;                       /* 1: the residual truncated toward zero, not rounded (small ones lost) */
} cd_enc_t;
static const cd_enc_t ENC_CLEAN = {5u, 1u, 0, 12, 0};

/* one unit of 28 through predictor f at shift sh: the console's decode of the nearest nibbles (step 2^(ex - sh)),
 * written to out; the squared error back. o, oo: the decoder's history (old, older) before the unit */
static float cd_try(const int16_t *blk, int32_t o, int32_t oo, uint32_t f, int32_t sh, int32_t ex, int32_t qmin,
                    int32_t qmax, int32_t trunc, int16_t *out)
{
    int32_t k = ex - sh, hp, hn, f0 = PRED[f][0], f1 = PRED[f][1];
    uint32_t j;
    float err = 0.0f;
    if (k < 0)
        k = 0;
    hp = hn = k ? 1 << (k - 1) : 0;      /* the nearest nibble (half a step added, then floored) */
    if (trunc) {                         /* (CRUNCH: toward zero, as a cheap encoder: small residuals lost, no DC) */
        hp = 0;
        hn = (1 << k) - 1;
    }
    for (j = 0; j < 28u; j++) {
        int32_t p = (o * f0 + oo * f1 + 32) >> 6, q = blk[j] - p, s, d;
        q = (q + (q < 0 ? hn : hp)) >> k;
        q = q < qmin ? qmin : q > qmax ? qmax : q;
        s = sat16(((q * (1 << ex)) >> sh) + p);   /* (a multiply: a negative shifted left is undefined) */
        d = s - blk[j];
        err += (float)(d * d);
        oo = o;
        o = s;
        out[j] = (int16_t)s;
    }
    return err;
}

/* one unit of 28 (blk) encoded and decoded into out: each allowed predictor's residual on the input ranked, the two
 * best simulated at the finest shift that holds their peak (the decoder's own history: what the console would hear),
 * the better one kept. h: the decoder's history (old, older), carried on. bits 2..4; xa8: XA's 8-bit mode (bytes
 * shifted up by 8, else nibbles by 12); nfilt 5 SPU, 4 XA; filter 0 auto, 1..nfilt one forced */
static void cd_unit(const int16_t *blk, int32_t *h, uint32_t bits, uint32_t xa8, uint32_t nfilt, uint32_t filter,
                    const cd_enc_t *e, int16_t *out)
{
    int16_t tmp[28];
    uint32_t f, f0 = 0, f1 = nfilt - 1u, j, cand[2] = {0, 0};
    int32_t qmax = xa8 ? 127 : (1 << (bits - 1u)) - 1, qmin = -qmax - 1;
    int32_t ex = xa8 ? 8 : 12;           /* the nibble (or byte) is shifted up by 12 (or 8) */
    int32_t peak[5] = {0, 0, 0, 0, 0}, sh[2] = {12, 12};
    float e0 = -1.0f, e1 = -1.0f, ea, eb;
    if (filter && filter <= nfilt)
        f0 = f1 = filter - 1u;
    else if (e->nf < nfilt)
        f1 = e->nf - 1u;                 /* (CRUNCH: the gentler predictors only) */
    for (f = f0; f <= f1; f++) {         /* the residual's energy and peak on the input */
        int32_t o = h[0], oo = h[1];
        float en = 0.0f;
        for (j = 0; j < 28u; j++) {
            int32_t p = (o * PRED[f][0] + oo * PRED[f][1] + 32) >> 6, r = blk[j] - p;
            en += (float)r * (float)r;
            r = r < 0 ? -r : r;
            if (r > peak[f])
                peak[f] = r;
            oo = o;
            o = blk[j];
        }
        if (e0 < 0.0f || en < e0) {
            e1 = e0; cand[1] = cand[0];
            e0 = en; cand[0] = f;
        } else if (e1 < 0.0f || en < e1) {
            e1 = en; cand[1] = f;
        }
    }
    if (e1 < 0.0f)
        cand[1] = cand[0];
    for (j = 0; j < 2u; j++) {           /* the finest shift whose largest nibble reaches the peak (CRUNCH: coarser) */
        int32_t s;
        for (s = 12; s > 0; s--)
            if (((int32_t)qmax << ex >> s) >= peak[cand[j]])
                break;
        s -= e->bias;
        sh[j] = s < 0 ? 0 : s > e->shmax ? e->shmax : s;
    }
    ea = cd_try(blk, h[0], h[1], cand[0], sh[0], ex, qmin, qmax, e->trunc, out);
    if (e->sim && cand[1] != cand[0]) {
        eb = cd_try(blk, h[0], h[1], cand[1], sh[1], ex, qmin, qmax, e->trunc, tmp);
        if (eb < ea)
            for (j = 0; j < 28u; j++)
                out[j] = tmp[j];
    }
    h[0] = out[27];
    h[1] = out[26];
}

/* CRUNCH 0..1 into one unit's choices, each a step at a time: a fraction is a chance (dice per unit), so the sound
 * thickens smoothly as the knob turns. Up to 1: predictors 5 -> 2 (the strong low-frequency ones go first), the
 * simulation gone by 0.5 (the first ranked kept), up to 2 shifts coarser (where the format has them: loud units are
 * at its coarsest already), the finest shift 12 -> 4 (the smallest step 1 -> 256 LSB: quiet tails turn to grain,
 * then gate) and the residual truncated toward zero instead of rounded (a cheap encoder's way; it reaches loud units
 * too). Measured at 11.025 kHz (tests/engine/psx_color_demo.c), the codec's SNR at CRUNCH 0 / .25 / .5 / .75 / 1:
 * the pad 34 / 26 / 22 / 15 / 10 dB, the break 29 / 22 / 18 / 15 / 13, the break 34 dB down 28 / 21 / 18 / 11 / 3 */
static void crunch_enc(cd_enc_t *e, float c, uint32_t *rnd)
{
    int32_t v;
    e->nf = (uint32_t)(5.0f - 3.0f * c + brk_randf(rnd));
    e->nf = e->nf < 2u ? 2u : e->nf > 5u ? 5u : e->nf;
    e->sim = brk_randf(rnd) >= 2.0f * c;
    e->bias = (int32_t)(2.0f * c + brk_randf(rnd));
    v = (int32_t)(12.0f - 8.0f * c + brk_randf(rnd));
    e->shmax = v < 0 ? 0 : v > 12 ? 12 : v;
    e->trunc = brk_randf(rnd) < c;
}

BRK_COLD void psx_adpcm_roundtrip(const int16_t *in, int16_t *out, uint32_t units, uint32_t bits, uint32_t filter,
                                  float crunch)
{
    int32_t h[2] = {0, 0};
    uint32_t u, rnd = 0x1234567u;
    bits = bits < 2u ? 2u : bits > 4u ? 4u : bits;
    for (u = 0; u < units; u++) {
        cd_enc_t e = ENC_CLEAN;
        if (crunch > 0.0f)
            crunch_enc(&e, brk_clampf(crunch, 0.0f, 1.0f), &rnd);
        cd_unit(in + 28u * u, h, bits, 0u, 5u, filter, &e, out + 28u * u);
    }
}

/* a TPT state-variable low-pass stage (Zavalishin's form): g = tan(pi fc / fs), k = 1 / Q */
static inline float svf_lp(float *z, float x, float g, float k)
{
    float hp = (x - (k + g) * z[0] - z[1]) / (1.0f + g * (g + k)), bp = g * hp + z[0], lp = g * bp + z[1];
    z[0] = g * hp + bp;
    z[1] = g * bp + lp;
    return lp;
}

/* ---- the colour stage: GRIT grown into the PS1's sample sound (AFFINE's) ---- */
/* The input taken down to the codec's rate (FILTERED: a 4th-order Butterworth at 0.45 x the rate first, as the
 * console's samples were prepared; RAW: straight, its aliases kept, as some games had them), each 28 samples through
 * the SPU-ADPCM round trip (or kept as 16-bit PCM: ADPCM off), played back at 44.1 kHz through the four-point
 * interpolator. One clock drives both sides: codec samples in Q16, the playback CL_LEAD samples behind the newest
 * whatever the rate, so it never drifts (GRIT's old float clocks did at 16, 32 and 37.8 kHz: the playback wandered
 * through its ring within a minute); a new rate glides in; the dry is delayed by the same latency for the mix. */
const char *const PSXC_RATE_NAME[PSXC_RATES] = {"GRIT", "44.1K", "22K", "11K", "8K"};
const float PSXC_RATE_HZ[PSXC_RATES] = {0.0f, 44100.0f, 22050.0f, 11025.0f, 8000.0f};
#define CL_ONE (1u << 16)                /* one codec sample on the clock (Q16) */
#define CL_RING 64u                      /* decoded samples, a power of two that divides 65536 (the clock's wrap) */
#define CL_LEAD 32                       /* playback behind the clock: a unit (28) + the interpolator's reach (2) + 2 */
#define CL_DRY 256u                      /* the dry's delay line, frames (8 kHz FILTERED's latency: 182) */
#define CL_GLIDE 6.0f                    /* a rate change: latency moved a block at most (2.3 %, 25-110 ms) */
#define CL_K1 (1.0f / 0.5412f)           /* the anti-alias filter: Butterworth, 4th order: Q 0.541 and 1.307 */
#define CL_K2 (1.0f / 1.3066f)
typedef struct {
    float z[4];                          /* the anti-alias filter's states (two SVF stages) */
    float prev;                          /* the input sample before (the fractional sampling) */
    int32_t hist[2];                     /* the decoder's history (old, older) */
    int16_t blk[28];                     /* the unit being gathered */
    int16_t dec[CL_RING];                /* decoded samples, each at its codec index & (CL_RING - 1) */
} cl_ch_t;
static struct {
    cl_ch_t ch[2];
    uint32_t acc;                        /* the clock: codec samples, Q16 (wraps; the ring follows its low bits) */
    uint32_t inc;                        /* codec samples per output sample, Q16 (11025 Hz: 4000h) */
    uint32_t nb;                         /* samples gathered into the unit */
    float iinc;                          /* 1 / inc: a codec sample's place between two input samples */
    float g, aad;                        /* the anti-alias filter (g = tan(pi fc / fs)); its delay at DC, samples */
    float raw_s, mix_s, fade, smooth_s;  /* RAW (0 filtered .. 1 raw), the mix, the bus's fade in, SMOOTH: eased */
    uint32_t dlat, dw, fresh, run;       /* the dry's delay (frames), its write place; 1: just cleared; the bus's */
    uint32_t bits, filter, adpcm, rnd;
    float crunch;
    float dry[CL_DRY][2];
} cl;
static volatile float p_grate = 22050.0f, p_crunch, p_csmooth = 1.0f;
static volatile uint32_t p_gbits = 4u, p_gfilt, p_crate, p_craw, p_cadpcm = 1u;

void psx_grit_set(float rate, uint32_t bits, uint32_t filter)
{
    p_grate = rate;
    p_gbits = bits < 2u ? 2u : bits > 4u ? 4u : bits;
    p_gfilt = filter;
}
void psx_color_set(uint32_t rate, uint32_t raw, uint32_t adpcm, float crunch, float smooth)
{
    p_crate = rate < PSXC_RATES ? rate : 0u;
    p_craw = raw ? 1u : 0u;
    p_cadpcm = adpcm ? 1u : 0u;
    p_crunch = brk_clampf(crunch, 0.0f, 1.0f);
    p_csmooth = brk_clampf(smooth, 0.0f, 1.0f);
}

BRK_COLD static void cl_clear(void)
{
    uint8_t *z = (uint8_t *)&cl;
    uint32_t i;
    for (i = 0; i < sizeof cl; i++)
        z[i] = 0u;
    cl.fresh = 1u;
    cl.rnd = 0x1234567u;
}

/* the latency: the playback behind the clock (CL_LEAD codec samples), the input's sample before, the filter's delay */
static float cl_latency(uint32_t raw) { return (float)CL_LEAD * 65536.0f * cl.iinc + 1.0f + (raw ? 0.0f : cl.aad); }
float psx_color_latency(void) { return cl.inc ? cl_latency(p_craw) : 0.0f; }

/* the clock's step (Q16 codec samples an output sample) toward hz, from cur (iinc its inverse; 0: none yet): a new rate
 * eases in, the latency moving CL_GLIDE samples a block at most; hz back as it is on the way (the colour stage, XA) */
static uint32_t clk_inc(uint32_t cur, float iinc, float *hz)
{
    uint32_t inc = (uint32_t)(*hz * (65536.0f / 44100.0f) + 0.5f);
    if (cur && inc != cur) {
        float lt = (float)CL_LEAD * 65536.0f / (float)inc, lc = (float)CL_LEAD * 65536.0f * iinc;
        if (lt > lc + CL_GLIDE)
            lt = lc + CL_GLIDE;
        else if (lt < lc - CL_GLIDE)
            lt = lc - CL_GLIDE;
        else
            lt = 0.0f;
        if (lt > 0.0f) {                 /* (on the way: the rate and its filter between the two) */
            inc = (uint32_t)((float)CL_LEAD * 65536.0f / lt + 0.5f);
            *hz = (float)inc * (44100.0f / 65536.0f);
        }
    }
    return inc;
}

BRK_COLD static void cl_block(void)
{
    uint32_t ri = p_crate, inc;
    float hz = brk_clampf(ri ? PSXC_RATE_HZ[ri] : p_grate, 2000.0f, 44100.0f);
    inc = clk_inc(cl.inc, cl.iinc, &hz);
    if (inc != cl.inc) {
        cl.inc = inc;
        cl.iinc = 1.0f / (float)inc;
        cl.g = brk_tan_pi(brk_clampf(0.45f * hz, 1000.0f, 19000.0f) / 44100.0f);
        cl.aad = (CL_K1 + CL_K2) / (2.0f * cl.g);   /* (a bilinear low-pass's delay at DC: sum of 1/Q over 2g) */
    }
    cl.bits = p_gbits;
    cl.filter = p_gfilt;
    cl.adpcm = p_cadpcm;
    cl.crunch = p_crunch;
}

/* a unit gathered (k: the codec index of its last sample): through the round trip (or kept), into the ring */
static void cl_unit(uint32_t k)
{
    cd_enc_t e = ENC_CLEAN;
    int16_t out[28];
    uint32_t c, j, base = k - 27u;
    if (cl.crunch > 0.0f && cl.adpcm)
        crunch_enc(&e, cl.crunch, &cl.rnd);
    for (c = 0; c < 2u; c++) {
        cl_ch_t *h = &cl.ch[c];
        if (cl.adpcm) {
            cd_unit(h->blk, h->hist, cl.bits, 0u, 5u, cl.filter, &e, out);
        } else {                         /* (16-bit PCM; the history follows, ready for the codec's return) */
            for (j = 0; j < 28u; j++)
                out[j] = h->blk[j];
            h->hist[0] = out[27];
            h->hist[1] = out[26];
        }
        for (j = 0; j < 28u; j++)
            h->dec[(base + j) & (CL_RING - 1u)] = out[j];
    }
}

/* playback at i + fr / 256 (fr: 8 bits, as the SPU's pitch counter gives its table). SMOOTH sm: 1 the SPU's gaussian
 * (its table, its maths: the soft, dull top), 0.5 linear, 0 the nearest sample (no interpolation: the images left
 * above the rate, bright and metallic); between, the two neighbours blended */
static inline float cl_play(const int16_t *dec, int32_t i, uint32_t fr, float sm)
{
    int32_t b = dec[(uint32_t)i & (CL_RING - 1u)], d = dec[(uint32_t)(i + 1) & (CL_RING - 1u)];
    float lin, g, nr;
    if (sm > 0.5f) {                     /* (the gaussian: the oldest, older, old, new samples) */
        int32_t a = dec[(uint32_t)(i - 1) & (CL_RING - 1u)], e = dec[(uint32_t)(i + 2) & (CL_RING - 1u)];
        g = (float)(((PSX_GAUSS[0xFFu - fr] * a) >> 15) + ((PSX_GAUSS[0x1FFu - fr] * b) >> 15) +
                    ((PSX_GAUSS[0x100u + fr] * d) >> 15) + ((PSX_GAUSS[fr] * e) >> 15));
        if (sm >= 1.0f)
            return g * S2F;
        lin = (float)b + (float)((d - b) * (int32_t)fr) * (1.0f / 256.0f);
        return (lin + (g - lin) * (2.0f * sm - 1.0f)) * S2F;
    }
    lin = (float)b + (float)((d - b) * (int32_t)fr) * (1.0f / 256.0f);
    if (sm >= 0.5f)
        return lin * S2F;
    nr = (float)(fr < 128u ? b : d);
    return (nr + (lin - nr) * (2.0f * sm)) * S2F;
}

/* the stage over a block, in place: mix 0..1 with the dry delayed to match; want: the bus's fade toward 1 (0: out) */
static void cl_process(float *l, float *r, uint32_t n, float mix, uint32_t want)
{
    uint32_t i, inc, dl0, dl1, xf;
    float iinc, g, dr, dm, df, ds, smooth = p_csmooth, rn = 1.0f / (float)(n ? n : 1u);
    cl_block();
    inc = cl.inc;
    iinc = cl.iinc;
    g = cl.g;
    mix = brk_clampf(mix, 0.0f, 1.0f);
    dl1 = (uint32_t)(cl_latency(p_craw) + 0.5f);
    if (cl.fresh) {                      /* (just cleared: everything where it is set, nothing eased in) */
        cl.raw_s = p_craw ? 1.0f : 0.0f;
        cl.mix_s = mix;
        cl.smooth_s = smooth;
        cl.dlat = dl1;
        cl.fresh = 0u;
    }
    dl0 = cl.dlat;
    xf = dl0 != dl1;                     /* (the latency changed: the dry crossfades to its new delay over the block) */
    dr = ((p_craw ? 1.0f : 0.0f) - cl.raw_s) * rn;
    dm = (mix - cl.mix_s) * rn;
    ds = (smooth - cl.smooth_s) * rn;
    df = ((want ? 1.0f : 0.0f) - cl.fade) * rn;
    if (cl.raw_s >= 1.0f && !p_craw) {
        uint32_t c;                      /* (back to FILTERED: the filter, idle while RAW, starts where its input is) */
        for (c = 0; c < 2u; c++) {
            cl.ch[c].z[0] = cl.ch[c].z[2] = 0.0f;
            cl.ch[c].z[1] = cl.ch[c].z[3] = cl.ch[c].prev;
        }
    }
    {                                    /* (the block's state in locals: the float stores below could alias it) */
        uint32_t acc = cl.acc, dw = cl.dw, nb = cl.nb;
        float rs = cl.raw_s, m = cl.mix_s, sm = cl.smooth_s, f = cl.fade, p0 = cl.ch[0].prev, p1 = cl.ch[1].prev;
        for (i = 0; i < n; i++) {
            float x0 = l[i], x1 = r[i], y0 = x0, y1 = x1, w0, w1;
            uint32_t a = acc, d = (0u - a) & (CL_ONE - 1u);
            cl.dry[dw][0] = x0;
            cl.dry[dw][1] = x1;
            rs += dr;
            m += dm;
            sm += ds;
            f += df;
            if (rs < 1.0f) {             /* FILTERED, RAW, or between while it eases */
                float f0 = svf_lp(&cl.ch[0].z[2], svf_lp(&cl.ch[0].z[0], x0, g, CL_K1), g, CL_K2),
                      f1 = svf_lp(&cl.ch[1].z[2], svf_lp(&cl.ch[1].z[0], x1, g, CL_K1), g, CL_K2);
                y0 = rs <= 0.0f ? f0 : f0 + (x0 - f0) * rs;
                y1 = rs <= 0.0f ? f1 : f1 + (x1 - f1) * rs;
            }
            while (d < inc) {            /* the codec's samples between the input before and this one */
                float ph = (float)d * iinc;
                cl.ch[0].blk[nb] = (int16_t)f2s(p0 + (y0 - p0) * ph);
                cl.ch[1].blk[nb] = (int16_t)f2s(p1 + (y1 - p1) * ph);
                if (++nb == 28u) {
                    cl_unit((a + d) >> 16);
                    nb = 0;
                }
                d += CL_ONE;
            }
            p0 = y0;
            p1 = y1;
            {                            /* playback, CL_LEAD codec samples behind the clock */
                int32_t pi = (int32_t)(a >> 16) - CL_LEAD;
                uint32_t fr = (a >> 8) & 0xFFu;
                w0 = cl_play(cl.ch[0].dec, pi, fr, sm);
                w1 = cl_play(cl.ch[1].dec, pi, fr, sm);
            }
            acc = a + inc;
            if (m < 1.0f || xf) {        /* the mix, against the dry delayed by the latency */
                uint32_t q0 = (dw - dl0) & (CL_DRY - 1u);
                float d0 = cl.dry[q0][0], d1 = cl.dry[q0][1];
                if (xf) {
                    uint32_t q1 = (dw - dl1) & (CL_DRY - 1u);
                    float t = (float)(i + 1u) * rn;
                    d0 += (cl.dry[q1][0] - d0) * t;
                    d1 += (cl.dry[q1][1] - d1) * t;
                }
                if (m < 1.0f) {
                    w0 = d0 + (w0 - d0) * m;
                    w1 = d1 + (w1 - d1) * m;
                }
            }
            if (f < 1.0f) {              /* the bus's fade: in from (or out to) the sound as it came */
                w0 = x0 + (w0 - x0) * f;
                w1 = x1 + (w1 - x1) * f;
            }
            l[i] = w0;
            r[i] = w1;
            dw = (dw + 1u) & (CL_DRY - 1u);
        }
        cl.acc = acc;
        cl.dw = dw;
        cl.nb = nb;
        cl.ch[0].prev = p0;
        cl.ch[1].prev = p1;
    }
    cl.raw_s = p_craw ? 1.0f : 0.0f;     /* (landed exactly) */
    cl.mix_s = mix;
    cl.smooth_s = smooth;
    cl.fade = want ? 1.0f : 0.0f;
    cl.dlat = dl1;
}

void psx_grit_process(float *l, float *r, uint32_t n, float mix)
{
    if (mix <= 0.0f)
        return;
    cl.fade = 1.0f;                      /* (called alone: no fade, the stage at once) */
    cl_process(l, r, n, mix, 1u);
}

/* ---- XA: the CD audio codec: XA-ADPCM (four predictors) at 37.8 or 18.9 kHz, on the colour stage's clock (Q16, the
 * playback CL_LEAD codec samples behind the newest whatever the rate: it never drifts; its float clocks before
 * wandered 14..181 samples against the decoder over minutes and read stale units), played back through a cubic (the CD
 * decoder's own interpolation is a long, flatter filter: psx-spx, "25-point Zigzag Interpolation"). The anti-alias
 * filter always (the CD's audio was prepared at its rate); the dry delayed to the latency for a part mix; a rate
 * change glides; the bus primes it a block and fades it in and out ---- */
#define XA_DRY 128u                      /* the dry's delay, frames (18.9 kHz: 77) */
typedef struct {
    float z[4], prev;                    /* the anti-alias filter's states; the input sample before */
    int32_t hist[2];                     /* the decoder's history (old, older) */
    int16_t blk[28];                     /* the unit being gathered */
    int16_t dec[CL_RING];                /* decoded samples, each at its codec index & (CL_RING - 1) */
} xa_ch_t;
static struct {
    xa_ch_t ch[2];
    uint32_t acc, inc, nb;               /* the clock (Q16 codec samples), its step, samples gathered */
    float iinc, g, aad;                  /* 1 / inc; the anti-alias filter (g = tan(pi fc / fs)), its delay at DC */
    float mix_s, fade;                   /* the mix eased; the bus's fade in */
    uint32_t dlat, dw, fresh, run;       /* the dry's delay, its write place; 1: just cleared; the bus's */
    uint32_t bits8, stereo;
    float dry[XA_DRY][2];
} xa;
static volatile uint32_t p_xhalf, p_xbits8, p_xstereo = 1u;

void psx_xa_set(uint32_t half_rate, uint32_t bits8, uint32_t stereo)
{
    p_xhalf = half_rate ? 1u : 0u;
    p_xbits8 = bits8 ? 1u : 0u;
    p_xstereo = stereo ? 1u : 0u;
}

BRK_COLD static void xa_clear(void)
{
    uint8_t *z = (uint8_t *)&xa;
    uint32_t i;
    for (i = 0; i < sizeof xa; i++)
        z[i] = 0u;
    xa.fresh = 1u;
    xa.stereo = p_xstereo;
}

static float xa_lat(void) { return (float)CL_LEAD * 65536.0f * xa.iinc + 1.0f + xa.aad; }
float psx_xa_latency(void) { return xa.inc ? xa_lat() : 0.0f; }

BRK_COLD static void xa_block(void)
{
    float hz = p_xhalf ? 18900.0f : 37800.0f;
    uint32_t inc = clk_inc(xa.inc, xa.iinc, &hz);
    if (inc != xa.inc) {
        xa.inc = inc;
        xa.iinc = 1.0f / (float)inc;
        xa.g = brk_tan_pi(brk_clampf(0.45f * hz, 1000.0f, 19000.0f) / 44100.0f);
        xa.aad = (CL_K1 + CL_K2) / (2.0f * xa.g);
    }
    xa.bits8 = p_xbits8;
    if (p_xstereo && !xa.stereo)         /* (mono to stereo: the right side starts where the one side is) */
        xa.ch[1] = xa.ch[0];
    xa.stereo = p_xstereo;
}

static void xa_unit(uint32_t k)          /* a unit gathered (k: the codec index of its last sample) */
{
    int16_t out[28];
    uint32_t c, j, base = k - 27u;
    for (c = 0; c < (xa.stereo ? 2u : 1u); c++) {
        xa_ch_t *h = &xa.ch[c];
        cd_unit(h->blk, h->hist, xa.bits8 ? 8u : 4u, xa.bits8, 4u, 0u, &ENC_CLEAN, out);
        for (j = 0; j < 28u; j++)
            h->dec[(base + j) & (CL_RING - 1u)] = out[j];
    }
}

static inline float xa_play(const int16_t *dec, int32_t i, uint32_t fr)   /* Catmull-Rom at i + fr / 256 */
{
    float a = (float)dec[(uint32_t)(i - 1) & (CL_RING - 1u)], b = (float)dec[(uint32_t)i & (CL_RING - 1u)],
          d = (float)dec[(uint32_t)(i + 1) & (CL_RING - 1u)], e = (float)dec[(uint32_t)(i + 2) & (CL_RING - 1u)],
          t = (float)fr * (1.0f / 256.0f);
    return (b + 0.5f * t * (d - a + t * (2.0f * a - 5.0f * b + 4.0f * d - e + t * (3.0f * (b - d) + e - a)))) * S2F;
}

/* XA over a block, in place: mix 0..1 against the dry delayed to match; want: the bus's fade toward 1 (0: out) */
static void xa_proc(float *l, float *r, uint32_t n, float mix, uint32_t want)
{
    uint32_t i, inc, dl0, dl1, xf, st;
    float iinc, g, dm, df, rn = 1.0f / (float)(n ? n : 1u);
    xa_block();
    inc = xa.inc;
    iinc = xa.iinc;
    g = xa.g;
    st = xa.stereo;
    mix = brk_clampf(mix, 0.0f, 1.0f);
    dl1 = (uint32_t)(xa_lat() + 0.5f);
    if (xa.fresh) {
        xa.mix_s = mix;
        xa.dlat = dl1;
        xa.fresh = 0u;
    }
    dl0 = xa.dlat;
    xf = dl0 != dl1;
    dm = (mix - xa.mix_s) * rn;
    df = ((want ? 1.0f : 0.0f) - xa.fade) * rn;
    {
        uint32_t acc = xa.acc, dw = xa.dw, nb = xa.nb;
        float m = xa.mix_s, f = xa.fade, p0 = xa.ch[0].prev, p1 = xa.ch[1].prev;
        for (i = 0; i < n; i++) {
            float x0 = l[i], x1 = r[i], y0, y1 = 0.0f, w0, w1;
            uint32_t a = acc, d = (0u - a) & (CL_ONE - 1u);
            xa.dry[dw][0] = x0;
            xa.dry[dw][1] = x1;
            m += dm;
            f += df;
            if (st) {
                y0 = svf_lp(&xa.ch[0].z[2], svf_lp(&xa.ch[0].z[0], x0, g, CL_K1), g, CL_K2);
                y1 = svf_lp(&xa.ch[1].z[2], svf_lp(&xa.ch[1].z[0], x1, g, CL_K1), g, CL_K2);
            } else {
                y0 = svf_lp(&xa.ch[0].z[2], svf_lp(&xa.ch[0].z[0], 0.5f * (x0 + x1), g, CL_K1), g, CL_K2);
            }
            while (d < inc) {            /* the codec's samples between the input before and this one */
                float ph = (float)d * iinc;
                xa.ch[0].blk[nb] = (int16_t)f2s(p0 + (y0 - p0) * ph);
                if (st)
                    xa.ch[1].blk[nb] = (int16_t)f2s(p1 + (y1 - p1) * ph);
                if (++nb == 28u) {
                    xa_unit((a + d) >> 16);
                    nb = 0;
                }
                d += CL_ONE;
            }
            p0 = y0;
            p1 = y1;
            {                            /* playback, CL_LEAD codec samples behind the clock */
                int32_t pi = (int32_t)(a >> 16) - CL_LEAD;
                uint32_t fr = (a >> 8) & 0xFFu;
                w0 = xa_play(xa.ch[0].dec, pi, fr);
                w1 = st ? xa_play(xa.ch[1].dec, pi, fr) : w0;
            }
            acc = a + inc;
            if (m < 1.0f || xf) {        /* the mix, against the dry delayed by the latency */
                uint32_t q0 = (dw - dl0) & (XA_DRY - 1u);
                float d0 = xa.dry[q0][0], d1 = xa.dry[q0][1];
                if (xf) {
                    uint32_t q1 = (dw - dl1) & (XA_DRY - 1u);
                    float t = (float)(i + 1u) * rn;
                    d0 += (xa.dry[q1][0] - d0) * t;
                    d1 += (xa.dry[q1][1] - d1) * t;
                }
                if (m < 1.0f) {
                    w0 = d0 + (w0 - d0) * m;
                    w1 = d1 + (w1 - d1) * m;
                }
            }
            if (f < 1.0f) {              /* the bus's fade: in from (or out to) the sound as it came */
                w0 = x0 + (w0 - x0) * f;
                w1 = x1 + (w1 - x1) * f;
            }
            l[i] = w0;
            r[i] = w1;
            dw = (dw + 1u) & (XA_DRY - 1u);
        }
        xa.acc = acc;
        xa.dw = dw;
        xa.nb = nb;
        xa.ch[0].prev = p0;
        xa.ch[1].prev = p1;
    }
    xa.mix_s = mix;
    xa.fade = want ? 1.0f : 0.0f;
    xa.dlat = dl1;
}

void psx_xa_process(float *l, float *r, uint32_t n, float mix)
{
    if (mix <= 0.0f)
        return;
    xa.fade = 1.0f;                      /* (called alone: no fade, the stage at once) */
    xa_proc(l, r, n, mix, 1u);
}

/* ================================================================================================== CRUSH --- */
const char *const CRUSH_MODE_NAME[CRUSH_MODES] = {"TRUNC", "ROUND", "DITHER"};
static volatile float p_cbits = 16.0f, p_cmask;
static volatile uint32_t p_cmode = CRUSH_ROUND;
static uint32_t crush_rnd = 0x1234567u;
void psx_crush_set(float bits, uint32_t mode, float mask)
{
    p_cbits = brk_clampf(bits, 1.0f, 16.0f);
    p_cmode = mode < CRUSH_MODES ? mode : CRUSH_ROUND;
    p_cmask = brk_clampf(mask, 0.0f, 1.0f);
}
static inline float crush1(float x, float q, float iq, uint32_t mode, int32_t flip)
{
    float y = x * q, f;
    int32_t k;
    if (mode == CRUSH_DITHER)            /* TPDF: two uniform draws, one step wide */
        y += brk_randf(&crush_rnd) - brk_randf(&crush_rnd);
    f = mode == CRUSH_TRUNC ? y : (y < 0.0f ? y - 0.5f : y + 0.5f);
    k = (int32_t)f;
    if (mode == CRUSH_TRUNC && f < 0.0f && (float)k != f)
        k--;                             /* (a cheap converter floors) */
    if (flip) {                          /* MASK: one bit of the 16-bit word flipped on every sample */
        int32_t s = sat16((int32_t)((float)k * iq * 32768.0f)) ^ flip;
        return (float)s * S2F;
    }
    return (float)k * iq;
}
void psx_crush_process(float *l, float *r, uint32_t n, float mix)
{
    uint32_t i, mode = p_cmode;
    float b = p_cbits, q = brk_exp2f(b - 1.0f), iq = 1.0f / q, m = p_cmask;
    int32_t flip = m > 0.0f ? 1 << (4 + (int32_t)(m * 10.0f + 0.5f)) : 0;
    if (mix <= 0.0f)
        return;
    for (i = 0; i < n; i++) {
        l[i] += (crush1(l[i], q, iq, mode, flip) - l[i]) * mix;
        r[i] += (crush1(r[i], q, iq, mode, flip) - r[i]) * mix;
    }
}

/* ================================================================================================== REDUX --- */
const char *const REDUX_MODE_NAME[REDUX_MODES] = {"HARD", "SOFT", "JITTER"};
static volatile float p_rhz = 44100.0f, p_rjit;
static volatile uint32_t p_rmode;
static struct { float ph, hl, hr, al, ar, bl, br, next; uint32_t rnd; } rx = {0, 0, 0, 0, 0, 0, 0, 1.0f, 0x9E3779B9u};
void psx_redux_set(float hz, uint32_t mode, float jitter)
{
    p_rhz = brk_clampf(hz, 200.0f, 44100.0f);
    p_rmode = mode < REDUX_MODES ? mode : 0u;
    p_rjit = brk_clampf(jitter, 0.0f, 1.0f);
}
void psx_redux_process(float *l, float *r, uint32_t n, float mix)
{
    uint32_t i, mode = p_rmode;
    float step = p_rhz / 44100.0f, tc = brk_tan_pi(brk_clampf(0.45f * p_rhz, 20.0f, 20000.0f) / 44100.0f),
          g = tc / (1.0f + tc), jit = mode == REDUX_JITTER ? (p_rjit > 0.0f ? p_rjit : 0.5f) : 0.0f;
    if (mix <= 0.0f)
        return;
    for (i = 0; i < n; i++) {
        float xl = l[i], xr = r[i];
        if (mode == REDUX_SOFT) {        /* two one-poles at under half the new rate: fewer aliases */
            rx.al += g * (xl - rx.al); rx.bl += g * (rx.al - rx.bl); xl = rx.bl;
            rx.ar += g * (xr - rx.ar); rx.br += g * (rx.ar - rx.br); xr = rx.br;
        }
        rx.ph += step;
        if (rx.ph >= rx.next) {          /* the converter's clock ticks: a new sample held */
            rx.ph -= rx.next;
            rx.hl = xl;
            rx.hr = xr;
            rx.next = 1.0f + jit * 0.9f * (brk_randf(&rx.rnd) - 0.5f) * 2.0f;
            if (rx.next < 0.1f)
                rx.next = 0.1f;
        }
        l[i] += (rx.hl - l[i]) * mix;
        r[i] += (rx.hr - r[i]) * mix;
    }
}

/* =================================================================================================== SKIP --- */
const char *const SKIP_DIV_NAME[4] = {"1/64", "1/32", "1/16", "1/8"};
static volatile uint32_t p_sdiv = 2u;
static volatile float p_schance, p_sseek;
static struct {
    uint8_t *ring;                       /* (mu-law: a byte a sample, the disc's grit) */
    uint32_t frames, w;                  /* the capture, its length, where the next frame goes */
    uint32_t on, left, len, reps;        /* skipping: samples left in this repeat, its length, repeats to go */
    float rd, rate;                      /* where it reads (frames back from w at its start), its speed */
    uint32_t start;                      /* the repeat's first frame */
    float fade;                          /* in and out of the skip (0..1) */
    int32_t last_div;                    /* the last boundary seen */
    uint32_t rnd;
} sk = {0, 0, 0, 0, 0, 0, 0, 0.0f, 1.0f, 0, 0.0f, -1, 0x2545F491u};
void psx_skip_init(uint8_t *ring, uint32_t frames)
{
    uint32_t i;
    sk.ring = ring;
    sk.frames = frames;
    for (i = 0; ring && i < 2u * frames; i++)
        ring[i] = 0xFFu;                 /* (mu-law's silence) */
    sk.w = 0;
    sk.on = 0;
}
uint32_t psx_skip_active(void) { return sk.on || sk.fade > 0.01f; }
void psx_skip_set(uint32_t div, float chance, float seek)
{
    p_sdiv = div < 4u ? div : 2u;
    p_schance = brk_clampf(chance, 0.0f, 1.0f);
    p_sseek = brk_clampf(seek, 0.0f, 1.0f);
}
void psx_skip_process(float *l, float *r, uint32_t n, float beat, float spb, float mix)
{
    uint32_t i;
    float per = spb * 4.0f / (float)(64u >> (p_sdiv * 1u));   /* a 1/64 .. 1/8 note in samples */
    float divs_per_beat = spb / per;
    if (!sk.ring || spb <= 0.0f)
        return;
    if (per > (float)sk.frames - 64.0f)
        per = (float)sk.frames - 64.0f;
    for (i = 0; i < n; i++) {
        float b = beat + (float)i / spb, yl, yr;
        int32_t d = (int32_t)(b * divs_per_beat);
        uint32_t wi = sk.w;
        sk.ring[2u * wi] = psx_ulaw_enc(f2s(l[i]));
        sk.ring[2u * wi + 1u] = psx_ulaw_enc(f2s(r[i]));
        sk.w = wi + 1u >= sk.frames ? 0u : wi + 1u;
        if (d != sk.last_div) {          /* a boundary: maybe the laser slips */
            sk.last_div = d;
            if (sk.on && sk.reps) {
                sk.reps--;
                sk.left = sk.len;
                sk.rd = 0.0f;
            } else if (sk.on) {
                sk.on = 0;
            } else if (mix > 0.0f && p_schance > 0.0f && brk_randf(&sk.rnd) < p_schance) {
                sk.on = 1u;
                sk.len = (uint32_t)per;
                sk.left = sk.len;
                sk.start = (wi + sk.frames - sk.len) % sk.frames;
                sk.reps = brk_rand(&sk.rnd) % 3u;
                sk.rd = 0.0f;
                sk.rate = 1.0f;
            }
        }
        /* fade 64 samples in and out (no clicks at the cuts) */
        sk.fade += ((sk.on ? 1.0f : 0.0f) - sk.fade) * (1.0f / 48.0f);
        if (sk.fade < 0.0005f) {
            sk.fade = 0.0f;
            continue;
        }
        {
            uint32_t p = (sk.start + (uint32_t)sk.rd) % sk.frames;
            float edge = 1.0f;
            float pos = sk.rd, rem = (float)sk.len - pos;
            if (pos < 32.0f)
                edge = pos / 32.0f;      /* (each repeat's own edges, a short ramp) */
            if (rem < 32.0f)
                edge = rem > 0.0f ? rem / 32.0f : 0.0f;
            yl = (float)PSX_ULAW_DEC[sk.ring[2u * p]] * S2F * edge;
            yr = (float)PSX_ULAW_DEC[sk.ring[2u * p + 1u]] * S2F * edge;
            if (sk.on) {
                sk.rd += sk.rate;        /* SEEK: the repeat slows as the laser searches */
                sk.rate -= p_sseek * 1.5f / (float)(sk.len ? sk.len : 1u);
                if (sk.rate < 0.25f)
                    sk.rate = 0.25f;
                if (sk.rd >= (float)sk.len) {
                    sk.rd = 0.0f;
                    sk.rate = 1.0f;
                }
            }
            yl = l[i] + (yl - l[i]) * sk.fade;
            yr = r[i] + (yr - r[i]) * sk.fade;
        }
        l[i] += (yl - l[i]) * mix;
        r[i] += (yr - r[i]) * mix;
    }
}

/* =============================================================================================== the bus --- */
const char *const CODEC_NAME[CODECS] = {"OFF", "GRIT", "XA"};
static volatile psx_bus_t p_bus;
static psx_bus_t bus;                    /* the render's copy */
static float send_s, crush_s, redux_s;   /* the gestures, eased per block (no steps heard) */
static float vgain_s;                    /* the send into SPU VERB at the last block's end (each block glides from it) */
static uint32_t verb_idle = 1000u;       /* blocks with nothing in or out of SPU VERB (it rests: no CPU spent) */

BRK_COLD void psx_bus_defaults(psx_bus_t *b)
{
    uint8_t *z = (uint8_t *)b;
    uint32_t i;
    for (i = 0; i < sizeof *b; i++)
        z[i] = 0u;
    b->vtype = PSXV_SPACE_ECHO;
    b->vdepth = 0.8f;
    b->vsize = 1.0f / 1.0f;
    b->vtone = 0.2f;
    b->cmode = CRUSH_ROUND;
    b->rmode = REDUX_HARD;
    b->rjit = 0.5f;
    b->codec = CODEC_OFF;
    b->crate = 22050.0f;
    b->cbits = 4u;
    b->xstereo = 1u;
    b->cmix = 1.0f;
    b->skdiv = 2u;
    b->skmix = 1.0f;
}
void psx_bus_set(const psx_bus_t *b) { *(psx_bus_t *)&p_bus = *b; }
static uint32_t bus_shed;
void psx_bus_shed(uint32_t level) { bus_shed = level; }
float psx_crush_bits(float crush) { return 16.0f - 14.0f * brk_clampf(crush, 0.0f, 1.0f); }
float psx_redux_hz(float redux) { return 44100.0f * brk_exp2f(-7.785f * brk_clampf(redux, 0.0f, 1.0f)); }
uint32_t psx_verb_busy(void) { return verb_idle < 8u; }
BRK_COLD void psx_verb_taps(uint32_t type, float size, float *ms, float *gain)
{
    static const uint8_t C[4] = {R_MLCOMB1, R_MLCOMB2, R_MLCOMB3, R_MLCOMB4}, V[4] = {R_VCOMB1, R_VCOMB2, R_VCOMB3,
                                                                                     R_VCOMB4};
    const uint16_t *r = PRESET[type < PSXV_N ? type : 0u];
    float s = size <= 1.0f ? 0.25f + 0.75f * size : size;
    uint32_t k;
    for (k = 0; k < 4u; k++) {           /* (the comb reads the reflections written at mLSAME: the gap, 8-byte units, 4
                                          * cells each at 22.05 kHz) */
        int32_t d = (int32_t)r[R_MLSAME] - (int32_t)r[C[k]];
        ms[k] = (float)(d < 0 ? -d : d) * 4.0f * s * (1000.0f / 22050.0f);
        gain[k] = (float)(int16_t)r[V[k]] * (1.0f / 32768.0f);
    }
}
float psx_verb_tail(uint32_t type)
{
    static const float T[PSXV_N] = {0.80f, 0.95f, 1.10f, 2.35f, 2.25f, 1.35f, 4.2f, 9.9f, 0.75f};   /* (CHAOS: past the
                                          * 8 s measured) */
    return T[type < PSXV_N ? type : 0u];
}

void psx_bus_process(float *l, float *r, const float *vsend, uint32_t n, float beat, float spb)
{
    bus = *(const psx_bus_t *)&p_bus;
    send_s += (brk_clampf(bus.send, 0.0f, 1.0f) - send_s) * 0.3f;
    crush_s += (brk_clampf(bus.crush, 0.0f, 1.0f) - crush_s) * 0.3f;
    redux_s += (brk_clampf(bus.redux, 0.0f, 1.0f) - redux_s) * 0.3f;
    /* CODEC: the sound as the console's samples (rests first when the render runs over) */
    if (bus_shed >= 1u) {
        cl.run = 0u;                     /* (shed: GRIT and XA stop at once, as before; back, they start afresh) */
        xa.run = 0u;
    } else {
        uint32_t want = bus.codec == CODEC_GRIT && bus.cmix > 0.0f;
        if (want && !cl.run) {           /* GRIT on: cleared, a block run unheard (its latency filled), then faded in */
            cl_clear();
            cl.run = 1u;
        }
        if (cl.run) {                    /* (and off: faded out over a block, to the sound undelayed) */
            psx_grit_set(bus.crate, bus.cbits, bus.cfilt);
            cl_process(l, r, n, bus.cmix, want && cl.run == 2u);
            if (cl.run == 1u)
                cl.run = 2u;
            else if (!want && cl.fade <= 0.0f)
                cl.run = 0u;
        }
        want = bus.codec == CODEC_XA && bus.cmix > 0.0f;
        if (want && !xa.run) {           /* XA the same way */
            psx_xa_set(bus.xhalf, 0u, bus.xstereo);
            xa_clear();
            xa.run = 1u;
        }
        if (xa.run) {
            psx_xa_set(bus.xhalf, 0u, bus.xstereo);
            xa_proc(l, r, n, bus.cmix, want && xa.run == 2u);
            if (xa.run == 1u)
                xa.run = 2u;
            else if (!want && xa.fade <= 0.0f)
                xa.run = 0u;
        }
    }
    /* SPU VERB: what's sent in, the reverb added in place; it rests once nothing has gone in or come out for ~8 s */
    if (vb.wk && bus_shed < 3u) {
        float s = send_s + brk_clampf(bus.vthrow, 0.0f, 1.0f), ei = 0.0f, eo = 0.0f;
        int sending = s > 0.0005f || vsend || bus.vfreeze > 0.0f;
        if (sending)
            verb_idle = 0u;
        if (verb_idle < 1400u) {
            psx_verb_set(bus.vtype, bus.vdepth, bus.vsize, bus.vtone, bus.vfreeze);
            verb_core(l, r, vgain_s, s, vsend, l, r, n, 1, &ei, &eo);   /* (the send glides over the block) */
            if (ei < 1e-4f && eo < 1e-4f && bus.vfreeze <= 0.0f)
                verb_idle++;
        }
        vgain_s = s;
    }
    /* CRUSH, REDUX: the gestures, off at 0 */
    if (crush_s > 0.002f) {
        psx_crush_set(psx_crush_bits(crush_s), bus.cmode, bus.cmask);
        psx_crush_process(l, r, n, 1.0f);
    }
    if (redux_s > 0.002f) {
        psx_redux_set(psx_redux_hz(redux_s), bus.rmode, bus.rjit);
        psx_redux_process(l, r, n, 1.0f);
    }
    /* SKIP: a disc that can't read (a skip under way finishes, its fade too, after the chance is let go: else its
     * state would stay on) */
    if (bus_shed < 2u && ((bus.skchance > 0.0f && bus.skmix > 0.0f) || psx_skip_active())) {
        psx_skip_set(bus.skdiv, bus.skchance, bus.skseek);
        psx_skip_process(l, r, n, beat, spb, bus.skmix);
    }
}

/* ================================================================================================== mu-law --- */
uint8_t psx_ulaw_enc(int32_t s)
{
    uint32_t sign = 0u, mag, seg = 0u;
    if (s < 0) {
        sign = 0x80u;
        s = -s;
    }
    mag = (uint32_t)(s > 32635 ? 32635 : s) + 132u;   /* (the bias: segments from 0) */
    while (seg < 7u && mag >= (256u << seg))
        seg++;
    return (uint8_t)~(sign | seg << 4 | ((mag >> (seg + 3u)) & 15u));
}
const int16_t PSX_ULAW_DEC[256] = {
#define U(b) (int16_t)((((~(b)) & 0x80) ? -1 : 1) * ((((((~(b)) & 15) << 3) + 132) << (((~(b)) >> 4) & 7)) - 132))
#define U8(b) U(b), U(b + 1), U(b + 2), U(b + 3), U(b + 4), U(b + 5), U(b + 6), U(b + 7)
#define U32(b) U8(b), U8(b + 8), U8(b + 16), U8(b + 24)
    U32(0), U32(32), U32(64), U32(96), U32(128), U32(160), U32(192), U32(224)
#undef U32
#undef U8
#undef U
};

/* =================================================================================================== init --- */
static void verb_clear(void)            /* the work area, the resamplers, the eased settings: silence */
{
    uint32_t i;
    for (i = 0; vb.wk && i <= vb.msk; i++)
        vb.wk[i] = 0;
    for (i = 0; i < 78u; i++)
        vb.in_h[0][i] = vb.in_h[1][i] = 0;
    vb.in_i = 0u;
    for (i = 0; i < 40u; i++)
        vb.out_h[0][i] = vb.out_h[1][i] = 0;
    vb.out_i = vb.odd = 0u;
    vb.held[0] = vb.held[1] = 0;
    vb.tone_z[0] = vb.tone_z[1] = 0.0f;
    vb.xfade = 0u;
    vb.eo = 0.0f;
    vb.hush = 1000u;
    vb.glided = 0u;
    vb.in_s = 1.0f;
}

BRK_COLD void psx_init(void)
{
    verb_clear();
    verb_idle = 1000u;
    send_s = crush_s = redux_s = vgain_s = 0.0f;
    if (sk.ring)
        psx_skip_init(sk.ring, sk.frames);
    cl_clear();
    psx_xa_set(0u, 0u, 1u);
    xa_clear();
    psx_grit_set(22050.0f, 4u, 0u);
    psx_color_set(PSXC_RATE_GRIT, 0u, 1u, 0.0f, 1.0f);
    psx_crush_set(16.0f, CRUSH_ROUND, 0.0f);
    psx_redux_set(44100.0f, REDUX_HARD, 0.0f);
    psx_skip_set(2u, 0.0f, 0.0f);
    psx_verb_set(PSXV_ROOM, 0.75f, 1.0f, 0.0f, 0.0f);
    psx_verb_input(1.0f);
    {
        psx_bus_t b;
        psx_bus_defaults(&b);
        psx_bus_set(&b);
    }
}
