/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-art-gtk-extensions.h - Access gtk/gdk attributes as libart rectangles.

   Copyright (C) 2000 Eazel, Inc.

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

/* The following functions accept gtk/gdk structures and
 * return their bounds/frames as ArtIRects, where:
 *
 * bounds: The (x,y) and (width, height) of something.
 * frame: The (width, height) of something.
 *
 * These are very useful in code that uses libart functions
 * to do operations on ArtIRects (such as intersection)
 */

#ifndef NAUTILUS_ART_GTK_EXTENSIONS_H
#define NAUTILUS_ART_GTK_EXTENSIONS_H

#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-art-extensions.h>

BEGIN_GNOME_DECLS

GdkRectangle nautilus_gdk_rectangle_assign_irect                  (const ArtIRect     *irect);
ArtIRect     nautilus_irect_assign_gdk_rectangle                  (const GdkRectangle *gdk_rectangle);
ArtIRect     nautilus_irect_screen_get_frame                      (void);
ArtIRect     nautilus_irect_gdk_window_get_bounds                 (const GdkWindow    *gdk_window);
ArtIRect     nautilus_irect_gdk_window_get_screen_relative_bounds (const GdkWindow    *gdk_window);
ArtIRect     nautilus_irect_gtk_widget_get_bounds                 (const GtkWidget    *gtk_widget);
ArtIRect     nautilus_irect_gtk_widget_get_frame                  (const GtkWidget    *gtk_widget);
ArtIRect     nautilus_irect_gdk_window_clip_dirty_area_to_screen  (const GdkWindow    *gdk_window,
								   const ArtIRect     *dirty_area);

END_GNOME_DECLS

#endif /* NAUTILUS_ART_GTK_EXTENSIONS_H */
