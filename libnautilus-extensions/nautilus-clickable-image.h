/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clickable-image.h - A clickable image widget.

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

/* NautilusClickableImage is a NautilusLabeledImage sublclass that can handle
 * event useful for detect click events.
 *
 * The following signals are provided by NautilusClickableImage:
 *
 * "clicked" - Widget was clicked
 * "enter"   - Pointer entered widget.
 * "leave"   - Pointer left widget.
 *
 * NautilusClickableImage is a NO_WINDOW widget.  It does it event detection
 * by monitoring events on the first windowed ancestor in its widget hierarchy.
 *
 * Being a NO_WINDOW widget, it will work nicely with tile_pixbufs.
 */

#ifndef NAUTILUS_CLICKABLE_IMAGE_H
#define NAUTILUS_CLICKABLE_IMAGE_H

#include <libnautilus-extensions/nautilus-labeled-image.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_CLICKABLE_IMAGE            (nautilus_clickable_image_get_type ())
#define NAUTILUS_CLICKABLE_IMAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CLICKABLE_IMAGE, NautilusClickableImage))
#define NAUTILUS_CLICKABLE_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CLICKABLE_IMAGE, NautilusClickableImageClass))
#define NAUTILUS_IS_CLICKABLE_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CLICKABLE_IMAGE))
#define NAUTILUS_IS_CLICKABLE_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CLICKABLE_IMAGE))

typedef struct _NautilusClickableImage	        NautilusClickableImage;
typedef struct _NautilusClickableImageClass     NautilusClickableImageClass;
typedef struct _NautilusClickableImageDetails   NautilusClickableImageDetails;

struct _NautilusClickableImage
{
	/* Superclass */
	NautilusLabeledImage labeled_image;

	/* Private things */
	NautilusClickableImageDetails *details;
};

struct _NautilusClickableImageClass
{
	NautilusLabeledImageClass parent_class;
	
	void (*clicked) (NautilusClickableImage *image);
	void (*enter) (NautilusClickableImage *image);
	void (*leave) (NautilusClickableImage *image);
};

GtkType    nautilus_clickable_image_get_type  (void);
GtkWidget *nautilus_clickable_image_new       (const char *text,
					       GdkPixbuf  *pixbuf);
GtkWidget *nautilus_clickable_image_new_solid (const char *text,
					       GdkPixbuf  *pixbuf,
					       guint       drop_shadow_offset,
					       guint32     drop_shadow_color,
					       guint32     text_color,
					       float       x_alignment,
					       float       y_alignment,
					       int         x_padding,
					       int         y_padding,
					       guint32     background_color,
					       GdkPixbuf  *tile_pixbuf);

END_GNOME_DECLS

#endif /* NAUTILUS_CLICKABLE_IMAGE_H */


