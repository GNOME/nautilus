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

#include <librsvg/rsvg-ft.h>

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
		    guint font_size,
		    const char *text,
		    guint text_length)
{
	NautilusGlyph *glyph;
	RsvgFTGlyph *rsvg_glyph;
	const double affine[6] = { 1, 0, 0, 1, 0, 0 };
	int glyph_xy[2];

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);
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

/**
 * nautilus_glyph_get_width:
 * @glyph: A NautilusGlyph.
 *
 * Returns: The width of the glyph.
 */
int
nautilus_glyph_get_width (const NautilusGlyph *glyph)
{
	g_return_val_if_fail (glyph != NULL, 0);
	g_return_val_if_fail (glyph->rsvg_glyph != NULL, 0);

	return glyph->rsvg_glyph->width;
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
	g_return_val_if_fail (glyph != NULL, 0);
	g_return_val_if_fail (glyph->rsvg_glyph != NULL, 0);

	return glyph->rsvg_glyph->height;
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
	g_return_val_if_fail (glyph->rsvg_glyph != NULL, NAUTILUS_DIMENSIONS_EMPTY);

	glyph_dimensions.width = glyph->rsvg_glyph->width;
	glyph_dimensions.height = glyph->rsvg_glyph->height;

	return glyph_dimensions;
}

/* FIXME bugzilla.eazel.com xxxx: We should really use libart
 * over here to do the alpha compositing.  The reason why
 * im not doing so is because of the currently confusing 
 * location of libart stable/HEAD headers for some rgba functions.
 * It not hard to figure that out, Ill do so soon.  For now this
 * code works, even if not super optimized like the libart blending
 * code.
 */
static void
color_blend_with_opacity (guchar background_r,
			  guchar background_g,
			  guchar background_b,
			  guchar foreground_r,
			  guchar foreground_g,
			  guchar foreground_b,
			  int opacity,
			  guchar *blend_r_out,
			  guchar *blend_g_out,
			  guchar *blend_b_out)
{
	g_return_if_fail (opacity > NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity < NAUTILUS_OPACITY_FULLY_OPAQUE);
	g_return_if_fail (blend_r_out != NULL);
	g_return_if_fail (blend_g_out != NULL);
	g_return_if_fail (blend_b_out != NULL);

	/* This blending operation is the same as that in libart */
	*blend_r_out = background_r + (((foreground_r - background_r) * opacity + 0x80) >> 8);
	*blend_g_out = background_g + (((foreground_g - background_g) * opacity + 0x80) >> 8);
	*blend_b_out = background_b + (((foreground_b - background_b) * opacity + 0x80) >> 8);
}

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

	g_return_if_fail (glyph != NULL);
	g_return_if_fail (glyph->rsvg_glyph != NULL);
	g_return_if_fail (glyph->rsvg_glyph->buf != NULL);
	g_return_if_fail (glyph->rsvg_glyph->width > 0);
	g_return_if_fail (glyph->rsvg_glyph->height > 0);
	g_return_if_fail (glyph->rsvg_glyph->rowstride > 0);
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (destination_x >= 0 && destination_x < gdk_pixbuf_get_width (pixbuf));
	g_return_if_fail (destination_y >= 0 && destination_y < gdk_pixbuf_get_height (pixbuf));
	/* FIXME bugzilla.eazel.com xxxx: We currently dont handle opacities
	 * other than 0xFF.
	 */
	g_return_if_fail (opacity == NAUTILUS_OPACITY_FULLY_OPAQUE);

	/* Clip the pixbuf to the clip area; bail if no work  */
	target = nautilus_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area);
	if (art_irect_empty (&target)) {
		return;
	}
		
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

	/* Debug code to be yanked real soon */
	if (0) nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							   FALSE,
							   target.x0,
							   target.y0,
							   target.x1,
							   target.y1,
							   NAUTILUS_RGBA_COLOR_OPAQUE_GREEN,
							   NAUTILUS_OPACITY_FULLY_OPAQUE,
							   0);

	if (0) nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							   FALSE,
							   glyph_bounds.x0,
							   glyph_bounds.y0,
							   glyph_bounds.x1,
							   glyph_bounds.y1,
							   NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
							   NAUTILUS_OPACITY_FULLY_OPAQUE,
							   0);

	if (0) nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							   FALSE,
							   render_area.x0,
							   render_area.y0,
							   render_area.x1,
							   render_area.y1,
							   NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							   NAUTILUS_OPACITY_FULLY_OPAQUE,
							   -1);

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

			/* FIXME bugzilla.eazel.com xxxx: We should really use libart
			 * over here to do the alpha compositing.  The reason why
			 * im not doing so is because of the currently confusing 
			 * location of libart stable/HEAD headers for some rgba functions.
			 * It not hard to figure that out, Ill do so soon.  For now this
			 * code works, even if not super optimized like the libart blending
			 * code.
			 */
			} else if (point_opacity != NAUTILUS_OPACITY_FULLY_TRANSPARENT) {
				const guchar background_r = *(pixbuf_x_offset + 0);
				const guchar background_g = *(pixbuf_x_offset + 1);
				const guchar background_b = *(pixbuf_x_offset + 2);
				guchar blend_r;
				guchar blend_g;
				guchar blend_b;

				color_blend_with_opacity (background_r,
							  background_g,
							  background_b,
							  foreground_r,
							  foreground_g,
							  foreground_b,
							  point_opacity,
							  &blend_r,
							  &blend_g,
							  &blend_b);

				*(pixbuf_x_offset + 0) = blend_r;
				*(pixbuf_x_offset + 1) = blend_g;
				*(pixbuf_x_offset + 2) = blend_b;
				if (pixbuf_has_alpha) {
					*(pixbuf_x_offset + 3) = NAUTILUS_OPACITY_FULLY_OPAQUE;
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
