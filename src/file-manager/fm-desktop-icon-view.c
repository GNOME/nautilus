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
#include "fm-desktop-icon-view.h"

#include <X11/Xatom.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <ctype.h>
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
#include <libgnome/gnome-dentry.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-mime.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
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
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char untranslated_trash_link_name[] = N_("Trash");
#define TRASH_LINK_NAME _(untranslated_trash_link_name)

#define DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL		"/commands/Empty Trash Conditional"
#define DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL	"/commands/Unmount Volume Conditional"

#define DESKTOP_BACKGROUND_POPUP_PATH_DISKS	"/popups/background/Before Zoom Items/Volume Items/Disks"

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4000

struct FMDesktopIconViewDetails
{
	BonoboUIComponent *ui;
	GList *mount_black_list;

	/* For the desktop rescanning
	 */
	guint delayed_init_signal;
	guint done_loading_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
};

typedef struct {
	FMDesktopIconView *view;
	char *mount_path;
} MountParameters;

static void     fm_desktop_icon_view_initialize                   (FMDesktopIconView      *desktop_icon_view);
static void     fm_desktop_icon_view_initialize_class             (FMDesktopIconViewClass *klass);
static void     fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor   *trash,
								   gboolean                state,
								   gpointer                callback_data);
static void     home_uri_changed                                  (gpointer                user_data);
static void     volume_mounted_callback                           (NautilusVolumeMonitor  *monitor,
								   NautilusVolume         *volume,
								   FMDesktopIconView      *icon_view);
static void     volume_unmounted_callback                         (NautilusVolumeMonitor  *monitor,
								   NautilusVolume         *volume,
								   FMDesktopIconView      *icon_view);
static int      desktop_icons_compare_callback                    (NautilusIconContainer  *container,
								   NautilusFile           *file_a,
								   NautilusFile           *file_b,
								   FMDesktopIconView      *icon_view);
static void     delete_all_mount_links                            (void);
static void     update_home_link_and_delete_copies                (void);
static void     update_trash_link_and_delete_copies               (void);
static gboolean real_supports_auto_layout                         (FMIconView             *view);
static void     real_merge_menus                                  (FMDirectoryView        *view);
static void     real_update_menus                                 (FMDirectoryView        *view);
static gboolean real_supports_zooming                             (FMDirectoryView        *view);
static void     update_disks_menu                                 (FMDesktopIconView      *view);
static void     free_volume_black_list                            (FMDesktopIconView      *view);
static gboolean	volume_link_is_selection 			  (FMDirectoryView 	  *view);

EEL_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView,
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
panel_desktop_area_changed (FMDesktopIconView *icon_view)
{
	long *borders = NULL;
	GdkAtom type_returned;
	int format_returned;
	unsigned long items_returned;
	unsigned long bytes_after_return;
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view));

	icon_container = get_icon_container (icon_view);

	gdk_error_trap_push ();
	if (XGetWindowProperty (GDK_DISPLAY (),
				GDK_ROOT_WINDOW (),
				gdk_atom_intern ("GNOME_PANEL_DESKTOP_AREA",
						 FALSE),
				0 /* long_offset */, 
				4 /* long_length */,
				False /* delete */,
				XA_CARDINAL,
				&type_returned,
				&format_returned,
				&items_returned,
				&bytes_after_return,
				(unsigned char **)&borders) != Success) {
		if (borders != NULL)
			XFree (borders);
		borders = NULL;
	}
			    
	if (gdk_error_trap_pop ()
	    || borders == NULL
	    || type_returned != XA_CARDINAL
	    || items_returned != 4
	    || format_returned != 32) {
		nautilus_icon_container_set_margins (icon_container,
						     0, 0, 0, 0);
	} else {
		nautilus_icon_container_set_margins (icon_container,
						     borders[0 /* left */],
						     borders[1 /* right */],
						     borders[2 /* top */],
						     borders[3 /* bottom */]);
	}

	if (borders != NULL)
		XFree (borders);
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
		if (xevent->xproperty.atom == gdk_atom_intern ("GNOME_PANEL_DESKTOP_AREA", FALSE)) {
			panel_desktop_area_changed (icon_view);
		}
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static void
fm_desktop_icon_view_destroy (GtkObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Remove the property filter */
	gdk_window_remove_filter (GDK_ROOT_PARENT (),
				  desktop_icon_view_property_filter,
				  icon_view);

	/* Remove desktop rescan timeout. */
	if (icon_view->details->reload_desktop_timeout != 0) {
		gtk_timeout_remove (icon_view->details->reload_desktop_timeout);
	}

	if (icon_view->details->done_loading_signal != 0) {
		gtk_signal_disconnect (GTK_OBJECT (fm_directory_view_get_model
						   (FM_DIRECTORY_VIEW (icon_view))),
				       icon_view->details->done_loading_signal);
	}

	if (icon_view->details->delayed_init_signal != 0) {
		gtk_signal_disconnect (GTK_OBJECT (icon_view),
				       icon_view->details->delayed_init_signal);
	}
	
	/* Delete all of the link files. */
	delete_all_mount_links ();
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_HOME_URI,
					      home_uri_changed,
					      icon_view);

	/* Clean up details */	
	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui);
		bonobo_object_unref (BONOBO_OBJECT (icon_view->details->ui));
	}
	
	free_volume_black_list (icon_view);
	
	g_free (icon_view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_desktop_icon_view_initialize_class (FMDesktopIconViewClass *klass)
{
	GtkObjectClass		*object_class;
	FMDirectoryViewClass	*fm_directory_view_class;
	FMIconViewClass		*fm_icon_view_class;

	object_class		= GTK_OBJECT_CLASS (klass);
	fm_directory_view_class	= FM_DIRECTORY_VIEW_CLASS (klass);
	fm_icon_view_class	= FM_ICON_VIEW_CLASS (klass);

	object_class->destroy = fm_desktop_icon_view_destroy;

	fm_directory_view_class->merge_menus = real_merge_menus;
	fm_directory_view_class->update_menus = real_update_menus;
	fm_directory_view_class->supports_zooming = real_supports_zooming;

	fm_icon_view_class->supports_auto_layout = real_supports_auto_layout;
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
	gtk_signal_emit_stop_by_name (GTK_OBJECT(icon_container), "middle_click");

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
	
	case NAUTILUS_DEVICE_ZIP_DRIVE:
		icon_name = "i-zipdisk";
		break;

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

/* Update home link to point to new home uri */
static void
home_uri_changed (gpointer callback_data)
{
	update_home_link_and_delete_copies ();
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
	desktop_icon_view->details->done_loading_signal = 
		gtk_signal_connect (GTK_OBJECT (fm_directory_view_get_model
						(FM_DIRECTORY_VIEW (desktop_icon_view))),
				    "done_loading",
				    GTK_SIGNAL_FUNC (done_loading), desktop_icon_view);

	/* Monitor desktop directory. */
	desktop_icon_view->details->reload_desktop_timeout =
		gtk_timeout_add (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_view);

	gtk_signal_disconnect (GTK_OBJECT (desktop_icon_view),
			       desktop_icon_view->details->delayed_init_signal);

	desktop_icon_view->details->delayed_init_signal = 0;
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
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

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	

	/* Do a reload on the desktop if we don't have FAM, a smarter
	 * way to keep track of the items on the desktop.
	 */
	if (!nautilus_monitor_active ()) {
		desktop_icon_view->details->delayed_init_signal = gtk_signal_connect
			(GTK_OBJECT (desktop_icon_view), "begin_loading",
			 GTK_SIGNAL_FUNC (delayed_init), desktop_icon_view);
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

	delete_all_mount_links ();
	update_home_link_and_delete_copies ();
	update_trash_link_and_delete_copies ();

	/* Create initial mount links */
	nautilus_volume_monitor_each_mounted_volume (nautilus_volume_monitor_get (),
					     	     create_one_mount_link,
						     desktop_icon_view);
	
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "middle_click",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_handle_middle_click),
			    desktop_icon_view);
			    
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "compare_icons",
			    GTK_SIGNAL_FUNC (desktop_icons_compare_callback),
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (desktop_icon_view),
			    "event",
			    GTK_SIGNAL_FUNC (event_callback),
			    desktop_icon_view);

	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_trash_monitor_get ()),
					"trash_state_changed",
					fm_desktop_icon_view_trash_state_changed_callback,
					desktop_icon_view,
					GTK_OBJECT (desktop_icon_view));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_volume_monitor_get ()),
					"volume_mounted",
					volume_mounted_callback,
					desktop_icon_view,
					GTK_OBJECT (desktop_icon_view));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_volume_monitor_get ()),
					"volume_unmounted",
					volume_unmounted_callback,
					desktop_icon_view,
					GTK_OBJECT (desktop_icon_view));
	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_HOME_URI,
					   home_uri_changed,
				  	   desktop_icon_view);

	/* Read out the panel desktop area and update the icon container
	 * accordingly */
	panel_desktop_area_changed (desktop_icon_view);

	/* Setup the property filter */
	gdk_window_add_filter (GDK_ROOT_PARENT (),
			       desktop_icon_view_property_filter,
			       desktop_icon_view);
}

static void
new_terminal_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
	eel_gnome_open_terminal (NULL);
}

static void
change_background_callback (BonoboUIComponent *component, 
	  		    gpointer data, 
			    const char *verb)
{
	nautilus_launch_application_from_command 
		(_("Background"),
		 "background-properties-capplet", NULL, FALSE);
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

static void
unmount_volume_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
        FMDirectoryView *view;
	NautilusFile *file;
	char *uri, *path, *mount_uri, *mount_path;
	GList *selection;
			    
        g_assert (FM_IS_DIRECTORY_VIEW (data));
        
        view = FM_DIRECTORY_VIEW (data);
        
       if (!volume_link_is_selection (view)) {
		return;       
       }
              
	selection = fm_directory_view_get_selection (view);

	file = NAUTILUS_FILE (selection->data);
	uri = nautilus_file_get_uri (file);
	path = gnome_vfs_get_local_path_from_uri (uri);
	if (path != NULL) {
		mount_uri = nautilus_link_local_get_link_uri (path);
		mount_path = gnome_vfs_get_local_path_from_uri (mount_uri);
		if (mount_path != NULL) {
			nautilus_volume_monitor_mount_unmount_removable (nautilus_volume_monitor_get (), mount_path, FALSE);
		}
		
		g_free (mount_path);
		g_free (mount_uri);
		g_free (path);
	}
	g_free (uri);

	nautilus_file_list_free (selection);
}

static gboolean
trash_link_is_selection (FMDirectoryView *view)
{
	GList *selection;
	gboolean result;
	char *uri, *path;

	result = FALSE;
	
	selection = fm_directory_view_get_selection (view);

	if (eel_g_list_exactly_one_item (selection)
	    && nautilus_file_is_nautilus_link (NAUTILUS_FILE (selection->data))) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
		/* It's probably OK that this only works for local
		 * items, since the trash we care about is on the desktop.
		 */
		path = gnome_vfs_get_local_path_from_uri (uri);
		if (path != NULL && nautilus_link_local_is_trash_link (path)) {
			result = TRUE;
		}
		g_free (path);
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
	char *uri, *path;

	result = FALSE;
	
	selection = fm_directory_view_get_selection (view);

	if (eel_g_list_exactly_one_item (selection)
	    && nautilus_file_is_nautilus_link (NAUTILUS_FILE (selection->data))) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
		/* It's probably OK that this only works for local
		 * items, since the volume we care about is on the desktop.
		 */
		path = gnome_vfs_get_local_path_from_uri (uri);
		if (path != NULL && nautilus_link_local_is_volume_link (path)) {
			result = TRUE;
		}
		g_free (path);
		g_free (uri);
	}
	
	nautilus_file_list_free (selection);

	return result;
}

static void
fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
						   gboolean state,
						   gpointer callback_data)
{
	char *path;

	path = nautilus_make_path (desktop_directory, TRASH_LINK_NAME);

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
	char *uri;
	GList one_item_list;

	unlink (path);

	uri = gnome_vfs_get_uri_from_local_path (path);
	if (uri == NULL) {
		return;
	}

	one_item_list.data = uri;
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
	
	link_path = nautilus_make_path (desktop_directory, volume_name);
	unlink_and_notify (link_path);

	g_free (volume_name);
	g_free (link_path);
}

/* update_link_and_delete_copies
 * 
 * Look for a particular type of link on the desktop. If the right
 * link is there, update its target URI. Delete any extra links of
 * that type.
 * 
 * @is_link_function: predicate function to test whether a link is the right type.
 * @link_name: if non-NULL, only a link with this name is considered a match.
 * @link_target_uri: new URI to set as link target.
 */
static gboolean
update_link_and_delete_copies (gboolean (*is_link_function) (const char *path),
			       const char *link_name,
			       const char *link_target_uri)
{
	DIR *dir;
	gboolean found_link;
	struct dirent *dir_entry;
	char *link_path;

	dir = opendir (desktop_directory);
	if (dir == NULL) {
		return FALSE;
	}

	found_link = FALSE;

	while ((dir_entry = readdir (dir)) != NULL) {
		link_path = nautilus_make_path (desktop_directory, dir_entry->d_name);
		if ((* is_link_function) (link_path)) {
			if (!found_link &&
			     (link_name == NULL || strcmp (dir_entry->d_name, link_name) == 0)) {
				nautilus_link_local_set_link_uri (link_path, link_target_uri);
				found_link = TRUE;
			} else {
				unlink_and_notify (link_path);
			}
		}
		g_free (link_path);
	}
	
	closedir (dir);

	return found_link;
}

/* update_home_link_and_delete_copies
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
static void
update_home_link_and_delete_copies (void)
{
	char *home_link_name, *home_uri;

	/* Note to translators: If it's hard to compose a good home
	 * icon name from the user name, you can use a string without
	 * an "%s" here, in which case the home icon name will not
	 * include the user's name, which should be fine. To avoid a
	 * warning, put "%.0s" somewhere in the string, which will
	 * match the user name string passed by the C code, but not
	 * put the user name in the final string.
	 */
	home_link_name = g_strdup_printf (_("%s's Home"), g_get_user_name ());
	
	home_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
	
	if (!update_link_and_delete_copies (nautilus_link_local_is_home_link,
					    NULL,
					    home_uri)) {
		nautilus_link_local_create (desktop_directory,
					    home_link_name,
					    "temp-home", 
					    home_uri,
					    NULL,
					    NAUTILUS_LINK_HOME);
	}
	
	g_free (home_link_name);
	g_free (home_uri);
}

static void
update_trash_link_and_delete_copies (void)
{

	/* Check for trash link */
	if (!update_link_and_delete_copies (nautilus_link_local_is_trash_link,
					    TRASH_LINK_NAME,
					    EEL_TRASH_URI)) {
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

static void
delete_all_mount_links (void)
{
	update_link_and_delete_copies (nautilus_link_local_is_volume_link,
				       "", "");
}

static char *
get_local_path (NautilusFile *file)
{
	char *uri, *local_path;

	uri = nautilus_file_get_uri (file);
	local_path = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);
	return local_path;
}

/* Sort as follows:
 *   1) home link
 *   2) mount links
 *   3) other
 *   4) trash link
 */

typedef enum {
	SORT_HOME_LINK,
	SORT_MOUNT_LINK,
	SORT_OTHER,
	SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NautilusFile *file)
{
	char *path;
	SortCategory category;

	if (!nautilus_file_is_nautilus_link (file)) {
		category = SORT_OTHER;
	} else {
		path = get_local_path (file);
		g_return_val_if_fail (path != NULL, SORT_OTHER);
		
		switch (nautilus_link_local_get_link_type (path)) {
		case NAUTILUS_LINK_HOME:
			category = SORT_HOME_LINK;
			break;
		case NAUTILUS_LINK_MOUNT:
			category = SORT_MOUNT_LINK;
			break;
		case NAUTILUS_LINK_TRASH:
			category = SORT_TRASH_LINK;
			break;
		default:
			category = SORT_OTHER;
			break;
		}
		
		g_free (path);
	}
	
	return category;
}

static int
desktop_icons_compare_callback (NautilusIconContainer *container,
				NautilusFile *file_a,
				NautilusFile *file_b,
				FMDesktopIconView *icon_view)
{
	SortCategory category_a, category_b;
	
	category_a = get_sort_category (file_a);
	category_b = get_sort_category (file_b);

	if (category_a == category_b) {
		return nautilus_file_compare_for_sort 
			(file_a, file_b, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, 
			 fm_directory_view_should_sort_directories_first (FM_DIRECTORY_VIEW (icon_view)), 
			 FALSE);
	}

	/* We know the answer, so prevent the other handlers
	 * from overwriting our result.
	 */
	gtk_signal_emit_stop_by_name (GTK_OBJECT (container),
				      "compare_icons");
	if (category_a < category_b) {
		return -1;
	} else {
		return +1;
	}
}

static MountParameters *
mount_parameters_new (FMDesktopIconView *view, const char *mount_path)
{
	MountParameters *new_parameters;

	g_assert (FM_IS_DESKTOP_ICON_VIEW (view));
	g_assert (!eel_str_is_empty (mount_path)); 

	new_parameters = g_new (MountParameters, 1);
	gtk_object_ref (GTK_OBJECT (view));
	new_parameters->view = view;
	new_parameters->mount_path = g_strdup (mount_path);

	return new_parameters;
}

static void
mount_parameters_free (MountParameters *parameters)
{
	g_assert (parameters != NULL);

	gtk_object_unref (GTK_OBJECT (parameters->view));
	g_free (parameters->mount_path);
	g_free (parameters);
}

static void
mount_parameters_free_wrapper (gpointer user_data)
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

	g_assert (BONOBO_IS_UI_COMPONENT (component));

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	parameters = (MountParameters *) user_data;
	nautilus_volume_monitor_mount_unmount_removable 
		(nautilus_volume_monitor_get (),
		 parameters->mount_path,
		 strcmp (state, "1") == 0);
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
			(view->details->ui,
			 command_name,
			 mount_or_unmount_removable_volume,
			 mount_parameters_new (view, nautilus_volume_get_mount_path (volume)),
			 mount_parameters_free_wrapper);			 
		g_free (command_name);
		
	}
}

static void
real_update_menus (FMDirectoryView *view)
{
	FMDesktopIconView *desktop_view;
	char *label;
	gboolean include_empty_trash, include_unmount_volume;
	
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
		if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH)) {
			label = g_strdup (_("Empty Trash..."));
		} else {
			label = g_strdup (_("Empty Trash"));
		}
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
	include_unmount_volume = volume_link_is_selection (view);
	nautilus_bonobo_set_hidden
		(desktop_view->details->ui,
		 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL,
		 !include_unmount_volume);
	if (include_unmount_volume) {
		label = g_strdup (_("Unmount Volume"));
		nautilus_bonobo_set_label
			(desktop_view->details->ui, 
			 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL,
			 label);
		nautilus_bonobo_set_sensitive 
			(desktop_view->details->ui, 
			 DESKTOP_COMMAND_UNMOUNT_VOLUME_CONDITIONAL, TRUE);
		g_free (label);
	}

	bonobo_ui_component_thaw (desktop_view->details->ui, NULL);
}

static void
real_merge_menus (FMDirectoryView *view)
{
	FMDesktopIconView *desktop_view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Change Background", change_background_callback),
		BONOBO_UI_VERB ("Empty Trash Conditional", empty_trash_callback),
		BONOBO_UI_VERB ("New Terminal", new_terminal_callback),
		BONOBO_UI_VERB ("Reset Background", reset_background_callback),
		BONOBO_UI_VERB ("Unmount Volume Conditional", unmount_volume_callback),
		BONOBO_UI_VERB_END
	};

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

	desktop_view = FM_DESKTOP_ICON_VIEW (view);

	desktop_view->details->ui = bonobo_ui_component_new ("Desktop Icon View");
	bonobo_ui_component_set_container (desktop_view->details->ui,
					   fm_directory_view_get_bonobo_ui_container (view));
	bonobo_ui_util_set_ui (desktop_view->details->ui,
			       DATADIR,
			       "nautilus-desktop-icon-view-ui.xml",
			       "nautilus");
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
