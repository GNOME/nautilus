#ifndef __ART_RGBA_SVP_H__
#define __ART_RGBA_SVP_H__

#include <libart_lgpl/art_alphagamma.h>

void
art_rgba_svp_alpha (const ArtSVP *svp,
		    int x0, int y0, int x1, int y1,
		    art_u32 rgba,
		    art_u8 *buf, int rowstride,
		    ArtAlphaGamma *alphagamma);

#endif
