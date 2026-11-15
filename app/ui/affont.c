/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: every page writes in AFFINE's pixel font (app/aff_font.h: MONO2K's face, CC BY-SA 4.0), whole-pixel scales.
 * LOAF BREAKER's pages call fm1-base's cv_text / text_w with its anti-aliased faces (FONT_T, FONT_XS, FONT_B, each a
 * line height); here those calls are routed to the pixel face, the capitals centred in each face's line: FONT_B at 2x,
 * FONT_T at 1x, FONT_XS at AFF_XS_Z. Included by app.c before ui/common.c (the pages after it see the macros). */
#include "aff_font.h"

#ifndef AFF_XS_Z
#define AFF_XS_Z 1                       /* (FONT_XS's scale: LOAF's values and small names) */
#endif

/* ---------------------------------------------------------------------------------------------- the font --- */
static int32_t aff_text_w(const char *s, int32_t z)
{
    int32_t w = 0;
    for (; *s; s++) {
        uint32_t c = (uint8_t)*s;
        if (c >= 'a' && c <= 'z')
            c -= 32u;
        w += (int32_t)(c >= AFF_FIRST && c <= AFF_LAST ? AFF_W[c - AFF_FIRST] : 2u) * z + z;
    }
    return w > 0 ? w - z : 0;
}
/* s at (x, y) (y: the capitals' top), z times; band: the screen row the canvas starts at. align 0 left, 1 right,
 * 2 centre. Returns the x after it */
static int32_t aff_text(int32_t x, int32_t y, const char *s, int32_t z, uint16_t c, int32_t band, int align)
{
    if (align == 1)
        x -= aff_text_w(s, z);
    else if (align == 2)
        x -= aff_text_w(s, z) / 2;
    y -= (int32_t)AFF_CAP_TOP * z + band;   /* (cv_rect clips to the canvas's band) */
    for (; *s; s++) {
        uint32_t ch = (uint8_t)*s, g, r, w;
        if (ch >= 'a' && ch <= 'z')
            ch -= 32u;
        if (ch < AFF_FIRST || ch > AFF_LAST) {
            x += 3 * z;
            continue;
        }
        g = ch - AFF_FIRST;
        w = AFF_W[g];
        for (r = 0; r < AFF_ROWS; r++) {
            uint32_t bits = AFF_BITS[g * AFF_ROWS + r], k = 0;
            while (bits) {               /* runs of lit pixels as one rect each */
                uint32_t run = 0;
                while (!(bits & 0x80u)) {
                    bits = (bits << 1) & 0xFFu;
                    k++;
                }
                while (bits & 0x80u) {
                    bits = (bits << 1) & 0xFFu;
                    run++;
                }
                cv_rect(x + (int32_t)k * z, y + (int32_t)r * z, (int32_t)run * z, z, c);
                k += run;
            }
        }
        x += (int32_t)w * z + z;
    }
    return x - z;
}

/* LOAF's faces on AFFINE's: a face's scale, and where its capitals sit in its line */
static int32_t aff_fz(const felucca_font_t *f) { return f == &FONT_B ? 2 : f == &FONT_XS ? AFF_XS_Z : 1; }
static int32_t aff_foff(const felucca_font_t *f, int32_t z, int32_t n)
{
    int32_t o = ((int32_t)f->h * n - (int32_t)AFF_CAP_H * z) / 2;
    return o > 0 ? o : 0;
}
static int32_t aff_cv_textn(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint32_t n)
{
    int32_t z = aff_fz(f) * (int32_t)n;
    return aff_text(x, y + aff_foff(f, z, (int32_t)n), s, z, c, 0, 0) + z;   /* (an advance past the last, as fm1-base's) */
}
static int32_t aff_cv_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    return aff_cv_textn(x, y, f, s, c, 1u);
}
static int32_t aff_text_wf(const felucca_font_t *f, const char *s) { return aff_text_w(s, aff_fz(f)); }
#define cv_text aff_cv_text
#define text_w aff_text_wf
