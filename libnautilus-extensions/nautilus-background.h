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
#include <gdk-pixbuf/gdk-pixbuf.h>

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

typedef enum {
	NAUTILUS_BACKGROUND_TILED = 0, /* zero makes this the default placement */
	NAUTILUS_BACKGROUND_CENTERED,
	NAUTILUS_BACKGROUND_SCALED,
	NAUTILUS_BACKGROUND_SCALED_ASPECT
} NautilusBackgroundImagePlacement;

GtkType                          nautilus_background_get_type                         (void);
NautilusBackground *             nautilus_background_new                              (void);


/* Calls to change a background. */
void                             nautilus_background_set_color                        (NautilusBackground               *background,
										       const char                       *color_or_gradient);
void                             nautilus_background_set_image_uri                    (NautilusBackground               *background,
										       const char                       *image_uri);
void                             nautilus_background_reset                            (NautilusBackground               *background);
void                             nautilus_background_set_image_placement              (NautilusBackground               *background,
										       NautilusBackgroundImagePlacement  placement);
/* Calls to interrogate the current state of a background. */
char *                           nautilus_background_get_color                        (NautilusBackground               *background);
char *                           nautilus_background_get_image_uri                    (NautilusBackground               *background);
NautilusBackgroundImagePlacement nautilus_background_get_image_placement              (NautilusBackground               *background);
gboolean                         nautilus_background_is_dark                          (NautilusBackground               *background);
gboolean                         nautilus_background_is_set                           (NautilusBackground               *background);
gboolean                         nautilus_background_is_loaded                        (NautilusBackground               *background);

/* For preping the background to be used in one of the two calls
 * below. Only intended to be called by nautilus_background_canvas_group_update.
 */
void                             nautilus_background_pre_draw                         (NautilusBackground               *background,
										       int                               entire_width,
										       int                               entire_height);
/* For updating the canvas, non-aa case. Note: nautilus_background_pre_draw
 * must have been previously called. Only intended to be called by
 * nautilus_background_canvas_group_draw.
 */
void                             nautilus_background_draw                             (NautilusBackground               *background,
										       GdkDrawable                      *drawable,
										       GdkGC                            *gc,
										       int                               drawable_x,
										       int                               drawable_y,
										       int                               drawable_width,
										       int                               drawable_height);
/* For updating the canvas, aa case. Note: nautilus_background_pre_draw
 * must have been previously called. Only intended to be called by
 * nautilus_background_canvas_group_render.
 */
void                             nautilus_background_draw_aa                          (NautilusBackground               *background,
										       GnomeCanvasBuf                   *buffer);
/* Used to fill a drawable with a background.
 *  - entire_width/height describe the total area the background covers
 *  - drawable_x/y/width/height describe the portion of that area the drawable covers
 */
void                             nautilus_background_draw_to_drawable                 (NautilusBackground               *background,
										       GdkDrawable                      *drawable,
										       GdkGC                            *gc,
										       int                               drawable_x,
										       int                               drawable_y,
										       int                               drawable_width,
										       int                               drawable_height,
										       int                               entire_width,
										       int                               entire_height);

/* Used to fill a drawable with a background.
 *  - entire_width/height describe the total area the background covers
 *  - buffer is a portion of that area
 */
void                             nautilus_background_draw_to_canvas                   (NautilusBackground               *background,
										       GnomeCanvasBuf                   *buffer,
										       int                               entire_width,
										       int                               entire_height);
/* Used to fill a pixbuf with a background.
 *  - entire_width/height describe the total area the background covers
 *  - drawable_x/y/width/height describe the portion of that area the pixbuf covers
 */
void                             nautilus_background_draw_to_pixbuf                   (NautilusBackground               *background,
										       GdkPixbuf                        *pixbuf,
										       int                               pixbuf_x,
										       int                               pixbuf_y,
										       int                               pixbuf_width,
										       int                               pixbuf_height,
										       int                               entire_width,
										       int                               entire_height);
							       
/* Handles a dragged color being dropped on a widget to change the background color. */
void                             nautilus_background_receive_dropped_color            (NautilusBackground               *background,
										       GtkWidget                        *widget,
										       int                               drop_location_x,
										       int                               drop_location_y,
										       const GtkSelectionData           *dropped_color);

/* Handles a special-case image name that means "reset to default background" too. */
void                             nautilus_background_receive_dropped_background_image (NautilusBackground               *background,
										       const char                       *image_uri);

/* Gets or creates a background so that it's attached to a widget. */
NautilusBackground *             nautilus_get_widget_background                       (GtkWidget                        *widget);

/* Return whether a background has beed attatched to the given widget. */
gboolean                         nautilus_widget_has_attached_background              (GtkWidget                        *widget);

/* Find the background ancestor for the widget. */
GtkWidget *                      nautilus_gtk_widget_find_background_ancestor         (GtkWidget                        *widget);

/* Find out if a nautilus background is too complex for GtkStyle, so that we have to draw it ourselves */
gboolean                         nautilus_background_is_too_complex_for_gtk_style     (NautilusBackground               *background);


typedef struct NautilusBackgroundDetails NautilusBackgroundDetails;

struct NautilusBackground
{
	GtkObject object;
	NautilusBackgroundDetails *details;
};

struct NautilusBackgroundClass
{
	GtkObjectClass parent_class;

	/* This signal is emitted whenever the background settings are
	 * changed.
	 */
	void (* settings_changed) (NautilusBackground *);

	/* This signal is emitted whenever the appearance of the
	 * background has changed, like when the background settings are
	 * altered or when an image is loaded.
	 */
	void (* appearance_changed) (NautilusBackground *);

	/* This signal is emitted when image loading is over - whether it
	 * was successfully loaded or not.
	 */
	void (* image_loading_done) (NautilusBackground *background, gboolean successful_load);

	/* This signal is emitted when the background is reset by receiving
	   the reset property from a drag
	 */
	void (* reset) (NautilusBackground *);

};

#endif /* NAUTILUS_BACKGROUND_H */
