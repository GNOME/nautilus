/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-font-factory.h: Class for obtaining fonts.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_FONT_FACTORY_H
#define NAUTILUS_FONT_FACTORY_H

#include <gdk/gdk.h>
#include <gtk/gtkobject.h>

/* A There's a single NautilusFontFactory object. */
GtkObject *nautilus_font_factory_get                       (void);


/* Get a font by familiy. */
GdkFont *  nautilus_font_factory_get_font_by_family        (const char *family,
							    guint       size_in_pixels);


/* Get a font according to the family set in preferences. */
GdkFont *  nautilus_font_factory_get_font_from_preferences (guint       size_in_pixels);

#endif /* NAUTILUS_FONT_FACTORY_H */
