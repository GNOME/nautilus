/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background.h: Object for the background of a widget.

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
  
   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_BACKGROUND_H
#define NAUTILUS_BACKGROUND_H

/* Windows for Nautilus can contain backgrounds that are either tiled
   with an image, a solid color, or a color gradient. This class manages
   the process of loading the image if necessary and parsing the string
   that specifies either a color or color gradient.

   The color or gradient is always present, even if there's a tiled image
   on top of it. This is used when the tiled image can't be loaded for
   some reason (or just has not been loaded yet).

   The NautilusBackground object is easier to modify than a GtkStyle.
   You can just call nautilus_get_window_background and modify the
   returned background directly, unlike a style, which must be copied,
   modified and then set.
*/

#include <gdk/gdktypes.h>
#include <gtk/gtkwidget.h>

typedef struct _NautilusBackground NautilusBackground;
typedef struct _NautilusBackgroundClass NautilusBackgroundClass;

#define NAUTILUS_TYPE_BACKGROUND \
	(nautilus_background_get_type ())
#define NAUTILUS_BACKGROUND(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BACKGROUND, NautilusBackground))
#define NAUTILUS_BACKGROUND_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BACKGROUND, NautilusBackgroundClass))
#define NAUTILUS_IS_BACKGROUND(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BACKGROUND))
#define NAUTILUS_IS_BACKGROUND_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BACKGROUND))

GtkType             nautilus_background_get_type           (void);
NautilusBackground *nautilus_background_new                (void);

void                nautilus_background_set_color          (NautilusBackground *background,
							    const char         *color_or_gradient);
void                nautilus_background_set_tile_image_uri (NautilusBackground *background,
							    const char         *image_uri);

char *              nautilus_background_get_color          (NautilusBackground *background);
char *              nautilus_background_get_tile_image_uri (NautilusBackground *background);

void                nautilus_background_draw               (NautilusBackground *background,
							    GdkDrawable        *drawable,
							    GdkGC              *gc,
							    GdkColormap        *colormap,
							    const GdkRectangle *rectangle);

/* Gets the background attached to a widget.

   If the widget doesn't already have a NautilusBackground object,
   this will create one. To change the widget's background, you can
   just call nautilus_background methods on the widget.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
NautilusBackground *nautilus_get_widget_background         (GtkWidget          *widget);

typedef struct _NautilusBackgroundDetails NautilusBackgroundDetails;

struct _NautilusBackground
{
	GtkObject object;
	NautilusBackgroundDetails *details;
};

struct _NautilusBackgroundClass
{
	GtkObjectClass parent_class;

	/* This signal is emitted when the background image is
	   finished loading.  This allows a window to draw with a
	   color background if the image takes a lot time to load.
	*/
	void (* changed) (NautilusBackground *);
};

#endif /* NAUTILUS_BACKGROUND_H */
