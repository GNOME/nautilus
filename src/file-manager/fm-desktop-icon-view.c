/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

   Copyright (C) 2000 Eazel, Inc.mou

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
*/

#include <config.h>
#include "fm-desktop-icon-view.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <bonobo/bonobo-ui-util.h>
#include <gdk/gdkx.h>
#include <gtk/gtkcheckmenuitem.h>
#include <libgnome/gnome-dentry.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-directory-notify.h>
#include <libnautilus-extensions/nautilus-file-changes-queue.h>
#include <libnautilus-extensions/nautilus-file-operations.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-trash-monitor.h>
#include <libnautilus-extensions/nautilus-volume-monitor.h>
#include <src/nautilus-application.h>
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

struct FMDesktopIconViewDetails
{
	BonoboUIComponent *ui;
	GList *mount_black_list;
};

typedef struct {
	GnomeVFSResult expected_result;
	char *uri;
	char *target_uri;
	GdkPoint point;
	GnomeDesktopEntry *entry;
} CreateLinkData;

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
static void     icon_view_create_nautilus_links                   (NautilusIconContainer  *container,
								   const GList            *item_uris,
								   int                     x,
								   int                     y,
								   FMDirectoryView        *view);
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


NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView,
				   fm_desktop_icon_view,
				   FM_TYPE_ICON_VIEW)

static NautilusIconContainer *
get_icon_container (FMDesktopIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
fm_desktop_icon_view_destroy (GtkObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Delete all of the link files. */
	delete_all_mount_links ();
	
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_HOME_URI,
					      home_uri_changed,
					      icon_view);

	/* Clean up details */	
	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui);
		bonobo_object_unref (BONOBO_OBJECT (icon_view->details->ui));
	}
	
	free_volume_black_list (icon_view);
	
	g_free (icon_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
	nautilus_g_list_free_deep (icon_view->details->mount_black_list);
	icon_view->details->mount_black_list = NULL;
}

static gboolean
volume_in_black_list (FMDesktopIconView *icon_view,
		      const NautilusVolume *volume)
{
	GList *p;

	for (p = icon_view->details->mount_black_list; p != NULL; p = p->next) {
		if (strcmp ((char *) p->data, volume->mount_path) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}


static char *
create_unique_volume_name (const char *desktop_path, const NautilusVolume *volume)
{
	GnomeVFSURI *uri;
	char *uri_path, *new_name;
	int index;
	char *volume_name;
	
	new_name = NULL;

	/* Start with an index of one. If we collide, the file collided with will be the actual
	 * number one. We will rename with the next available number.
	 */	   
	index = 1;
			
	volume_name = nautilus_volume_monitor_get_volume_name (volume);	

	uri_path = g_strdup_printf ("%s/%s",desktop_path, volume_name);		
	uri = gnome_vfs_uri_new (uri_path);
	
	/* Check for exiting filename and create a unique name. */
	while (gnome_vfs_uri_exists (uri)) {
		gnome_vfs_uri_unref (uri);
		g_free (uri_path);
		
		index++;
		
		g_free (new_name);
		new_name = g_strdup_printf ("%s (%d)", volume_name, index);
		
		uri_path = g_strdup_printf ("%s/%s", desktop_path, new_name);
		uri = gnome_vfs_uri_new (uri_path);		
	}
	
	if (new_name != NULL) {
		g_free (volume_name);
		volume_name = new_name;	
	}
	
	if (strcmp (volume_name, volume->volume_name) != 0) {
		nautilus_volume_monitor_set_volume_name (nautilus_volume_monitor_get (),
							 volume, volume_name);
	}

	gnome_vfs_uri_unref (uri);
	g_free (uri_path);
	
	return volume_name;
}

static void
create_mount_link (FMDesktopIconView *icon_view,
		   const NautilusVolume *volume)
{
	char *desktop_path, *target_uri, *volume_name;
	const char *icon_name;

	if (volume_in_black_list (icon_view, volume)) {
		return;
	}
	
	/* FIXME bugzilla.eazel.com 5412: Design a comprehensive desktop mounting strategy */
	if (!nautilus_volume_monitor_volume_is_removable (volume)) {
		return;
	}
	
	/* Get icon type */
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDDA:
	case NAUTILUS_VOLUME_CDROM:
		icon_name = "i-cdrom.png";
		break;
		
	case NAUTILUS_VOLUME_FLOPPY:
		icon_name = "i-floppy.png";
		break;
		
	default:
		icon_name = "i-blockdev.png";
		break;	
	}

	desktop_path = nautilus_get_desktop_directory ();
	target_uri =  nautilus_volume_monitor_get_target_uri (volume);
	
	volume_name = create_unique_volume_name (desktop_path, volume);
	
	/* Create link */
	nautilus_link_local_create (desktop_path, volume_name, icon_name, target_uri, NAUTILUS_LINK_MOUNT);
				    
	g_free (desktop_path);
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

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
	GList *list;
	NautilusIconContainer *icon_container;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;

	icon_container = get_icon_container (desktop_icon_view);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	

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

	nautilus_gtk_adjustment_set_value (hadj, 0);
	nautilus_gtk_adjustment_set_value (vadj, 0);

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

	gtk_signal_connect (GTK_OBJECT (nautilus_trash_monitor_get ()),
			    "trash_state_changed",
			    fm_desktop_icon_view_trash_state_changed_callback,
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (nautilus_volume_monitor_get ()),
			    "volume_mounted",
			    volume_mounted_callback,
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (nautilus_volume_monitor_get ()),
			    "volume_unmounted",
			    volume_unmounted_callback,
			    desktop_icon_view);
			    
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "create_nautilus_links",
			    GTK_SIGNAL_FUNC (icon_view_create_nautilus_links),
			    desktop_icon_view);

	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HOME_URI, home_uri_changed,
				  	   desktop_icon_view);
}

static void
new_terminal_callback (BonoboUIComponent *component, gpointer data, const char *verb)
{
	nautilus_gnome_open_terminal (NULL);
}

static void
change_background_callback (BonoboUIComponent *component, 
	  		    gpointer data, 
			    const char *verb)
{
	nautilus_launch_application_from_command 
		("background-properties-capplet", NULL, FALSE);
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
	nautilus_background_reset 
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

	if (nautilus_g_list_exactly_one_item (selection)
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

	if (nautilus_g_list_exactly_one_item (selection)
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
	char *desktop_path, *path;

	desktop_path = nautilus_get_desktop_directory ();
	path = nautilus_make_path (desktop_path, TRASH_LINK_NAME);

	/* Change the XML file to have a new icon. */
	nautilus_link_local_set_icon (path, state ? "trash-empty.png" : "trash-full.png");

	g_free (path);
	g_free (desktop_path);
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
	char *link_path, *desktop_path, *volume_name;

	g_assert (volume != NULL);
	
	volume_name = nautilus_volume_monitor_get_volume_name (volume);
	if (volume_name == NULL) {
		return;
	}
	
	desktop_path = nautilus_get_desktop_directory ();
	link_path = nautilus_make_path (desktop_path, volume_name);
	unlink_and_notify (link_path);

	g_free (volume_name);
	g_free (desktop_path);
	g_free (link_path);
}

static void
create_link_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	char *uri, *target_uri, *position;
	GnomeVFSResult expected_result;
	CreateLinkData *info;
	GList one_item_list;
	NautilusFile *file;

	info = (CreateLinkData *) callback_data;
	
	uri = info->uri;
	target_uri = info->target_uri;
	expected_result = info->expected_result;

	/* Set metadata attributes */
	file = nautilus_file_get (uri);
	position = g_strdup_printf ("%d,%d", info->point.x, info->point.y);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_ICON_POSITION, NULL, position);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, info->entry->icon);

	/* Notify directory that this new file has been created. */
	one_item_list.data = uri;
	one_item_list.next = NULL;
	one_item_list.prev = NULL;
	nautilus_directory_notify_files_added (&one_item_list);
	
	g_free (uri);
	g_free (position);
	g_free (target_uri);
	gnome_desktop_entry_free (info->entry);
	g_free (callback_data);
}


static void
icon_view_create_nautilus_links (NautilusIconContainer *container, const GList *item_uris,
			   	 int x, int y, FMDirectoryView *view)
{
	const GList *element;
	char *desktop_path, *target_uri, *link_path, *link_uri, *program_path;
	GnomeVFSURI *uri;
	GnomeDesktopEntry *entry;
	CreateLinkData *info;
	GnomeVFSAsyncHandle *handle;
	int index;
	gboolean make_nautilus_link;
	char *link_name, *last_slash, *message;
	const char *icon_name;
	
	if (item_uris == NULL) {
		return;
	}

	make_nautilus_link = FALSE;
	program_path = NULL;
	
	desktop_path = nautilus_get_desktop_directory ();

	/* Iterate through all of the URIs in the list */
	for (element = item_uris, index = 0; element != NULL; element = element->next, index++) {
		entry = gnome_desktop_entry_load ((char *)element->data);
		if (entry != NULL) {
			if (entry->terminal) {
				/* FIXME bugzilla.eazel.com 5623: Create better text here */				
				message = _("Nautilus does not currently support "
					    "launchers that require a terminal.");
				nautilus_show_warning_dialog (message, _("Unable to Create Link"),
					  fm_directory_view_get_containing_window (view));
				break;
			}
			
			/* Verify that have an actual path */			
			if (entry->exec[index][0] != '/') {
				program_path = gnome_is_program_in_path(entry->exec[index]);
				if (program_path != NULL) {
					target_uri = gnome_vfs_get_uri_from_local_path (program_path);
					g_free (program_path);
					program_path = NULL;
				} else {
					/* We may have a link or other unknown url here.  Assign it to the target uri */
					target_uri = g_strdup (entry->exec[index]);
					make_nautilus_link = TRUE;
				}
			} else {
				/* Create a URI path from the entry's executable local path */
				target_uri = gnome_vfs_get_uri_from_local_path (entry->exec[index]);
			}

			if (target_uri != NULL) {
				if (make_nautilus_link) {
					/* We just punt and create a NautilusLink if we have determined
					 * that the data will not be valid for a GnomeVFS symbolic link. */

					last_slash = strrchr (target_uri, '/');
					if (last_slash != NULL) {
						/* Make sure that the string contains more that the final slash */
						if (strlen (last_slash) > 1) {
							link_name = g_strdup_printf (_("Link to %s"), last_slash + 1);
						} else {
							link_name = g_strdup (_("Link to Unknown"));
						}
					} else {
						link_name = g_strdup_printf (_("Link to %s"), target_uri);
					}

					if (link_name != NULL) {
						if (entry->icon != NULL) {
							icon_name = entry->icon;
						} else {
							icon_name = "gnome-unknown.png";
						}
						nautilus_link_local_create (desktop_path, link_name, icon_name, 
								    	    target_uri, NAUTILUS_LINK_GENERIC);
						g_free (link_name);
					}
					g_free (target_uri);
				} else {
					/* Create an actual GnomeVFS symbolic link. */						
					uri = gnome_vfs_uri_new (target_uri);
					if (uri != NULL) {
						link_path = g_strdup_printf ("%s/%s", desktop_path, entry->name);			
						link_uri = gnome_vfs_get_uri_from_local_path (link_path);
						
						/* Create symbolic link */
						info = g_malloc (sizeof (CreateLinkData));
						info->uri = link_uri;			
						info->target_uri = target_uri;
						info->expected_result = GNOME_VFS_OK;
						info->point.x = x;
						info->point.y = y;
						info->entry = entry;
						
						gnome_vfs_async_create_symbolic_link (&handle, gnome_vfs_uri_new (link_path), 
										      target_uri, create_link_callback, info);
															
						g_free (link_path);
						gnome_vfs_uri_unref (uri);
					}
				}
			}
		}
	}

	g_free (desktop_path);
}

static gboolean
update_link_and_delete_copies (gboolean (*is_link_function) (const char *path),
			       const char *link_name,
			       const char *link_target_uri)
{
	DIR *dir;
	gboolean found_link;
	char *desktop_path;
	struct dirent *dir_entry;
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	dir = opendir (desktop_path);
	if (dir == NULL) {
		g_free (desktop_path);
		return FALSE;
	}

	found_link = FALSE;

	while ((dir_entry = readdir (dir)) != NULL) {
		link_path = nautilus_make_path (desktop_path, dir_entry->d_name);
		if ((* is_link_function) (link_path)) {
			if (strcmp (dir_entry->d_name, link_name) == 0) {
				nautilus_link_local_set_link_uri (link_path, link_target_uri);
				found_link = TRUE;
			} else {
				unlink_and_notify (link_path);
			}
		}
		g_free (link_path);
	}
	
	closedir (dir);

	g_free (desktop_path);
	
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
	char *desktop_path, *home_link_name, *home_uri;

	desktop_path = nautilus_get_desktop_directory ();

	/* Note to translators: If it's hard to compose a good home
	 * icon name from the user name, you can use a string without
	 * an "%s" here, in which case the home icon name will not
	 * include the user's name, which should be fine. To avoid a
	 * warning, put "%.0s" somewhere in the string, which will
	 * match the user name string passed by the C code, but not
	 * put the user name in the final string.
	 */
	home_link_name = g_strdup_printf (_("%s's Home"), g_get_user_name ());
	
	home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
	
	if (!update_link_and_delete_copies (nautilus_link_local_is_home_link,
					    home_link_name,
					    home_uri)) {
		nautilus_link_local_create (desktop_path,
					    home_link_name,
					    "temp-home.png", 
					    home_uri,
					    NAUTILUS_LINK_HOME);
	}
	
	g_free (desktop_path);
	g_free (home_link_name);
	g_free (home_uri);
}

static void
update_trash_link_and_delete_copies (void)
{
	char *desktop_path;

	/* Check for trash link */
	if (update_link_and_delete_copies (nautilus_link_local_is_trash_link,
					   TRASH_LINK_NAME,
					   NAUTILUS_TRASH_URI)) {
		return;
	}

	desktop_path = nautilus_get_desktop_directory ();
	nautilus_link_local_create (desktop_path,
				    TRASH_LINK_NAME,
				    "trash-empty.png", 
				    NAUTILUS_TRASH_URI,
				    NAUTILUS_LINK_TRASH);
	g_free (desktop_path);
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

	/* Let the previous handler do the compare. */
	if (category_a == category_b) {
		return 0;
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
	g_assert (!nautilus_str_is_empty (mount_path)); 

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
		name = strrchr (volume->mount_path, '/');
		if (name != NULL) {
			name = name + 1;
		} else {
			name = volume->mount_path;
		}

		nautilus_bonobo_add_numbered_toggle_menu_item 
			(view->details->ui,
			 DESKTOP_BACKGROUND_POPUP_PATH_DISKS,
			 index,
			 name);

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
			 mount_parameters_new (view, volume->mount_path),
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

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));

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
		if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH)) {
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

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

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
