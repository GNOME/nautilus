/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   nautilus-directory-background.c: Helper for the background of a widget
                                    that is viewing a particular directory.
 
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

#include <config.h>
#include "nautilus-directory-background.h"

#include "libnautilus-extensions/nautilus-gdk-extensions.h"
#include "nautilus-background.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-theme.h"
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>

static void background_changed_callback     (NautilusBackground *background, NautilusDirectory  *directory);
static void background_reset_callback       (NautilusBackground *background, NautilusDirectory  *directory);

static void saved_settings_changed_callback (NautilusDirectory  *directory, NautilusBackground *background);
                                         
static void nautilus_directory_background_receive_root_window_changes (NautilusBackground *background);

static const char *default_theme_source = "directory";
static const char *desktop_theme_source = "desktop";
                       
void
static nautilus_directory_background_set_desktop (NautilusBackground *background)
{
	gtk_object_set_data (GTK_OBJECT (background), "theme_source", (gpointer) desktop_theme_source);
	nautilus_directory_background_receive_root_window_changes (background); 
}

static gboolean
nautilus_directory_background_is_desktop (NautilusBackground *background)
{
	/* == works because we're carful to always use the same string.
	 */
	return gtk_object_get_data (GTK_OBJECT (background), "theme_source") == desktop_theme_source;
}

static const char *nautilus_directory_background_peek_theme_source (NautilusBackground *background)
{
	char *theme_source;

	theme_source = gtk_object_get_data (GTK_OBJECT (background), "theme_source");

	return theme_source != NULL ? theme_source : default_theme_source;
}

/* utility routine to handle mapping local image files in themes to a uri */
static char*
theme_image_path_to_uri (char *image_file)
{
	char *image_path;
	char *image_uri;

	if (image_file != NULL && !nautilus_istr_has_prefix (image_file, "file://")) {
		
		if (nautilus_str_has_prefix (image_file, "./")) {
			image_path = nautilus_theme_get_image_path (image_file + 2);
		} else {
			image_path = g_strdup_printf ("%s/%s", NAUTILUS_DATADIR, image_file);
		}
		
		g_assert (g_file_exists (image_path));
		
		image_uri = gnome_vfs_get_uri_from_local_path (image_path);
		g_free (image_path);
	} else {
		image_uri = g_strdup (image_file);
	}

	return image_uri;
}

/* FIXME bugzilla.eazel.com 2190: combine mode (image over gradient) does not work for the
 * GNOME desktop. Two things to address are supporting it in the background capplet and
 * in whatever code initializes the desktop.
 * 
 * None of our themes currently specify this for the desktop.
 */
 
static void
nautilus_directory_background_get_default_settings (const char* theme_source,
						    char **color,
						    char **image,
						    NautilusBackgroundImagePlacement *placement,
						    gboolean *combine)
{
	char *combine_str;
	char *image_local_path;

	*placement = NAUTILUS_BACKGROUND_TILED;
	
	*color = nautilus_theme_get_theme_data (theme_source, NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR);

	image_local_path = nautilus_theme_get_theme_data (theme_source, NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE);
	*image = theme_image_path_to_uri (image_local_path);
	g_free (image_local_path);

	combine_str = nautilus_theme_get_theme_data (theme_source, "COMBINE");
	*combine = combine_str != NULL;
	g_free (combine_str);	
}

static gboolean
nautilus_gnome_config_string_match_no_case (const char *path, const char *test_value)
{
	char *value;
	gboolean result;
	value = gnome_config_get_string (path);
	result = !nautilus_strcasecmp (value, test_value);
	g_free (value);
	return result;
}

/* This enum is from gnome-source/control-center/capplets/background-properties/render-background.h */
enum {
	WALLPAPER_TILED,
	WALLPAPER_CENTERED,
	WALLPAPER_SCALED,
	WALLPAPER_SCALED_KEEP,
	WALLPAPER_EMBOSSED
};

static void
nautilus_directory_background_read_desktop_settings (char **color,
						     char **image,
						     NautilusBackgroundImagePlacement *placement,
						     gboolean *combine)
{
	int	 image_alignment;
	char*	 image_local_path;
	char*	 default_image_file;
	gboolean no_alignment;
	NautilusBackgroundImagePlacement default_placement;
	
	char	*end_color;
	char	*start_color;
	char	*default_color;
	gboolean use_gradient;
	gboolean is_horizontal;
	gboolean no_start_color;
	gboolean no_end_color;

	nautilus_directory_background_get_default_settings (desktop_theme_source, &default_color, &default_image_file, &default_placement, combine);
	/* note - value of combine comes from the theme, not currently setable in gnome_config */

	image_local_path = gnome_config_get_string ("/Background/Default/wallpaper=none");
	image_alignment  = gnome_config_get_int_with_default ("/Background/Default/wallpaperAlign", &no_alignment);

	if (nautilus_strcasecmp (image_local_path, "none") != 0) {
		*image = gnome_vfs_get_uri_from_local_path (image_local_path);
	} else {
		*image = NULL;
	}
	
	g_free(image_local_path);
	g_free(default_image_file);	

	if (no_alignment) {
		*placement = default_placement;
	} else {
		 switch (image_alignment) {
		 	default:
		 		g_assert_not_reached ();
			case WALLPAPER_EMBOSSED:
				/* FIXME bugzilla.eazel.com 2193: we don't support embossing.
				 * Just treat it as centered - ugh.
				 */
			case WALLPAPER_CENTERED:
				*placement = NAUTILUS_BACKGROUND_CENTERED;
				break;
			case WALLPAPER_TILED:
				*placement = NAUTILUS_BACKGROUND_TILED;
				break;
			case WALLPAPER_SCALED:
				*placement = NAUTILUS_BACKGROUND_SCALED;
				break;
			case WALLPAPER_SCALED_KEEP:
				*placement = NAUTILUS_BACKGROUND_SCALED_ASPECT;
				break;
		 }
	}

	end_color     = gnome_config_get_string_with_default ("/Background/Default/color2", &no_end_color);
	start_color   = gnome_config_get_string_with_default ("/Background/Default/color1", &no_start_color);
	use_gradient  = !nautilus_gnome_config_string_match_no_case ("/Background/Default/simple=solid", "solid");
	is_horizontal = !nautilus_gnome_config_string_match_no_case ("/Background/Default/gradient=vertical", "vertical");

	if (use_gradient) {
		if (no_start_color || no_end_color) {
			*color = g_strdup (default_color);
		} else {
			*color = nautilus_gradient_new (start_color, end_color , is_horizontal);
		}
	} else {
		*color = g_strdup (no_start_color ? default_color : start_color);
	}

	g_free(start_color);
	g_free(end_color);
	g_free(default_color);
}

static void
nautilus_directory_background_write_desktop_settings (char *color, char *image, NautilusBackgroundImagePlacement placement, gboolean combine)
{
	char *end_color;
	char *start_color;
	char *image_local_path;

	int wallpaper_align;

	int i;
	int wallpaper_count;
	char *wallpaper_path_i;
	char *wallpaper_config_path_i;
	gboolean found_wallpaper;

	if (color != NULL) {
		start_color = nautilus_gradient_get_start_color_spec (color);
		gnome_config_set_string ("/Background/Default/color1", start_color);		
		g_free (start_color);

		/* if color is not a gradient, this ends up writing same as start_color */
		end_color = nautilus_gradient_get_end_color_spec (color);
		gnome_config_set_string ("/Background/Default/color2", end_color);		
		g_free (end_color);

		gnome_config_set_string ("/Background/Default/simple", nautilus_gradient_is_gradient (color) ? "gradient" : "solid");
		gnome_config_set_string ("/Background/Default/gradient", nautilus_gradient_is_horizontal (color) ? "horizontal" : "vertical");
	}

	if (image != NULL) {
		image_local_path = gnome_vfs_get_local_path_from_uri (image);
		gnome_config_set_string ("/Background/Default/wallpaper", image_local_path);
		switch (placement) {
			case NAUTILUS_BACKGROUND_TILED:
				wallpaper_align = WALLPAPER_TILED;
				break;	
			case NAUTILUS_BACKGROUND_CENTERED:
				wallpaper_align = WALLPAPER_CENTERED;
				break;	
			case NAUTILUS_BACKGROUND_SCALED:
				wallpaper_align = WALLPAPER_SCALED;
				break;	
			case NAUTILUS_BACKGROUND_SCALED_ASPECT:
				wallpaper_align = WALLPAPER_SCALED_KEEP;
				break;
			default:
				g_assert_not_reached ();
				wallpaper_align = WALLPAPER_TILED;
				break;	
		}
		
		gnome_config_set_int ("/Background/Default/wallpaperAlign", wallpaper_align);

		wallpaper_count = gnome_config_get_int ("/Background/Default/wallpapers=0");
		found_wallpaper = FALSE;
		for (i = 1; i <= wallpaper_count && !found_wallpaper; ++i) {
			wallpaper_config_path_i = g_strdup_printf ("/Background/Default/wallpaper%d", i);
			wallpaper_path_i = gnome_config_get_string (wallpaper_config_path_i);
			g_free (wallpaper_config_path_i);
			if (nautilus_strcmp (wallpaper_path_i, image_local_path) == 0) {
				found_wallpaper = TRUE;
			}
			
			g_free (wallpaper_path_i);			
		}

		if (!found_wallpaper) {
			gnome_config_set_int ("/Background/Default/wallpapers", wallpaper_count + 1);
			gnome_config_set_string ("/Background/Default/wallpapers_dir", image_local_path);
			wallpaper_config_path_i = g_strdup_printf ("/Background/Default/wallpaper%d", wallpaper_count + 1);
			gnome_config_set_string (wallpaper_config_path_i, image_local_path);
			g_free (wallpaper_config_path_i);
		}
		
		g_free (image_local_path);		
	} else {
		gnome_config_set_string ("/Background/Default/wallpaper", "none");
	}

	gnome_config_sync ();
}

static void
nautilus_directory_background_write_desktop_default_settings ()
{
	char *color;
	char *image;
	gboolean combine;
	NautilusBackgroundImagePlacement placement;
	nautilus_directory_background_get_default_settings (desktop_theme_source, &color, &image, &placement, &combine);
	nautilus_directory_background_write_desktop_settings (color, image, placement, combine);
}

static int
call_settings_changed (NautilusBackground *background)
{
	NautilusDirectory *directory;
	directory = gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory");
	if (directory) {
		saved_settings_changed_callback (directory, background);
	}
	return FALSE;
}

static GdkFilterReturn
nautilus_directory_background_event_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;
	NautilusBackground *background;

	xevent = (XEvent *) gdk_xevent;

	if (xevent->type == PropertyNotify && xevent->xproperty.atom == gdk_atom_intern("ESETROOT_PMAP_ID", TRUE)) {
	    	background = NAUTILUS_BACKGROUND (data);
	    	/* FIXME bugzilla.eazel.com 2220:
	    	 * We'd like to call saved_settings_changed_callback right here, directly.
	    	 * However, the current version of the property-background capplet saves
	    	 * the new setting in gnome_config AFTER setting the root window's property -
	    	 * i.e. after we get this event. How long afterwards is not knowable - we
	    	 * guess half a second. Fixing this requires changing the capplet.
	    	 */

	    	/* FIXME bugzilla.eazel.com 3038:
	    	 * We don't want to respond to an event we originated.
	    	 */
	    	gtk_timeout_add (500, (GtkFunction) (call_settings_changed), background);
	}

	return GDK_FILTER_CONTINUE;
}

static void
desktop_background_destroyed_callback (NautilusBackground *background, void *georgeWBush)
{
	gdk_window_remove_filter (GDK_ROOT_PARENT(), nautilus_directory_background_event_filter, background);
}

static void
nautilus_directory_background_receive_root_window_changes (NautilusBackground *background)
{
	XWindowAttributes attribs = { 0 };

	/* set up a filter on the root window to get notified about property changes */
	gdk_window_add_filter (GDK_ROOT_PARENT(), nautilus_directory_background_event_filter, background);

	gdk_error_trap_push ();

	/* select events, we need to trap the kde status thingies anyway */
	XGetWindowAttributes (GDK_DISPLAY (), GDK_ROOT_WINDOW (), &attribs);
	XSelectInput (GDK_DISPLAY (), GDK_ROOT_WINDOW (), attribs.your_event_mask | PropertyChangeMask);
	
	gdk_flush ();

	gdk_error_trap_pop ();

	gtk_signal_connect (GTK_OBJECT (background),
 			    "destroy",
			    GTK_SIGNAL_FUNC (desktop_background_destroyed_callback),
			    NULL);
}

/* Create a persistant pixmap. We create a separate display
 * and set the closedown mode on it to RetainPermanent
 * (copied from gnome-source/control-panels/capplets/background-properties/render-background.c)
 */
static GdkPixmap *
make_root_pixmap (gint width, gint height)
{
	Pixmap result;

	gdk_flush ();

	XSetCloseDownMode (gdk_display, RetainPermanent);

	result = XCreatePixmap (gdk_display,
				DefaultRootWindow (gdk_display),
				width, height,
				DefaultDepthOfScreen (DefaultScreenOfDisplay (GDK_DISPLAY())));

	return gdk_pixmap_foreign_new (result);
}

/* (copied from gnome-source/control-panels/capplets/background-properties/render-background.c)
 */
static void
dispose_root_pixmap (GdkPixmap *pixmap)
{
	/* Unrefing a foreign pixmap causes it to be destroyed - so we include
	 * this bad hack, that will work for GTK+-1.2 until the problem
	 * is fixed in the next release
	 */

	GdkWindowPrivate *private = (GdkWindowPrivate *)pixmap;
	
	gdk_xid_table_remove (private->xwindow);
	g_dataset_destroy (private);
	g_free (private);

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
	GdkAtom type;
	gulong nitems, bytes_after;
	gint format;
	guchar *data_esetroot;
	Pixmap pixmap_id = GDK_WINDOW_XWINDOW (pixmap);

	XGrabServer (GDK_DISPLAY());

	XGetWindowProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
			    gdk_atom_intern("ESETROOT_PMAP_ID", FALSE),
			    0L, 1L, False, XA_PIXMAP,
			    &type, &format, &nitems, &bytes_after,
			    &data_esetroot);

	if (type == XA_PIXMAP) {
		if (format == 32 && nitems == 4)
			XKillClient(GDK_DISPLAY(), *((Pixmap*)data_esetroot));

		XFree (data_esetroot);
	}

	XChangeProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
			 gdk_atom_intern("ESETROOT_PMAP_ID", FALSE), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);
	XChangeProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
			 gdk_atom_intern("_XROOTPMAP_ID", FALSE), XA_PIXMAP,
			 32, PropModeReplace,
			 (guchar *) &pixmap_id, 1);

	XSetWindowBackgroundPixmap (GDK_DISPLAY(), GDK_ROOT_WINDOW(), pixmap_id);
	XClearWindow (GDK_DISPLAY (), GDK_ROOT_WINDOW ());

	XUngrabServer (GDK_DISPLAY());
	
	XFlush(GDK_DISPLAY());
}

static void
image_loading_done_callback (NautilusBackground *background, gboolean successful_load, void *disconnect_signal)
{
	GdkGC        *gc;
	GdkPixmap    *bg_pixmap;
	int	      width;
	int	      height;

        g_assert (NAUTILUS_IS_BACKGROUND (background));

        if ((gboolean) GPOINTER_TO_INT (disconnect_signal)) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
					       GTK_SIGNAL_FUNC (image_loading_done_callback),
					       disconnect_signal);
	}

	/* need to update the root view whether loading succeeded or not
	 */
	
	width  = gdk_screen_width ();
	height = gdk_screen_height ();
	
	bg_pixmap = make_root_pixmap (width, height);
	gc = gdk_gc_new (bg_pixmap);

	nautilus_background_draw_to_drawable (background, bg_pixmap, gc, 0, 0, width, height, width, height);
			    
	set_root_pixmap (bg_pixmap);
	
	dispose_root_pixmap (bg_pixmap);
	gdk_gc_unref (gc);
}

static void
nautilus_directory_update_root_window (NautilusBackground *background)
{
	if (nautilus_background_is_loaded (background)) {
		image_loading_done_callback (background, TRUE, GINT_TO_POINTER (FALSE));
	} else {
		gtk_signal_connect (GTK_OBJECT (background),
				    "image_loading_done",
				    GTK_SIGNAL_FUNC (image_loading_done_callback),
				    GINT_TO_POINTER (TRUE));
	}
}

/* return true if the background is not in the default state */
gboolean
nautilus_directory_background_is_set (NautilusBackground *background)
{
	char *color;
	char *image;
	char *default_color;
	char *default_image;
	
	gboolean is_set;
	gboolean combine;
	gboolean default_combine;
	
	NautilusBackgroundImagePlacement placement;
	NautilusBackgroundImagePlacement default_placement;

	color = nautilus_background_get_color (background);
	image = nautilus_background_get_image_uri (background);
	combine = nautilus_background_get_combine_mode (background);
	placement = nautilus_background_get_image_placement (background);
	
	nautilus_directory_background_get_default_settings (
		nautilus_directory_background_peek_theme_source (background),
		&default_color, &default_image, &default_placement, &default_combine);

	is_set = (default_color != NULL && nautilus_strcmp (color, default_color) != 0) ||
		 (default_image != NULL && nautilus_strcmp (image, default_image) != 0) ||
		 (placement != default_placement) ||
		 (combine != default_combine);

	g_free (color);
	g_free (image);
	g_free (default_color);
	g_free (default_image);
	
	return is_set;
}

/* handle the background changed signal */
static void
background_changed_callback (NautilusBackground *background,
                             NautilusDirectory *directory)
{
  	char *color, *image;
        
        g_assert (NAUTILUS_IS_BACKGROUND (background));
        g_assert (NAUTILUS_IS_DIRECTORY (directory));
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory")
                  == directory);
        

	color = nautilus_background_get_color (background);
	image = nautilus_background_get_image_uri (background);

	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_write_desktop_settings (color, image, nautilus_background_get_image_placement (background), nautilus_background_get_combine_mode (background));
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (directory),
	                                          saved_settings_changed_callback,
	                                          background);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
						 NULL,
						 color);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
						 NULL,
						 image);
						 
	        /* Unblock the handler. */
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (directory),
	                                            saved_settings_changed_callback,
	                                            background);
	}

	g_free (color);
	g_free (image);
	
	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_update_root_window (background);
	}
}

/* handle the directory changed signal */
static void
saved_settings_changed_callback (NautilusDirectory *directory,
                            NautilusBackground *background)
{
        char *color;
        char *image;
	gboolean combine;
	NautilusBackgroundImagePlacement placement;
	
        g_assert (NAUTILUS_IS_DIRECTORY (directory));
        g_assert (NAUTILUS_IS_BACKGROUND (background));
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory")
                  == directory);

	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_read_desktop_settings (&color, &image, &placement, &combine);
	} else {
		color = nautilus_directory_get_metadata (directory,
	                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
	                                                 NULL);
		image = nautilus_directory_get_metadata (directory,
	                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
	                                                 NULL);
	        placement = NAUTILUS_BACKGROUND_TILED; /* non-tiled only avail for desktop, at least for now */
		combine = FALSE; /* only from theme, at least for now */

		/* if there's none, read the default from the theme */
		if (color == NULL && image == NULL) {
			nautilus_directory_background_get_default_settings (
				nautilus_directory_background_peek_theme_source (background),
				&color, &image, &placement, &combine);	
		}
	}

        /* Block the other handler while we are responding to changes
         * in the metadata so it doesn't try to change the metadata.
         */
        gtk_signal_handler_block_by_func (GTK_OBJECT (background),
                                          background_changed_callback,
                                          directory);

		nautilus_background_set_color (background, color);     
		nautilus_background_set_image_uri (background, image);
        nautilus_background_set_combine_mode (background, combine);
        nautilus_background_set_image_placement (background, placement);
	
	/* Unblock the handler. */
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (background),
                                        background_changed_callback,
                                        directory);
	
	g_free (color);
	g_free (image);
}

/* handle the theme changing */
static void
nautilus_directory_background_theme_changed (gpointer user_data)
{
	NautilusDirectory *directory;
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (user_data);
	directory = gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory");
	if (directory) {
		saved_settings_changed_callback (directory, background);
	}
}

/* handle the background reset signal by setting values from the current theme */
static void
background_reset_callback (NautilusBackground *background,
                           NautilusDirectory *directory)
{
	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_write_desktop_default_settings ();
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (directory),
	                                          saved_settings_changed_callback,
	                                          background);

		/* reset the metadata */
		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
						 NULL,
						 NULL);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
						 NULL,
						 NULL);
	        /* Unblock the handler. */
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (directory),
	                                            saved_settings_changed_callback,
	                                            background);
	}

	saved_settings_changed_callback (directory, background);

	/* We don't want the default reset handler running.
	 * It will set color and image_uri  to NULL.
	 */
	gtk_signal_emit_stop_by_name (GTK_OBJECT (background), "reset");

	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_update_root_window (background);
	}
}

/* handle the background destroyed signal */
static void
background_destroyed_callback (NautilusBackground *background,
                               NautilusDirectory *directory)
{
        gtk_signal_disconnect_by_func (GTK_OBJECT (directory),
                                       GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                       background);
        nautilus_directory_file_monitor_remove (directory, background);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_directory_background_theme_changed,
					      background);
}

/* key routine that hooks up a background and directory */
void
nautilus_connect_background_to_directory_metadata (GtkWidget *widget,
                                                   NautilusDirectory *directory)
{
	NautilusBackground *background;
	gpointer old_directory;

	/* Get at the background object we'll be connecting. */
	background = nautilus_get_widget_background (widget);


	/* Check if it is already connected. */
	old_directory = gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory");
	if (old_directory == directory) {
		return;
	}

	/* Disconnect old signal handlers. */
	if (old_directory != NULL) {
		g_assert (NAUTILUS_IS_DIRECTORY (old_directory));
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_changed_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_destroyed_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_reset_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (old_directory),
                                               GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                               background);
		nautilus_directory_file_monitor_remove (old_directory, background);
		nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_directory_background_theme_changed,
					      background);

	}

        /* Attach the new directory. */
        nautilus_directory_ref (directory);
        gtk_object_set_data_full (GTK_OBJECT (background),
                                  "nautilus_background_directory",
                                  directory,
                                  (GtkDestroyNotify) nautilus_directory_unref);

        /* Connect new signal handlers. */
        if (directory != NULL) {
                gtk_signal_connect (GTK_OBJECT (background),
                                    "settings_changed",
                                    GTK_SIGNAL_FUNC (background_changed_callback),
                                    directory);
                gtk_signal_connect (GTK_OBJECT (background),
                                    "destroy",
                                    GTK_SIGNAL_FUNC (background_destroyed_callback),
                                    directory);
                gtk_signal_connect (GTK_OBJECT (background),
				    "reset",
				    GTK_SIGNAL_FUNC (background_reset_callback),
				    directory);
		gtk_signal_connect (GTK_OBJECT (directory),
                                    "metadata_changed",
                                    GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                    background);
        	
		/* arrange to receive directory metadata */
                /* FIXME bugzilla.eazel.com 2551: 
                 * This says we want to monitor the file
                 * metadata, and it has the side effect of monitoring
                 * the file list. We want to monitor the directory
                 * metadata, which would not mean monitoring the file
                 * list.  This may require a change to the
                 * NautilusDirectory API.
                 */
		nautilus_directory_file_monitor_add (directory,
						     background,
						     NULL, TRUE, FALSE);					     
		
		/* arrange for notification when the theme changes */
		nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
                                                   nautilus_directory_background_theme_changed, background);	

	}

        /* Update the background based on the directory metadata. */
        saved_settings_changed_callback (directory, background);
}

void
nautilus_connect_desktop_background_to_directory_metadata (GtkWidget *widget,
                                                   NautilusDirectory *directory)
{
	nautilus_directory_background_set_desktop (nautilus_get_widget_background (widget));

	/* Strictly speaking, we don't need to know about metadata changes, since
	 * the desktop setting aren't stored there. But, hooking up to metadata
	 * changes is actually a small part of what this fn does, and we do need
	 * the other stuff (hooked up to background & theme changes). Being notified
	 * of metadata changes on the directory is a waste, but won't hurt, so I don't
	 * think it's worth refactoring the fn at this point.
	 */
	nautilus_connect_background_to_directory_metadata (widget, directory);
}

void
nautilus_connect_background_to_directory_metadata_by_uri (GtkWidget *widget,
                                                          const char *uri)
{
        NautilusDirectory *directory;
        directory = nautilus_directory_get (uri);
        nautilus_connect_background_to_directory_metadata (widget, directory);
        nautilus_directory_unref (directory);
}
