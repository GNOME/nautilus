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

#include <libgnomeui/gnome-canvas.h>

typedef struct NautilusBackground NautilusBackground;
typedef struct NautilusBackgroundClass NautilusBackgroundClass;

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

GtkType             nautilus_background_get_type              (void);
NautilusBackground *nautilus_background_new                   (void);

/* Calls to change a background. */
void                nautilus_background_set_color             (NautilusBackground     *background,
							       const char             *color_or_gradient);
void                nautilus_background_set_tile_image_uri    (NautilusBackground     *background,
							       const char             *image_uri);

/* Calls to interrogate the current state of a background. */
char *              nautilus_background_get_color             (NautilusBackground     *background);
char *              nautilus_background_get_tile_image_uri    (NautilusBackground     *background);

/* Explicitly fills a rectangle with a background. */
void                nautilus_background_draw                  (NautilusBackground     *background,
							       GdkDrawable            *drawable,
							       GdkGC                  *gc,
							       const GdkRectangle     *rectangle,
							       int                     origin_x,
							       int                     origin_y);

/* Explicitly fills a rectangle with a background on the anti-aliased canvas. */
void                nautilus_background_draw_aa               (NautilusBackground     *background,
							       GnomeCanvasBuf	      *buffer,
							       int		      entire_width,
							       int		      entire_height);
							       
/* Handles a dragged color being dropped on a widget to change the background color. */
void		    nautilus_background_receive_dropped_color (NautilusBackground     *background,
							       GtkWidget              *widget,
							       int                     drop_location_x,
							       int                     drop_location_y,
							       const GtkSelectionData *dropped_color);

/* Gets or creates a background so that it's attached to a widget. */
NautilusBackground *nautilus_get_widget_background            (GtkWidget              *widget);

typedef struct NautilusBackgroundDetails NautilusBackgroundDetails;

struct NautilusBackground
{
	GtkObject object;
	NautilusBackgroundDetails *details;
};

struct NautilusBackgroundClass
{
	GtkObjectClass parent_class;

	/* This signal is emitted when the background image is
	   finished loading. This allows a window to draw with a
	   color background if the image takes a lot time to load.
	*/
	void (* changed) (NautilusBackground *);
};

#endif /* NAUTILUS_BACKGROUND_H */
