/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-viewport.h - A subclass of GtkViewport with non broken drawing.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

#ifndef NAUTILUS_VIEWPORT_H
#define NAUTILUS_VIEWPORT_H

#include <gtk/gtkviewport.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-smooth-widget.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_VIEWPORT            (nautilus_viewport_get_type ())
#define NAUTILUS_VIEWPORT(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEWPORT, NautilusViewport))
#define NAUTILUS_VIEWPORT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEWPORT, NautilusViewportClass))
#define NAUTILUS_IS_VIEWPORT(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEWPORT))
#define NAUTILUS_IS_VIEWPORT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VIEWPORT))

typedef struct NautilusViewport	         NautilusViewport;
typedef struct NautilusViewportClass     NautilusViewportClass;
typedef struct NautilusViewportDetails   NautilusViewportDetails;

struct NautilusViewport
{
	/* Superclass */
	GtkViewport viewport;

	/* Private things */
	NautilusViewportDetails *details;
};

struct NautilusViewportClass
{
	GtkViewportClass parent_class;
	NautilusSmoothWidgetSetIsSmooth set_is_smooth;
};

GtkType           nautilus_viewport_get_type          (void);
GtkWidget *       nautilus_viewport_new               (GtkAdjustment          *hadjustment,
						       GtkAdjustment          *vadjustment);
void              nautilus_viewport_set_is_smooth     (NautilusViewport       *nautilus_viewport,
						       gboolean                is_smooth);
NautilusArtIPoint nautilus_viewport_get_scroll_offset (const NautilusViewport *nautilus_viewport);

END_GNOME_DECLS

#endif /* NAUTILUS_VIEWPORT_H */


