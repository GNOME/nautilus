/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-extensions.c - Extensions to Nautilus and gtk widget.

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

#include <config.h>
#include "eazel-services-extensions.h"

#include <eel/eel-clickable-image.h>
#include <eel/eel-smooth-widget.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <time.h>

static void listen_for_smooth_graphics_changes (void);

GdkPixbuf *
eazel_services_pixbuf_new (const char *name)
{
	char *image_file_path;
 	GdkPixbuf *pixbuf = NULL;

        g_return_val_if_fail (name != NULL, NULL);
	
	image_file_path = g_strdup_printf ("%s/%s", DATADIR "/pixmaps/nautilus", name);

	if (g_file_exists (image_file_path)) {
		pixbuf =gdk_pixbuf_new_from_file (image_file_path);
	}
	g_free (image_file_path);

	return pixbuf;
}

GtkWidget *
eazel_services_image_new (const char *icon_name,
			  const char *tile_name,
			  guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	listen_for_smooth_graphics_changes ();

	if (icon_name) {
		pixbuf = eazel_services_pixbuf_new (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	image = eel_image_new_solid (pixbuf, 0.5, 0.5, 0, 0, background_color, tile_pixbuf);

	eel_gdk_pixbuf_unref_if_not_null (pixbuf);
	eel_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

GtkWidget *
eazel_services_image_new_clickable (const char *icon_name,
				    const char *tile_name,
				    guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	listen_for_smooth_graphics_changes ();

	if (icon_name) {
		pixbuf = eazel_services_pixbuf_new (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	image = eel_clickable_image_new_solid (NULL,
						    pixbuf,
						    0,
						    0,
						    0,
						    0.5,
						    0.5,
						    0,
						    0,
						    background_color,
						    tile_pixbuf);

	eel_gdk_pixbuf_unref_if_not_null (pixbuf);
	eel_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

GtkWidget *
eazel_services_image_new_from_uri (const char *uri,
				   const char *tile_name,
				   guint32 background_color,
				   int max_width,
				   int max_height)
{
	GtkWidget *image = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;

	g_return_val_if_fail (uri != NULL, NULL);

	listen_for_smooth_graphics_changes ();

	/* load the image - synchronously, at least at first */
	pixbuf = eel_gdk_pixbuf_load (uri);
	
	/* pin the image to the specified dimensions if necessary */
	if (pixbuf && max_width > 0 && max_height > 0) {
		scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, max_width, max_height);
		gdk_pixbuf_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}
		
	/* create the image widget then release the pixbuf*/
	image = eazel_services_image_new (NULL, tile_name, background_color);

	if (pixbuf != NULL) {
		eel_image_set_pixbuf (EEL_IMAGE (image), pixbuf);
	}

	eel_gdk_pixbuf_unref_if_not_null (pixbuf);

	return image;
}



GtkWidget *
eazel_services_clickable_image_new_from_uri (const char *uri,
					     const char *tile_name,
					     guint32     background_color,
					     int         max_width,
					     int         max_height)
{
	GtkWidget *image = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;

	g_return_val_if_fail (uri != NULL, NULL);

	listen_for_smooth_graphics_changes ();

	/* load the image - synchronously, at least at first */
	pixbuf = eel_gdk_pixbuf_load (uri);
	
	/* pin the image to the specified dimensions if necessary */
	if (pixbuf && max_width > 0 && max_height > 0) {
		scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, max_width, max_height);
		gdk_pixbuf_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}
		
	/* create the image widget then release the pixbuf*/
	image = eazel_services_image_new_clickable (NULL, tile_name, background_color);

	if (pixbuf != NULL) {
		eel_labeled_image_set_pixbuf (EEL_LABELED_IMAGE (image), 
						   pixbuf);
	}

	eel_gdk_pixbuf_unref_if_not_null (pixbuf);

	return image;
}

GtkWidget *
eazel_services_label_new (const char *text,
			  guint drop_shadow_offset,
			  float xalign,
			  float yalign,
			  gint xpadding,
			  gint ypadding,
			  guint32 text_color,
			  guint32 background_color,
			  const char *tile_name,
			  gint num_larger_sizes,
			  gboolean bold)
{
 	GtkWidget *label;
	GdkPixbuf *tile_pixbuf = NULL;

	listen_for_smooth_graphics_changes ();

	if (tile_name != NULL) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	label = eel_label_new_solid (text,
					  drop_shadow_offset,
					  EAZEL_SERVICES_DROP_SHADOW_COLOR_RGB,
					  text_color,
					  xalign,
					  yalign,
					  xpadding,
					  ypadding,
					  background_color,
					  tile_pixbuf);


	if (num_larger_sizes < 0) {
		eel_label_make_smaller (EEL_LABEL (label), ABS (num_larger_sizes));
	} else if (num_larger_sizes > 0) {
		eel_label_make_larger (EEL_LABEL (label), num_larger_sizes);
	}

	if (bold) {
		eel_label_make_bold (EEL_LABEL (label));
	}
	
	eel_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return label;
}

GtkWidget *
eazel_services_label_new_clickable (const char *text,
				    guint drop_shadow_offset,
				    float xalign,
				    float yalign,
				    gint xpadding,
				    gint ypadding,
				    guint32 text_color,
				    guint32 background_color,
				    const char *tile_name,
				    gint num_larger_sizes,
				    gboolean bold)
{
 	GtkWidget *label;
	GdkPixbuf *tile_pixbuf = NULL;

	listen_for_smooth_graphics_changes ();

	if (tile_name != NULL) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	label = eel_clickable_image_new_solid (text,
						    NULL,
						    drop_shadow_offset,
						    EAZEL_SERVICES_DROP_SHADOW_COLOR_RGB,
						    text_color,
						    xalign,
						    yalign,
						    xpadding,
						    ypadding,
						    background_color,
						    tile_pixbuf);

	if (num_larger_sizes < 0) {
		eel_labeled_image_make_smaller (EEL_LABELED_IMAGE (label), ABS (num_larger_sizes));
	} else if (num_larger_sizes > 0) {
		eel_labeled_image_make_larger (EEL_LABELED_IMAGE (label), num_larger_sizes);
	}

	if (bold) {
		eel_labeled_image_make_bold (EEL_LABELED_IMAGE (label));
	}
	
	eel_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return label;
}

char *
eazel_services_get_current_date_string (void)
{
	time_t my_time;
	struct tm *my_localtime;

	my_time = time (NULL);

	if (my_time == -1) {
		return g_strdup (_("Unknown Date"));
	}

	my_localtime = localtime (&my_time);

        return eel_strdup_strftime (_("%A, %B %d"), my_localtime);
}

/* FIXME: It is obviously beyond words to describe how dauntingly lame
 *        this cut and pasted code here is.  
 *	  See also the USER_LEVEL macros in eazel-services-extensions.h
 */
#define SMOOTH_GRAPHICS_KEY "/apps/nautilus/preferences/smooth_graphics_mode"
#define USER_LEVEL_KEY "/apps/nautilus/user_level"

/* Code cut-n-pasted from nautilus-gconf-extensions.c */
static gboolean
eazel_services_gconf_handle_error (GError **error)
{
	char *message;
	static gboolean shown_dialog = FALSE;
	
	g_return_val_if_fail (error != NULL, FALSE);

	if (*error != NULL) {
		g_warning (_("GConf error:\n  %s"), (*error)->message);
		if (! shown_dialog) {
			shown_dialog = TRUE;

			message = g_strdup_printf (_("GConf error:\n  %s\n"
						     "All further errors shown "
						     "only on terminal"),
						   (*error)->message);
			eel_show_error_dialog (message, _("GConf Error"), NULL);
			g_free (message);
		}
		g_error_free (*error);
		*error = NULL;

		return TRUE;
	}

	return FALSE;
}

static GConfClient *global_gconf_client = NULL;

static void
preferences_unref_global_gconf_client (void)
{
	if (global_gconf_client == NULL) {
		gtk_object_unref (GTK_OBJECT (global_gconf_client));
	}

	global_gconf_client = NULL;
}

static GConfClient *
preferences_get_global_gconf_client (void)
{
	/* Initialize gconf if needed */
	if (!gconf_is_initialized ()) {
		GError *error = NULL;
		char *argv[] = { "eazel-services", NULL };
		
		if (!gconf_init (1, argv, &error)) {
			
			if (eazel_services_gconf_handle_error (&error)) {
				return NULL;
			}
		}
	}

	if (global_gconf_client == NULL) {
		global_gconf_client = gconf_client_get_default ();

		if (global_gconf_client == NULL) {
			return NULL;
		}

		g_atexit (preferences_unref_global_gconf_client);

		gconf_client_add_dir (global_gconf_client,
				      "/apps/nautilus",
				      GCONF_CLIENT_PRELOAD_NONE,
				      NULL);
	}

	return global_gconf_client;
}

static gboolean
preferences_gconf_get_boolean (const char *key)
{
	gboolean result;
	GConfClient *client;
	GError *error = NULL;
	
	g_return_val_if_fail (key != NULL, FALSE);
	
	client = preferences_get_global_gconf_client ();
	g_return_val_if_fail (client != NULL, FALSE);
	
	result = gconf_client_get_bool (client, key, &error);
	
	if (eazel_services_gconf_handle_error (&error)) {
		result = FALSE;
	}
	
	return result;
}

static char *
preferences_gconf_get_string (const char *key)
{
	char *result;
	GConfClient *client;
	GError *error = NULL;
	
	g_return_val_if_fail (key != NULL, NULL);
	
	client = preferences_get_global_gconf_client ();
	g_return_val_if_fail (client != NULL, NULL);
	
	result = gconf_client_get_string (client, key, &error);
	
	if (eazel_services_gconf_handle_error (&error)) {
		result = g_strdup ("");
	}
	
	return result;
}

static void
smooth_graphics_changed_notice (GConfClient *client, 
				guint connection_id, 
				GConfEntry *entry, 
				gpointer notice_data)
{
	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->key != NULL);

	eel_smooth_widget_global_set_is_smooth (preferences_gconf_get_boolean (SMOOTH_GRAPHICS_KEY));
}

static void
listen_for_smooth_graphics_changes (void)
{
	static gboolean notification_installed = FALSE;
	GConfClient *client;
	GError *error;

	if (notification_installed) {
		return;
	}

	if (!gconf_is_initialized ()) {
		char *argv[] = { "eazel-services", NULL };
		error = NULL;
		
		if (!gconf_init (1, argv, &error)) {
			if (eazel_services_gconf_handle_error (&error)) {
				return;
			}
		}
	}

	client = preferences_get_global_gconf_client ();
	g_return_if_fail (client != NULL);
	
	gconf_client_notify_add (client,
				 SMOOTH_GRAPHICS_KEY,
				 smooth_graphics_changed_notice,
				 NULL,
				 NULL,
				 &error);
	
	if (eazel_services_gconf_handle_error (&error)) {
		return;
	}

	notification_installed = TRUE;

	eel_smooth_widget_global_set_is_smooth (preferences_gconf_get_boolean (SMOOTH_GRAPHICS_KEY));
}

#define DEFAULT_USER_LEVEL		EAZEL_USER_LEVEL_INTERMEDIATE

int
eazel_services_get_user_level (void)
{
	char *user_level;
	int result;

	user_level = preferences_gconf_get_string (USER_LEVEL_KEY);

	if (eel_str_is_equal (user_level, "advanced")) {
		result = EAZEL_USER_LEVEL_ADVANCED;
	} else if (eel_str_is_equal (user_level, "intermediate")) {
		result = EAZEL_USER_LEVEL_INTERMEDIATE;
	} else if (eel_str_is_equal (user_level, "novice")) {
		result = EAZEL_USER_LEVEL_NOVICE;
	} else {
		result = DEFAULT_USER_LEVEL;
	}
	
	g_free (user_level);

	return result;
}
