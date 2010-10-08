/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-background.c: Object for the background of a widget.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "eel-background.h"
#include "eel-gdk-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include <gtk/gtk.h>
#include <eel/eel-canvas.h>
#include <eel/eel-canvas-util.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <math.h>
#include <stdio.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

static void set_image_properties (EelBackground *background);

static void init_fade (EelBackground *background);
static void free_fade (EelBackground *background);

static void eel_widget_queue_background_change (GtkWidget *widget);

G_DEFINE_TYPE (EelBackground, eel_background, G_TYPE_OBJECT);

struct EelBackgroundDetails {
	char *color;
	
	GnomeBG *bg;
	GtkWidget *widget;

	/* Realized data: */
	cairo_surface_t *background_surface;
	GnomeBGCrossfade *fade;
	int background_entire_width;
	int background_entire_height;
	GdkColor default_color;

	/* Desktop screen size watcher */
	gulong screen_size_handler;
	/* Desktop monitors configuration watcher */
	gulong screen_monitors_handler;
	guint change_idle_id;
};

static void
on_bg_changed (GnomeBG *bg, EelBackground *background)
{
	init_fade (background);
	eel_widget_queue_background_change (background->details->widget);
}

static void
on_bg_transitioned (GnomeBG *bg, EelBackground *background)
{
	free_fade (background);
	eel_widget_queue_background_change (background->details->widget);
}

static void
eel_background_init (EelBackground *background)
{
	background->details =
		G_TYPE_INSTANCE_GET_PRIVATE (background,
					     EEL_TYPE_BACKGROUND,
					     EelBackgroundDetails);

	background->details->default_color.red = 0xffff;
	background->details->default_color.green = 0xffff;
	background->details->default_color.blue = 0xffff;
	background->details->bg = gnome_bg_new ();

	g_signal_connect (background->details->bg, "changed",
			  G_CALLBACK (on_bg_changed), background);
	g_signal_connect (background->details->bg, "transitioned",
			  G_CALLBACK (on_bg_transitioned), background);
	
}

/* The safe way to clear an image from a background is:
 * 		eel_background_set_image_uri (NULL);
 * This fn is a private utility - it does NOT clear
 * the details->bg_uri setting.
 */
static void
eel_background_remove_current_image (EelBackground *background)
{
	if (background->details->bg != NULL) {
		g_object_unref (G_OBJECT (background->details->bg));
		background->details->bg = NULL;
	}
}

static void
free_fade (EelBackground *background)
{
	if (background->details->fade != NULL) {
		g_object_unref (background->details->fade);
		background->details->fade = NULL;
	}
}

static void
free_background_surface (EelBackground *background)
{
	cairo_surface_t *surface;

	surface = background->details->background_surface;
	if (surface != NULL) {
		cairo_surface_destroy (surface);
		background->details->background_surface = NULL;
	}
}


static EelBackgroundImagePlacement
placement_gnome_to_eel (GnomeBGPlacement p)
{
	switch (p) {
	case GNOME_BG_PLACEMENT_CENTERED:
		return EEL_BACKGROUND_CENTERED;
	case GNOME_BG_PLACEMENT_FILL_SCREEN:
		return EEL_BACKGROUND_SCALED;
	case GNOME_BG_PLACEMENT_SCALED:
		return EEL_BACKGROUND_SCALED_ASPECT;
	case GNOME_BG_PLACEMENT_ZOOMED:
		return EEL_BACKGROUND_ZOOM;
	case GNOME_BG_PLACEMENT_TILED:
		return EEL_BACKGROUND_TILED;
        case GNOME_BG_PLACEMENT_SPANNED:
                return EEL_BACKGROUND_SPANNED;
	}

	return EEL_BACKGROUND_TILED;
}

static GnomeBGPlacement
placement_eel_to_gnome (EelBackgroundImagePlacement p)
{
	switch (p) {
	case EEL_BACKGROUND_CENTERED:
		return GNOME_BG_PLACEMENT_CENTERED;
	case EEL_BACKGROUND_SCALED:
		return GNOME_BG_PLACEMENT_FILL_SCREEN;
	case EEL_BACKGROUND_SCALED_ASPECT:
		return GNOME_BG_PLACEMENT_SCALED;
	case EEL_BACKGROUND_ZOOM:
		return GNOME_BG_PLACEMENT_ZOOMED;
	case EEL_BACKGROUND_TILED:
		return GNOME_BG_PLACEMENT_TILED;
	case EEL_BACKGROUND_SPANNED:
		return GNOME_BG_PLACEMENT_SPANNED;
	}

	return GNOME_BG_PLACEMENT_TILED;
}

EelBackgroundImagePlacement
eel_background_get_image_placement (EelBackground *background)
{
	g_return_val_if_fail (EEL_IS_BACKGROUND (background), EEL_BACKGROUND_TILED);

	return placement_gnome_to_eel (gnome_bg_get_placement (background->details->bg));
}

void
eel_background_set_image_placement (EelBackground              *background,
				    EelBackgroundImagePlacement new_placement)
{
	g_return_if_fail (EEL_IS_BACKGROUND (background));

	gnome_bg_set_placement (background->details->bg,
				placement_eel_to_gnome (new_placement));
}


static void
eel_background_finalize (GObject *object)
{
	EelBackground *background;

	background = EEL_BACKGROUND (object);

	g_free (background->details->color);
	eel_background_remove_current_image (background);

	free_background_surface (background);

	free_fade (background);

	G_OBJECT_CLASS (eel_background_parent_class)->finalize (object);
}

static void
eel_background_class_init (EelBackgroundClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = eel_background_finalize;

	g_type_class_add_private (klass, sizeof (EelBackgroundDetails));
}

EelBackground *
eel_background_new (void)
{
	return EEL_BACKGROUND (g_object_new (EEL_TYPE_BACKGROUND, NULL));
}

static void
eel_background_unrealize (EelBackground *background)
{
	free_background_surface (background);
	
	background->details->background_entire_width = 0;
	background->details->background_entire_height = 0;
	background->details->default_color.red = 0xffff;
	background->details->default_color.green = 0xffff;
	background->details->default_color.blue = 0xffff;
}

static gboolean
eel_background_ensure_realized (EelBackground *background)
{
	GtkStyle *style;
	int entire_width;
	int entire_height;
	GdkScreen *screen;
	GdkWindow *window;

	screen = gtk_widget_get_screen (background->details->widget);
	entire_height = gdk_screen_get_height (screen);
	entire_width = gdk_screen_get_width (screen);

	/* Set the default color */
	
	/* Get the widget to which the window belongs and its style as well */
	style = gtk_widget_get_style (background->details->widget);
	background->details->default_color = style->base[GTK_STATE_NORMAL];

	/* If the window size is the same as last time, don't update */
	if (entire_width == background->details->background_entire_width &&
	    entire_height == background->details->background_entire_height) {
		return FALSE;
	}

	free_background_surface (background);

	set_image_properties (background);

	window = gtk_widget_get_window (background->details->widget);
	background->details->background_surface = gnome_bg_create_surface (background->details->bg,
									   window,
									   entire_width, entire_height,
									   TRUE);

	/* We got the surface and everything, so we don't care about a change
	   that is pending (unless things actually change after this time) */
	g_object_set_data (G_OBJECT (background->details->bg),
			   "ignore-pending-change", GINT_TO_POINTER (TRUE));
	
	background->details->background_entire_width = entire_width;
	background->details->background_entire_height = entire_height;

	return TRUE;
}

static void
set_image_properties (EelBackground *background)
{
	GdkColor c;
	if (!background->details->color) {
		c = background->details->default_color;
		gnome_bg_set_color (background->details->bg, GNOME_BG_COLOR_SOLID,
				    &c, NULL);
	} else if (!eel_gradient_is_gradient (background->details->color)) {
		eel_gdk_color_parse_with_white_default (background->details->color, &c);
		gnome_bg_set_color (background->details->bg, GNOME_BG_COLOR_SOLID, &c, NULL);
	} else {
		GdkColor c1;
		GdkColor c2;
		char *spec;

		spec = eel_gradient_get_start_color_spec (background->details->color);
		eel_gdk_color_parse_with_white_default (spec, &c1);
		g_free (spec);

		spec = eel_gradient_get_end_color_spec (background->details->color);
		eel_gdk_color_parse_with_white_default (spec, &c2);
		g_free (spec);

		if (eel_gradient_is_horizontal (background->details->color))
			gnome_bg_set_color (background->details->bg, GNOME_BG_COLOR_H_GRADIENT, &c1, &c2);
		else
			gnome_bg_set_color (background->details->bg, GNOME_BG_COLOR_V_GRADIENT, &c1, &c2);

	}
}

char *
eel_background_get_color (EelBackground *background)
{
	g_return_val_if_fail (EEL_IS_BACKGROUND (background), NULL);

	return g_strdup (background->details->color);
}

char *
eel_background_get_image_uri (EelBackground *background)
{
	const char *filename;
	
	g_return_val_if_fail (EEL_IS_BACKGROUND (background), NULL);

	filename = gnome_bg_get_filename (background->details->bg);
	if (filename) {
		return g_filename_to_uri (filename, NULL, NULL);
	}
	return NULL;
}

void
eel_background_set_color (EelBackground *background,
			  const char *color)
{
	if (g_strcmp0 (background->details->color, color) != 0) {
		g_free (background->details->color);
		background->details->color = g_strdup (color);
		
		set_image_properties (background);
	}
}

void
eel_background_set_image_uri (EelBackground *background,
			      const char *image_uri)
{
	char *filename;

	if (image_uri != NULL) {
		filename = g_filename_from_uri (image_uri, NULL, NULL);
	}
	else {
		filename = NULL;
	}
	
	gnome_bg_set_filename (background->details->bg, filename);
	set_image_properties (background);
	
	g_free (filename);
}

void
eel_background_receive_dropped_background_image (EelBackground *background,
						 const char *image_uri)
{
	GConfClient *client;

	/* Currently, we only support tiled images. So we set the placement.
	 */
	eel_background_set_image_placement (background, EEL_BACKGROUND_TILED);	
	eel_background_set_image_uri (background, image_uri);

	client = gconf_client_get_default ();
	gnome_bg_save_to_preferences (background->details->bg, client);

	g_object_unref (client);
}

static void
set_root_surface (EelBackground *background,
		  GdkScreen     *screen)
{
	gnome_bg_set_surface_as_root (screen, background->details->background_surface);
}


static void
on_fade_finished (GnomeBGCrossfade *fade,
		  GdkWindow *window,
		  EelBackground *background)
{
	eel_background_ensure_realized (background);
	set_root_surface (background, gdk_window_get_screen (window));
}

static gboolean
fade_to_surface (EelBackground *background,
		 GdkWindow     *window,
		 cairo_surface_t *surface)
{
	if (background->details->fade == NULL) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_set_end_surface (background->details->fade,
				                 surface)) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_is_started (background->details->fade)) {
		gnome_bg_crossfade_start (background->details->fade, window);
		g_signal_connect (background->details->fade,
				  "finished",
				  G_CALLBACK (on_fade_finished), background);
	}

	return gnome_bg_crossfade_is_started (background->details->fade);
}


static void
eel_background_set_up_widget (EelBackground *background, GtkWidget *widget)
{
	GtkStyle *style;
	GdkWindow *window;
	GdkWindow *widget_window;
	gboolean in_fade = FALSE;

	if (!gtk_widget_get_realized (widget)) {
		return;
	}

	widget_window = gtk_widget_get_window (widget);
	eel_background_ensure_realized (background);
	style = gtk_widget_get_style (widget);

	if (EEL_IS_CANVAS (widget)) {
		window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	} else {
		window = widget_window;
	}

	in_fade = fade_to_surface (background, window,
				   background->details->background_surface);

	if (!in_fade) {
		cairo_pattern_t *pattern;

		pattern = cairo_pattern_create_for_surface (background->details->background_surface);
		gdk_window_set_background_pattern (window, pattern);
		cairo_pattern_destroy (pattern);

		set_root_surface (background,
				  gtk_widget_get_screen (widget));
	}
}

static gboolean
on_background_changed (EelBackground *background)
{
	if (background->details->change_idle_id == 0) {
		return FALSE;
	}

	background->details->change_idle_id = 0;

	eel_background_unrealize (background);
	eel_background_set_up_widget (background, background->details->widget);

	gtk_widget_queue_draw (background->details->widget);

	return FALSE;
}

static void
init_fade (EelBackground *background)
{
	GtkWidget *widget;

	widget = background->details->widget;

	if (widget == NULL || !gtk_widget_get_realized (widget))
		return;

	if (background->details->fade == NULL) {
		GdkWindow *window;
		GdkScreen *screen;
		int old_width, old_height, width, height;

		/* If this was the result of a screen size change,
		 * we don't want to crossfade
		 */
		window = gtk_widget_get_window (widget);
		old_width = gdk_window_get_width (window);
		old_height = gdk_window_get_height (window);

		screen = gtk_widget_get_screen (widget);
		width = gdk_screen_get_width (screen);
		height = gdk_screen_get_height (screen);

		if (old_width == width && old_height == height) {
			background->details->fade = gnome_bg_crossfade_new (width, height);
			g_signal_connect_swapped (background->details->fade,
						"finished",
						G_CALLBACK (free_fade),
						background);
		}
	}

	if (background->details->fade != NULL && !gnome_bg_crossfade_is_started (background->details->fade)) {
		cairo_surface_t *start_surface;

		if (background->details->background_surface == NULL) {
			start_surface = gnome_bg_get_surface_from_root (gtk_widget_get_screen (widget));
		} else {
			start_surface = cairo_surface_reference (background->details->background_surface);
		}
		gnome_bg_crossfade_set_start_surface (background->details->fade,
						      start_surface);
                cairo_surface_destroy (start_surface);
	}
}

static void
eel_widget_queue_background_change (GtkWidget *widget)
{
	EelBackground *background;

	background = eel_get_widget_background (widget);

	if (background->details->change_idle_id > 0) {
		return;
	}

	background->details->change_idle_id = g_idle_add ((GSourceFunc) on_background_changed, background);
}

/* Callback used when the style of a widget changes.  We have to regenerate its
 * EelBackgroundStyle so that it will match the chosen GTK+ theme.
 */
static void
widget_style_set_cb (GtkWidget *widget, GtkStyle *previous_style, gpointer data)
{
	EelBackground *background;

	background = EEL_BACKGROUND (data);

	if (previous_style != NULL) {
		eel_widget_queue_background_change (widget);
	}
}

static void
screen_size_changed (GdkScreen *screen, EelBackground *background)
{
	eel_widget_queue_background_change (background->details->widget);
}

static void
widget_realized_setup (GtkWidget *widget, gpointer data)
{
	EelBackground *background;
	GdkScreen *screen;

	background = EEL_BACKGROUND (data);
	
	screen = gtk_widget_get_screen (widget);

	if (background->details->screen_size_handler > 0) {
		g_signal_handler_disconnect (screen,
					     background->details->screen_size_handler);
	}
	
	background->details->screen_size_handler = 
		g_signal_connect (screen, "size_changed",
				  G_CALLBACK (screen_size_changed), background);
	if (background->details->screen_monitors_handler > 0) {
		g_signal_handler_disconnect (screen,
					     background->details->screen_monitors_handler);
	}
	background->details->screen_monitors_handler =
		g_signal_connect (screen, "monitors-changed",
				  G_CALLBACK (screen_size_changed), background);

	init_fade (background);
}

static void
widget_realize_cb (GtkWidget *widget, gpointer data)
{
	EelBackground *background;

	background = EEL_BACKGROUND (data);

	widget_realized_setup (widget, data);
		
	eel_background_set_up_widget (background, widget);
}

static void
widget_unrealize_cb (GtkWidget *widget, gpointer data)
{
	EelBackground *background;

	background = EEL_BACKGROUND (data);

	if (background->details->screen_size_handler > 0) {
		        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
				                     background->details->screen_size_handler);
			background->details->screen_size_handler = 0;
	}
	if (background->details->screen_monitors_handler > 0) {
		        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
				                     background->details->screen_monitors_handler);
			background->details->screen_monitors_handler = 0;
	}
}

static void
on_widget_destroyed (GtkWidget *widget, EelBackground *background)
{
	if (background->details->change_idle_id != 0) {
		g_source_remove (background->details->change_idle_id);
		background->details->change_idle_id = 0;
	}

	background->details->widget = NULL;
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a EelBackground object,
   this will create one. To change the widget's background, you can
   just call eel_background methods on the widget.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
EelBackground *
eel_get_widget_background (GtkWidget *widget)
{
	gpointer data;
	EelBackground *background;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	/* Check for an existing background. */
	data = g_object_get_data (G_OBJECT (widget), "eel_background");
	if (data != NULL) {
		g_assert (EEL_IS_BACKGROUND (data));
		return data;
	}

	/* Store the background in the widget's data. */
	background = eel_background_new ();
	g_object_set_data_full (G_OBJECT (widget), "eel_background",
				background, g_object_unref);
	background->details->widget = widget;
 	g_signal_connect_object (widget, "destroy", G_CALLBACK (on_widget_destroyed), background, 0);

	eel_widget_queue_background_change (widget);

	g_signal_connect_object (widget, "style_set",
				 G_CALLBACK (widget_style_set_cb),
				 background,
				 0);
	g_signal_connect_object (widget, "realize",
				 G_CALLBACK (widget_realize_cb),
				 background,
				 0);
	g_signal_connect_object (widget, "unrealize",
				 G_CALLBACK (widget_unrealize_cb),
				 background,
				 0);

	return background;
}

/* self check code */

#if !defined (EEL_OMIT_SELF_CHECK)

void
eel_self_check_background (void)
{
	EelBackground *background;

	background = eel_background_new ();

	eel_background_set_color (background, NULL);
	eel_background_set_color (background, "");
	eel_background_set_color (background, "red");
	eel_background_set_color (background, "red-blue");
	eel_background_set_color (background, "red-blue:h");

	g_object_ref_sink (background);
	g_object_unref (background);
}

#endif
