/* ide-cairo.c
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

#include "ide-cairo.h"

cairo_region_t *
ide_cairo_region_create_from_clip_extents (cairo_t *cr)
{
  cairo_rectangle_int_t crect;
  GdkRectangle rect;

  g_return_val_if_fail (cr, NULL);

  gdk_cairo_get_clip_rectangle (cr, &rect);
  crect.x = rect.x;
  crect.y = rect.y;
  crect.width = rect.width;
  crect.height = rect.height;

  return cairo_region_create_rectangle (&crect);
}

void
ide_cairo_rounded_rectangle (cairo_t            *cr,
                            const GdkRectangle *rect,
                            gint                x_radius,
                            gint                y_radius)
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x1, x2;
  gint y1, y2;
  gint xr1, xr2;
  gint yr1, yr2;

  g_return_if_fail (cr);
  g_return_if_fail (rect);

  x = rect->x;
  y = rect->y;
  width = rect->width;
  height = rect->height;

  x1 = x;
  x2 = x1 + width;
  y1 = y;
  y2 = y1 + height;

  x_radius = MIN (x_radius, width / 2.0);
  y_radius = MIN (y_radius, width / 2.0);

  xr1 = x_radius;
  xr2 = x_radius / 2.0;
  yr1 = y_radius;
  yr2 = y_radius / 2.0;

  cairo_move_to (cr, x1 + xr1, y1);
  cairo_line_to (cr, x2 - xr1, y1);
  cairo_curve_to (cr, x2 - xr2, y1, x2, y1 + yr2, x2, y1 + yr1);
  cairo_line_to (cr, x2, y2 - yr1);
  cairo_curve_to (cr, x2, y2 - yr2, x2 - xr2, y2, x2 - xr1, y2);
  cairo_line_to (cr, x1 + xr1, y2);
  cairo_curve_to (cr, x1 + xr2, y2, x1, y2 - yr2, x1, y2 - yr1);
  cairo_line_to (cr, x1, y1 + yr1);
  cairo_curve_to (cr, x1, y1 + yr2, x1 + xr2, y1, x1 + xr1, y1);
  cairo_close_path (cr);
}
