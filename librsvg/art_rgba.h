#ifndef __ART_RGBA_H__
#define __ART_RGBA_H__

void
art_rgba_rgba_composite (art_u8 *dst, const art_u8 *src, int n);

void
art_rgba_fill_run (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int n);

void
art_rgba_run_alpha (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n);

#endif
