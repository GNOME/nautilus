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

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libgnomeui/gnome-canvas-util.h>

#include "nautilus-graphic-effects.h"

/* utility routine to bump the level of a color component with pinning */

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value;
	new_value += 24 + (new_value >> 3);
	if (new_value > 255)
		new_value = 255;
	return (guchar) new_value;
}

GdkPixbuf *
create_spotlight_pixbuf (GdkPixbuf* src)
{
	int i, j;
	int width, height, has_alpha, src_rowstride, dst_rowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	GdkPixbuf *dest;
	
	dest = gdk_pixbuf_new (gdk_pixbuf_get_format (src),
					 gdk_pixbuf_get_has_alpha (src),
					 gdk_pixbuf_get_bits_per_sample (src),
					 gdk_pixbuf_get_width (src),
					 gdk_pixbuf_get_height (src));
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	src_rowstride = gdk_pixbuf_get_rowstride (src);
	dst_rowstride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*dst_rowstride;
		pixsrc = original_pixels + i*src_rowstride;
		for (j = 0; j < width; j++) {		
			*(pixdest++) = lighten_component(*(pixsrc++));
			*(pixdest++) = lighten_component(*(pixsrc++));
			*(pixdest++) = lighten_component(*(pixsrc++));
			if (has_alpha) {
				*(pixdest++) = *(pixsrc++);
			}
		}
	}
	return dest;
}


/* the following routine was stolen from the panel to darken a pixbuf, by manipulating the saturation */

#define INTENSITY(r, g, b) (((r)*77 + (g)*150 + (b)*28)>>8)

/* saturation is 0-255, darken is 0-255 */

GdkPixbuf *
create_darkened_pixbuf (GdkPixbuf *src, int saturation, int darken)
{
	gint i, j;
	gint width, height, has_alpha, rowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	guchar intensity;
	guchar alpha;
	guchar negalpha;
	guchar r,g,b;
	GdkPixbuf *dest;

	dest = gdk_pixbuf_new (gdk_pixbuf_get_format (src),
					 gdk_pixbuf_get_has_alpha (src),
					 gdk_pixbuf_get_bits_per_sample (src),
					 gdk_pixbuf_get_width (src),
					 gdk_pixbuf_get_height (src));

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	rowstride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*rowstride;
		pixsrc = original_pixels + i*rowstride;
		for (j = 0; j < width; j++) {
			r = *(pixsrc++);
			g = *(pixsrc++);
			b = *(pixsrc++);
			intensity = INTENSITY(r,g,b);
			negalpha = ((255 - saturation)*darken)>>8;
			alpha = (saturation*darken)>>8;
			*(pixdest++) = (negalpha * intensity + alpha * r) >> 8;
			*(pixdest++) = (negalpha * intensity + alpha * g) >> 8;
			*(pixdest++) = (negalpha * intensity + alpha * b) >> 8;
			if (has_alpha)
				*(pixdest++) = *(pixsrc++);
		}
	}
	return dest;
}
#undef INTENSITY

/* this routine takes the source pixbuf and returns a new one that's semi-transparent, by
   clearing every other pixel's alpha value in a checkerboard grip.  We have to do the
   checkerboard instead of reducing the alpha since it will be turned into an alpha-less
   gdkpixmap and mask for the actual dragging */

GdkPixbuf * 
make_semi_transparent(GdkPixbuf *source_pixbuf)
{
	gint i, j, temp_alpha;
	gint width, height, has_alpha, src_rowstride, dst_rowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	guchar alpha_value;
	GdkPixbuf *dest_pixbuf;
	guchar start_alpha_value;
	
	has_alpha = gdk_pixbuf_get_has_alpha (source_pixbuf);
	width = gdk_pixbuf_get_width (source_pixbuf);
	height = gdk_pixbuf_get_height (source_pixbuf);
	src_rowstride = gdk_pixbuf_get_rowstride (source_pixbuf);
	
	/* allocate the destination pixbuf to be a clone of the source */

	dest_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_format (source_pixbuf),
				      TRUE,
				      gdk_pixbuf_get_bits_per_sample (source_pixbuf),
				      width,
				      height);
	dst_rowstride = gdk_pixbuf_get_rowstride (dest_pixbuf);
	
	/* set up pointers to the actual pixels */
	target_pixels = gdk_pixbuf_get_pixels (dest_pixbuf);
	original_pixels = gdk_pixbuf_get_pixels (source_pixbuf);

	/* loop through the pixels to do the actual work, copying from the source to the destination */
	
	start_alpha_value = ~0;
	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_rowstride;
		pixsrc = original_pixels + i * src_rowstride;
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
