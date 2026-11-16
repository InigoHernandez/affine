/* SPDX-License-Identifier: MPL-2.0 */
/* AFFINE: every page writes in AFFINE's pixel font (app/aff_font.h: MONO2K's face, CC BY-SA 4.0), whole-pixel scales.
 * LOAF BREAKER's pages call fm1-base's cv_text / text_w with its anti-aliased faces (FONT_T, FONT_XS, FONT_B, each a
 * line height); here those calls are routed to the pixel face, the capitals centred in each face's line: FONT_B at 2x,
 * FONT_T at 1x, FONT_XS at AFF_XS_Z. Included by app.c before ui/common.c (the pages after it see the macros). */
#include "aff_font.h"

#ifndef AFF_XS_Z
#define AFF_XS_Z 2                       /* (FONT_XS's scale: LOAF's values and small names; 1 until 0.1, which the
                                          * owner's FM-1 showed too small: 2x is the least, as MONO2K's) */
#endif
#ifndef AFF_T_Z
#define AFF_T_Z 2                        /* (FONT_T's: tabs, labels, help lines; 1 until 0.1) */
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
static int32_t aff_fz(const felucca_font_t *f) { return f == &FONT_B ? 2 : f == &FONT_XS ? AFF_XS_Z : AFF_T_Z; }
static int32_t aff_foff(const felucca_font_t *f, int32_t z, int32_t n)
{
    int32_t o = ((int32_t)f->h * n - (int32_t)AFF_CAP_H * z) / 2;
    return o > 0 ? o : 0;
}
/* ------------------------------------------------------------------------------------- the text audit --- */
/* The host's (AFF_AUDIT=1 in the environment; tests/scenarios.sh's logs, tools/text_audit.py): every text LOAF's pages
 * write into a canvas, from cv_begin to its blit; at the blit, a text past the canvas's edge, two that overlap, and one
 * under 2x (the owner, 0.1 on the FM-1: "everything in the UI looks very small"), each an "AUDIT" line */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#define AFF_AUD_N 96
static struct { int16_t x, y, w, h; uint8_t z; char s[24]; } aff_aud[AFF_AUD_N];
static int32_t aff_aud_n, aff_aud_on = -1;
static void aff_aud_add(int32_t x, int32_t ycap, int32_t z, const char *s)
{
    uint32_t i;
    if (aff_aud_on < 0) {
        const char *e = getenv("AFF_AUDIT");
        aff_aud_on = e && e[0] == '1';
    }
    if (!aff_aud_on || aff_aud_n >= AFF_AUD_N || !s[0])
        return;
    aff_aud[aff_aud_n].x = (int16_t)x;
    aff_aud[aff_aud_n].y = (int16_t)ycap;
    aff_aud[aff_aud_n].w = (int16_t)aff_text_w(s, z);
    aff_aud[aff_aud_n].h = (int16_t)(AFF_CAP_H * (uint32_t)z);
    aff_aud[aff_aud_n].z = (uint8_t)z;
    for (i = 0; i < 23u && s[i]; i++)
        aff_aud[aff_aud_n].s[i] = s[i];
    aff_aud[aff_aud_n].s[i] = 0;
    aff_aud_n++;
}
static void aff_aud_check(uint32_t bx, uint32_t by)
{
    int32_t i, j;
    for (i = 0; i < aff_aud_n; i++) {
        int32_t x = aff_aud[i].x, y = aff_aud[i].y, w = aff_aud[i].w, h = aff_aud[i].h;
        if (x < 0 || x + w > (int32_t)cv_w || y < 0 || y + h > (int32_t)cv_h)
            fprintf(stderr, "AUDIT edge \"%s\" at (%d,%d) %dx%d in %ux%u at (%u,%u)\n", aff_aud[i].s, (int)(x + (int32_t)bx),
                    (int)(y + (int32_t)by), (int)w, (int)h, (unsigned)cv_w, (unsigned)cv_h, (unsigned)bx, (unsigned)by);
        if (aff_aud[i].z < 2u)
            fprintf(stderr, "AUDIT small \"%s\" at (%d,%d)\n", aff_aud[i].s, (int)(x + (int32_t)bx), (int)(y + (int32_t)by));
        for (j = i + 1; j < aff_aud_n; j++) {
            int32_t x2 = aff_aud[j].x, y2 = aff_aud[j].y;
            if (x < x2 + aff_aud[j].w && x2 < x + w && y < y2 + aff_aud[j].h && y2 < y + h)
                fprintf(stderr, "AUDIT overlap \"%s\" and \"%s\" at (%d,%d)\n", aff_aud[i].s, aff_aud[j].s,
                        (int)(x + (int32_t)bx), (int)(y + (int32_t)by));
        }
    }
    aff_aud_n = 0;
}
static void aff_cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    aff_aud_n = 0;
    cv_begin(w, h, bg);
}
#define cv_begin aff_cv_begin            /* (the check at the blit: common.c's rows_send, every page's) */
#else
#define aff_aud_add(x, y, z, s) ((void)0)
#endif

static int32_t aff_cv_textn(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint32_t n)
{
    int32_t z = aff_fz(f) * (int32_t)n, yc = y + aff_foff(f, z, (int32_t)n);
    aff_aud_add(x, yc + cv_oy, z, s);    /* (the canvas's own row: a picture drawn in two strips moves by cv_oy) */
    return aff_text(x, yc, s, z, c, 0, 0) + z;   /* (an advance past the last, as fm1-base's) */
}
static int32_t aff_cv_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    return aff_cv_textn(x, y, f, s, c, 1u);
}
static int32_t aff_text_wf(const felucca_font_t *f, const char *s) { return aff_text_w(s, aff_fz(f)); }
#define cv_text aff_cv_text
#define text_w aff_text_wf
