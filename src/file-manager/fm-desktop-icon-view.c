/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

   Copyright (C) 2000, 2001 Eazel, Inc.mou

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

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
	    Miguel de Icaza <miguel@ximian.com>
*/

#include <config.h>
#include "fm-icon-container.h"
#include "fm-desktop-icon-view.h"

#include <X11/Xatom.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <dirent.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gtk/gtkcheckmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/egg-screen-exec.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory-notify.h>
#include <libnautilus-private/nautilus-file-changes-queue.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-monitor.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-volume-monitor.h>
#include <libnautilus-private/nautilus-multihead-hacks.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gtk/gtkmessagedialog.h>

static const char untranslated_trash_link_name[] = N_("Trash");
#define TRASH_LINK_NAME _(untranslated_trash_link_name)

#define DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL		"/commands/Empty Trash Conditional"
#define DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL	"/commands/Unmount Volume Conditional"
#define DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL      "/commands/Protect Conditional"
#define DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL       "/commands/Format Conditional"
#define DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL     "/commands/Media Properties Conditional"

#define DESKTOP_BACKGROUND_POPUP_PATH_DISKS	"/popups/background/Before Zoom Items/Volume Items/Disks"

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4000

struct FMDesktopIconViewDetails
{
	BonoboUIComponent *ui;
	GList *mount_black_list;
	GdkWindow *root_window;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
};

typedef struct {
	FMDesktopIconView *view;
	char *mount_path;
} MountParameters;

typedef enum {
	DELETE_MOUNT_LINKS = 1<<0,
	UPDATE_HOME_LINK   = 1<<1,
	UPDATE_TRASH_LINK  = 1<<2
} UpdateType;

static void     fm_desktop_icon_view_init                   (FMDesktopIconView      *desktop_icon_view);
static void     fm_desktop_icon_view_class_init             (FMDesktopIconViewClass *klass);
static void     fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor   *trash,
								   gboolean                state,
								   gpointer                callback_data);
static void     home_uri_changed                                  (gpointer                user_data);
static void     default_zoom_level_changed                        (gpointer                user_data);
static void     volume_mounted_callback                           (NautilusVolumeMonitor  *monitor,
								   NautilusVolume         *volume,
								   FMDesktopIconView      *icon_view);
static void     volume_unmounted_callback                         (NautilusVolumeMonitor  *monitor,
								   NautilusVolume         *volume,
								   FMDesktopIconView      *icon_view);
static void     update_desktop_directory                          (UpdateType              type);
static gboolean real_supports_auto_layout                         (FMIconView             *view);
static void     real_merge_menus                                  (FMDirectoryView        *view);
static void     real_update_menus                                 (FMDirectoryView        *view);
static gboolean real_supports_zooming                             (FMDirectoryView        *view);
static void     update_disks_menu                                 (FMDesktopIconView      *view);
static void     free_volume_black_list                            (FMDesktopIconView      *view);
static gboolean	volume_link_is_selection 			  (FMDirectoryView 	  *view);
static NautilusDeviceType volume_link_device_type                 (FMDirectoryView        *view);
static void     fm_desktop_icon_view_update_icon_container_fonts  (FMDesktopIconView      *view);

EEL_CLASS_BOILERPLATE (FMDesktopIconView,
			      fm_desktop_icon_view,
			      FM_TYPE_ICON_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

static void
desktop_directory_changed_callback (gpointer callback_data)
{
	g_free (desktop_directory);
	desktop_directory = nautilus_get_desktop_directory ();
}

static NautilusIconContainer *
get_icon_container (FMDesktopIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
icon_container_set_workarea (NautilusIconContainer *icon_container,
			     GdkScreen             *screen,
			     long                  *workareas,
			     int                    n_items)
{
	int left, right, top, bottom;
	int screen_width, screen_height;
	int i;

	left = right = top = bottom = 0;

	screen_width  = gdk_screen_get_width (screen);
	screen_height = gdk_screen_get_height (screen);

	for (i = 0; i < n_items; i += 4) {
		int x      = workareas [i];
		int y      = workareas [i + 1];
		int width  = workareas [i + 2];
		int height = workareas [i + 3];

		if ((x + width) > screen_width || (y + height) > screen_height)
			continue;

		left   = MAX (left, x);
		right  = MAX (right, screen_width - width - x);
		top    = MAX (top, y);
		bottom = MAX (bottom, screen_height - height - y);
	}

	nautilus_icon_container_set_margins (icon_container,
					     left, right, top, bottom);
}

static void
net_workarea_changed (FMDesktopIconView *icon_view,
		      GdkWindow         *window)
{
	long *workareas = NULL;
	GdkAtom type_returned;
	int format_returned;
	int length_returned;
	NautilusIconContainer *icon_container;
	GdkScreen *screen;

	g_return_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view));

	icon_container = get_icon_container (icon_view);

	gdk_error_trap_push ();
	if (!gdk_property_get (window,
			       gdk_atom_intern ("_NET_WORKAREA", FALSE),
			       gdk_x11_xatom_to_atom (XA_CARDINAL),
			       0, G_MAXLONG, FALSE,
			       &type_returned,
			       &format_returned,
			       &length_returned,
			       (guchar **) &workareas)) {
		workareas = NULL;
	}

	if (gdk_error_trap_pop ()
	    || workareas == NULL
	    || type_returned != gdk_x11_xatom_to_atom (XA_CARDINAL)
	    || (length_returned % 4) != 0
	    || format_returned != 32) {
		nautilus_icon_container_set_margins (icon_container,
						     0, 0, 0, 0);
	} else {
		screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));

		icon_container_set_workarea (
			icon_container, screen, workareas, length_returned / sizeof (long));
	}

	if (workareas != NULL)
		g_free (workareas);
}

static GdkFilterReturn
desktop_icon_view_property_filter (GdkXEvent *gdk_xevent,
				   GdkEvent *event,
				   gpointer data)
{
	XEvent *xevent = gdk_xevent;
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (data);
  
	switch (xevent->type) {
	case PropertyNotify:
		if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA"))
			net_workarea_changed (icon_view, event->any.window);
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static void
fm_desktop_icon_view_finalize (GObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (icon_view->details->reload_desktop_timeout != 0) {
		gtk_timeout_remove (icon_view->details->reload_desktop_timeout);
	}

	/* Delete all of the link files. */
	update_desktop_directory (DELETE_MOUNT_LINKS);
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_HOME_URI,
					 home_uri_changed,
					 icon_view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
					 default_zoom_level_changed,
					 icon_view);
	
	/* Clean up details */	
	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui, NULL);
		bonobo_object_unref (icon_view->details->ui);
		icon_view->details->ui = NULL;
	}
	
	free_volume_black_list (icon_view);
	
	g_free (icon_view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
fm_desktop_icon_view_class_init (FMDesktopIconViewClass *class)
{
	G_OBJECT_CLASS (class)->finalize = fm_desktop_icon_view_finalize;

	FM_DIRECTORY_VIEW_CLASS (class)->merge_menus = real_merge_menus;
	FM_DIRECTORY_VIEW_CLASS (class)->update_menus = real_update_menus;
	FM_DIRECTORY_VIEW_CLASS (class)->supports_zooming = real_supports_zooming;

	FM_ICON_VIEW_CLASS (class)->supports_auto_layout = real_supports_auto_layout;
}

static void
fm_desktop_icon_view_handle_middle_click (NautilusIconContainer *icon_container,
					  GdkEventButton *event,
					  FMDesktopIconView *desktop_icon_view)
{
	XButtonEvent x_event;
	
	/* During a mouse click we have the pointer and keyboard grab.
	 * We will send a fake event to the root window which will cause it
	 * to try to get the grab so we need to let go ourselves.
	 */
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);

	/* Stop the event because we don't want anyone else dealing with it. */	
	gdk_flush ();
	g_signal_stop_emission_by_name (icon_container, "middle_click");

	/* build an X event to represent the middle click. */
	x_event.type = ButtonPress;
	x_event.send_event = True;
	x_event.display = GDK_DISPLAY ();
	x_event.window = GDK_ROOT_WINDOW ();
	x_event.root = GDK_ROOT_WINDOW ();
	x_event.subwindow = 0;
	x_event.time = event->time;
	x_event.x = event->x;
	x_event.y = event->y;
	x_event.x_root = event->x_root;
	x_event.y_root = event->y_root;
	x_event.state = event->state;
	x_event.button = event->button;
	x_event.same_screen = True;
	
	/* Send it to the root window, the window manager will handle it. */
	XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), True,
		    ButtonPressMask, (XEvent *) &x_event);
}

static void
free_volume_black_list (FMDesktopIconView *icon_view)
{
	eel_g_list_free_deep (icon_view->details->mount_black_list);
	icon_view->details->mount_black_list = NULL;
}

static gboolean
volume_in_black_list (FMDesktopIconView *icon_view,
		      const NautilusVolume *volume)
{
	GList *p;
	
	g_return_val_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view), TRUE);

	for (p = icon_view->details->mount_black_list; p != NULL; p = p->next) {
		if (strcmp ((char *) p->data, nautilus_volume_get_mount_path (volume)) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}


static char *
create_unique_volume_name (const NautilusVolume *volume)
{
	GnomeVFSURI *uri;
	char *uri_path, *new_name;
	int index;
	char *volume_name, *original_volume_name;
	
	new_name = NULL;

	/* Start with an index of one. If we collide, the file collided with will be the actual
	 * number one. We will rename with the next available number.
	 */	   
	index = 1;
			
	volume_name = nautilus_volume_get_name (volume);	
		
	uri_path = g_strdup_printf ("%s/%s", desktop_directory, volume_name);		
	uri = gnome_vfs_uri_new (uri_path);
	
	/* Check for existing filename and create a unique name. */
	while (gnome_vfs_uri_exists (uri)) {
		gnome_vfs_uri_unref (uri);
		g_free (uri_path);
		
		index++;
		
		g_free (new_name);
		new_name = g_strdup_printf ("%s (%d)", volume_name, index);
		
		uri_path = g_strdup_printf ("%s/%s", desktop_directory, new_name);
		uri = gnome_vfs_uri_new (uri_path);		
	}
	
	if (new_name != NULL) {
		g_free (volume_name);
		volume_name = new_name;	
	}
	
	original_volume_name = nautilus_volume_get_name (volume);
	if (strcmp (volume_name, original_volume_name) != 0) {
		nautilus_volume_monitor_set_volume_name (nautilus_volume_monitor_get (),
							 volume, volume_name);
	}
	g_free (original_volume_name);

	gnome_vfs_uri_unref (uri);
	g_free (uri_path);
	
	return volume_name;
}

static void
create_mount_link (FMDesktopIconView *icon_view,
		   const NautilusVolume *volume)
{
	char *target_uri, *volume_name;
	const char *icon_name;

	if (volume_in_black_list (icon_view, volume)) {
		return;
	}
	
	/* FIXME bugzilla.gnome.org 45412: Design a comprehensive desktop mounting strategy */
	if (!nautilus_volume_is_removable (volume)) {
		return;
	}

	/* Get icon type */
	icon_name = "i-blockdev";
	switch (nautilus_volume_get_device_type (volume)) {
	case NAUTILUS_DEVICE_AUDIO_CD:
	case NAUTILUS_DEVICE_CDROM_DRIVE:
		icon_name = "i-cdrom";
		break;

	case NAUTILUS_DEVICE_FLOPPY_DRIVE:
		icon_name = "i-floppy";
		break;

	case NAUTILUS_DEVICE_JAZ_DRIVE:
		icon_name = "i-zipdisk2";
		break;

	case NAUTILUS_DEVICE_MEMORY_STICK:
		icon_name = "gnome-ccperiph";
		break;
	
	case NAUTILUS_DEVICE_NFS:
		icon_name = "i-nfs";
		break;

	case NAUTILUS_DEVICE_SMB:
		icon_name = "i-smb";
		break;
	
	case NAUTILUS_DEVICE_ZIP_DRIVE:
		icon_name = "i-zipdisk";
		break;

	case NAUTILUS_DEVICE_APPLE:
	case NAUTILUS_DEVICE_WINDOWS:
	case NAUTILUS_DEVICE_CAMERA:
	case NAUTILUS_DEVICE_UNKNOWN:
		break;
	}

	target_uri = nautilus_volume_get_target_uri (volume);

	volume_name = create_unique_volume_name (volume);

	/* Create link */
	nautilus_link_local_create (desktop_directory, volume_name, icon_name, target_uri, NULL, NAUTILUS_LINK_MOUNT);

	g_free (target_uri);
	g_free (volume_name);
}

static gboolean
create_one_mount_link (const NautilusVolume *volume, gpointer callback_data)
{
	create_mount_link (FM_DESKTOP_ICON_VIEW (callback_data), volume);
	return TRUE;
}

static void
event_callback (GtkWidget *widget, GdkEvent *event, FMDesktopIconView *desktop_icon_view)
{
}

static void
unrealized_callback (GtkWidget *widget, FMDesktopIconView *desktop_icon_view)
{
	g_return_if_fail (desktop_icon_view->details->root_window != NULL);

	/* Remove the property filter */
	gdk_window_remove_filter (desktop_icon_view->details->root_window,
				  desktop_icon_view_property_filter,
				  desktop_icon_view);
	desktop_icon_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, FMDesktopIconView *desktop_icon_view)
{
	GdkWindow *root_window;

	g_return_if_fail (desktop_icon_view->details->root_window == NULL);

	root_window = gdk_screen_get_root_window (gtk_widget_get_screen (widget));

	desktop_icon_view->details->root_window = root_window;

	/* Read out the workarea geometry and update the icon container accordingly */
	net_workarea_changed (desktop_icon_view, root_window);

	/* Setup the property filter */
	gdk_window_set_events (root_window, GDK_PROPERTY_NOTIFY);
	gdk_window_add_filter (root_window,
			       desktop_icon_view_property_filter,
			       desktop_icon_view);
}

static NautilusZoomLevel
get_default_zoom_level (void)
{
	static gboolean auto_storage_added = FALSE;
	static NautilusZoomLevel default_zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;

	if (!auto_storage_added) {
		auto_storage_added = TRUE;
		eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
					       (int *) &default_zoom_level);
	}

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
default_zoom_level_changed (gpointer user_data)
{
	NautilusZoomLevel new_level;
	FMDesktopIconView *desktop_icon_view;

	desktop_icon_view = FM_DESKTOP_ICON_VIEW (user_data);
	new_level = get_default_zoom_level ();

	nautilus_icon_container_set_zoom_level (get_icon_container (desktop_icon_view),
						new_level);
}

/* Update home link to point to new home uri */
static void
home_uri_changed (gpointer callback_data)
{
	update_desktop_directory (UPDATE_HOME_LINK);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	FMDesktopIconView *desktop_icon_view;
	struct stat buf;

	desktop_icon_view = FM_DESKTOP_ICON_VIEW (data);
	if (desktop_icon_view->details->pending_rescan) {
		return TRUE;
	}
	
	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_icon_view->details->pending_rescan = TRUE;

	nautilus_directory_force_reload (
		fm_directory_view_get_model (
			FM_DIRECTORY_VIEW (desktop_icon_view)));
	return TRUE;
}

static void
done_loading (GtkObject *DirectoryView, FMDesktopIconView *desktop_icon_view)
{
	struct stat buf;

	desktop_icon_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NautilusDirectory model does not
 * exist always in the desktop_icon_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (FMDesktopIconView *desktop_icon_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (fm_directory_view_get_model (FM_DIRECTORY_VIEW (desktop_icon_view)),
				 "done_loading",
				 G_CALLBACK (done_loading), desktop_icon_view, 0);

	/* Monitor desktop directory. */
	desktop_icon_view->details->reload_desktop_timeout =
		gtk_timeout_add (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_view);

	g_signal_handler_disconnect (desktop_icon_view,
				     desktop_icon_view->details->delayed_init_signal);

	desktop_icon_view->details->delayed_init_signal = 0;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (FM_IS_DESKTOP_ICON_VIEW (callback_data));
	
	fm_desktop_icon_view_update_icon_container_fonts (FM_DESKTOP_ICON_VIEW (callback_data));
}

static void
fm_desktop_icon_view_update_icon_container_fonts (FMDesktopIconView *icon_view)
{
	NautilusIconContainer *icon_container;
	char *font;
	
	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	font = eel_preferences_get (NAUTILUS_PREFERENCES_DESKTOP_FONT);

	nautilus_icon_container_set_font (icon_container, font);

	g_free (font);
}

static void
fm_desktop_icon_view_init (FMDesktopIconView *desktop_icon_view)
{
	GList *list;
	NautilusIconContainer *icon_container;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;

	if (desktop_directory == NULL) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
					      desktop_directory_changed_callback,
					      NULL);
		desktop_directory_changed_callback (NULL);
	}

	icon_container = get_icon_container (desktop_icon_view);

	nautilus_icon_container_set_use_drop_shadows (icon_container, TRUE);
	fm_icon_container_set_sort_desktop (FM_ICON_CONTAINER (icon_container), TRUE);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	

	/* Do a reload on the desktop if we don't have FAM, a smarter
	 * way to keep track of the items on the desktop.
	 */
	if (!nautilus_monitor_active ()) {
		desktop_icon_view->details->delayed_init_signal = g_signal_connect_object
			(desktop_icon_view, "begin_loading",
			 G_CALLBACK (delayed_init), desktop_icon_view, 0);
	}
	
	nautilus_icon_container_set_is_fixed_size (icon_container, TRUE);

	/* Set up default mount black list */
	list = g_list_prepend (NULL, g_strdup ("/proc"));
	list = g_list_prepend (list, g_strdup ("/boot"));
	desktop_icon_view->details->mount_black_list = list;

	/* Set allocation to be at 0, 0 */
	allocation = &GTK_WIDGET (icon_container)->allocation;
	allocation->x = 0;
	allocation->y = 0;
	
	gtk_widget_queue_resize (GTK_WIDGET (icon_container));

	hadj = GTK_LAYOUT (icon_container)->hadjustment;
	vadj = GTK_LAYOUT (icon_container)->vadjustment;

	eel_gtk_adjustment_set_value (hadj, 0);
	eel_gtk_adjustment_set_value (vadj, 0);

	fm_directory_view_ignore_hidden_file_preferences
		(FM_DIRECTORY_VIEW (desktop_icon_view));
	
	/* Set our default layout mode */
	nautilus_icon_container_set_layout_mode (icon_container,
						 NAUTILUS_ICON_LAYOUT_T_B_L_R);

	update_desktop_directory (DELETE_MOUNT_LINKS | UPDATE_HOME_LINK | UPDATE_TRASH_LINK);

	/* Create initial mount links */
	nautilus_volume_monitor_each_mounted_volume (nautilus_volume_monitor_get (),
					     	     create_one_mount_link,
						     desktop_icon_view);
	
	g_signal_connect_object (icon_container, "middle_click",
				 G_CALLBACK (fm_desktop_icon_view_handle_middle_click), desktop_icon_view, 0);
	g_signal_connect_object (desktop_icon_view, "event",
				 G_CALLBACK (event_callback), desktop_icon_view, 0);
	g_signal_connect_object (nautilus_trash_monitor_get (), "trash_state_changed",
				 G_CALLBACK (fm_desktop_icon_view_trash_state_changed_callback), desktop_icon_view, 0);	
	g_signal_connect_object (nautilus_volume_monitor_get (), "volume_mounted",
				 G_CALLBACK (volume_mounted_callback), desktop_icon_view, 0);
	g_signal_connect_object (nautilus_volume_monitor_get (), "volume_unmounted",
				 G_CALLBACK (volume_unmounted_callback), desktop_icon_view, 0);
	g_signal_connect_object (desktop_icon_view, "realize",
				 G_CALLBACK (realized_callback), desktop_icon_view, 0);
	g_signal_connect_object (desktop_icon_view, "unrealize",
				 G_CALLBACK (unrealized_callback), desktop_icon_view, 0);
	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_HOME_URI,
				      home_uri_changed,
				      desktop_icon_view);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
				      default_zoom_level_changed,
				      desktop_icon_view);
	
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_FONT,
						  font_changed_callback, 
						  desktop_icon_view, G_OBJECT (desktop_icon_view));
	
	default_zoom_level_changed (desktop_icon_view);
	fm_desktop_icon_view_update_icon_container_fonts (desktop_icon_view);
}

static void
new_terminal_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
        g_assert (FM_DIRECTORY_VIEW (data));

	eel_gnome_open_terminal_on_screen (NULL, gtk_widget_get_screen (GTK_WIDGET (data)));
}

static void
new_launcher_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
	char *desktop_directory;

        g_assert (FM_DIRECTORY_VIEW (data));

	desktop_directory = nautilus_get_desktop_directory ();

	nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (data)),
						  "gnome-desktop-item-edit", 
						  "gnome-desktop-item-edit --create-new",
						  desktop_directory, 
						  FALSE);
	g_free (desktop_directory);

}

static void
change_background_callback (BonoboUIComponent *component, 
	  		    gpointer data, 
			    const char *verb)
{
        g_assert (FM_DIRECTORY_VIEW (data));

	nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (data)),
						  _("Background"),
						  "gnome-background-properties",
						  NULL,
						  FALSE);
}

static void
empty_trash_callback (BonoboUIComponent *component, 
	  	      gpointer data, 
		      const char *verb)
{
        g_assert (FM_IS_DIRECTORY_VIEW (data));

	nautilus_file_operations_empty_trash (GTK_WIDGET (data));
}

static void
reset_background_callback (BonoboUIComponent *component, 
			   gpointer data, 
			   const char *verb)
{
	eel_background_reset 
		(fm_directory_view_get_background (FM_DIRECTORY_VIEW (data)));
}

static gboolean
have_volume_format_app (void)
{
	static int have_app = -1;
	char *app;

	if (have_app < 0) {
		app = g_find_program_in_path ("gmedia_format");
		if (app == NULL) {
			app = g_find_program_in_path ("gfloppy");
		}
		have_app = (app != NULL);
		g_free (app);
	}
	return have_app;
}

static gboolean
have_volume_properties_app (void)
{
	static int have_app = -1;
	char *app;

	if (have_app < 0) {
		app = g_find_program_in_path ("gmedia_prop");
		have_app = (app != NULL);
		g_free (app);
	}
	return have_app;
}

static gboolean
have_volume_protection_app (void)
{
	static int have_app = -1;
	char *app;

	if (have_app < 0) {
		app = g_find_program_in_path ("gmedia_prot");
		have_app = (app != NULL);
		g_free (app);
	}
	return have_app;
}

static void
volume_ops_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
        FMDirectoryView *view;
	NautilusFile *file;
	char *uri, *mount_uri, *mount_path;
	GList *selection;
	char *command;
	const char *device_path;
	char *rawdevice_path, *quoted_path;
	char *program;
	NautilusVolume *volume;
	gboolean status;
	GError *error;
	GtkWidget *dialog;
	GdkScreen *screen;

        g_assert (FM_IS_DIRECTORY_VIEW (data));
        
        view = FM_DIRECTORY_VIEW (data);
        
	if (!volume_link_is_selection (view)) {
		return;       
	}
              
	selection = fm_directory_view_get_selection (view);
	
	file = NAUTILUS_FILE (selection->data);
	
	if (!nautilus_file_is_local (file)) {
		nautilus_file_list_free (selection);
		return;
	}
		
	uri = nautilus_file_get_uri (file);
	mount_uri = nautilus_link_local_get_link_uri (uri);
	mount_path = gnome_vfs_get_local_path_from_uri (mount_uri);
	g_free (uri);
	g_free (mount_uri);
	if (mount_path == NULL) {
		nautilus_file_list_free (selection);
		return;
	}

	volume = nautilus_volume_monitor_get_volume_for_path (nautilus_volume_monitor_get (), mount_path);
	device_path = nautilus_volume_get_device_path (volume);
	if (device_path == NULL) {
		g_free (mount_path);
		nautilus_file_list_free (selection);
		return;
	}
		
	/* Solaris specif cruft: */
	if (eel_str_has_prefix (device_path, "/vol/dev/")) {
		rawdevice_path = g_strconcat ("/vol/dev/r",
					device_path + strlen ("/vol/dev/"),
					NULL);
	} else {
		rawdevice_path = g_strdup (device_path);
	}
	
	quoted_path = g_shell_quote (rawdevice_path);
	g_free (rawdevice_path);
	rawdevice_path = quoted_path;
		
	if (strcmp (verb, "Unmount Volume Conditional") == 0) {
		nautilus_volume_monitor_mount_unmount_removable (nautilus_volume_monitor_get (),
								 mount_path, FALSE, TRUE);
	} else {
		command = NULL;
		
		if (strcmp (verb, "Format Conditional") == 0) {
			program = g_find_program_in_path ("gmedia_format");
			if (program != NULL) {
				command = g_strdup_printf ("%s -d %s", program, rawdevice_path);
				g_free (program);
			} else {
				program = g_find_program_in_path ("gfloppy");
				if (program != NULL) {
					command = g_strdup_printf ("%s --device %s", program, device_path);
					g_free (program);
				}
			}
		} else if (strcmp (verb, "Media Properties Conditional") == 0) {
			program = g_find_program_in_path ("gmedia_prop");
			if (program) {
				command = g_strdup_printf ("%s %s", program, rawdevice_path);
				g_free (program);
			} 
		} else if (strcmp (verb, "Protect Conditional") == 0) {
			program = g_find_program_in_path ("gmedia_prot");
			if (program) {
				command = g_strdup_printf ("%s %s", program, rawdevice_path);
				g_free (program);
			} 
		}

		if (command) {
			error = NULL;

			screen = gtk_widget_get_screen (GTK_WIDGET (view));

			status = egg_screen_execute_command_line_async (screen, command, &error);
			if (!status) {
				dialog = gtk_message_dialog_new (NULL, 0,
								 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
								 _("Error executing utility program '%s': %s"), command, error->message);
				g_signal_connect (G_OBJECT (dialog), "response", 
						  G_CALLBACK (gtk_widget_destroy), NULL);
				gtk_window_set_screen (GTK_WINDOW (dialog), screen);
				gtk_widget_show (dialog);
				g_error_free (error);
			}
			g_free (command);
		}
	}
	
	g_free (rawdevice_path);
	g_free (mount_path);
	nautilus_file_list_free (selection);
}

static gboolean
trash_link_is_selection (FMDirectoryView *view)
{
	GList *selection;
	gboolean result;
	char *uri;

	result = FALSE;
	
	selection = fm_directory_view_get_selection (view);

	if (eel_g_list_exactly_one_item (selection)
	    && nautilus_file_is_nautilus_link (NAUTILUS_FILE (selection->data))) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
		/* It's probably OK that this only works for local
		 * items, since the trash we care about is on the desktop.
		 */
		if (nautilus_link_local_is_trash_link (uri, NULL)) {
			result = TRUE;
		}
		g_free (uri);
	}
	
	nautilus_file_list_free (selection);

	return result;
}

static gboolean
volume_link_is_selection (FMDirectoryView *view)
{
	GList *selection;
	gboolean result;
	char *uri;

	result = FALSE;
	
	selection = fm_directory_view_get_selection (view);

	if (eel_g_list_exactly_one_item (selection)
	    && nautilus_file_is_nautilus_link (NAUTILUS_FILE (selection->data))) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
		/* It's probably OK that this only works for local
		 * items, since the volume we care about is on the desktop.
		 */
		if (nautilus_link_local_is_volume_link (uri, NULL)) {
			result = TRUE;
		}
		g_free (uri);
	}
	
	nautilus_file_list_free (selection);

	return result;
}

/*
 *  Returns Device Type for device icon on desktop
 */

static NautilusDeviceType
volume_link_device_type (FMDirectoryView *view)
{
	GList *selection;
	gchar *uri, *mount_uri, *mount_path;
	NautilusVolume *volume;

	selection = fm_directory_view_get_selection (view);

	if (selection == NULL) {
		return NAUTILUS_DEVICE_UNKNOWN;
	}

	volume = NULL;
	
	uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
	mount_uri = nautilus_link_local_get_link_uri (uri);
	mount_path = gnome_vfs_get_local_path_from_uri (mount_uri);
	if(mount_path != NULL) {
		volume = nautilus_volume_monitor_get_volume_for_path (nautilus_volume_monitor_get (), mount_path);
		g_free (mount_path);
	}
	g_free (mount_uri);
	g_free (uri);
	nautilus_file_list_free (selection);

	if (volume != NULL)
		return nautilus_volume_get_device_type (volume);

	return NAUTILUS_DEVICE_UNKNOWN;
}


static void
fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
						   gboolean state,
						   gpointer callback_data)
{
	char *path;

	path = g_build_filename (desktop_directory, TRASH_LINK_NAME, NULL);

	nautilus_link_local_set_icon (path, state ? "trash-empty" : "trash-full");

	g_free (path);
}

static void
volume_mounted_callback (NautilusVolumeMonitor *monitor,
			 NautilusVolume *volume, 
			 FMDesktopIconView *icon_view)
{
	create_mount_link (icon_view, volume);
}

static void
unlink_and_notify (const char *path)
{
	char *uri, *unescaped_uri;
	GList one_item_list;

	unlink (path);

	uri = gnome_vfs_get_uri_from_local_path (path);
	if (uri == NULL) {
		return;
	}
 
	unescaped_uri = gnome_vfs_unescape_string (uri, NULL);
	g_free (uri);

	one_item_list.data = unescaped_uri;
	one_item_list.next = NULL;
	one_item_list.prev = NULL;
	nautilus_directory_notify_files_removed (&one_item_list);
}

static void
volume_unmounted_callback (NautilusVolumeMonitor *monitor,
			   NautilusVolume *volume, 
			   FMDesktopIconView *icon_view)
{
	char *link_path, *volume_name;

	g_assert (volume != NULL);
	
	volume_name = nautilus_volume_get_name (volume);
	if (volume_name == NULL) {
		return;
	}
	
	link_path = g_build_filename (desktop_directory, volume_name, NULL);
	unlink_and_notify (link_path);

	g_free (volume_name);
	g_free (link_path);
}

static MountParameters *
mount_parameters_new (FMDesktopIconView *view, const char *mount_path)
{
	MountParameters *new_parameters;

	g_assert (FM_IS_DESKTOP_ICON_VIEW (view));
	g_assert (!eel_str_is_empty (mount_path)); 

	new_parameters = g_new (MountParameters, 1);
	new_parameters->view = view;
	new_parameters->mount_path = g_strdup (mount_path);

	return new_parameters;
}

static void
mount_parameters_free (MountParameters *parameters)
{
	g_assert (parameters != NULL);

	g_free (parameters->mount_path);
	g_free (parameters);
}

static void
mount_parameters_free_wrapper (gpointer user_data, GClosure *closure)
{
	mount_parameters_free ((MountParameters *)user_data);
}

static void
mount_or_unmount_removable_volume (BonoboUIComponent *component,
	       			   const char *path,
	       			   Bonobo_UIComponent_EventType type,
	       			   const char *state,
	       			   gpointer user_data)
{
	MountParameters *parameters;
	gboolean mount;

	g_assert (BONOBO_IS_UI_COMPONENT (component));

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	parameters = (MountParameters *) user_data;
	mount = (strcmp (state, "1") == 0);
	nautilus_volume_monitor_mount_unmount_removable 
		(nautilus_volume_monitor_get (),
		 parameters->mount_path,
		 mount, !mount);
	update_disks_menu (parameters->view);
}	       

/* Fill in the context menu with an item for each disk that is or could be mounted */
static void
update_disks_menu (FMDesktopIconView *view)
{
	const GList *disk_list, *element;
	guint index;
	char *name;
	char *command_name;
	char *command_path;
	NautilusVolume *volume;
	
	/* Clear any previously inserted items */
	nautilus_bonobo_remove_menu_items_and_commands
		(view->details->ui, DESKTOP_BACKGROUND_POPUP_PATH_DISKS);
	
	/* Get a list containing the all removable volumes in the volume monitor */
	disk_list = nautilus_volume_monitor_get_removable_volumes (nautilus_volume_monitor_get ());

	/* Iterate list and populate menu with removable volumes */
	for (element = disk_list, index = 0; 
	     element != NULL; 
	     element = element->next, ++index) {
		volume = element->data;
		

		/* Determine human-readable name from mount path */
		name = nautilus_volume_monitor_get_mount_name_for_display (nautilus_volume_monitor_get (), volume);
		
		nautilus_bonobo_add_numbered_toggle_menu_item 
			(view->details->ui,
			 DESKTOP_BACKGROUND_POPUP_PATH_DISKS,
			 index,
			 name);
		g_free (name);

		command_name = nautilus_bonobo_get_numbered_menu_item_command
			(view->details->ui,
			 DESKTOP_BACKGROUND_POPUP_PATH_DISKS,
			 index);

		command_path = g_strconcat ("/commands/", command_name, NULL);
		nautilus_bonobo_set_toggle_state
			(view->details->ui,
			 command_path,
			nautilus_volume_monitor_volume_is_mounted (nautilus_volume_monitor_get (), volume));
		g_free (command_path);

		bonobo_ui_component_add_listener_full
			(view->details->ui, command_name,
			 g_cclosure_new (G_CALLBACK (mount_or_unmount_removable_volume),
					 mount_parameters_new (view, nautilus_volume_get_mount_path (volume)),
					 mount_parameters_free_wrapper));
		g_free (command_name);
	}
}

static void
real_update_menus (FMDirectoryView *view)
{
	FMDesktopIconView *desktop_view;
	char *label;
	gboolean include_empty_trash, include_media_commands;
	NautilusDeviceType media_type;
	char *unmount_label;
	
	g_assert (FM_IS_DESKTOP_ICON_VIEW (view));

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));

	desktop_view = FM_DESKTOP_ICON_VIEW (view);

	bonobo_ui_component_freeze (desktop_view->details->ui, NULL);

	/* Disks menu */
	update_disks_menu (desktop_view);

	/* Reset Background */
	nautilus_bonobo_set_sensitive 
		(desktop_view->details->ui, 
		 FM_DIRECTORY_VIEW_COMMAND_RESET_BACKGROUND,
		 nautilus_file_background_is_set 
		 	(fm_directory_view_get_background (view)));

	/* Empty Trash */
	include_empty_trash = trash_link_is_selection (view);
	nautilus_bonobo_set_hidden
		(desktop_view->details->ui,
		 DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL,
		 !include_empty_trash);
	if (include_empty_trash) {
		label = g_strdup (_("Empty Trash"));
		nautilus_bonobo_set_label
			(desktop_view->details->ui, 
			 DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL,
			 label);
		nautilus_bonobo_set_sensitive 
			(desktop_view->details->ui, 
			 DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL,
			 !nautilus_trash_monitor_is_empty ());
		g_free (label);
	}

	/* Unmount Volume */
	include_media_commands = volume_link_is_selection (view);

	if (include_media_commands) {
		media_type = volume_link_device_type (view);

		unmount_label = _("E_ject");
		
		switch(media_type) {
		case NAUTILUS_DEVICE_FLOPPY_DRIVE:
			if (have_volume_format_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
					DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, TRUE);
			}

			if (have_volume_properties_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
			 		DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			}

			nautilus_bonobo_set_hidden
				(desktop_view->details->ui,
				DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL, TRUE);
			break;
		
		case NAUTILUS_DEVICE_CDROM_DRIVE:
			nautilus_bonobo_set_hidden
				(desktop_view->details->ui,
				DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL, TRUE);

			nautilus_bonobo_set_hidden
				(desktop_view->details->ui,
				DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, TRUE);

			if (have_volume_properties_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
					DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			}
			break;	
		
		case NAUTILUS_DEVICE_ZIP_DRIVE:
		case NAUTILUS_DEVICE_JAZ_DRIVE:
			if (have_volume_format_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
					DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL, TRUE);
			}

			if (have_volume_properties_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL, TRUE);
			}

			if (have_volume_protection_app ()) {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL, FALSE);
				nautilus_bonobo_set_sensitive
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL, TRUE);
			} else {
				nautilus_bonobo_set_hidden
					(desktop_view->details->ui,
					 DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL, TRUE);
			}
			break;
		default:
			unmount_label = _("_Unmount Volume");
			break;
		}

		/* We always want a unmount entry */
		nautilus_bonobo_set_hidden
			(desktop_view->details->ui,
			 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL,
			 FALSE);
		nautilus_bonobo_set_sensitive
			(desktop_view->details->ui,
			DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL, TRUE);

		/* But call it eject for removable media */
		nautilus_bonobo_set_label
			(desktop_view->details->ui, 
			 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL, unmount_label);
	} else {
		nautilus_bonobo_set_hidden
			(desktop_view->details->ui,
			 DESKTOP_COMMAND_PROTECT_VOLUME_CONDITIONAL,
			 TRUE);
		
		nautilus_bonobo_set_hidden
			(desktop_view->details->ui,
			 DESKTOP_COMMAND_FORMAT_VOLUME_CONDITIONAL,
			 TRUE);
		
		nautilus_bonobo_set_hidden
			(desktop_view->details->ui,
			 DESKTOP_COMMAND_MEDIA_PROPERTIES_VOLUME_CONDITIONAL,
			 TRUE);
		
		nautilus_bonobo_set_hidden
			(desktop_view->details->ui,
			 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL,
			 TRUE);
	}
	
	bonobo_ui_component_thaw (desktop_view->details->ui, NULL);
}

static void
real_merge_menus (FMDirectoryView *view)
{
	FMDesktopIconView *desktop_view;
	Bonobo_UIContainer ui_container;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Change Background", change_background_callback),
		BONOBO_UI_VERB ("Empty Trash Conditional", empty_trash_callback),
		BONOBO_UI_VERB ("New Terminal", new_terminal_callback),
		BONOBO_UI_VERB ("New Launcher Desktop", new_launcher_callback),
		BONOBO_UI_VERB ("Reset Background", reset_background_callback),
		BONOBO_UI_VERB ("Unmount Volume Conditional", volume_ops_callback),
		BONOBO_UI_VERB ("Protect Conditional", volume_ops_callback),
		BONOBO_UI_VERB ("Format Conditional", volume_ops_callback),
		BONOBO_UI_VERB ("Media Properties Conditional", volume_ops_callback),
		BONOBO_UI_VERB_END
	};

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

	desktop_view = FM_DESKTOP_ICON_VIEW (view);

	desktop_view->details->ui = bonobo_ui_component_new ("Desktop Icon View");

	ui_container = fm_directory_view_get_bonobo_ui_container (view);
	bonobo_ui_component_set_container (desktop_view->details->ui,
					   ui_container, NULL);
	bonobo_object_release_unref (ui_container, NULL);
	bonobo_ui_util_set_ui (desktop_view->details->ui,
			       DATADIR,
			       "nautilus-desktop-icon-view-ui.xml",
			       "nautilus", NULL);
	bonobo_ui_component_add_verb_list_with_data (desktop_view->details->ui, verbs, view);
}

static gboolean
real_supports_auto_layout (FMIconView *view)
{
	/* Can't use auto-layout on the desktop, because doing so
	 * would cause all sorts of complications involving the
	 * fixed-size window.
	 */
	return FALSE;
}

static gboolean
real_supports_zooming (FMDirectoryView *view)
{
	/* Can't zoom on the desktop, because doing so would cause all
	 * sorts of complications involving the fixed-size window.
	 */
	return FALSE;
}

/* update_desktop_directory
 *
 * Look for a particular type of link on the desktop. If the right
 * link is there, update its target URI. Delete any extra links of
 * that type.
 */
static void
update_desktop_directory (UpdateType type)
{
	char *link_path;
	GnomeVFSResult result;
	GList *desktop_files, *l;
	GnomeVFSFileInfo *info;

	char *home_uri = NULL;
	char *home_link_name = NULL;
	gboolean found_home_link;
	gboolean found_trash_link;

	if (type & UPDATE_HOME_LINK) {
		/* Note to translators: If it's hard to compose a good home
		 * icon name from the user name, you can use a string without
		 * an "%s" here, in which case the home icon name will not
		 * include the user's name, which should be fine. To avoid a
		 * warning, put "%.0s" somewhere in the string, which will
		 * match the user name string passed by the C code, but not
		 * put the user name in the final string.
		 */
		home_link_name = g_strdup_printf (_("%s's Home"), g_get_user_name ());
	
#ifdef WEB_NAVIGATION_ENABLED
		home_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
#else
		home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
#endif
	}

	result = gnome_vfs_directory_list_load
		(&desktop_files, desktop_directory,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
		 GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	found_home_link = found_trash_link = FALSE;

	if (result != GNOME_VFS_OK) {
		desktop_files = NULL;
	}

	for (l = desktop_files; l; l = l->next) {
		info = l->data;

		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE &&
		    info->type != GNOME_VFS_FILE_TYPE_REGULAR &&
		    info->type != GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			continue;
		}
		
		link_path = g_build_filename (desktop_directory, info->name, NULL);


		if (type & DELETE_MOUNT_LINKS &&
		    nautilus_link_local_is_volume_link (link_path, info)) {
			unlink_and_notify (link_path);
		}


		if (type & UPDATE_HOME_LINK &&
		    nautilus_link_local_is_home_link (link_path, info)) {
			if (!found_home_link &&
			    nautilus_link_local_is_utf8 (link_path, info)) {
				nautilus_link_local_set_link_uri (link_path, home_uri);
				found_home_link = TRUE;
			} else {
				unlink_and_notify (link_path); /* kill duplicates */
			}
		}

		if (type & UPDATE_TRASH_LINK &&
		    nautilus_link_local_is_trash_link (link_path, info)) {
			if (!found_trash_link &&
			    nautilus_link_local_is_utf8 (link_path, info) &&
			    !strcmp (TRASH_LINK_NAME, info->name)) {
				nautilus_link_local_set_link_uri (link_path, EEL_TRASH_URI);
				found_trash_link = TRUE;
			} else {
				unlink_and_notify (link_path); /* kill duplicates */
			}
		}
		g_free (link_path);
	}

	gnome_vfs_file_info_list_free (desktop_files);

	if (type & UPDATE_HOME_LINK && !found_home_link &&
	    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
		nautilus_link_local_create (desktop_directory,
					    home_link_name,
					    "desktop-home", 
					    home_uri,
					    NULL,
					    NAUTILUS_LINK_HOME);
	}
	       
	if (type & UPDATE_TRASH_LINK) {
		if (!found_trash_link) {
			nautilus_link_local_create (desktop_directory,
						    TRASH_LINK_NAME,
						    "trash-empty", 
						    EEL_TRASH_URI,
						    NULL,
						    NAUTILUS_LINK_TRASH);
		}

		/* Make sure link represents current trash state */
		fm_desktop_icon_view_trash_state_changed_callback (nautilus_trash_monitor_get (),
								   nautilus_trash_monitor_is_empty (),
								   NULL);
	}

	g_free (home_link_name);
	g_free (home_uri);
}

