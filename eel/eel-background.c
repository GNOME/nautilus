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
#include "eel-gdk-pixbuf-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-gnome-extensions.h"
#include "eel-gtk-macros.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include "eel-marshal.h"
#include "eel-types.h"
#include "eel-type-builtins.h"
#include <gtk/gtk.h>
#include <eel/eel-canvas.h>
#include <eel/eel-canvas-util.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <math.h>
#include <stdio.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

static void       eel_background_class_init                (gpointer       klass);
static void       eel_background_init                      (gpointer       object,
							    gpointer       klass);
static void       eel_background_finalize                  (GObject       *object);
static GdkPixmap *eel_background_get_pixmap_and_color      (EelBackground *background,
							    GdkWindow     *window,
							    GdkColor      *color,
							    gboolean      *changes_with_size);
static void set_image_properties (EelBackground *background);

static void init_fade (EelBackground *background, GtkWidget *widget);
static void free_fade (EelBackground *background);

EEL_CLASS_BOILERPLATE (EelBackground, eel_background, GTK_TYPE_OBJECT)

enum {
	APPEARANCE_CHANGED,
	SETTINGS_CHANGED,
	RESET,
	LAST_SIGNAL
};

/* This is the size of the GdkRGB dither matrix, in order to avoid
 * bad dithering when tiling the gradient
 */
#define GRADIENT_PIXMAP_TILE_SIZE 128

static guint signals[LAST_SIGNAL];

struct EelBackgroundDetails {
	char *color;
	
	GnomeBG *bg;
	GtkWidget *widget;

	/* Realized data: */
	gboolean background_changes_with_size;
	GdkPixmap *background_pixmap;
	gboolean background_pixmap_is_unset_root_pixmap;
	GnomeBGCrossfade *fade;
	int background_entire_width;
	int background_entire_height;
	GdkColor default_color;

	gboolean use_base;
	
	/* Is this background attached to desktop window */
	gboolean is_desktop;
	/* Desktop screen size watcher */
	gulong screen_size_handler;
	/* Can we use common pixmap for root window and desktop window */
	gboolean use_common_pixmap;
	guint change_idle_id;
};

static void
eel_background_class_init (gpointer klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	eel_type_init ();

	signals[APPEARANCE_CHANGED] =
		g_signal_new ("appearance_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
			      G_STRUCT_OFFSET (EelBackgroundClass,
					       appearance_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[SETTINGS_CHANGED] =
		g_signal_new ("settings_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
			      G_STRUCT_OFFSET (EelBackgroundClass,
					       settings_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);
	signals[RESET] =
		g_signal_new ("reset",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
			      G_STRUCT_OFFSET (EelBackgroundClass,
					       reset),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	object_class->finalize = eel_background_finalize;
}

static void
on_bg_changed (GnomeBG *bg, EelBackground *background)
{
	init_fade (background, background->details->widget);
	g_signal_emit (G_OBJECT (background),
		       signals[APPEARANCE_CHANGED], 0);
}

static void
on_bg_transitioned (GnomeBG *bg, EelBackground *background)
{
	free_fade (background);
	g_signal_emit (G_OBJECT (background),
		       signals[APPEARANCE_CHANGED], 0);
}

static void
eel_background_init (gpointer object, gpointer klass)
{
	EelBackground *background;

	background = EEL_BACKGROUND (object);

	background->details = g_new0 (EelBackgroundDetails, 1);
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
free_background_pixmap (EelBackground *background)
{
	GdkDisplay *display;
	GdkPixmap *pixmap;

	pixmap = background->details->background_pixmap;
	if (pixmap != NULL) {
		/* If we created a root pixmap and didn't set it as background
		   it will live forever, so we need to kill it manually.
		   If set as root background it will be killed next time the
		   background is changed. */
		if (background->details->background_pixmap_is_unset_root_pixmap) {
			display = gdk_drawable_get_display (GDK_DRAWABLE (pixmap));
			XKillClient (GDK_DISPLAY_XDISPLAY (display),
				     GDK_PIXMAP_XID (pixmap));
		}
		g_object_unref (pixmap);
		background->details->background_pixmap = NULL;
	}
}


static void
eel_background_finalize (GObject *object)
{
	EelBackground *background;

	background = EEL_BACKGROUND (object);

	g_free (background->details->color);
	eel_background_remove_current_image (background);

	free_background_pixmap (background);

	free_fade (background);

	g_free (background->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
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

EelBackground *
eel_background_new (void)
{
	return EEL_BACKGROUND (g_object_new (EEL_TYPE_BACKGROUND, NULL));
}

static void
eel_background_unrealize (EelBackground *background)
{
	free_background_pixmap (background);
	
	background->details->background_entire_width = 0;
	background->details->background_entire_height = 0;
	background->details->default_color.red = 0xffff;
	background->details->default_color.green = 0xffff;
	background->details->default_color.blue = 0xffff;
}

static void
drawable_get_adjusted_size (EelBackground *background,
			    GdkDrawable   *drawable,
			    int		  *width,
			    int	          *height)
{
	GdkScreen *screen;
	
	/* 
	 * Screen resolution change makes root drawable have incorrect size.
	 */    
	gdk_drawable_get_size (drawable, width, height);

	if (background->details->is_desktop) {
		screen = gdk_drawable_get_screen (drawable);
		*width = gdk_screen_get_width (screen);
		*height = gdk_screen_get_height (screen);
	}
}

static gboolean
eel_background_ensure_realized (EelBackground *background, GdkWindow *window)
{
	gpointer data;
	GtkWidget *widget;
	GtkStyle *style;
	gboolean changed;
	int entire_width;
	int entire_height;
	
	drawable_get_adjusted_size (background, window, &entire_width, &entire_height);
	
	/* Set the default color */
	
	/* Get the widget to which the window belongs and its style as well */
	gdk_window_get_user_data (window, &data);
	widget = GTK_WIDGET (data);
	if (widget != NULL) {
		style = gtk_widget_get_style (widget);
		if (background->details->use_base) {
			background->details->default_color = style->base[GTK_STATE_NORMAL];
		} else {
			background->details->default_color = style->bg[GTK_STATE_NORMAL];
		}
		
		gdk_rgb_find_color (style->colormap, &(background->details->default_color));
	}

	/* If the pixmap doesn't change with the window size, never update
	 * it again.
	 */
	if (background->details->background_pixmap != NULL &&
	    !background->details->background_changes_with_size) {
		return FALSE;
	}

	/* If the window size is the same as last time, don't update */
	if (entire_width == background->details->background_entire_width &&
	    entire_height == background->details->background_entire_height) {
		return FALSE;
	}

	free_background_pixmap (background);

	changed = FALSE;

	set_image_properties (background);

	background->details->background_changes_with_size = gnome_bg_changes_with_size (background->details->bg);
	background->details->background_pixmap = gnome_bg_create_pixmap (background->details->bg,
									 window,
									 entire_width, entire_height,
									 background->details->is_desktop);
	background->details->background_pixmap_is_unset_root_pixmap = background->details->is_desktop;
		
	/* We got the pixmap and everything, so we don't care about a change
	   that is pending (unless things actually change after this time) */
	g_object_set_data (G_OBJECT (background->details->bg),
			   "ignore-pending-change", GINT_TO_POINTER (TRUE));
	changed = TRUE;
	
	
	background->details->background_entire_width = entire_width;
	background->details->background_entire_height = entire_height;
	
	return changed;
}

static GdkPixmap *
eel_background_get_pixmap_and_color (EelBackground *background,
				     GdkWindow     *window,
				     GdkColor      *color,
				     gboolean      *changes_with_size)
{
	int entire_width;
	int entire_height;

	drawable_get_adjusted_size (background, window, &entire_width, &entire_height);

	eel_background_ensure_realized (background, window);
	
	*color = background->details->default_color;
	*changes_with_size = background->details->background_changes_with_size;
	
	if (background->details->background_pixmap != NULL) {
		return g_object_ref (background->details->background_pixmap);
	} 
	return NULL;
}

void
eel_background_expose (GtkWidget                   *widget,
		       GdkEventExpose              *event)
{
	GdkColor color;
	int window_width;
	int window_height;
	gboolean changes_with_size;
	GdkPixmap *pixmap;
	GdkGC *gc;
	GdkGCValues gc_values;
	GdkGCValuesMask value_mask;

	EelBackground *background;
	
	if (event->window != widget->window) {
		return;
	}

	background = eel_get_widget_background (widget);

	drawable_get_adjusted_size (background, widget->window, &window_width, &window_height);
	
	pixmap = eel_background_get_pixmap_and_color (background,
						      widget->window,
						      &color,
						      &changes_with_size);

        if (!changes_with_size) {
                /* The background was already drawn by X, since we set
                 * the GdkWindow background/back_pixmap.
                 * No need to draw it again. */
                if (pixmap) {
                        g_object_unref (pixmap);
                }
                return;
        }
 
	if (pixmap) {
		gc_values.tile = pixmap;
		gc_values.ts_x_origin = 0;
		gc_values.ts_y_origin = 0;
		gc_values.fill = GDK_TILED;
		value_mask = GDK_GC_FILL | GDK_GC_TILE | GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN;
	} else {
		gdk_rgb_find_color (gtk_widget_get_colormap (widget), &color);
		gc_values.foreground = color;
		gc_values.fill = GDK_SOLID;
		value_mask = GDK_GC_FILL | GDK_GC_FOREGROUND;
	}
	
	gc = gdk_gc_new_with_values (widget->window, &gc_values, value_mask);
	
	gdk_gc_set_clip_rectangle (gc, &event->area);

	gdk_draw_rectangle (widget->window, gc, TRUE, 0, 0, window_width, window_height);
	
	g_object_unref (gc);
	
	if (pixmap) {
		g_object_unref (pixmap);
	}
}

static void
set_image_properties (EelBackground *background)
{
	if (!background->details->color) {
		gnome_bg_set_color (background->details->bg, GNOME_BG_COLOR_SOLID,
				 &background->details->default_color, NULL);
	} else if (!eel_gradient_is_gradient (background->details->color)) {
		GdkColor c;

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

/* Use style->base as the default color instead of bg */
void
eel_background_set_use_base (EelBackground *background,
			     gboolean use_base)
{
	background->details->use_base = use_base;
}

void
eel_background_set_color (EelBackground *background,
			  const char *color)
{
	if (eel_strcmp (background->details->color, color) != 0) {
		g_free (background->details->color);
		background->details->color = g_strdup (color);
		
		set_image_properties (background);
	}
}

static gboolean
eel_background_set_image_uri_helper (EelBackground *background,
				     const char *image_uri,
				     gboolean emit_signal)
{
	char *filename;

	if (image_uri != NULL) {
		filename = g_filename_from_uri (image_uri, NULL, NULL);
	}
	else {
		filename = NULL;
	}
	
	gnome_bg_set_filename (background->details->bg, filename);

	if (emit_signal) {
		g_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED], 0, GDK_ACTION_COPY);
	}

	set_image_properties (background);
	
	g_free (filename);
	
	return TRUE;
}

void
eel_background_set_image_uri (EelBackground *background, const char *image_uri)
{
	
	
	eel_background_set_image_uri_helper (background, image_uri, TRUE);
}

/* Use this fn to set both the image and color and avoid flash. The color isn't
 * changed till after the image is done loading, that way if an update occurs
 * before then, it will use the old color and image.
 */
static void
eel_background_set_image_uri_and_color (EelBackground *background, GdkDragAction action,
					const char *image_uri, const char *color)
{
	eel_background_set_image_uri_helper (background, image_uri, FALSE);
	eel_background_set_color (background, color);

	/* We always emit, even if the color didn't change, because the image change
	 * relies on us doing it here.
	 */

	g_signal_emit (background, signals[SETTINGS_CHANGED], 0, action);
}

void
eel_background_receive_dropped_background_image (EelBackground *background,
						 GdkDragAction action,
						 const char *image_uri)
{
	/* Currently, we only support tiled images. So we set the placement.
	 * We rely on eel_background_set_image_uri_and_color to emit
	 * the SETTINGS_CHANGED & APPEARANCE_CHANGE signals.
	 */
	eel_background_set_image_placement (background, EEL_BACKGROUND_TILED);
	
	eel_background_set_image_uri_and_color (background, action, image_uri, NULL);
}

/**
 * eel_background_is_set:
 * 
 * Check whether the background's color or image has been set.
 */
gboolean
eel_background_is_set (EelBackground *background)
{
	g_assert (EEL_IS_BACKGROUND (background));

	return background->details->color != NULL
		|| gnome_bg_get_filename (background->details->bg) != NULL;
}

/**
 * eel_background_reset:
 *
 * Emit the reset signal to forget any color or image that has been
 * set previously.
 */
void
eel_background_reset (EelBackground *background)
{
	g_return_if_fail (EEL_IS_BACKGROUND (background));

	g_signal_emit (GTK_OBJECT (background), signals[RESET], 0);
}

static void
set_root_pixmap (EelBackground *background,
                 GdkWindow     *window)
{
	GdkPixmap *pixmap, *root_pixmap;
	GdkScreen *screen;
	GdkColor color;
	gboolean changes_with_size;

	pixmap = eel_background_get_pixmap_and_color (background,
						      window,
						      &color,
						      &changes_with_size);
	screen = gdk_drawable_get_screen (window);

	if (background->details->use_common_pixmap) {
		background->details->background_pixmap_is_unset_root_pixmap = FALSE;
		root_pixmap = g_object_ref (pixmap);
	} else {
		root_pixmap = gnome_bg_create_pixmap (background->details->bg, window,
						      gdk_screen_get_width (screen), gdk_screen_get_height (screen), TRUE);
	}

	gnome_bg_set_pixmap_as_root (screen, pixmap);

	g_object_unref (pixmap);
	g_object_unref (root_pixmap);
}

static gboolean
fade_to_pixmap (EelBackground *background,
		 GdkWindow     *window,
		 GdkPixmap     *pixmap)
{
	if (background->details->fade == NULL) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_set_end_pixmap (background->details->fade,
				                pixmap)) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_is_started (background->details->fade)) {
		gnome_bg_crossfade_start (background->details->fade, window);
		if (background->details->is_desktop) {
			g_signal_connect_swapped (background->details->fade,
					          "finished",
						  G_CALLBACK (set_root_pixmap), background);
		}
	}

	return gnome_bg_crossfade_is_started (background->details->fade);
}


static void
eel_background_set_up_widget (EelBackground *background, GtkWidget *widget)
{
	GtkStyle *style;
	GdkPixmap *pixmap;
	GdkColor color;
	
	int window_width;
	int window_height;
	
	GdkWindow *window;
	gboolean changes_with_size;
	gboolean in_fade;

	if (!GTK_WIDGET_REALIZED (widget)) {
		return;
	}

	drawable_get_adjusted_size (background, widget->window, &window_width, &window_height);
	
	pixmap = eel_background_get_pixmap_and_color (background,
						      widget->window,
						      &color, 
						      &changes_with_size);

	style = gtk_widget_get_style (widget);
	
	gdk_rgb_find_color (style->colormap, &color);

	if (EEL_IS_CANVAS (widget)) {
		window = GTK_LAYOUT (widget)->bin_window;
	} else {
		window = widget->window;
	}

	if (background->details->fade != NULL) {
		in_fade = fade_to_pixmap (background, window, pixmap);
	} else {
		in_fade = FALSE;
	}

	if (!in_fade) {
	if (!changes_with_size || background->details->is_desktop) {
		gdk_window_set_back_pixmap (window, pixmap, FALSE);
	} else {
		gdk_window_set_back_pixmap (window, NULL, FALSE);
		gdk_window_set_background (window, &color);
	}
        }
	
	background->details->background_changes_with_size =
		gnome_bg_changes_with_size (background->details->bg);
	
	if (background->details->is_desktop && !in_fade) {
		set_root_pixmap (background, window);
	}
	
	if (pixmap) {
		g_object_unref (pixmap);
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
init_fade (EelBackground *background, GtkWidget *widget)
{
	if (widget == NULL || !GTK_WIDGET_REALIZED (widget))
		return;

	if (background->details->fade == NULL) {
		int old_width, old_height, width, height;

		/* If this was the result of a screen size change,
		 * we don't want to crossfade
		 */
		gdk_drawable_get_size (widget->window, &old_width, &old_height);
		drawable_get_adjusted_size (background, widget->window,
					    &width, &height);
		if (old_width == width && old_height == height) {
			background->details->fade = gnome_bg_crossfade_new (width, height);
			g_signal_connect_swapped (background->details->fade,
						"finished",
						G_CALLBACK (free_fade),
						background);
		}
	}

	if (background->details->fade != NULL && !gnome_bg_crossfade_is_started (background->details->fade)) {
		GdkPixmap *start_pixmap;

		if (background->details->background_pixmap == NULL) {
			start_pixmap = gnome_bg_get_pixmap_from_root (gtk_widget_get_screen (widget));
		} else {
			start_pixmap = g_object_ref (background->details->background_pixmap);
		}
		gnome_bg_crossfade_set_start_pixmap (background->details->fade,
						     start_pixmap);
                g_object_unref (start_pixmap);
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
	g_signal_emit (background, signals[APPEARANCE_CHANGED], 0);
}


static void
widget_realized_setup (GtkWidget *widget, gpointer data)
{
	EelBackground *background;
	
	background = EEL_BACKGROUND (data);
	
        if (background->details->is_desktop) {
		GdkWindow *root_window;	
		GdkScreen *screen;
		
		screen = gtk_widget_get_screen (widget);

		if (background->details->screen_size_handler > 0) {
		        g_signal_handler_disconnect (screen,
				                     background->details->screen_size_handler);
		}
	
		background->details->screen_size_handler = 
			g_signal_connect (screen, "size_changed",
            				  G_CALLBACK (screen_size_changed), background);

		root_window = gdk_screen_get_root_window(screen);			
		
		if (gdk_drawable_get_visual (root_window) == gtk_widget_get_visual (widget)) {
			background->details->use_common_pixmap = TRUE;
		} else {
			background->details->use_common_pixmap = FALSE;
		}

		init_fade (background, widget);
	}
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
	background->details->use_common_pixmap = FALSE;
}

void
eel_background_set_desktop (EelBackground *background, GtkWidget *widget, gboolean is_desktop)
{
	background->details->is_desktop = is_desktop;

	if (GTK_WIDGET_REALIZED(widget) && background->details->is_desktop) {
		widget_realized_setup (widget, background);
	}
	
}

gboolean
eel_background_is_desktop (EelBackground *background)
{
	return background->details->is_desktop;
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

   If the widget is a canvas, nothing more needs to be done.  For
   normal widgets, you need to call eel_background_expose() from your
   expose handler to draw the background.

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
	g_object_ref_sink (background);
	g_object_set_data_full (G_OBJECT (widget), "eel_background",
				background, g_object_unref);
	background->details->widget = widget;
 	g_signal_connect_object (widget, "destroy", G_CALLBACK (on_widget_destroyed), background, 0);

	/* Arrange to get the signal whenever the background changes. */
	g_signal_connect_object (background, "appearance_changed",
				 G_CALLBACK (eel_widget_queue_background_change), widget, G_CONNECT_SWAPPED);
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

/* determine if a background is darker or lighter than average, to help clients know what
   colors to draw on top with */
gboolean
eel_background_is_dark (EelBackground *background)
{
	return gnome_bg_is_dark (background->details->bg);
}
   
/* handle dropped colors */
void
eel_background_receive_dropped_color (EelBackground *background,
				      GtkWidget *widget,
				      GdkDragAction action,
				      int drop_location_x,
				      int drop_location_y,
				      const GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;
	char *new_gradient_spec;
	int left_border, right_border, top_border, bottom_border;

	g_return_if_fail (EEL_IS_BACKGROUND (background));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (selection_data != NULL);

	/* Convert the selection data into a color spec. */
	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	channels = (guint16 *) selection_data->data;
	color_spec = g_strdup_printf ("#%02X%02X%02X",
				      channels[0] >> 8,
				      channels[1] >> 8,
				      channels[2] >> 8);

	/* Figure out if the color was dropped close enough to an edge to create a gradient.
	   For the moment, this is hard-wired, but later the widget will have to have some
	   say in where the borders are.
	*/
	left_border = 32;
	right_border = widget->allocation.width - 32;
	top_border = 32;
	bottom_border = widget->allocation.height - 32;
	if (drop_location_x < left_border && drop_location_x <= right_border) {
		new_gradient_spec = eel_gradient_set_left_color_spec (background->details->color, color_spec);
	} else if (drop_location_x >= left_border && drop_location_x > right_border) {
		new_gradient_spec = eel_gradient_set_right_color_spec (background->details->color, color_spec);
	} else if (drop_location_y < top_border && drop_location_y <= bottom_border) {
		new_gradient_spec = eel_gradient_set_top_color_spec (background->details->color, color_spec);
	} else if (drop_location_y >= top_border && drop_location_y > bottom_border) {
		new_gradient_spec = eel_gradient_set_bottom_color_spec (background->details->color, color_spec);
	} else {
		new_gradient_spec = g_strdup (color_spec);
	}
	
	g_free (color_spec);

	eel_background_set_image_uri_and_color (background, action, NULL, new_gradient_spec);

	g_free (new_gradient_spec);
}

void
eel_background_save_to_gconf (EelBackground *background)
{
	GConfClient *client = gconf_client_get_default ();

	if (background->details->bg)
		gnome_bg_save_to_preferences (background->details->bg, client);
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
