/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - pixbuf manipulation routines for graphical effects.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This file contains pixbuf manipulation routines used for graphical effects like pre-lighting
   and selection hilighting */

#include <config.h>
#include "nautilus-graphic-effects.h"

/* shared utility to create a new pixbuf from the passed-in one */

static GdkPixbuf *
create_new_pixbuf (GdkPixbuf *src)
{
	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);

	return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       gdk_pixbuf_get_has_alpha (src),
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
}

static GdkPixbuf *
create_new_pixbuf_with_alpha (GdkPixbuf *src)
{
	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);

	return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       TRUE,
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
}

/* utility routine to bump the level of a color component with pinning */

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value;
	new_value += 24 + (new_value >> 3);
	if (new_value > 255) {
		new_value = 255;
	}
	return (guchar) new_value;
}

GdkPixbuf *
nautilus_create_spotlight_pixbuf (GdkPixbuf* src)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}


/* the following routine was stolen from the panel to darken a pixbuf, by manipulating the saturation */

/* saturation is 0-255, darken is 0-255 */

GdkPixbuf *
nautilus_create_darkened_pixbuf (GdkPixbuf *src, int saturation, int darken)
{
	gint i, j;
	gint width, height, src_row_stride, dest_row_stride;
	gboolean has_alpha;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;
	guchar intensity;
	guchar alpha;
	guchar negalpha;
	guchar r, g, b;
	GdkPixbuf *dest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dest_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dest_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {
			r = *pixsrc++;
			g = *pixsrc++;
			b = *pixsrc++;
			intensity = (r * 77 + g * 150 + b * 28) >> 8;
			negalpha = ((255 - saturation) * darken) >> 8;
			alpha = (saturation * darken) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * r) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * g) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * b) >> 8;
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

/* this routine colorizes the passed-in pixbuf by multiplying each pixel with the passed in color */

GdkPixbuf *
nautilus_create_colorized_pixbuf (GdkPixbuf *src,
				  int red_value,
				  int green_value,
				  int blue_value)
{
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	GdkPixbuf *dest;
	
	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*dst_row_stride;
		pixsrc = original_pixels + i*src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = (*pixsrc++ * red_value) >> 8;
			*pixdest++ = (*pixsrc++ * green_value) >> 8;
			*pixdest++ = (*pixsrc++ * blue_value) >> 8;
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

/* draw a frame with a drop shadow into the passed in pixbuf */

void
nautilus_draw_frame (GdkPixbuf *frame_pixbuf)
{
	int index, h_index, last_index, pin_width;
	int width, height, depth, rowstride, fill_value;
  	guchar *pixels, *temp_pixels;
	
	width = gdk_pixbuf_get_width (frame_pixbuf);
	height = gdk_pixbuf_get_height (frame_pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (frame_pixbuf);
	pixels = gdk_pixbuf_get_pixels (frame_pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (frame_pixbuf);

	/* loop through the pixbuf a scaleline at a time, drawing the frame */
	for (index = 0; index < height; index++) {
		/* special case the first and last few lines to make them dark */
		fill_value = 239;
		pin_width = width;
		last_index = height - index;
		if (index == 0)
			fill_value = 0;
		else if (index > (height - 6)) {
			fill_value = (index - (height - 5)) * 50;
			pin_width = width - (height - index);
		}	
		/* memset(pixels, fill_value, rowstride); */
		temp_pixels = pixels;
		for (h_index = 0; h_index < pin_width; h_index++) {
			*temp_pixels++ = fill_value;
			*temp_pixels++ = fill_value;
			*temp_pixels++ = fill_value;
			*temp_pixels++ = ((last_index < 6) && ((4 - last_index) > h_index)) ? 0 : 255;
		}
		
		/* draw the frame at the edge for each scanline */
		if (last_index > 5) {
			temp_pixels = pixels;
			*temp_pixels++ = 0;
			*temp_pixels++ = 0;
			*temp_pixels++ = 0;
			*temp_pixels++ = 255;
		}
		
		pixels += rowstride;
		
		/* handle the last 5 pixels specially for the drop shadow */
		
		temp_pixels = pixels - 5*4;

		if (last_index > 4) {
			*temp_pixels++ = 0;
			*temp_pixels++ = 0;
			*temp_pixels++ = 0;		
			*temp_pixels++ = 255;
		} else temp_pixels += 4;
		
		if (last_index > 3) {			
			*temp_pixels++ = 50;
			*temp_pixels++ = 50;
			*temp_pixels++ = 50;		
			*temp_pixels++ = index < 1 ? 0 : 255;
		}  else temp_pixels += 4;
		
		if (last_index > 2) {
			*temp_pixels++ = 100;
			*temp_pixels++ = 100;
			*temp_pixels++ = 100;		
			*temp_pixels++ = index < 2 ? 0 : 255;
		}  else temp_pixels += 4;
		
		if (last_index > 1) {
			*temp_pixels++ = 150;
			*temp_pixels++ = 150;
			*temp_pixels++ = 150;		
			*temp_pixels++ = index < 3 ? 0 : 255;
		}  else temp_pixels += 4;
		
		if (last_index > 0) {
			*temp_pixels++ = 200;
			*temp_pixels++ = 200;
			*temp_pixels++ = 200;		
			*temp_pixels++ = index < 4 ? 0 : 255;
		}
	}
}


/* this routine takes the source pixbuf and returns a new one that's semi-transparent, by
   clearing every other pixel's alpha value in a checkerboard grip.  We have to do the
   checkerboard instead of reducing the alpha since it will be turned into an alpha-less
   gdkpixmap and mask for the actual dragging */

GdkPixbuf *
nautilus_make_semi_transparent (GdkPixbuf *src)
{
	gint i, j, temp_alpha;
	gint width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;
	guchar alpha_value;
	GdkPixbuf *dest_pixbuf;
	guchar start_alpha_value;
	
	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest_pixbuf = create_new_pixbuf_with_alpha (src);

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest_pixbuf);
	
	/* set up pointers to the actual pixels */
	target_pixels = gdk_pixbuf_get_pixels (dest_pixbuf);
	original_pixels = gdk_pixbuf_get_pixels (src);

	/* loop through the pixels to do the actual work, copying from the source to the destination */
	start_alpha_value = ~0;
	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		alpha_value = start_alpha_value;
		for (j = 0; j < width; j++) {
			*pixdest++ = *pixsrc++; /* red */
			*pixdest++ = *pixsrc++; /* green */
			*pixdest++ = *pixsrc++; /* blue */
			
			if (has_alpha) {
				temp_alpha = *pixsrc++;
			} else {
				temp_alpha = ~0;
			}
			*pixdest++ = temp_alpha & alpha_value;
			
			alpha_value = ~alpha_value;
		}
		
		start_alpha_value = ~start_alpha_value;
	}
	
	return dest_pixbuf;
}
