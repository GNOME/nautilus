/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-graphic-effects.h: Pixmap manipulation routines for graphical effects.

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
 
   Authors: Andy Hertzfeld <andy@eazel.com>
 */

#ifndef NAUTILUS_GRAPHIC_EFFECTS_H
#define NAUTILUS_GRAPHIC_EFFECTS_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* return a lightened pixbuf for pre-lighting */
GdkPixbuf* create_spotlight_pixbuf (GdkPixbuf* source_pixbuf);

/* return a darkened pixbuf for selection hiliting */
GdkPixbuf* create_darkened_pixbuf (GdkPixbuf *src, int saturation, int darken);

/* return a pixbuf colorized with the color specified by the parameter */
GdkPixbuf* create_colorized_pixbuf(GdkPixbuf *src, int red_value, int green_value, int blue_value);

/* return a semi-transparent pixbuf from the source pixbuf using a checkboard
   stipple in the alpha channel (so it can be converted to an alpha-less pixmap) */
GdkPixbuf* make_semi_transparent(GdkPixbuf *source_pixbuf);

#endif /* NAUTILUS_GRAPHIC_EFFECTS_H */
