/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   nautilus-directory-background.c: Helper for the background of a widget
                                    that is viewing a particular location.
 
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-directory-background.h"

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-background.h>
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-file-attributes.h"
#include <eel/eel-string.h>
#include "nautilus-theme.h"
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libbackground/preferences.h>

static void background_changed_callback     (EelBackground *background, 
                                             NautilusFile       *file);
static void background_reset_callback       (EelBackground *background, 
                                             NautilusFile       *file);

static void saved_settings_changed_callback (NautilusFile       *file, 
                                             EelBackground *background);
                                         
static void nautilus_file_background_receive_gconf_changes (EelBackground *background);

static void nautilus_file_update_desktop_pixmaps (EelBackground *background);

static void nautilus_file_background_write_desktop_settings (char *color,
							     char *image,
							     EelBackgroundImagePlacement placement);

static gboolean nautilus_file_background_matches_default_settings (
			const char* color, const char* default_color,
			const char* image, const char* default_image,
			EelBackgroundImagePlacement placement, EelBackgroundImagePlacement default_placement);

static void
desktop_background_realized (NautilusIconContainer *icon_container, void *disconnect_signal)
{
	EelBackground *background;

        if (GPOINTER_TO_INT (disconnect_signal)) {
                g_signal_handlers_disconnect_by_func
                        (icon_container,
                         G_CALLBACK (desktop_background_realized),
                         disconnect_signal);
	}

	background = eel_get_widget_background (GTK_WIDGET (icon_container));
        eel_background_set_is_constant_size (background, TRUE);
                                          
	g_object_set_data (G_OBJECT (background), "icon_container", (gpointer) icon_container);

	nautilus_file_update_desktop_pixmaps (background);
}

static const char *default_theme_source = "directory";
static const char *desktop_theme_source = "desktop";

void
nautilus_connect_desktop_background_to_file_metadata (NautilusIconContainer *icon_container,
                                                      NautilusFile *file)
{
	EelBackground *background;

	background = eel_get_widget_background (GTK_WIDGET (icon_container));

	g_object_set_data (G_OBJECT (background), "theme_source", (gpointer) desktop_theme_source);

	/* Strictly speaking, we don't need to know about metadata changes, since
	 * the desktop setting aren't stored there. But, hooking up to metadata
	 * changes is actually a small part of what this fn does, and we do need
	 * the other stuff (hooked up to background & theme changes, initialize
	 * the background). Being notified of metadata changes on the file is a
	 * waste, but won't hurt, so I don't think it's worth refactoring the fn
	 * at this point.
	 */
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (icon_container), file);

	if (GTK_WIDGET_REALIZED (icon_container)) {
		desktop_background_realized (icon_container, GINT_TO_POINTER (FALSE));
	} else {
		g_signal_connect (icon_container, "realize",
                                  G_CALLBACK (desktop_background_realized), GINT_TO_POINTER (TRUE));
	}

	nautilus_file_background_receive_gconf_changes (background); 
}

static gboolean
background_is_desktop (EelBackground *background)
{
	/* == works because we're carful to always use the same string.
	 */
	return g_object_get_data (G_OBJECT (background), "theme_source") == desktop_theme_source;
}

static const char *nautilus_file_background_peek_theme_source (EelBackground *background)
{
	char *theme_source;

	theme_source = g_object_get_data (G_OBJECT (background), "theme_source");

	return theme_source != NULL ? theme_source : default_theme_source;
}

static GdkWindow *
background_get_desktop_background_window (EelBackground *background)
{
	gpointer layout;

	layout = g_object_get_data (G_OBJECT (background), "icon_container");
	return layout != NULL ? GTK_LAYOUT (layout)->bin_window : NULL;
}

/* utility routine to handle mapping local image files in themes to a uri */
static char*
theme_image_path_to_uri (char *image_file, const char *theme_name)
{
	char *image_path;
	char *image_uri;

	if (image_file != NULL && !eel_istr_has_prefix (image_file, "file://")) {
		
		if (eel_str_has_prefix (image_file, "./")) {
			image_path = nautilus_theme_get_image_path_from_theme (image_file + 2, theme_name);
		} else {
			image_path = g_strdup_printf ("%s/%s", NAUTILUS_DATADIR, image_file);
		}
		
		if (image_path && g_file_test (image_path, G_FILE_TEST_EXISTS)) {
			image_uri = gnome_vfs_get_uri_from_local_path (image_path);
		} else {
			image_uri = NULL;
		}
		
		g_free (image_path);
	} else {
		image_uri = g_strdup (image_file);
	}

	return image_uri;
}
 
static void
nautilus_file_background_get_default_settings_for_theme (const char* theme_name,
							  const char* theme_source,
							  char **color,
							  char **image,
							  EelBackgroundImagePlacement *placement)
{
	char *image_local_path;

	if (placement != NULL) {
		*placement = EEL_BACKGROUND_TILED;
	}

	if (color != NULL) {
		*color = nautilus_theme_get_theme_data_from_theme (theme_source, NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR, theme_name);
	}

	if (image != NULL) {
		image_local_path = nautilus_theme_get_theme_data_from_theme (theme_source, NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE, theme_name);
		*image = theme_image_path_to_uri (image_local_path, theme_name);
		g_free (image_local_path);
	}
}

static void
nautilus_file_background_get_default_settings (const char* theme_source,
                                               char **color,
                                               char **image,
                                               EelBackgroundImagePlacement *placement)
{
	char *theme_name;
	theme_name = nautilus_theme_get_theme ();
	nautilus_file_background_get_default_settings_for_theme
		(theme_name, theme_source, color, image, placement);
	g_free (theme_name);
}

static void
nautilus_file_background_read_desktop_settings (char **color,
                                                char **image,
                                                EelBackgroundImagePlacement *placement)
{
	char*	 default_image_uri;
	EelBackgroundImagePlacement default_placement;
	
	char	*end_color;
	char	*start_color;
	char	*default_color;
	gboolean use_gradient;
	gboolean is_horizontal;

	char *theme_name;
	BGPreferences *prefs;

        prefs = BG_PREFERENCES (bg_preferences_new ());

	bg_preferences_load (prefs);

	theme_name = nautilus_theme_get_theme ();

	nautilus_file_background_get_default_settings_for_theme
		(theme_name, desktop_theme_source, &default_color, &default_image_uri, &default_placement);

        if (prefs->wallpaper_enabled) {
                if (prefs->wallpaper_filename != NULL &&
                    prefs->wallpaper_filename [0] != '\0') {
                        *image = gnome_vfs_get_uri_from_local_path (prefs->wallpaper_filename);
                } else {
                        *image = g_strdup (default_image_uri);
                }
	}
        else {
		*image = NULL;
	}
	
        switch (prefs->wallpaper_type) {
        default:
                g_assert_not_reached ();

/*        case WPTYPE_EMBOSSED:*/
                /* FIXME bugzilla.gnome.org 42193: we don't support embossing.
                 * Just treat it as centered - ugh.
                 */
        case WPTYPE_CENTERED:
                *placement = EEL_BACKGROUND_CENTERED;
                break;
        case WPTYPE_TILED:
                *placement = EEL_BACKGROUND_TILED;
                break;
        case WPTYPE_STRETCHED:
                *placement = EEL_BACKGROUND_SCALED;
                break;
        case WPTYPE_SCALED:
                *placement = EEL_BACKGROUND_SCALED_ASPECT;
                break;
        }
	
        end_color     = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (prefs->color2));
	start_color   = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (prefs->color1));
	use_gradient  = prefs->gradient_enabled;
	is_horizontal = (prefs->orientation == ORIENTATION_HORIZ);

	if (use_gradient) {
		*color = eel_gradient_new (start_color, end_color, is_horizontal);
	} else {
		*color = g_strdup (start_color);
	}

	g_free (start_color);
	g_free (end_color);

	g_free (theme_name);	
	g_free (default_color);
	g_free (default_image_uri);

	g_object_unref (prefs);
}

static void
nautilus_file_background_write_desktop_settings (char *color, char *image, EelBackgroundImagePlacement placement)
{
	char *end_color;
	char *start_color;
	char *default_image_uri;

	wallpaper_type_t wallpaper_align;
	BGPreferences *prefs;

        prefs = BG_PREFERENCES (bg_preferences_new ());
	bg_preferences_load (prefs);

	if (color != NULL) {
		start_color = eel_gradient_get_start_color_spec (color);
		gdk_color_parse (start_color, prefs->color1);
		g_free (start_color);

		/* if color is not a gradient, this ends up writing same as start_color */
		end_color = eel_gradient_get_end_color_spec (color);
		gdk_color_parse (end_color, prefs->color2);
		g_free (end_color);

		if (eel_gradient_is_gradient (color)) {
			prefs->gradient_enabled = TRUE;
			prefs->orientation = eel_gradient_is_horizontal (color) ? ORIENTATION_HORIZ : ORIENTATION_VERT;
		} else {
			prefs->gradient_enabled = FALSE;
			prefs->orientation = ORIENTATION_SOLID;
		}
	} else {
		/* We set it to white here because that's how backgrounds with a NULL color
		 * are drawn by Nautilus - due to usage of eel_gdk_color_parse_with_white_default.
		 */
		gdk_color_parse ("#FFFFFF", prefs->color1);
		gdk_color_parse ("#FFFFFF", prefs->color2);
		prefs->gradient_enabled = FALSE;
		prefs->orientation = ORIENTATION_SOLID;
	}

	g_free (prefs->wallpaper_filename);
	if (image != NULL) {
		prefs->wallpaper_filename = gnome_vfs_get_local_path_from_uri (image);
                prefs->wallpaper_enabled = TRUE;
		switch (placement) {
			case EEL_BACKGROUND_TILED:
				wallpaper_align = WPTYPE_TILED;
				break;	
			case EEL_BACKGROUND_CENTERED:
				wallpaper_align = WPTYPE_CENTERED;
				break;	
			case EEL_BACKGROUND_SCALED:
				wallpaper_align = WPTYPE_STRETCHED;
				break;	
			case EEL_BACKGROUND_SCALED_ASPECT:
				wallpaper_align = WPTYPE_SCALED;
				break;
			default:
				g_assert_not_reached ();
				wallpaper_align = WPTYPE_TILED;
				break;	
		}
	
		prefs->wallpaper_type = wallpaper_align;
	} else {
                prefs->wallpaper_enabled = FALSE;
                /* Need to set something, or libbackground will barf */
                nautilus_file_background_get_default_settings
			(desktop_theme_source, NULL, &default_image_uri, NULL);
		prefs->wallpaper_filename = gnome_vfs_get_local_path_from_uri (default_image_uri);
                g_free (default_image_uri);
	}

	bg_preferences_save (prefs);
	g_object_unref (prefs);
}

static void
nautilus_file_background_write_desktop_default_settings ()
{
	char *color;
	char *image;
	EelBackgroundImagePlacement placement;
	nautilus_file_background_get_default_settings (desktop_theme_source, &color, &image, &placement);
	nautilus_file_background_write_desktop_settings (color, image, placement);
}

static int
call_settings_changed (EelBackground *background)
{
	NautilusFile *file;
	file = g_object_get_data (G_OBJECT (background), "eel_background_file");
	if (file) {
		saved_settings_changed_callback (file, background);
	}
	return FALSE;
}

static void
desktop_background_destroyed_callback (EelBackground *background, void *georgeWBush)
{
	guint notification_id;

        notification_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (background), "desktop_gconf_notification"));
	eel_gconf_notification_remove (notification_id);
}

static void
desktop_background_gconf_notify_cb (GConfClient *client, guint notification_id, GConfEntry *entry, gpointer data)
{
	call_settings_changed (EEL_BACKGROUND (data));
}

static void
nautilus_file_background_receive_gconf_changes (EelBackground *background)
{
	guint notification_id;

        eel_gconf_monitor_add ("/desktop/gnome/background");
        notification_id = eel_gconf_notification_add ("/desktop/gnome/background", desktop_background_gconf_notify_cb, background);

	g_object_set_data (G_OBJECT (background), "desktop_gconf_notification", GUINT_TO_POINTER (notification_id));
			
	g_signal_connect (background, "destroy",
                          G_CALLBACK (desktop_background_destroyed_callback), NULL);
}

/* Create a persistent pixmap. We create a separate display
 * and set the closedown mode on it to RetainPermanent
 * (copied from gnome-source/control-panels/capplets/background-properties/render-background.c)
 */
static GdkPixmap *
make_root_pixmap (gint width, gint height)
{
	Display *display;
        char *display_name;
	Pixmap result;
	GdkPixmap *gdk_pixmap;

	gdk_flush ();

        display_name = DisplayString (GDK_DISPLAY ());
	display = XOpenDisplay (display_name);

        if (display == NULL) {
                g_warning ("Unable to open display '%s' when setting background pixmap\n",
                           (display_name) ? display_name : "NULL");
                return NULL;
        }

	XSetCloseDownMode (display, RetainPermanent);

	result = XCreatePixmap (display,
				DefaultRootWindow (display),
				width, height,
				DefaultDepthOfScreen (DefaultScreenOfDisplay (GDK_DISPLAY())));

	XCloseDisplay (display);

	gdk_pixmap = gdk_pixmap_foreign_new (result);
	gdk_drawable_set_colormap (GDK_DRAWABLE (gdk_pixmap),
				   gdk_drawable_get_colormap (gdk_get_default_root_window ()));

	return gdk_pixmap;
}

/* Set the root pixmap, and properties pointing to it. We
 * do this atomically with XGrabServer to make sure that
 * we won't leak the pixmap if somebody else it setting
 * it at the same time. (This assumes that they follow the
 * same conventions we do
 * (copied from gnome-source/control-panels/capplets/background-properties/render-background.c)
 */
static void 
set_root_pixmap (GdkPixmap *pixmap)
{
	int     result;
	gint    format;
	gulong  nitems;
	gulong  bytes_after;
	guchar *data_esetroot;
	Pixmap  pixmap_id;
	Atom type;

	data_esetroot = NULL;

	XGrabServer (GDK_DISPLAY());

	result = XGetWindowProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
				     gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
				     0L, 1L, False, XA_PIXMAP,
				     &type, &format, &nitems, &bytes_after,
				     &data_esetroot);

	if (data_esetroot != NULL) {
		if (result == Success && type == XA_PIXMAP && format == 32 && nitems == 1) {
			gdk_error_trap_push ();
			XKillClient(GDK_DISPLAY(), *(Pixmap *)data_esetroot);
			gdk_flush ();
			gdk_error_trap_pop ();
		}
		XFree (data_esetroot);
	}

	pixmap_id = GDK_WINDOW_XWINDOW (pixmap);

	XChangeProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
			 gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);
	XChangeProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
			 gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);

	XSetWindowBackgroundPixmap (GDK_DISPLAY (), GDK_ROOT_WINDOW (), pixmap_id);
	XClearWindow (GDK_DISPLAY (), GDK_ROOT_WINDOW ());

	XUngrabServer (GDK_DISPLAY());
	
	XFlush(GDK_DISPLAY());
}
	
static void
image_loading_done_callback (EelBackground *background, gboolean successful_load, void *disconnect_signal)
{
	int	      width;
	int	      height;
	GdkGC        *gc;
	GdkPixmap    *pixmap;
	GdkWindow    *background_window;

        if (GPOINTER_TO_INT (disconnect_signal)) {
		g_signal_handlers_disconnect_by_func
                        (background,
                         G_CALLBACK (image_loading_done_callback),
                         disconnect_signal);
	}

	width  = gdk_screen_width ();
	height = gdk_screen_height ();

	pixmap = make_root_pixmap (width, height);
        if (pixmap == NULL) {
                return;
        }
        
	gc = gdk_gc_new (pixmap);
	eel_background_draw_to_drawable (background, pixmap, gc, 0, 0, width, height, width, height);
	g_object_unref (gc);

	set_root_pixmap (pixmap);

	background_window = background_get_desktop_background_window (background);
	if (background_window != NULL &&
            gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap))
		gdk_window_set_back_pixmap (background_window, pixmap, FALSE);
	}

        g_object_unref (pixmap);
}

static void
nautilus_file_update_desktop_pixmaps (EelBackground *background)
{	
	if (eel_background_is_loaded (background)) {
		image_loading_done_callback (background, TRUE, GINT_TO_POINTER (FALSE));
	} else {
		g_signal_connect (background, "image_loading_done",
                                  G_CALLBACK (image_loading_done_callback),
                                  GINT_TO_POINTER (TRUE));
	}
}

static gboolean
nautilus_file_background_matches_default_settings (
			const char* color, const char* default_color,
			const char* image, const char* default_image,
			EelBackgroundImagePlacement placement, EelBackgroundImagePlacement default_placement)
{
	gboolean match_color;
	gboolean match_image;

	/* A NULL default color or image is not considered when determining a match.
	 */
	
	match_color = (default_color == NULL) || eel_strcmp (color, default_color) == 0;
	
	match_image = (default_image == NULL) ||
		      ((eel_strcmp (image, default_image) == 0) && (placement == default_placement));
	return match_color && match_image;
}

/* return true if the background is not in the default state */
gboolean
nautilus_file_background_is_set (EelBackground *background)
{
	char *color;
	char *image;
	char *default_color;
	char *default_image;
	
	gboolean matches;
	
	EelBackgroundImagePlacement placement;
	EelBackgroundImagePlacement default_placement;

	color = eel_background_get_color (background);
	image = eel_background_get_image_uri (background);
	placement = eel_background_get_image_placement (background);
	
	nautilus_file_background_get_default_settings (
		nautilus_file_background_peek_theme_source (background),
		&default_color, &default_image, &default_placement);
		 			    
	matches = nautilus_file_background_matches_default_settings (color, default_color,
                                                                     image, default_image,
                                                                     placement, default_placement);
	
	g_free (color);
	g_free (image);
	g_free (default_color);
	g_free (default_image);
	
	return !matches;
}

/* handle the background changed signal */
static void
background_changed_callback (EelBackground *background,
                             NautilusFile       *file)
{
  	char *color;
  	char *image;
        
        g_assert (EEL_IS_BACKGROUND (background));
        g_assert (NAUTILUS_IS_FILE (file));
        g_assert (g_object_get_data (G_OBJECT (background), "eel_background_file") == file);
        

	color = eel_background_get_color (background);
	image = eel_background_get_image_uri (background);

	if (background_is_desktop (background)) {
		nautilus_file_background_write_desktop_settings (color, image, eel_background_get_image_placement (background));
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
                g_signal_handlers_block_by_func (
                        file, G_CALLBACK (saved_settings_changed_callback), background);

		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR,
                                            NULL,
                                            color);

		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE,
                                            NULL,
                                            image);
						 
	        /* Unblock the handler. */
                g_signal_handlers_unblock_by_func (
                        file, G_CALLBACK (saved_settings_changed_callback), background);
	}

	g_free (color);
	g_free (image);
	
	if (background_is_desktop (background)) {
		nautilus_file_update_desktop_pixmaps (background);
	}
}

static void
initialize_background_from_settings (NautilusFile *file,
				     EelBackground *background)
{
        char *color;
        char *image;
	EelBackgroundImagePlacement placement;
	
        g_assert (NAUTILUS_IS_FILE (file));
        g_assert (EEL_IS_BACKGROUND (background));
        g_assert (g_object_get_data (G_OBJECT (background), "eel_background_file")
                  == file);

	if (background_is_desktop (background)) {
		nautilus_file_background_read_desktop_settings (&color, &image, &placement);
	} else {
		color = nautilus_file_get_metadata (file,
                                                    NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR,
                                                    NULL);
		image = nautilus_file_get_metadata (file,
                                                    NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE,
                                                    NULL);
	        placement = EEL_BACKGROUND_TILED; /* non-tiled only avail for desktop, at least for now */

		/* if there's none, read the default from the theme */
		if (color == NULL && image == NULL) {
			nautilus_file_background_get_default_settings 
                                (nautilus_file_background_peek_theme_source (background),
                                 &color, &image, &placement);	
		}
	}

        /* Block the other handler while we are responding to changes
         * in the metadata so it doesn't try to change the metadata.
         */
        g_signal_handlers_block_by_func
                (background,
                 G_CALLBACK (background_changed_callback),
                 file);

	eel_background_set_color (background, color);     
        if (background_is_desktop(background)) {
                eel_background_set_image_uri_sync (background, image);
        }
        else {
                eel_background_set_image_uri (background, image);
        }
        eel_background_set_image_placement (background, placement);
	
	/* Unblock the handler. */
        g_signal_handlers_unblock_by_func
                (background,
                 G_CALLBACK (background_changed_callback),
                 file);
	
	g_free (color);
	g_free (image);
}

/* handle the file changed signal */
static void
saved_settings_changed_callback (NautilusFile *file,
                                 EelBackground *background)
{
	initialize_background_from_settings (file, background);
	
	if (background_is_desktop (background)) {
		nautilus_file_update_desktop_pixmaps (background);
	}
}

/* handle the theme changing */
static void
nautilus_file_background_theme_changed (gpointer user_data)
{
	NautilusFile *file;
	EelBackground *background;

	background = EEL_BACKGROUND (user_data);
	file = g_object_get_data (G_OBJECT (background), "eel_background_file");
	if (file) {
		saved_settings_changed_callback (file, background);
	}
}

/* handle the background reset signal by setting values from the current theme */
static void
background_reset_callback (EelBackground *background,
                           NautilusFile  *file)
{
	if (background_is_desktop (background)) {
		nautilus_file_background_write_desktop_default_settings ();
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        g_signal_handlers_block_by_func (
                        file,
                        G_CALLBACK (saved_settings_changed_callback),
                        background);

		/* reset the metadata */
		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR,
                                            NULL,
                                            NULL);

		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE,
                                            NULL,
                                            NULL);
	        /* Unblock the handler. */
	        g_signal_handlers_unblock_by_func (
                        file,
                        G_CALLBACK (saved_settings_changed_callback),
                        background);
	}

	saved_settings_changed_callback (file, background);
}

/* handle the background destroyed signal */
static void
background_destroyed_callback (EelBackground *background,
                               NautilusFile *file)
{
        g_signal_handlers_disconnect_by_func
                (file,
                 G_CALLBACK (saved_settings_changed_callback), background);
        nautilus_file_monitor_remove (file, background);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
                                         nautilus_file_background_theme_changed,
                                         background);
}

/* key routine that hooks up a background and location */
void
nautilus_connect_background_to_file_metadata (GtkWidget    *widget,
                                              NautilusFile *file)
{
	EelBackground *background;
	gpointer old_file;
        GList *attributes;

	/* Get at the background object we'll be connecting. */
	background = eel_get_widget_background (widget);

	/* Check if it is already connected. */
	old_file = g_object_get_data (G_OBJECT (background), "eel_background_file");
	if (old_file == file) {
		return;
	}

	/* Disconnect old signal handlers. */
	if (old_file != NULL) {
		g_assert (NAUTILUS_IS_FILE (old_file));
		g_signal_handlers_disconnect_by_func
                        (background,
                         G_CALLBACK (background_changed_callback), old_file);
		g_signal_handlers_disconnect_by_func
                        (background,
                         G_CALLBACK (background_destroyed_callback), old_file);
		g_signal_handlers_disconnect_by_func
                        (background,
                         G_CALLBACK (background_reset_callback), old_file);
		g_signal_handlers_disconnect_by_func
                        (old_file,
                         G_CALLBACK (saved_settings_changed_callback), background);
		nautilus_file_monitor_remove (old_file, background);
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
                                                 nautilus_file_background_theme_changed,
                                                 background);

	}

        /* Attach the new directory. */
        nautilus_file_ref (file);
        g_object_set_data_full (G_OBJECT (background), "eel_background_file",
                                file, (GDestroyNotify) nautilus_file_unref);

        /* Connect new signal handlers. */
        if (file != NULL) {
                g_signal_connect_object (background, "settings_changed",
                                         G_CALLBACK (background_changed_callback), file, 0);
                g_signal_connect_object (background, "destroy",
                                         G_CALLBACK (background_destroyed_callback), file, 0);
                g_signal_connect_object (background, "reset",
                                         G_CALLBACK (background_reset_callback), file, 0);
		g_signal_connect_object (file, "changed",
                                         G_CALLBACK (saved_settings_changed_callback), background, 0);
        	
		/* arrange to receive file metadata */
                attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
		nautilus_file_monitor_add (file,
                                           background,
                                           attributes);					     
		g_list_free (attributes);

		/* arrange for notification when the theme changes */
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
                                              nautilus_file_background_theme_changed, background);	

	}

        /* Update the background based on the file metadata. */
        initialize_background_from_settings (file, background);
}

void
nautilus_connect_background_to_file_metadata_by_uri (GtkWidget *widget,
                                                     const char *uri)
{
        NautilusFile *file;
        file = nautilus_file_get (uri);
        nautilus_connect_background_to_file_metadata (widget, file);
        nautilus_file_unref (file);
}
