/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-buffered-widget.h - A buffered widget for alpha compositing.

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

/* NautilusBufferedWidget is a virtual widget class that encapsulates the
 * details of finding a suitable background for compositing pixbufs. 

 * The background can be installed as NautilusBackground on a NautilusImage
 * widget or any of its ancestors.
 *
 * The background can also be that provided by the GtkStyle attatched to the 
 * widget.
 *
 * The best background will automatically be found and used by the widget.
 * 
 * Also, a tile_pixbuf can be installed to create tiling effects on top of 
 * the default background.
 */

#ifndef NAUTILUS_BUFFERED_WIDGET_H
#define NAUTILUS_BUFFERED_WIDGET_H

#include <gtk/gtkmisc.h>
#include <libgnome/gnome-defs.h>

#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_BUFFERED_WIDGET            (nautilus_buffered_widget_get_type ())
#define NAUTILUS_BUFFERED_WIDGET(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BUFFERED_WIDGET, NautilusBufferedWidget))
#define NAUTILUS_BUFFERED_WIDGET_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BUFFERED_WIDGET, NautilusBufferedWidgetClass))
#define NAUTILUS_IS_BUFFERED_WIDGET(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BUFFERED_WIDGET))
#define NAUTILUS_IS_BUFFERED_WIDGET_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BUFFERED_WIDGET))

typedef struct _NautilusBufferedWidget	        NautilusBufferedWidget;
typedef struct _NautilusBufferedWidgetClass     NautilusBufferedWidgetClass;
typedef struct _NautilusBufferedWidgetDetail    NautilusBufferedWidgetDetail;

struct _NautilusBufferedWidget
{
	/* Superclass */
	GtkMisc			misc;

	/* Private things */
	NautilusBufferedWidgetDetail	*detail;
};

struct _NautilusBufferedWidgetClass
{
	GtkMiscClass		parent_class;

	void (*render_buffer_pixbuf) (NautilusBufferedWidget *buffered_widget, GdkPixbuf *buffer);
};

GtkType    nautilus_buffered_widget_get_type        (void);
void       nautilus_buffered_widget_clear_buffer    (NautilusBufferedWidget       *buffered_widget);
void       nautilus_buffered_widget_set_tile_pixbuf (NautilusBufferedWidget       *image,
						     GdkPixbuf                    *pixbuf);
GdkPixbuf* nautilus_buffered_widget_get_tile_pixbuf (const NautilusBufferedWidget *image);

END_GNOME_DECLS

#endif /* NAUTILUS_BUFFERED_WIDGET_H */


