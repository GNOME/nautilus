/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.h - A widget to display a alpha composited pixbufs.

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

/* NautilusImage is a widget that is capable of displaying alpha composited
 * pixbufs over complex backgrounds.  The background can be installed as
 * NautilusBackground on a NautilusImage widget or any of its ancestors.
 *
 * The background can also be that provided by the GtkStyle attatched to the 
 * widget.
 *
 * The best background will automatically be found and used by the widget.
 * 
 */

#ifndef NAUTILUS_IMAGE_H
#define NAUTILUS_IMAGE_H

#include <libnautilus-extensions/nautilus-buffered-widget.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>

/* NautilusImage is GtkWidget that draws a string using the high quality
 * anti aliased librsvg/freetype.
 */

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_IMAGE            (nautilus_image_get_type ())
#define NAUTILUS_IMAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_IMAGE, NautilusImage))
#define NAUTILUS_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_IMAGE, NautilusImageClass))
#define NAUTILUS_IS_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_IMAGE))
#define NAUTILUS_IS_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_IMAGE))

typedef struct _NautilusImage	       NautilusImage;
typedef struct _NautilusImageClass     NautilusImageClass;
typedef struct _NautilusImageDetail    NautilusImageDetail;

struct _NautilusImage
{
	/* Superclass */
	NautilusBufferedWidget		buffered_widget;

	/* Private things */
	NautilusImageDetail		*detail;
};

struct _NautilusImageClass
{
	NautilusBufferedWidgetClass	parent_class;
};

GtkType    nautilus_image_get_type          (void);
GtkWidget *nautilus_image_new               (void);
void       nautilus_image_set_pixbuf        (NautilusImage       *image,
					     GdkPixbuf           *pixbuf);
GdkPixbuf* nautilus_image_get_pixbuf        (const NautilusImage *image);
void       nautilus_image_set_overall_alpha (NautilusImage       *image,
					     guchar               pixbuf_alpha);

END_GNOME_DECLS

#endif /* NAUTILUS_IMAGE_H */


