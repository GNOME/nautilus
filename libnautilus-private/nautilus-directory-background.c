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

static void background_changed_callback     (EelBackground *background, 
                                             NautilusFile       *file);
static void background_reset_callback       (EelBackground *background, 
                                             NautilusFile       *file);

static void saved_settings_changed_callback (NautilusFile       *file, 
                                             EelBackground *background);
                                         
static void nautilus_file_background_receive_root_window_changes (EelBackground *background);

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

        if ((gboolean) GPOINTER_TO_INT (disconnect_signal)) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (icon_container),
					       GTK_SIGNAL_FUNC (desktop_background_realized),
					       disconnect_signal);
	}

	background = eel_get_widget_background (GTK_WIDGET (icon_container));

	gtk_object_set_data (GTK_OBJECT (background), "icon_container", (gpointer) icon_container);

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

	gtk_object_set_data (GTK_OBJECT (background), "theme_source", (gpointer) desktop_theme_source);

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
		gtk_signal_connect (GTK_OBJECT (icon_container), "realize", GTK_SIGNAL_FUNC (desktop_background_realized), GINT_TO_POINTER (TRUE));
	}

	nautilus_file_background_receive_root_window_changes (background); 
}

static gboolean
background_is_desktop (EelBackground *background)
{
	/* == works because we're carful to always use the same string.
	 */
	return gtk_object_get_data (GTK_OBJECT (background), "theme_source") == desktop_theme_source;
}

static const char *nautilus_file_background_peek_theme_source (EelBackground *background)
{
	char *theme_source;

	theme_source = gtk_object_get_data (GTK_OBJECT (background), "theme_source");

	return theme_source != NULL ? theme_source : default_theme_source;
}

static GdkWindow *
background_get_desktop_background_window (EelBackground *background)
{
	gpointer layout;

	layout = gtk_object_get_data (GTK_OBJECT (background), "icon_container");
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
		
		if (image_path && g_file_exists (image_path)) {
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

static gboolean
eel_gnome_config_string_match_no_case_with_default (const char *path, const char *test_value, gboolean *was_default)
{
	char *value;
	gboolean result;
	value = gnome_config_get_string_with_default (path, was_default);
	result = !eel_strcasecmp (value, test_value);
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
nautilus_file_background_read_desktop_settings (char **color,
                                                char **image,
                                                EelBackgroundImagePlacement *placement)
{
	int	 image_alignment;
	char*	 image_local_path;
	char*	 default_image_uri;
	gboolean no_alignment_set;
	gboolean no_image_set;
	EelBackgroundImagePlacement default_placement;
	
	char	*end_color;
	char	*start_color;
	char	*default_color;
	gboolean use_gradient;
	gboolean is_horizontal;
	gboolean no_start_color_set;
	gboolean no_end_color_set;
	gboolean no_gradient_set;
	gboolean no_gradient_orientation_set;

	char *theme_name;
	char *cur_theme_name;
	gboolean no_theme_name_set;
	gboolean switch_to_cur_theme_default;

	theme_name = gnome_config_get_string_with_default ("/Background/Default/nautilus_theme", &no_theme_name_set);

	if (no_theme_name_set) {
		nautilus_file_background_get_default_settings
			(desktop_theme_source, &default_color, &default_image_uri, &default_placement);
	} else {
		nautilus_file_background_get_default_settings_for_theme
			(theme_name, desktop_theme_source, &default_color, &default_image_uri, &default_placement);
	}

	image_local_path = gnome_config_get_string_with_default ("/Background/Default/wallpaper", &no_image_set);

	if (no_image_set) {
		*image = g_strdup (default_image_uri);
	} else if (eel_strcasecmp (image_local_path, "none") != 0) {
		*image = gnome_vfs_get_uri_from_local_path (image_local_path);
	} else {
		*image = NULL;
	}
	
	g_free(image_local_path);

	image_alignment  = gnome_config_get_int_with_default ("/Background/Default/wallpaperAlign", &no_alignment_set);

	if (no_alignment_set) {
		*placement = default_placement;
	} else {
		 switch (image_alignment) {
		 	default:
		 		g_assert_not_reached ();
			case WALLPAPER_EMBOSSED:
				/* FIXME bugzilla.gnome.org 42193: we don't support embossing.
				 * Just treat it as centered - ugh.
				 */
			case WALLPAPER_CENTERED:
				*placement = EEL_BACKGROUND_CENTERED;
				break;
			case WALLPAPER_TILED:
				*placement = EEL_BACKGROUND_TILED;
				break;
			case WALLPAPER_SCALED:
				*placement = EEL_BACKGROUND_SCALED;
				break;
			case WALLPAPER_SCALED_KEEP:
				*placement = EEL_BACKGROUND_SCALED_ASPECT;
				break;
		 }
	}

	end_color     = gnome_config_get_string_with_default ("/Background/Default/color2", &no_end_color_set);
	start_color   = gnome_config_get_string_with_default ("/Background/Default/color1", &no_start_color_set);
	use_gradient  = !eel_gnome_config_string_match_no_case_with_default ("/Background/Default/simple", "solid", &no_gradient_set);
	is_horizontal = !eel_gnome_config_string_match_no_case_with_default ("/Background/Default/gradient", "vertical", &no_gradient_orientation_set);

	if (no_gradient_set || no_gradient_orientation_set || no_start_color_set) {
		*color = g_strdup (default_color);
	} else if (use_gradient) {
		if (no_end_color_set) {
			*color = g_strdup (default_color);
		} else {
			*color = eel_gradient_new (start_color, end_color , is_horizontal);
		}
	} else {
		*color = g_strdup (start_color);
	}

	g_free(start_color);
	g_free(end_color);

	/* Since we share our settings with the background-capplet, we can't
	 * write the default values specially (e.g. by removing them entirely).
	 * 
	 * The best we can do is check to see if the settings match the default values
	 * for the theme name that's stored with them. If so, we assume it means use
	 * the default - i.e. the default from the current theme.
	 * 
	 *  - there must be a theme name stored with the settings
	 *  - if the stored theme name matches the current theme, then
	 *    we don't need to do anything since we're already using
	 *    the current theme's default values. 
	 * 
	 */
	cur_theme_name = nautilus_theme_get_theme ();

	switch_to_cur_theme_default = !no_theme_name_set &&
				      (eel_strcmp (theme_name, cur_theme_name) != 0) &&
				      nautilus_file_background_matches_default_settings
					(*color, default_color,
					 *image, default_image_uri,
					 *placement, default_placement);

	if (switch_to_cur_theme_default) {
		g_free (*color);
		g_free (*image);
		nautilus_file_background_get_default_settings (desktop_theme_source, color, image, placement);
	}

	if (switch_to_cur_theme_default || no_theme_name_set) {
		/* Writing out the actual settings for the current theme so that the
		 * background capplet will show the right settings.
		 */
		nautilus_file_background_write_desktop_settings (*color, *image, *placement);
	}

	g_free (theme_name);	
	g_free (cur_theme_name);	
	g_free(default_color);
	g_free(default_image_uri);
}

static void
nautilus_file_background_write_desktop_settings (char *color, char *image, EelBackgroundImagePlacement placement)
{
	char *end_color;
	char *start_color;
	char *image_local_path;
	char *theme_name;

	int wallpaper_align;

	int i;
	int wallpaper_count;
	char *wallpaper_path_i;
	char *wallpaper_config_path_i;
	gboolean found_wallpaper;

	if (color != NULL) {
		start_color = eel_gradient_get_start_color_spec (color);
		gnome_config_set_string ("/Background/Default/color1", start_color);		
		g_free (start_color);

		/* if color is not a gradient, this ends up writing same as start_color */
		end_color = eel_gradient_get_end_color_spec (color);
		gnome_config_set_string ("/Background/Default/color2", end_color);		
		g_free (end_color);

		gnome_config_set_string ("/Background/Default/simple", eel_gradient_is_gradient (color) ? "gradient" : "solid");
		gnome_config_set_string ("/Background/Default/gradient", eel_gradient_is_horizontal (color) ? "horizontal" : "vertical");
	} else {
		/* We set it to white here because that's how backgrounds with a NULL color
		 * are drawn by Nautilus - due to usage of eel_gdk_color_parse_with_white_default.
		 */
		gnome_config_set_string ("/Background/Default/color1", "rgb:FFFF/FFFF/FFFF");		
		gnome_config_set_string ("/Background/Default/color2", "rgb:FFFF/FFFF/FFFF");		
		gnome_config_set_string ("/Background/Default/simple", "solid");
		gnome_config_set_string ("/Background/Default/gradient", "vertical");
	}

	if (image != NULL) {
		image_local_path = gnome_vfs_get_local_path_from_uri (image);
		gnome_config_set_string ("/Background/Default/wallpaper", image_local_path);
		switch (placement) {
			case EEL_BACKGROUND_TILED:
				wallpaper_align = WALLPAPER_TILED;
				break;	
			case EEL_BACKGROUND_CENTERED:
				wallpaper_align = WALLPAPER_CENTERED;
				break;	
			case EEL_BACKGROUND_SCALED:
				wallpaper_align = WALLPAPER_SCALED;
				break;	
			case EEL_BACKGROUND_SCALED_ASPECT:
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
			if (eel_strcmp (wallpaper_path_i, image_local_path) == 0) {
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

	theme_name = nautilus_theme_get_theme ();
	gnome_config_set_string ("/Background/Default/nautilus_theme", theme_name);
	g_free (theme_name);

	gnome_config_sync ();
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
	file = gtk_object_get_data (GTK_OBJECT (background), "eel_background_file");
	if (file) {
		saved_settings_changed_callback (file, background);
	}
	return FALSE;
}

/* We don't want to respond to our own changes to the root pixmap.
 * Since there's no way to determine the origin of the x-event (or mark it)
 * we use this variable determine if we think the next PropertyNotify is
 * due to us.
 */
static int set_root_pixmap_count = 0;

static GdkFilterReturn
nautilus_file_background_event_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;
	EelBackground *background;

	xevent = (XEvent *) gdk_xevent;

	if (xevent->type == PropertyNotify && xevent->xproperty.atom == gdk_atom_intern("ESETROOT_PMAP_ID", TRUE)) {

		/* If we caused it, ignore it.
		 */
		if (set_root_pixmap_count > 0) {
			--set_root_pixmap_count;
			return GDK_FILTER_CONTINUE;
		}
		
	    	background = EEL_BACKGROUND (data);
	    	/* FIXME bugzilla.gnome.org 42220:
	    	 * We'd like to call saved_settings_changed_callback right here, directly.
	    	 * However, the current version of the property-background capplet saves
	    	 * the new setting in gnome_config AFTER setting the root window's property -
	    	 * i.e. after we get this event. How long afterwards is not knowable - we
	    	 * guess half a second. Fixing this requires changing the capplet.
	    	 */
	    	gtk_timeout_add (500, (GtkFunction) (call_settings_changed), background);
	}

	return GDK_FILTER_CONTINUE;
}

static void
desktop_background_destroyed_callback (EelBackground *background, void *georgeWBush)
{
	gdk_window_remove_filter (GDK_ROOT_PARENT(), nautilus_file_background_event_filter, background);
}

static void
nautilus_file_background_receive_root_window_changes (EelBackground *background)
{
	XWindowAttributes attribs = { 0 };

	/* set up a filter on the root window to get notified about property changes */
	gdk_window_add_filter (GDK_ROOT_PARENT(), nautilus_file_background_event_filter, background);

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
	Display *display;
	Pixmap result;

	gdk_flush ();

	display = XOpenDisplay (gdk_display_name);

	XSetCloseDownMode (display, RetainPermanent);

	result = XCreatePixmap (display,
				DefaultRootWindow (display),
				width, height,
				DefaultDepthOfScreen (DefaultScreenOfDisplay (GDK_DISPLAY())));

	XCloseDisplay (display);

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
	int     result;
	gint    format;
	gulong  nitems;
	gulong  bytes_after;
	guchar *data_esetroot;
	Pixmap  pixmap_id;
	GdkAtom type;

	data_esetroot = NULL;

	XGrabServer (GDK_DISPLAY());

	result = XGetWindowProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(),
				     gdk_atom_intern("ESETROOT_PMAP_ID", FALSE),
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

	++set_root_pixmap_count;
	
	pixmap_id = GDK_WINDOW_XWINDOW (pixmap);

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
image_loading_done_callback (EelBackground *background, gboolean successful_load, void *disconnect_signal)
{
	int	      width;
	int	      height;
	GdkGC        *gc;
	GdkPixmap    *pixmap;
	GdkWindow    *background_window;

        if ((gboolean) GPOINTER_TO_INT (disconnect_signal)) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
					       GTK_SIGNAL_FUNC (image_loading_done_callback),
					       disconnect_signal);
	}

	width  = gdk_screen_width ();
	height = gdk_screen_height ();

	pixmap = make_root_pixmap (width, height);
	gc = gdk_gc_new (pixmap);
	eel_background_draw_to_drawable (background, pixmap, gc, 0, 0, width, height, width, height);
	gdk_gc_unref (gc);

	set_root_pixmap (pixmap);

	background_window = background_get_desktop_background_window (background);
	if (background_window != NULL) {
		gdk_window_set_back_pixmap (background_window, pixmap, FALSE);
	}

	/* We'd like to simply unref pixmap here, but due to a bug in gdk's handling of
	 * foreign pixmaps, we can't - it would free the X resource.
	 *
	 * gdk_window_set_back_pixmap does not need the gdk pixmap object to stick around.
	 * It simply uses X resource inside it. dispose_root_pixmap free's the gdk object
	 * and not the X resource.
	 */
	dispose_root_pixmap (pixmap);
}

static void
nautilus_file_update_desktop_pixmaps (EelBackground *background)
{	
	if (eel_background_is_loaded (background)) {
		image_loading_done_callback (background, TRUE, GINT_TO_POINTER (FALSE));
	} else {
		gtk_signal_connect (GTK_OBJECT (background),
				    "image_loading_done",
				    GTK_SIGNAL_FUNC (image_loading_done_callback),
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
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "eel_background_file") == file);
        

	color = eel_background_get_color (background);
	image = eel_background_get_image_uri (background);

	if (background_is_desktop (background)) {
		nautilus_file_background_write_desktop_settings (color, image, eel_background_get_image_placement (background));
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (file),
	                                          saved_settings_changed_callback,
	                                          background);

		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR,
                                            NULL,
                                            color);

		nautilus_file_set_metadata (file,
                                            NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE,
                                            NULL,
                                            image);
						 
	        /* Unblock the handler. */
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (file),
	                                            saved_settings_changed_callback,
	                                            background);
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
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "eel_background_file")
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
        gtk_signal_handler_block_by_func (GTK_OBJECT (background),
                                          background_changed_callback,
                                          file);

	eel_background_set_color (background, color);     
	eel_background_set_image_uri (background, image);
        eel_background_set_image_placement (background, placement);
	
	/* Unblock the handler. */
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (background),
                                            background_changed_callback,
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
	file = gtk_object_get_data (GTK_OBJECT (background), "eel_background_file");
	if (file) {
		saved_settings_changed_callback (file, background);
	}
}

/* handle the background reset signal by setting values from the current theme */
static void
background_reset_callback (EelBackground *background,
                           NautilusFile       *file)
{
	if (background_is_desktop (background)) {
		nautilus_file_background_write_desktop_default_settings ();
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (file),
	                                          saved_settings_changed_callback,
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
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (file),
	                                            saved_settings_changed_callback,
	                                            background);
	}

	saved_settings_changed_callback (file, background);
}

/* handle the background destroyed signal */
static void
background_destroyed_callback (EelBackground *background,
                               NautilusFile       *file)
{
        gtk_signal_disconnect_by_func (GTK_OBJECT (file),
                                       GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                       background);
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
	old_file = gtk_object_get_data (GTK_OBJECT (background), "eel_background_file");
	if (old_file == file) {
		return;
	}

	/* Disconnect old signal handlers. */
	if (old_file != NULL) {
		g_assert (NAUTILUS_IS_FILE (old_file));
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_changed_callback),
                                               old_file);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_destroyed_callback),
                                               old_file);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_reset_callback),
                                               old_file);
		gtk_signal_disconnect_by_func (GTK_OBJECT (old_file),
                                               GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                               background);
		nautilus_file_monitor_remove (old_file, background);
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
                                                 nautilus_file_background_theme_changed,
                                                 background);

	}

        /* Attach the new directory. */
        nautilus_file_ref (file);
        gtk_object_set_data_full (GTK_OBJECT (background),
                                  "eel_background_file",
                                  file,
                                  (GtkDestroyNotify) nautilus_file_unref);

        /* Connect new signal handlers. */
        if (file != NULL) {
                gtk_signal_connect (GTK_OBJECT (background),
                                    "settings_changed",
                                    GTK_SIGNAL_FUNC (background_changed_callback),
                                    file);
                gtk_signal_connect (GTK_OBJECT (background),
                                    "destroy",
                                    GTK_SIGNAL_FUNC (background_destroyed_callback),
                                    file);
                gtk_signal_connect (GTK_OBJECT (background),
				    "reset",
				    GTK_SIGNAL_FUNC (background_reset_callback),
				    file);
		gtk_signal_connect (GTK_OBJECT (file),
                                    "changed",
                                    GTK_SIGNAL_FUNC (saved_settings_changed_callback),
                                    background);
        	
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
