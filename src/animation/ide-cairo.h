/* ide-cairo.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

cairo_region_t *ide_cairo_region_create_from_clip_extents (cairo_t            *cr);
void            ide_cairo_rounded_rectangle               (cairo_t            *cr,
                                                           const GdkRectangle *rect,
                                                           gint                x_radius,
                                                           gint                y_radius);

static inline gboolean
_ide_cairo_rectangle_x2 (const cairo_rectangle_int_t *rect)
{
  return rect->x + rect->width;
}

static inline gboolean
_ide_cairo_rectangle_y2 (const cairo_rectangle_int_t *rect)
{
  return rect->y + rect->height;
}

static inline gboolean
_ide_cairo_rectangle_center (const cairo_rectangle_int_t *rect)
{
  return rect->x + (rect->width/2);
}

static inline gboolean
_ide_cairo_rectangle_middle (const cairo_rectangle_int_t *rect)
{
  return rect->y + (rect->height/2);
}

static inline cairo_bool_t
_ide_cairo_rectangle_contains_rectangle (const cairo_rectangle_int_t *a,
                                         const cairo_rectangle_int_t *b)
{
    return (a->x <= b->x &&
            a->x + (int) a->width >= b->x + (int) b->width &&
            a->y <= b->y &&
            a->y + (int) a->height >= b->y + (int) b->height);
}

G_END_DECLS