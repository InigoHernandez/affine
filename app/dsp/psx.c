/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE's PS1-era effects (psx.h). AFFINE's own code; the hardware's documented numbers come from psx-spx
 * (https://psx-spx.consoledev.net/: "SPU Reverb Examples", "Reverb Buffer Resampling", "CDROM XA-ADPCM"). */
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
static volatile float p_vdepth = 0.75f, p_vsize = 1.0f, p_vtone, p_vfreeze;
static struct {
    int16_t *wk;
    uint32_t msk, cur;                   /* the work area, its mask, the buffer address (cells) */
    uint32_t type;                       /* the preset in use (a change clears the area: the hardware's garbage avoided) */
    int32_t off[R_N];                    /* this block's addresses in cells (the presets' x 4, scaled by SIZE) */
    int32_t v[R_N];                      /* this block's volumes (signed 16-bit) */
    float size_s, depth_s, tone_z[2];    /* SIZE and DEPTH eased, TONE's one-pole */
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
}

void psx_verb_set(uint32_t type, float depth, float size, float tone, float freeze)
{
    p_vtype = type < PSXV_N ? type : 0u;
    p_vdepth = brk_clampf(depth, 0.0f, 1.0f);
    p_vsize = brk_clampf(size, 0.0f, 4.0f);
    p_vtone = brk_clampf(tone, 0.0f, 1.0f);
    p_vfreeze = brk_clampf(freeze, 0.0f, 1.0f);
}

BRK_COLD static void verb_block(void)
{
    uint32_t t = p_vtype, i, maxo = 0;
    float s, fr = p_vfreeze, k;
    if (t != vb.type) {                  /* a new preset: the area cleared (its old reflections would read as noise) */
        for (i = 0; i <= vb.msk; i++)
            vb.wk[i] = 0;
        vb.type = t;
    }
    for (i = R_MLSAME; i <= R_MRAPF2; i++)
        if (PRESET[t][i] > maxo)
            maxo = PRESET[t][i];
    /* SIZE: 0..1 a quarter .. the hardware's; past 1 as far as the area allows (small rooms can grow) */
    s = p_vsize <= 1.0f ? 0.25f + 0.75f * p_vsize : p_vsize;
    if (maxo && s * (float)(maxo * 4u + PRESET[t][R_DAPF1] * 4u) > (float)(vb.msk - 2u))
        s = (float)(vb.msk - 2u) / (float)(maxo * 4u + PRESET[t][R_DAPF1] * 4u);
    vb.size_s += (s - vb.size_s) * 0.2f; /* (eased: the addresses glide, a tape's pitch bend, not clicks) */
    for (i = 0; i < R_N; i++) {
        uint32_t r = PRESET[t][i];
        if (i == R_DAPF1 || i == R_DAPF2 || (i >= R_MLSAME && i <= R_MRAPF2) || i == R_DLSAME || i == R_DRSAME)
            vb.off[i] = (int32_t)((float)(r * 4u) * vb.size_s + 0.5f);
        vb.v[i] = (int32_t)(int16_t)r;
    }
    /* (dLSAME..dRDIFF sit inside the m range of the enum above: R_DLSAME..R_DRDIFF are scaled by that test too) */
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

/* the reverb over n samples: in = src x gain + vsend (each side; vsend mono, 0 none), out written (add 0) or added
 * (add 1: in place over src is fine, each sample read before it is written); the input's and the output's sums of
 * magnitudes back (so the bus knows when it may rest) */
static void verb_core(const float *srcl, const float *srcr, float gain, const float *vsend, float *outl, float *outr,
                      uint32_t n, int add, float *ein, float *eout)
{
    uint32_t i, k, ch;
    float tc = brk_tan_pi(1500.0f * brk_exp2f(3.5f * (1.0f - p_vtone)) / 44100.0f), g = tc / (1.0f + tc);
    float d0 = vb.depth_s, d1 = p_vdepth, dd, ei = 0.0f, eo = 0.0f;
    verb_block();
    vb.depth_s = d1;
    dd = (d1 - d0) / (float)(n ? n : 1u);
    for (i = 0; i < n; i++) {
        float x[2], v = vsend ? vsend[i] : 0.0f, ol, or_;
        int32_t y[2];
        x[0] = srcl[i] * gain + v;
        x[1] = srcr[i] * gain + v;
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
            verb_tick(rin[0], rin[1], &y[0], &y[1]);
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
    verb_core(inl, inr, 1.0f, 0, outl, outr, n, 0, &ei, &eo);
}

/* ======================================================================================= SPU GRIT and XA --- */
/* the predictors (psx-spx, "XA-ADPCM": pos_xa_adpcm_table, neg_xa_adpcm_table; SPU-ADPCM has five, XA four) */
static const int16_t PRED[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
/* the interpolator: a four-term Nuttall window over four samples (psx-spx describes the console's "gaussian" table
 * as a four-term cosine window of the Nuttall family, normalised to 255/256): AFFINE computes its own from Nuttall's
 * coefficients (1981) at init. NUT[i] = w(i / 256), i = 0..512 (x = 0..2 samples from the centre) */
static int16_t NUT[513];
BRK_COLD static void nut_init(void)
{
    uint32_t i;
    const float a0 = 0.355768f, a1 = 0.487396f, a2 = 0.144232f, a3 = 0.012604f, sc = 8160.0f / 0.355768f;
    for (i = 0; i <= 512u; i++) {
        float x = (float)i / 256.0f;     /* cos(pi x / 2) = sin of a quarter turn past x / 4 */
        float c1 = brk_sin_turns(0.25f + x * 0.25f), c2 = brk_sin_turns(0.25f + x * 0.5f),
              c3 = brk_sin_turns(0.25f + x * 0.75f);
        float w = sc * (a0 + a1 * c1 + a2 * c2 + a3 * c3);
        NUT[i] = (int16_t)(w < 0.0f ? w - 0.5f : w + 0.5f);
    }
}

#define CD_RING 128u                     /* decoded samples, a power of two */
typedef struct {
    float z[4];                          /* the anti-alias filter before the codec's rate: two SVF stages' states */
    float prev;                          /* the input sample before (the fractional sampling) */
    int16_t blk[28];                     /* the unit being gathered */
    uint32_t nb;
    int32_t old, older;                  /* the decoder's history (the encoder runs it too: no drift) */
    int16_t dec[CD_RING];
    uint32_t wpos;                       /* decoded samples written */
    float rpos;                          /* where playback reads, in decoded samples (trails wpos by ~1.3 units) */
} cd_ch_t;
typedef struct {
    cd_ch_t ch[2];
    float ph;                            /* the input side's phase toward the next codec sample (0..1) */
    float rate, step, inv;               /* the codec's rate, rate / 44100, its inverse */
    float g, k1, k2;                     /* the anti-alias filter: 4th-order Butterworth at 0.45 x rate (TPT SVFs) */
    uint32_t bits, filter, nfilt, xa, stereo, on;
} codec_t;
static codec_t grit, xa;

/* one unit of 28 through predictor f at shift sh: the console's decode of the nearest nibbles (step 2^(ex - sh));
 * the squared error back; write: the decoded samples into the ring and the history */
static float cd_try(cd_ch_t *h, uint32_t f, int32_t sh, int32_t ex, int32_t qmin, int32_t qmax, int write)
{
    int32_t o = h->old, oo = h->older, k = ex - sh, half, f0 = PRED[f][0], f1 = PRED[f][1];
    uint32_t j;
    float err = 0.0f;
    if (k < 0)
        k = 0;
    half = k ? 1 << (k - 1) : 0;
    for (j = 0; j < 28u; j++) {
        int32_t p = (o * f0 + oo * f1 + 32) >> 6, q = (h->blk[j] - p + half) >> k, s, d;
        q = q < qmin ? qmin : q > qmax ? qmax : q;
        s = sat16(((q << ex) >> sh) + p);
        d = s - h->blk[j];
        err += (float)(d * d);
        oo = o;
        o = s;
        if (write) {
            h->dec[h->wpos & (CD_RING - 1u)] = (int16_t)s;
            h->wpos++;
        }
    }
    if (write) {
        h->old = o;
        h->older = oo;
    }
    return err;
}

/* one unit of 28: each predictor's residual on the input ranked, the two best simulated at the finest shift that
 * holds their peak (the decoder's own history: what the console would hear), the better one decoded into the ring */
static void cd_unit(codec_t *c, cd_ch_t *h)
{
    uint32_t f, f0 = 0, f1 = c->nfilt - 1u, j, cand[2] = {0, 0};
    int32_t qmax = c->xa && c->bits >= 8u ? 127 : (1 << (c->bits - 1u)) - 1, qmin = -qmax - 1;
    int32_t ex = c->xa && c->bits >= 8u ? 8 : 12;   /* the nibble (or byte) is shifted up by 12 (or 8) */
    int32_t peak[5] = {0, 0, 0, 0, 0}, sh[2] = {12, 12};
    float en[5], e0 = -1.0f, e1 = -1.0f, ea, eb;
    if (c->filter && c->filter <= c->nfilt)
        f0 = f1 = c->filter - 1u;
    for (f = f0; f <= f1; f++) {         /* the residual's energy and peak on the input */
        int32_t o = h->old, oo = h->older;
        float e = 0.0f;
        for (j = 0; j < 28u; j++) {
            int32_t p = (o * PRED[f][0] + oo * PRED[f][1] + 32) >> 6, r = h->blk[j] - p;
            e += (float)r * (float)r;
            r = r < 0 ? -r : r;
            if (r > peak[f])
                peak[f] = r;
            oo = o;
            o = h->blk[j];
        }
        en[f] = e;
        if (e0 < 0.0f || e < e0) {
            e1 = e0; cand[1] = cand[0];
            e0 = e; cand[0] = f;
        } else if (e1 < 0.0f || e < e1) {
            e1 = e; cand[1] = f;
        }
    }
    if (e1 < 0.0f)
        cand[1] = cand[0];
    for (j = 0; j < 2u; j++) {           /* the finest shift whose largest nibble reaches the peak */
        int32_t s;
        for (s = 12; s > 0; s--)
            if (((int32_t)qmax << ex >> s) >= peak[cand[j]])
                break;
        sh[j] = s;
    }
    ea = cd_try(h, cand[0], sh[0], ex, qmin, qmax, 0);
    eb = cand[1] != cand[0] ? cd_try(h, cand[1], sh[1], ex, qmin, qmax, 0) : ea;
    j = eb < ea ? 1u : 0u;
    cd_try(h, cand[j], sh[j], ex, qmin, qmax, 1);
    (void)en;
}

static inline int32_t cd_at(const cd_ch_t *h, int32_t i) { return h->dec[(uint32_t)i & (CD_RING - 1u)]; }

/* one input sample in, one output sample out (44.1 kHz both sides; the codec between them at its own rate). ph: where
 * the next codec sample falls after the input sample before, in input samples (cd_process advances it) */
/* a TPT state-variable low-pass stage (Zavalishin's form): g = tan(pi fc / fs), k = 1 / Q */
static inline float svf_lp(float *z, float x, float g, float k)
{
    float hp = (x - (k + g) * z[0] - z[1]) / (1.0f + g * (g + k)), bp = g * hp + z[0], lp = g * bp + z[1];
    z[0] = g * hp + bp;
    z[1] = g * bp + lp;
    return lp;
}

static float cd_sample(codec_t *c, cd_ch_t *h, float x, float ph)
{
    int32_t i, a, b, d, e;
    uint32_t fr;
    float pos, out, y;
    /* the anti-alias filter: the console's samples were prepared at their rate, band-limited */
    y = svf_lp(&h->z[2], svf_lp(&h->z[0], x, c->g, c->k1), c->g, c->k2);
    while (ph < 1.0f) {                  /* the codec's samples between the input before and this one */
        h->blk[h->nb++] = (int16_t)f2s(h->prev + (y - h->prev) * ph);
        if (h->nb == 28u) {
            cd_unit(c, h);
            h->nb = 0;
        }
        ph += c->inv;
    }
    h->prev = y;
    /* playback: four decoded samples; SPU through the cosine window (the voice's soft top), XA through a cubic
     * (the CD decoder's interpolation is a long, flatter filter: psx-spx, "25-point Zigzag Interpolation") */
    pos = h->rpos;
    i = (int32_t)pos;
    fr = (uint32_t)((pos - (float)i) * 256.0f);
    a = cd_at(h, i - 1); b = cd_at(h, i); d = cd_at(h, i + 1); e = cd_at(h, i + 2);
    if (!c->xa) {
        out = (float)(NUT[256u + fr] * a + NUT[fr] * b + NUT[256u - fr] * d + NUT[512u - fr] * e) * (1.0f / 32768.0f);
    } else {                             /* Catmull-Rom */
        float t = (float)fr * (1.0f / 256.0f), fa = (float)a, fb = (float)b, fd = (float)d, fe = (float)e;
        out = fb + 0.5f * t * (fd - fa + t * (2.0f * fa - 5.0f * fb + 4.0f * fd - fe + t * (3.0f * (fb - fd) + fe - fa)));
    }
    pos += c->step;
    if (pos >= 1024.0f && h->wpos >= 1024u + 64u) {   /* (both wound back together: float keeps its fraction) */
        pos -= 1024.0f;
        h->wpos -= 1024u;
    }
    h->rpos = pos;
    return out * S2F;
}

static void cd_set(codec_t *c, float rate, uint32_t bits, uint32_t filter, uint32_t isxa, uint32_t stereo)
{
    c->rate = brk_clampf(rate, 2000.0f, 44100.0f);
    c->step = c->rate / 44100.0f;
    c->inv = 44100.0f / c->rate;
    c->g = brk_tan_pi(brk_clampf(0.45f * c->rate, 1000.0f, 19000.0f) / 44100.0f);
    c->k1 = 1.0f / 0.5412f;              /* Butterworth, 4th order: Q 0.541 and 1.307 */
    c->k2 = 1.0f / 1.3066f;
    c->bits = bits < 2u ? 2u : bits > 8u ? 8u : bits;
    if (!isxa && c->bits > 4u)
        c->bits = 4u;
    if (isxa && c->bits != 8u && c->bits > 4u)
        c->bits = 4u;
    c->filter = filter;
    c->nfilt = isxa ? 4u : 5u;
    c->xa = isxa;
    c->stereo = stereo;
}

static void cd_process(codec_t *c, float *l, float *r, uint32_t n, float mix)
{
    uint32_t i;
    if (mix <= 0.0f)
        return;
    for (i = 0; i < n; i++) {
        float a, b, ph = c->ph;
        if (c->stereo) {
            a = cd_sample(c, &c->ch[0], l[i], ph);
            b = cd_sample(c, &c->ch[1], r[i], ph);
        } else {
            a = b = cd_sample(c, &c->ch[0], 0.5f * (l[i] + r[i]), ph);
        }
        while (ph < 1.0f)
            ph += c->inv;
        c->ph = ph - 1.0f;
        l[i] += (a - l[i]) * mix;
        r[i] += (b - r[i]) * mix;
    }
}

static void cd_clear(codec_t *c)
{
    uint32_t k, j;
    for (k = 0; k < 2u; k++) {
        cd_ch_t *h = &c->ch[k];
        h->z[0] = h->z[1] = h->z[2] = h->z[3] = h->prev = 0.0f;
        h->nb = 0;
        h->old = h->older = 0;
        for (j = 0; j < CD_RING; j++)
            h->dec[j] = 0;
        h->wpos = 64u;                   /* (playback starts 64 decoded samples behind: past a whole unit) */
        h->rpos = 0.0f;
    }
    c->ph = 0.0f;
}

void psx_grit_set(float rate, uint32_t bits, uint32_t filter) { cd_set(&grit, rate, bits, filter, 0u, 1u); }
void psx_grit_process(float *l, float *r, uint32_t n, float mix) { cd_process(&grit, l, r, n, mix); }
void psx_xa_set(uint32_t half_rate, uint32_t bits8, uint32_t stereo)
{
    cd_set(&xa, half_rate ? 18900.0f : 37800.0f, bits8 ? 8u : 4u, 0u, 1u, stereo);
}
void psx_xa_process(float *l, float *r, uint32_t n, float mix) { cd_process(&xa, l, r, n, mix); }

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
    /* CODEC: the sound as the console's samples */
    if (bus.codec == CODEC_GRIT) {
        psx_grit_set(bus.crate, bus.cbits, bus.cfilt);
        psx_grit_process(l, r, n, bus.cmix);
    } else if (bus.codec == CODEC_XA) {
        psx_xa_set(bus.xhalf, 0u, bus.xstereo);
        psx_xa_process(l, r, n, bus.cmix);
    }
    /* SPU VERB: what's sent in, the reverb added in place; it rests once nothing has gone in or come out for ~8 s */
    if (vb.wk) {
        float s = send_s + brk_clampf(bus.vthrow, 0.0f, 1.0f), ei = 0.0f, eo = 0.0f;
        int sending = s > 0.0005f || vsend || bus.vfreeze > 0.0f;
        if (sending)
            verb_idle = 0u;
        if (verb_idle < 1400u) {
            psx_verb_set(bus.vtype, bus.vdepth, bus.vsize, bus.vtone, bus.vfreeze);
            verb_core(l, r, s, vsend, l, r, n, 1, &ei, &eo);
            if (ei < 1e-4f && eo < 1e-4f && bus.vfreeze <= 0.0f)
                verb_idle++;
        }
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
    if ((bus.skchance > 0.0f && bus.skmix > 0.0f) || psx_skip_active()) {
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
}

BRK_COLD void psx_init(void)
{
    nut_init();
    verb_clear();
    verb_idle = 1000u;
    send_s = crush_s = redux_s = 0.0f;
    if (sk.ring)
        psx_skip_init(sk.ring, sk.frames);
    cd_clear(&grit);
    cd_clear(&xa);
    psx_grit_set(22050.0f, 4u, 0u);
    psx_xa_set(0u, 0u, 1u);
    psx_crush_set(16.0f, CRUSH_ROUND, 0.0f);
    psx_redux_set(44100.0f, REDUX_HARD, 0.0f);
    psx_skip_set(2u, 0.0f, 0.0f);
    psx_verb_set(PSXV_ROOM, 0.75f, 1.0f, 0.0f, 0.0f);
    {
        psx_bus_t b;
        psx_bus_defaults(&b);
        psx_bus_set(&b);
    }
}
