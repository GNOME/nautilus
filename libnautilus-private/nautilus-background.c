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

static void nautilus_background_initialize_class (gpointer klass);
static void nautilus_background_initialize (gpointer object, gpointer klass);
static void nautilus_background_destroy (GtkObject *object);
static void nautilus_background_finalize (GtkObject *object);

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (NautilusBackground, nautilus_background, GTK_TYPE_OBJECT)

enum {
	CHANGED,
	LAST_SIGNAL
};

static GtkObjectClass *parent_class;
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

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);
	
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

void
nautilus_background_set_color (NautilusBackground *background,
			       const char *color)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	g_free (background->details->color);
	background->details->color = g_strdup (color);

	gtk_signal_emit (GTK_OBJECT (background), signals[CHANGED]);
}

void
nautilus_background_attach_to_canvas (NautilusBackground *background,
				      GnomeCanvas *canvas)
{
	/* Since there's no signal to override in GnomeCanvas to control
	   drawing the background, we change the class of the canvas root.
	   This gives us a chance to draw the background before any of the
	   objects draw themselves, and has no effect on the bounds or
	   anything related to scrolling.

	   We settled on this after less-than-thrilling results using a
	   canvas item as the background. The canvas item contributed to
	   the bounds of the canvas and had to constantly be resized.
	*/

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	g_assert (GTK_OBJECT (canvas->root)->klass == gtk_type_class (GNOME_TYPE_CANVAS_GROUP)
		  || GTK_OBJECT (canvas->root)->klass == gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP));

	GTK_OBJECT (canvas->root)->klass = gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP);

	nautilus_background_canvas_group_set_background (NAUTILUS_BACKGROUND_CANVAS_GROUP (canvas->root), background);
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
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
