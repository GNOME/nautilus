/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-glyph.c - A wrapper for rsvg glyphs.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-glyph.h"
#include "nautilus-art-extensions.h"
#include "nautilus-scalable-font-private.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-debug-drawing.h"

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb.h>
#include <librsvg/art_rgba.h>
#include <librsvg/rsvg-ft.h>

static gboolean glyph_is_valid             (const NautilusGlyph *glyph);
static int      glyph_get_width_space_safe (const NautilusGlyph *glyph);
static int      glyph_get_height_space_safe (const NautilusGlyph *glyph);

/* Detail member struct */
struct NautilusGlyph
{
	RsvgFTGlyph *rsvg_glyph;
	int glyph_xy[2];
};

/**
 * nautilus_glyph_new:
 * @font: A NautilusScalableFont.
 * @text: Text to use to construct the glyph.
 * @text_length: How much of the given text to use.
 * @font_size: Font size to use when constructing the glyph.
 *
 * Returns: The newly constructed glyph.
 */
NautilusGlyph *
nautilus_glyph_new (const NautilusScalableFont *font,
		    int font_size,
		    const char *text,
		    int text_length)
{
	NautilusGlyph *glyph;
	RsvgFTGlyph *rsvg_glyph;
	const double affine[6] = { 1, 0, 0, 1, 0, 0 };
	int glyph_xy[2];
	
	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);
	g_return_val_if_fail (font_size > 0, NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);

	rsvg_glyph = rsvg_ft_render_string (nautilus_scalable_font_get_rsvg_context (font),
					    nautilus_scalable_font_get_rsvg_handle (font),
					    text,
					    text_length,
					    font_size,
					    font_size,
					    affine,
					    glyph_xy);
	g_return_val_if_fail (rsvg_glyph != NULL, NULL);

	glyph = g_new0 (NautilusGlyph, 1);
	glyph->rsvg_glyph = rsvg_glyph;
	glyph->glyph_xy[0] = glyph_xy[0];
	glyph->glyph_xy[1] = glyph_xy[1];

	return glyph;
}

/**
 * nautilus_glyph_free:
 * @glyph: A NautilusGlyph.
 *
 * Returns: Free the glyph and any resoures associate with it.
 */
void
nautilus_glyph_free (NautilusGlyph *glyph)
{
	g_return_if_fail (glyph != NULL);

	rsvg_ft_glyph_unref (glyph->rsvg_glyph);
	g_free (glyph);
}

static int
glyph_get_width_space_safe (const NautilusGlyph *glyph)
{
	g_return_val_if_fail (glyph != NULL, 0);
	g_return_val_if_fail (glyph->rsvg_glyph != NULL, 0);

	/* Check for the case when we have only spaces. */
	if (glyph->rsvg_glyph->width == 0 && glyph->rsvg_glyph->xpen > 0.0) {
		return (int) glyph->rsvg_glyph->xpen;
	}

	return glyph->rsvg_glyph->width;
}

static int
glyph_get_height_space_safe (const NautilusGlyph *glyph)
{
	g_return_val_if_fail (glyph != NULL, 0);
	g_return_val_if_fail (glyph->rsvg_glyph != NULL, 0);

	/* Check for the case when we have only spaces. */
	if (glyph->rsvg_glyph->width == 0 && glyph->rsvg_glyph->xpen > 0.0) {
		return 1;
	}

	return glyph->rsvg_glyph->height;
}

/**
 * nautilus_glyph_get_width:
 * @glyph: A NautilusGlyph.
 *
 * Returns: The width of the glyph.
 */
int
nautilus_glyph_get_width (const NautilusGlyph *glyph)
{
	g_return_val_if_fail (glyph_is_valid (glyph), 0);
	
	return glyph_get_width_space_safe (glyph);
}

/**
 * nautilus_glyph_get_height:
 * @glyph: A NautilusGlyph.
 *
 * Returns: The height of the glyph.
 */
int
nautilus_glyph_get_height (const NautilusGlyph *glyph)
{
	g_return_val_if_fail (glyph_is_valid (glyph), 0);

	return glyph_get_height_space_safe (glyph);
}

/**
 * nautilus_glyph_get_dimensions:
 * @glyph: A NautilusGlyph.
 *
 * Returns: An ArtIRect representing the dimensions occupied by the glyph.
 */
NautilusDimensions
nautilus_glyph_get_dimensions (const NautilusGlyph *glyph)
{
	NautilusDimensions glyph_dimensions;

	g_return_val_if_fail (glyph != NULL, NAUTILUS_DIMENSIONS_EMPTY);
	g_return_val_if_fail (glyph_is_valid (glyph), NAUTILUS_DIMENSIONS_EMPTY);

	glyph_dimensions.width = glyph_get_width_space_safe (glyph);
	glyph_dimensions.height = glyph_get_height_space_safe (glyph);

	return glyph_dimensions;
}

/**
 * nautilus_glyph_get_underline_rectangle:
 * @glyph: A NautilusGlyph.
 * @rectangle: The ArtIRect to store the underline dimensions in.
 *
 * Fills @rectangle with the dimensions of the underline rectangle suitable
 * for @glyph.
 */
void
nautilus_glyph_get_underline_rectangle (const NautilusGlyph *glyph,
					ArtIRect	    *rectangle)
{
	g_return_if_fail (glyph != NULL);
	g_return_if_fail (glyph_is_valid (glyph));

	rectangle->x0 = 0;
	rectangle->x1 = glyph_get_width_space_safe (glyph);
	rectangle->y0 = glyph->rsvg_glyph->underline_position;
	rectangle->y1 = rectangle->y0 + glyph->rsvg_glyph->underline_thickness;
}

/* A glyph is valid if IT and the RsvgGlyph it wraps area not NULL */
static gboolean
glyph_is_valid (const NautilusGlyph *glyph)
{
	return glyph != NULL
		&& glyph->rsvg_glyph != NULL
		&& glyph_get_width_space_safe (glyph) > 0
		&& glyph_get_height_space_safe (glyph) > 0;
}


/* Color packing macros for nautilus_glyph_draw_to_pixbuf */
#define RGBA_COLOR_PACK(r, g, b, a)	\
( ((r) << 24) |				\
  ((g) << 16) |				\
  ((b) <<  8) |				\
  ((a) <<  0) )

#define RGB_COLOR_PACK(r, g, b)		\
( ((r) << 24) |				\
  ((g) << 16) |				\
  ((b) <<  8) )

#define RGBA_COLOR_PACK_ALPHA(a)	(a)
#define RGBA_COLOR_GET_RED(c)		(((c) >> 24) & 0xFF)
#define RGBA_COLOR_GET_GREEN(c)		(((c) >> 16) & 0xFF)
#define RGBA_COLOR_GET_BLUE(c)		(((c) >> 8)  & 0xFF)
#define RGBA_COLOR_GET_ALPHA(c)		(((c) >> 0)  & 0xFF)

#define ABGR_COLOR_PACK(r, g, b, a)	\
( ((a) << 24) |				\
  ((b) << 16) |				\
  ((g) <<  8) |				\
  ((r) <<  0) )

#define BGR_COLOR_PACK(r, g, b)		\
( ((b) << 16) |				\
  ((g) <<  8) |				\
  ((r) <<  0) )

#define ABGR_COLOR_PACK_ALPHA(a)	((a) << 24)
#define ABGR_COLOR_GET_RED(c)		(((c) >> 0)  & 0xFF)
#define ABGR_COLOR_GET_GREEN(c)		(((c) >> 8)  & 0xFF)
#define ABGR_COLOR_GET_BLUE(c)		(((c) >> 16) & 0xFF)
#define ABGR_COLOR_GET_ALPHA(c)		(((c) >> 24) & 0xFF)



#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define PACK_COLOR_WITH_ALPHA(r, g, b, a)	ABGR_COLOR_PACK(r, g, b, a)
#define PACK_COLOR(r, g, b)			BGR_COLOR_PACK(r, g, b)
#define PACK_ALPHA(a)				ABGR_COLOR_PACK_ALPHA(a)
#define GET_RED(c)				ABGR_COLOR_GET_RED(c)
#define GET_GREEN(c)				ABGR_COLOR_GET_GREEN(c)
#define GET_BLUE(c)				ABGR_COLOR_GET_BLUE(c)
#define GET_ALPHA(c)				ABGR_COLOR_GET_ALPHA(c)
#else
#define PACK_COLOR_WITH_ALPHA(r, g, b, a) 	RGBA_COLOR_PACK(r, g, b, a)
#define PACK_COLOR(r, g, b)			RGB_COLOR_PACK(r, g, b)
#define PACK_ALPHA(a)				RGBA_COLOR_PACK_ALPHA(a)
#define GET_RED(c)				RGBA_COLOR_GET_RED(c)
#define GET_GREEN(c)				RGBA_COLOR_GET_GREEN(c)
#define GET_BLUE(c)				RGBA_COLOR_GET_BLUE(c)
#define GET_ALPHA(c)				RGBA_COLOR_GET_ALPHA(c)
#endif

/**
 * nautilus_glyph_draw_to_pixbuf:
 * @glyph: A NautilusGlyph.
 * @pixbuf: The destination GdkPixbuf.
 * @destination_x: The destination X coordinate.
 * @destination_y: The destination Y coordinate.
 * @clip_area: The clip area or NULL.
 * @color: Color to render the glyph with.
 * @opacity: Overall opacity of the rendered glyph.
 *
 * Render a glyph into a GdkPixbuf.   If &clip_area is not NULL, all
 * rendering is guaranteed to lie within &clip_area.  If &clip_are is NULL,
 * rendering will occur in the full extent of the pixbuf.
 */
void
nautilus_glyph_draw_to_pixbuf (const NautilusGlyph *glyph,
			       GdkPixbuf *pixbuf,
			       int destination_x,
			       int destination_y,
			       const ArtIRect *clip_area,
			       guint32 color,
			       int opacity)
{
 	int pixbuf_rowstride;
 	guchar *pixbuf_pixels;
 	guint pixbuf_pixel_offset;
 	gboolean pixbuf_has_alpha;
	ArtIRect target;
	guchar *pixbuf_x_offset;
	guchar *pixbuf_y_offset;
	int glyph_rowstride;
	const guchar *glyph_buffer;
	NautilusDimensions glyph_dimensions;
	ArtIRect glyph_bounds;
	const guchar *glyph_x_offset;
	const guchar *glyph_y_offset;
	int glyph_left_skip;
	int glyph_top_skip;
	ArtIRect render_area;
	int x;
	int y;
	const guchar foreground_r = NAUTILUS_RGBA_COLOR_GET_R (color);
	const guchar foreground_g = NAUTILUS_RGBA_COLOR_GET_G (color);
	const guchar foreground_b = NAUTILUS_RGBA_COLOR_GET_B (color);

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (glyph_is_valid (glyph));

	/* FIXME bugzilla.eazel.com 7346: We currently dont handle opacities
	 * other than 0xFF.
	 */
	g_return_if_fail (opacity == NAUTILUS_OPACITY_FULLY_OPAQUE);

	/* Check for just spaces */
	if (glyph->rsvg_glyph->buf == NULL || glyph->rsvg_glyph->rowstride <= 0) {
		return;
	}

	/* Clip the pixbuf to the clip area; bail if no work  */
	target = nautilus_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area);
	if (art_irect_empty (&target)) {
		return;
	}

	g_return_if_fail (glyph->rsvg_glyph->buf != NULL);
	g_return_if_fail (glyph->rsvg_glyph->rowstride > 0);
	
	glyph_dimensions = nautilus_glyph_get_dimensions (glyph);
	glyph_rowstride = glyph->rsvg_glyph->rowstride;
	glyph_buffer = glyph->rsvg_glyph->buf;

	pixbuf_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixbuf_pixels = gdk_pixbuf_get_pixels (pixbuf);
	pixbuf_has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixbuf_pixel_offset = pixbuf_has_alpha ? 4 : 3;

	/* Compute the whole glyph bounds */
	nautilus_art_irect_assign (&glyph_bounds,
				   destination_x,
				   destination_y,
				   glyph_dimensions.width,
				   glyph_dimensions.height);

	/*
	 * Determine the actual render area.  The render area is 
	 * the area of the glyph bounds that lies within the pixbuf
	 * clip area;  bail if no work.
	 */
	art_irect_intersect (&render_area, &target, &glyph_bounds);
	if (art_irect_empty (&render_area)) {
 		return;
 	}

	/* Compute the offset into the pixbuf where we want to render. */
	pixbuf_y_offset = 
		pixbuf_pixels
		+ (render_area.y0 * pixbuf_rowstride)
		+ (render_area.x0 * pixbuf_pixel_offset);

	/* Compute how much of the glyph to skip on the left */
	g_assert (render_area.y0 >= glyph_bounds.y0);
	glyph_left_skip = render_area.x0 - glyph_bounds.x0;

	/* Compute how much of the glyph to skip above */
	g_assert (render_area.x0 >= glyph_bounds.x0);
	glyph_top_skip = render_area.y0 - glyph_bounds.y0;

	/* The glyph buffer is an array of bytes.  Each byte
	 * represents the opacity corresponding to the pixel
	 * at that offset.
	 */

	/* Compute the offset into the glyph buffer from where we want to read
	 * it contents.
	 *
	 * The glyph buffer is an array of bytes.  Each byte represents an 
	 * opacity for a pixel at the corresponding offset.
	 */
	glyph_y_offset = 
		glyph_buffer
		+ (glyph_top_skip * glyph_rowstride)
		+ glyph_left_skip;


	/* Thanks to the careful clipping above, the iterations below
	 * should always be within the bounds of both the pixbuf's pixels
	 * and the glyph's buffer.
	 */

	if (pixbuf_has_alpha) {
		/* Speed optimization -- avoid calling art_rgba_run_alpha, precompute
		 * src_rgb outside of the loop
		 */
		 
		guint32 src_color, dst_color;
		
		src_color = PACK_COLOR(foreground_r, foreground_g, foreground_b);
		
		for (y = render_area.y0 ; y < render_area.y1; y++) {
			pixbuf_x_offset = pixbuf_y_offset;
			glyph_x_offset = glyph_y_offset;
			
			/* Iterate horizontally */
			for (x = render_area.x0 ; x < render_area.x1; x++) {
				guchar dest_alpha;
				const guchar source_alpha = *glyph_x_offset;
				dst_color = *(guint32 *) pixbuf_x_offset;
				dest_alpha = GET_ALPHA (dst_color);
		
				if (dest_alpha) { 
					int dst_r, dst_g, dst_b, tmp, a, c;

					tmp = (255 - source_alpha) * (255 - dest_alpha) + 0x80;
					a = 255 - ((tmp + (tmp >> 8)) >> 8);
					c = ((source_alpha << 16) + (a >> 1)) / a;

					dst_r = GET_RED (dst_color);
					dst_g = GET_GREEN (dst_color);
					dst_b = GET_BLUE (dst_color);
	
					dst_r += (((foreground_r - dst_r) * c + 0x8000) >> 16);
					dst_g += (((foreground_g - dst_g) * c + 0x8000) >> 16);
					dst_b += (((foreground_b - dst_b) * c + 0x8000) >> 16);
	
					*(guint32 *) pixbuf_x_offset = PACK_COLOR_WITH_ALPHA (dst_r, dst_g, dst_b, a);
				} else {
	  				*(guint32 *) pixbuf_x_offset = PACK_ALPHA (source_alpha) | src_color;
				}
	
				/* Advance to the next pixel */
				pixbuf_x_offset += pixbuf_pixel_offset;
	
				/* Advance to the next opacity */
				glyph_x_offset++;
			}
	
			/* Advance to the next pixbuf pixel row */
			pixbuf_y_offset += pixbuf_rowstride;
	
			/* Advance to the next glyph buffer row */
			glyph_y_offset += glyph_rowstride;
		}
	} else {
		/* Iterate vertically */
		for (y = render_area.y0 ; y < render_area.y1; y++) {
			pixbuf_x_offset = pixbuf_y_offset;
			glyph_x_offset = glyph_y_offset;
			
			/* Iterate horizontally */
			for (x = render_area.x0 ; x < render_area.x1; x++) {
				const guchar point_opacity = *glyph_x_offset;
	
				/* Optimize the common fully opaque case */
				if (point_opacity == NAUTILUS_OPACITY_FULLY_OPAQUE) {
					*(pixbuf_x_offset + 0) = foreground_r;
					*(pixbuf_x_offset + 1) = foreground_g;
					*(pixbuf_x_offset + 2) = foreground_b;
					if (pixbuf_has_alpha) {
						*(pixbuf_x_offset + 3) = NAUTILUS_OPACITY_FULLY_OPAQUE;
					}
				/* If the opacity is not fully opaque or fully transparent,
				 * we need to to alpha blending.
				 */
				} else if (point_opacity != NAUTILUS_OPACITY_FULLY_TRANSPARENT) {
					if (pixbuf_has_alpha) {
						art_rgba_run_alpha (pixbuf_x_offset,
								    foreground_r,
								    foreground_g,
								    foreground_b,
								    point_opacity,
								    1);
					} else {
						art_rgb_run_alpha (pixbuf_x_offset,
								   foreground_r,
								   foreground_g,
								   foreground_b,
								   point_opacity,
								   1);
					}
				}
	
				/* Advance to the next pixel */
				pixbuf_x_offset += pixbuf_pixel_offset;
	
				/* Advance to the next opacity */
				glyph_x_offset++;
			}
	
			/* Advance to the next pixbuf pixel row */
			pixbuf_y_offset += pixbuf_rowstride;
	
			/* Advance to the next glyph buffer row */
			glyph_y_offset += glyph_rowstride;
		}
	}
}

ArtIRect
nautilus_glyph_intersect (const NautilusGlyph *glyph,
			  int glyph_x,
			  int glyph_y,
			  const ArtIRect *rectangle)
{
	ArtIRect intersection;
	ArtIRect bounds;
	NautilusDimensions dimensions;

	g_return_val_if_fail (glyph_is_valid (glyph), NAUTILUS_ART_IRECT_EMPTY);
	
	dimensions = nautilus_glyph_get_dimensions (glyph);
	bounds = nautilus_art_irect_assign_dimensions (glyph_x, glyph_y, &dimensions);

	if (rectangle == NULL) {
		return bounds;
	}

	art_irect_intersect (&intersection, rectangle, &bounds);

	/* In theory, this is not needed because a rectangle is empty
	 * regardless of how MUCH negative the dimensions are.  
	 * However, to make debugging and self checks simpler, we
	 * consistenly return a standard empty rectangle.
	 */
	if (art_irect_empty (&intersection)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	return intersection;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#include <string.h>
#include <memory.h>

gboolean
nautilus_glyph_compare (NautilusGlyph *a, NautilusGlyph *b)
{
	int y;

	if (a->glyph_xy[0] != b->glyph_xy[0]
	    || a->glyph_xy[1] != b->glyph_xy[1]) {
		return FALSE;
	}

	if (a->rsvg_glyph->width != b->rsvg_glyph->width
	    || a->rsvg_glyph->height != b->rsvg_glyph->height
	    || a->rsvg_glyph->underline_position != b->rsvg_glyph->underline_position
	    || a->rsvg_glyph->underline_thickness != b->rsvg_glyph->underline_thickness
	    || a->rsvg_glyph->xpen != b->rsvg_glyph->xpen
	    || a->rsvg_glyph->ypen != b->rsvg_glyph->ypen
	    || a->rsvg_glyph->rowstride != b->rsvg_glyph->rowstride) {
		return FALSE;
	}

	for (y = 0; y < a->rsvg_glyph->height; y++) {
		if (memcmp (a->rsvg_glyph->buf + y * a->rsvg_glyph->rowstride,
			    b->rsvg_glyph->buf + y * b->rsvg_glyph->rowstride,
			    a->rsvg_glyph->rowstride) != 0) {
			return FALSE;
		}
	}

	return TRUE;
}

#endif /* NAUTILUS_OMIT_SELF_CHECK */
