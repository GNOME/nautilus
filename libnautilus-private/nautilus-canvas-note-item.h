/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-canvas-note-item.h: annotation canvas item for Nautilus
  
   Copyright (C) 2001 Eazel, Inc.
  
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
  
   based on gnome_canvas_rect_item by Federico Mena Quintero
   
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_CANVAS_NOTE_ITEM_H
#define NAUTILUS_CANVAS_NOTE_ITEM_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>

#include <libart_lgpl/art_svp.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_CANVAS_NOTE_ITEM	(nautilus_canvas_note_item_get_type ())
#define NAUTILUS_CANVAS_NOTE_ITEM(obj)	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CANVAS_NOTE_ITEM, NautilusCanvasNoteItem))
#define NAUTILUS_CANVAS_NOTE_ITEM_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_CANVAS_NOTE_ITEM, NautilusCanvasNoteItemClass))
#define NAUTILUS_IS_CANVAS_NOTE_ITEM(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CANVAS_NOTE_ITEM))
#define NAUTILUS_IS_CANVAS_NOTE_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CANVAS_NOTE_ITEM))


typedef struct _NautilusCanvasNoteItem      NautilusCanvasNoteItem;
typedef struct _NautilusCanvasNoteItemClass NautilusCanvasNoteItemClass;

struct _NautilusCanvasNoteItem {
	GnomeCanvasItem item;

	double x1;			/* canvas coordinates of bounding box */
	double y1;
	double x2;
	double y2;
	
	double width;			/* Outline width */

	guint fill_color;		/* Fill color, RGBA */
	guint outline_color;		/* Outline color, RGBA */

	gulong fill_pixel;		/* Fill color */
	gulong outline_pixel;		/* Outline color */

	GdkBitmap *fill_stipple;	/* Stipple for fill */
	GdkBitmap *outline_stipple;	/* Stipple for outline */

	GdkGC *fill_gc;			/* GC for filling */
	GdkGC *outline_gc;		/* GC for outline */

	/* text message */
	char *note_text;		/* text for annotation */
	
	/* Antialiased specific stuff follows */
	ArtSVP *fill_svp;		/* The SVP for the filled shape */
	ArtSVP *outline_svp;		/* The SVP for the outline shape */

	/* Configuration flags */
	unsigned int width_pixels : 1;	/* Is outline width specified in pixels or units? */
};

struct _NautilusCanvasNoteItemClass {
	GnomeCanvasItemClass parent_class;
};

/* Standard Gtk function */
GtkType nautilus_canvas_note_item_get_type (void);


END_GNOME_DECLS

#endif
