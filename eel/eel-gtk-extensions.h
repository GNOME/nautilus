
/* eel-gtk-extensions.h - interface for new functions that operate on
  			       gtk classes. Perhaps some of these should be
  			       rolled into gtk someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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
   see <http://www.gnu.org/licenses/>.

   Authors: John Sullivan <sullivan@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

/* GtkWindow */
char *                eel_gtk_window_get_geometry_string              (GtkWindow            *window);

/* GtkMenu and GtkMenuItem */
GtkMenuItem *         eel_gtk_menu_append_separator                   (GtkMenu              *menu);
GtkMenuItem *         eel_gtk_menu_insert_separator                   (GtkMenu              *menu,
								       int                   index);