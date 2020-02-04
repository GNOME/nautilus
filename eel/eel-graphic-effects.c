/* Eel - pixbuf manipulation routines for graphical effects.
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains pixbuf manipulation routines used for graphical effects like pre-lighting
 *  and selection hilighting */

#include <config.h>

#include "eel-graphic-effects.h"
#include "eel-glib-extensions.h"

#include <math.h>
#include <string.h>

/* shared utility to create a new pixbuf from the passed-in one */

static GdkPixbuf *
create_new_pixbuf (GdkPixbuf *src)
{
    g_assert (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB);
    g_assert ((!gdk_pixbuf_get_has_alpha (src)
               && gdk_pixbuf_get_n_channels (src) == 3)
              || (gdk_pixbuf_get_has_alpha (src)
                  && gdk_pixbuf_get_n_channels (src) == 4));

    return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
                           gdk_pixbuf_get_has_alpha (src),
                           gdk_pixbuf_get_bits_per_sample (src),
                           gdk_pixbuf_get_width (src),
                           gdk_pixbuf_get_height (src));
}

/* utility routine to bump the level of a color component with pinning */

const int HOVER_COMPONENT_ADDITION = 15;

static guchar
lighten_component (guchar cur_value)
{
    int new_value = cur_value;
    new_value = cur_value + HOVER_COMPONENT_ADDITION;
    if (new_value > 255)
    {
        new_value = 255;
    }
    return (guchar) new_value;
}

GdkPixbuf *
eel_create_spotlight_pixbuf (GdkPixbuf *src)
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

    for (i = 0; i < height; i++)
    {
        pixdest = target_pixels + i * dst_row_stride;
        pixsrc = original_pixels + i * src_row_stride;
        for (j = 0; j < width; j++)
        {
            *pixdest++ = lighten_component (*pixsrc++);
            *pixdest++ = lighten_component (*pixsrc++);
            *pixdest++ = lighten_component (*pixsrc++);
            if (has_alpha)
            {
                *pixdest++ = *pixsrc++;
            }
        }
    }
    return dest;
}

/* This routine colorizes %src by multiplying each pixel with colors in %dest. */

GdkPixbuf *
eel_create_colorized_pixbuf (GdkPixbuf *src,
                             GdkPixbuf *dest)
{
    int i, j;
    int width, height, has_alpha, src_row_stride, dst_row_stride;
    guchar *target_pixels;
    guchar *original_pixels;
    guchar *pixsrc;
    guchar *pixdest;

    g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
    g_return_val_if_fail (gdk_pixbuf_get_colorspace (dest) == GDK_COLORSPACE_RGB, NULL);

    g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
                           && gdk_pixbuf_get_n_channels (src) == 3)
                          || (gdk_pixbuf_get_has_alpha (src)
                              && gdk_pixbuf_get_n_channels (src) == 4), NULL);
    g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (dest)
                           && gdk_pixbuf_get_n_channels (dest) == 3)
                          || (gdk_pixbuf_get_has_alpha (dest)
                              && gdk_pixbuf_get_n_channels (dest) == 4), NULL);

    g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);
    g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (dest) == 8, NULL);

    has_alpha = gdk_pixbuf_get_has_alpha (src);
    width = gdk_pixbuf_get_width (src);
    height = gdk_pixbuf_get_height (src);
    src_row_stride = gdk_pixbuf_get_rowstride (src);
    dst_row_stride = gdk_pixbuf_get_rowstride (dest);
    target_pixels = gdk_pixbuf_get_pixels (dest);
    original_pixels = gdk_pixbuf_get_pixels (src);

    for (i = 0; i < height; i++)
    {
        pixdest = target_pixels + i * dst_row_stride;
        pixsrc = original_pixels + i * src_row_stride;
        for (j = 0; j < width; j++)
        {
            *pixdest = (*pixsrc++ **pixdest) >> 8;
            pixdest++;
            *pixdest = (*pixsrc++ **pixdest) >> 8;
            pixdest++;
            *pixdest = (*pixsrc++ **pixdest) >> 8;
            pixdest++;
            if (has_alpha)
            {
                *pixdest++ = *pixsrc++;
            }
        }
    }
    return dest;
}
