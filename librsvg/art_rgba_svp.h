/*
 * art_rgba_svp.h: Render a sorted vector path over an RGBA buffer.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __ART_RGBA_SVP_H__
#define __ART_RGBA_SVP_H__

#include <libart_lgpl/art_alphagamma.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void
art_rgba_svp_alpha (const ArtSVP *svp,
		    int x0, int y0, int x1, int y1,
		    art_u32 rgba,
		    art_u8 *buf, int rowstride,
		    ArtAlphaGamma *alphagamma);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
