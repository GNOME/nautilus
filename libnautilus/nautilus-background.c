/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background.c: Object for the background of a widget.
 
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nautilus-background.h"

#include <gtk/gtksignal.h>
#include "gdk-extensions.h"
#include "nautilus-background-canvas-group.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"

static void nautilus_background_initialize_class (gpointer       klass);
static void nautilus_background_initialize       (gpointer       object,
						  gpointer       klass);
static void nautilus_background_destroy          (GtkObject     *object);
static void nautilus_background_finalize         (GtkObject     *object);

static void nautilus_background_draw_flat_box    (GtkStyle      *style,
						  GdkWindow     *window,
						  GtkStateType   state_type,
						  GtkShadowType  shadow_type,
						  GdkRectangle  *area,
						  GtkWidget     *widget,
						  gchar         *detail,
						  gint           x,
						  gint           y,
						  gint           width,
						  gint           height);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackground, nautilus_background, GTK_TYPE_OBJECT)

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _NautilusBackgroundDetails
{
	char *color;
	char *tile_image_uri;
};

static void
nautilus_background_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = nautilus_background_destroy;
	object_class->finalize = nautilus_background_finalize;
}

static void
nautilus_background_initialize (gpointer object, gpointer klass)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND(object);

	background->details = g_new0 (NautilusBackgroundDetails, 1);
}

static void
nautilus_background_destroy (GtkObject *object)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_background_finalize (GtkObject *object)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (object);
	g_free (background->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

NautilusBackground *
nautilus_background_new (void)
{
	return NAUTILUS_BACKGROUND (gtk_type_new (NAUTILUS_TYPE_BACKGROUND));
}

void
nautilus_background_draw (NautilusBackground *background,
			  GdkDrawable *drawable,
			  GdkGC *gc,
			  GdkColormap *colormap,
			  const GdkRectangle *rectangle)
{
	char *start_color_spec;
	char *end_color_spec;
	GdkColor start_color;
	GdkColor end_color;
	gboolean horizontal_gradient;

	start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
	end_color_spec = nautilus_gradient_get_end_color_spec (background->details->color);
	horizontal_gradient = nautilus_gradient_is_horizontal (background->details->color);

	nautilus_gdk_color_parse_with_white_default (start_color_spec, &start_color);
	nautilus_gdk_color_parse_with_white_default (end_color_spec, &end_color);

	g_free (start_color_spec);
	g_free (end_color_spec);

	nautilus_fill_rectangle_with_gradient  (drawable, gc, colormap, rectangle,
						&start_color, &end_color, horizontal_gradient);
}

char *
nautilus_background_get_color (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NULL);

	return g_strdup (background->details->color);
}

void
nautilus_background_set_color (NautilusBackground *background,
			       const char *color)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	g_free (background->details->color);
	background->details->color = g_strdup (color);

	gtk_signal_emit (GTK_OBJECT (background), signals[CHANGED]);
}

static GtkStyleClass *
nautilus_gtk_style_get_default_class (void)
{
	static GtkStyleClass *default_class;

	if (default_class == NULL) {
		GtkStyle *style;

		style = gtk_style_new ();
		default_class = style->klass;
		gtk_style_unref (style);
	}

	return default_class;
}

static void
nautilus_gdk_window_update_sizes (GdkWindow *window, int *width, int *height)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (width != NULL);
	g_return_if_fail (height != NULL);

	if (*width == -1 && *height == -1)
		gdk_window_get_size (window, width, height);
	else if (*width == -1)
		gdk_window_get_size (window, width, NULL);
	else if (*height == -1)
		gdk_window_get_size (window, NULL, height);
}

static void
nautilus_background_draw_flat_box (GtkStyle *style,
				   GdkWindow *window,
				   GtkStateType state_type,
				   GtkShadowType shadow_type,
				   GdkRectangle *area,
				   GtkWidget *widget,
				   char *detail,
				   int x,
				   int y,
				   int width,
				   int height)
{
	gboolean call_parent;
	NautilusBackground *background;
	GdkGC *gc;
	GdkRectangle rectangle;

	call_parent = TRUE;

	background = NULL;
	if (state_type == GTK_STATE_NORMAL) {
		background = nautilus_get_widget_background (widget);
		if (background != NULL) {
			if (nautilus_gradient_is_gradient (background->details->color))
				call_parent = FALSE;
		}
	}

	if (call_parent) {
		(* nautilus_gtk_style_get_default_class()->draw_flat_box)
			(style, window, state_type, shadow_type, area, widget,
			 detail, x, y, width, height);
		return;
	}

	gc = gdk_gc_new (window);

	nautilus_gdk_window_update_sizes (window, &width, &height);
	
	rectangle.x = x;
	rectangle.y = y;
	rectangle.width = width;
	rectangle.height = height;
	
	nautilus_background_draw (background, window, gc,
				  gtk_widget_get_colormap(widget),
				  &rectangle);
	
	gdk_gc_unref (gc);
}

static GtkStyleClass *
nautilus_background_get_gtk_style_class (void)
{
	static GtkStyleClass *klass;

	if (klass == NULL) {
		static GtkStyleClass klass_storage;

		klass = &klass_storage;
		*klass = *nautilus_gtk_style_get_default_class ();

		klass->draw_flat_box = nautilus_background_draw_flat_box;
	}

	return klass;
}

static void
nautilus_background_set_widget_style (NautilusBackground *background,
				      GtkWidget *widget)
{
	GtkStyle *style;
	char *start_color_spec;
	GdkColor color;
	
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	style = gtk_widget_get_style (widget);
	
	/* Make a copy of the style. */
	style = gtk_style_copy (style);
	
	/* Give it the special class that allows it to draw gradients. */
	style->klass = nautilus_background_get_gtk_style_class ();

	/* Set up the colors in the style. */
	start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
	nautilus_gdk_color_parse_with_white_default
		(start_color_spec, &color);
	g_free (start_color_spec);
	style->bg[GTK_STATE_NORMAL] = color;
	style->base[GTK_STATE_NORMAL] = color;
	style->bg[GTK_STATE_ACTIVE] = color;
	style->base[GTK_STATE_ACTIVE] = color;
	
	/* Put the style in the widget. */
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);
}

static void
nautilus_background_set_up_canvas (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	/* Attach ourselves to a canvas in a way that will work.
	   Changing the style is not sufficient.

	   Since there's no signal to override in GnomeCanvas to control
	   drawing the background, we change the class of the canvas root.
	   This gives us a chance to draw the background before any of the
	   objects draw themselves, and has no effect on the bounds or
	   anything related to scrolling.

	   We settled on this after less-than-thrilling results using a
	   canvas item as the background. The canvas item contributed to
	   the bounds of the canvas and had to constantly be resized.
	*/
	if (GNOME_IS_CANVAS (widget)) {
		g_assert (GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass
			  == gtk_type_class (GNOME_TYPE_CANVAS_GROUP)
			  || GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass
			  == gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP));

		GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass =
			gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP);
	}
}

static void
nautilus_widget_background_changed (GtkWidget *widget, NautilusBackground *background)
{
	nautilus_background_set_widget_style (background, widget);
	nautilus_background_set_up_canvas (widget);

	gtk_widget_queue_clear (widget);
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a NautilusBackground object,
   this will create one. To change the widget's background, you can
   just call nautilus_background methods on the widget.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
NautilusBackground *
nautilus_get_widget_background (GtkWidget *widget)
{
	gpointer data;
	NautilusBackground *background;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	/* Check for an existing background. */
	data = gtk_object_get_data (GTK_OBJECT (widget), "nautilus_background");
	if (data != NULL) {
		g_assert (NAUTILUS_IS_BACKGROUND (data));
		return data;
	}

	/* Store the background in the widget's data. */
	background = nautilus_background_new ();
	gtk_object_set_data_full (GTK_OBJECT (widget), "nautilus_background",
				  background, (GtkDestroyNotify) gtk_object_unref);
	gtk_object_ref (GTK_OBJECT (background));
	gtk_object_sink (GTK_OBJECT (background));

	/* Arrange to get the signal whenever the background changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (background), "changed",
					       nautilus_widget_background_changed,
					       GTK_OBJECT (widget));
	nautilus_widget_background_changed (widget, background);

	return background;
}

void
nautilus_background_receive_dropped_color (NautilusBackground *background,
					   GtkWidget *widget,
					   int drop_location_x,
					   int drop_location_y,
					   const GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;
	char *new_gradient_spec;
	int left_border, right_border, top_border, bottom_border;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (selection_data != NULL);

	/* Convert the selection data into a color spec. */
	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	channels = (guint16 *)selection_data->data;
	color_spec = g_strdup_printf ("rgb:%04hX/%04hX/%04hX", channels[0], channels[1], channels[2]);

	/* Figure out if the color was dropped close enough to an edge to create a gradient.
	   For the moment, this is hard-wired, but later the widget will have to have some
	   say in where the borders are.
	*/
	left_border = 32;
	right_border = widget->allocation.width - 32;
	top_border = 32;
	bottom_border = widget->allocation.height - 32;
	if (drop_location_x < left_border && drop_location_x <= right_border)
		new_gradient_spec = nautilus_gradient_set_left_color_spec (background->details->color, color_spec);
	else if (drop_location_x >= left_border && drop_location_x > right_border)
		new_gradient_spec = nautilus_gradient_set_right_color_spec (background->details->color, color_spec);
	else if (drop_location_y < top_border && drop_location_y <= bottom_border)
		new_gradient_spec = nautilus_gradient_set_top_color_spec (background->details->color, color_spec);
	else if (drop_location_y >= top_border && drop_location_y > bottom_border)
		new_gradient_spec = nautilus_gradient_set_bottom_color_spec (background->details->color, color_spec);
	else
		new_gradient_spec = g_strdup (color_spec);
	
	g_free (color_spec);

	nautilus_background_set_color (background, new_gradient_spec);
	
	g_free (new_gradient_spec);
}

/* self check code */

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_background (void)
{
	NautilusBackground *background;

	background = nautilus_background_new ();

	nautilus_background_set_color (background, NULL);
	nautilus_background_set_color (background, "");
	nautilus_background_set_color (background, "red");
	nautilus_background_set_color (background, "red-blue");
	nautilus_background_set_color (background, "red-blue:h");

	gtk_object_unref (GTK_OBJECT (background));
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
