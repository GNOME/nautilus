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

#include "eel-graphic-effects.h"

GdkTexture *
eel_create_spotlight_texture (GdkTexture *texture)
{
    int width;
    int height;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned char *data;
    int stride;
    g_autoptr (GBytes) bytes = NULL;
    GdkTexture *prelit_texture;

    g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

    width = gdk_texture_get_width (texture);
    height = gdk_texture_get_height (texture);
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);
    data = cairo_image_surface_get_data (surface);
    stride = cairo_image_surface_get_stride (surface);

    gdk_texture_download (texture, data, stride);

    cairo_surface_mark_dirty (surface);

    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_ADD);

    cairo_push_group (cr);

    /* This is *close enough* to the original look.
     * The magic alpha value was selected after visual comparison.
     */
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.18);
    cairo_paint (cr);

    cairo_pop_group_to_source (cr);

    cairo_mask_surface (cr, surface, 0.0, 0.0);

    cairo_surface_flush (surface);

    bytes = g_bytes_new (data, height * stride);
    prelit_texture = gdk_memory_texture_new (width, height, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, bytes, stride);

    cairo_destroy (cr);
    cairo_surface_destroy (surface);

    return prelit_texture;
}
