/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

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

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
*/

#include <config.h>
#include "fm-desktop-icon-view.h"

#include "nautilus-trash-monitor.h"
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
#include <libnautilus-extensions/nautilus-volume-monitor.h>
#include <src/nautilus-application.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TRASH_LINK_NAME _("Trash")

#define DESKTOP_COMMAND_EMPTY_TRASH_CONDITIONAL	"/commands/Empty Trash Conditional"

#define DESKTOP_BACKGROUND_POPUP_PATH_DISKS	"/popups/background/Before Zoom Items/Volume Items/Disks"

struct FMDesktopIconViewDetails
{
	BonoboUIComponent *ui;
};

typedef struct {
	GnomeVFSResult expected_result;
	char *uri;
	char *target_uri;
	GdkPoint point;
	GnomeDesktopEntry *entry;
} CallbackData;


static void     fm_desktop_icon_view_initialize                           (FMDesktopIconView      *desktop_icon_view);
static void     fm_desktop_icon_view_initialize_class                     (FMDesktopIconViewClass *klass);
static void     fm_desktop_icon_view_trash_state_changed_callback         (NautilusTrashMonitor   *trash,
									   gboolean                state,
									   gpointer                callback_data);
static void     volume_mounted_callback         			  (NautilusVolumeMonitor  *monitor,
									   NautilusVolume     	  *volume,
									   FMDesktopIconView      *icon_view);
static void     volume_unmounted_callback         			  (NautilusVolumeMonitor  *monitor,
									   NautilusVolume     	  *volume,
									   FMDesktopIconView      *icon_view);
static void	icon_view_create_nautilus_links 			  (NautilusIconContainer  *container, 
									   const GList 		  *item_uris,
			   	 					   int 			  x, 
			   	 					   int 			  y, 
			   	 					   FMDirectoryView 	  *view);
static void     place_home_directory                                      (FMDesktopIconView      *icon_view);
static void     remove_old_mount_links                                    (void);
static int      desktop_icons_compare_callback                            (NautilusIconContainer  *container,
									   NautilusFile           *file_a,
									   NautilusFile           *file_b,
									   FMDesktopIconView      *icon_view);
static void	create_or_rename_trash 					  (void);
								   
static gboolean real_supports_auto_layout     	 		  	  (FMIconView  	    *view);
static void	real_merge_menus 		     	 		  (FMDirectoryView  *view);
static void	real_update_menus 		     	 		  (FMDirectoryView  *view);
static gboolean real_supports_zooming 	     	 		  	  (FMDirectoryView  *view);


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

	/* Clean up details */	
	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui);
		bonobo_object_unref (BONOBO_OBJECT (icon_view->details->ui));
	}
	g_free (icon_view->details);

	/* Clean up any links that may be left over */
	remove_old_mount_links ();
	
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
create_mount_link (const NautilusVolume *volume)
{
	gboolean result;
	char *desktop_path, *target_uri, *icon_name, *volume_name;

	/* FIXME bugzilla.eazel.com 2174: This hack was moved here
	 * from the volume monitor code.
	 */
	/* Make a link only for the root partition for now. */
	if (volume->type == NAUTILUS_VOLUME_EXT2
	    && strcmp (volume->mount_path, "/") != 0) {
		return;
	}
	
	/* Get icon type */
	if (volume->type == NAUTILUS_VOLUME_CDROM) {
		icon_name = g_strdup ("i-cdrom.png");
	} else if (volume->type == NAUTILUS_VOLUME_FLOPPY) {
		icon_name = g_strdup ("i-floppy.png");
	} else {
		icon_name = g_strdup ("i-blockdev.png");
	}
	
	desktop_path = nautilus_get_desktop_directory ();
	target_uri = gnome_vfs_get_uri_from_local_path (volume->mount_path);
	volume_name = nautilus_volume_monitor_get_volume_name (volume);
	
	/* Create link */
	result = nautilus_link_local_create
		(desktop_path, volume_name, icon_name, 
		 target_uri, NAUTILUS_LINK_MOUNT);
	/* FIXME bugzilla.eazel.com 2526: Ignoring the result here OK? */

	g_free (desktop_path);
	g_free (target_uri);
	g_free (icon_name);
	g_free (volume_name);
}

static gboolean
startup_create_mount_links (const NautilusVolume *volume, gpointer data)
{
	create_mount_link (volume);
	return TRUE;
}

static void
event_callback (GtkWidget *widget, GdkEvent *event, FMDesktopIconView *desktop_icon_view)
{
}

/* Update home link to point to new home uri */
static void
home_uri_changed (gpointer user_data)
{
	FMDesktopIconView *desktop_icon_view;

	desktop_icon_view = FM_DESKTOP_ICON_VIEW (user_data);
	place_home_directory (desktop_icon_view);
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
	NautilusIconContainer *icon_container;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	
	icon_container = get_icon_container (desktop_icon_view);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	

	nautilus_icon_container_set_is_fixed_size (icon_container, TRUE);

	/* Set allocation to be at 0, 0 */
	allocation = &GTK_WIDGET (icon_container)->allocation;
	allocation->x = 0;
	allocation->y = 0;
	gtk_widget_queue_resize (GTK_WIDGET (icon_container));

	hadj = GTK_LAYOUT (icon_container)->hadjustment;
	vadj = GTK_LAYOUT (icon_container)->vadjustment;

	nautilus_gtk_adjustment_set_value (hadj, 0);
	nautilus_gtk_adjustment_set_value (vadj, 0);

	/* Set our default layout mode */
	nautilus_icon_container_set_layout_mode (icon_container,
						 NAUTILUS_ICON_LAYOUT_T_B_L_R);

	/* Check for and clean up any old mount links that may have been left behind */		
	remove_old_mount_links ();

	/* Setup home directory link */
	place_home_directory (desktop_icon_view);

	/* Create trash link */
	create_or_rename_trash ();

	/* Create initial mount links */
	nautilus_volume_monitor_each_mounted_volume (nautilus_volume_monitor_get (),
					     	     startup_create_mount_links, desktop_icon_view);
	
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

	nautilus_file_operations_empty_trash (GTK_WIDGET (FM_DIRECTORY_VIEW (data)));
}

static void
reset_background_callback (BonoboUIComponent *component, 
			   gpointer data, 
			   const char *verb)
{
	g_assert (FM_IS_DIRECTORY_VIEW (data));

	nautilus_background_reset 
		(fm_directory_view_get_background (FM_DIRECTORY_VIEW (data)));
}

static void
quit_desktop_callback (BonoboUIComponent *component, 
		       gpointer data, 
		       const char *verb)
{
	nautilus_application_close_desktop ();
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

static void
fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
						   gboolean state, gpointer callback_data)
{
	char *desktop_directory_path, *path;

	desktop_directory_path = nautilus_get_desktop_directory ();
	path = nautilus_make_path (desktop_directory_path, TRASH_LINK_NAME);

	/* Change the XML file to have a new icon. */
	nautilus_link_local_set_icon (path, state ? "trash-empty.png" : "trash-full.png");

	g_free (path);
	g_free (desktop_directory_path);
}

static void
volume_mounted_callback (NautilusVolumeMonitor *monitor,
			 NautilusVolume *volume, 
			 FMDesktopIconView *icon_view)
{
	create_mount_link (volume);
}

static void
volume_unmounted_callback (NautilusVolumeMonitor *monitor,
			   NautilusVolume *volume, 
			   FMDesktopIconView *icon_view)
{
	GnomeVFSResult result;
	char *link_path, *link_uri, *desktop_path, *volume_name;
	GList dummy_list;

	g_assert (volume);
	
	desktop_path = nautilus_get_desktop_directory ();
	volume_name = nautilus_volume_monitor_get_volume_name (volume);
	if (volume_name == NULL) {
		return;
	}
	
	link_path = nautilus_make_path (desktop_path, volume_name);
	link_uri = gnome_vfs_get_uri_from_local_path (link_path);
	if (link_uri == NULL) {
		g_free (link_path);
		g_free (volume_name);
		return;
	}
	
	/* Remove mounted device icon from desktop */
	dummy_list.data = link_uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_removed (&dummy_list);
	
	result = gnome_vfs_unlink (link_uri);
	if (result != GNOME_VFS_OK) {
		/* FIXME bugzilla.eazel.com 2526: OK to ignore error? */
	}
	g_free (link_uri);
	g_free (link_path);
	g_free (desktop_path);
	g_free (volume_name);
}

static void
create_link_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	char *uri, *target_uri, *position;
	GnomeVFSResult expected_result;
	CallbackData *info;
	GList dummy_list;
	NautilusFile *file;

	info = (CallbackData*) callback_data;
	
	uri = info->uri;
	target_uri = info->target_uri;
	expected_result = info->expected_result;

	/* Set metadata attributes */
	file = nautilus_file_get (uri);
	position = g_strdup_printf ("%d,%d", info->point.x, info->point.y);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_ICON_POSITION, "0,0", position);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, "icon", info->entry->icon);

	/* Notify directory that this new file has been created. */
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;

	nautilus_directory_notify_files_added (&dummy_list);
	
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
	CallbackData *info;
	GnomeVFSAsyncHandle *handle;
	int index;
	gboolean make_nautilus_link;
	
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
					char *link_name = NULL;
					char *last_slash = strrchr (target_uri, '/');
					
					if (last_slash != NULL) {
						/* Make sure that the string contains more that the final slash */
						if (strlen (last_slash) > 1) {
							link_name = g_strdup_printf ("%s %s", _("Link to"), last_slash + 1);
						} else {
							link_name = g_strdup (_("Link to Unknown"));
						}
					} else {
						link_name = g_strdup_printf ("%s %s", _("Link to"), target_uri);
					}

					if (link_name != NULL) {
						char *icon_name;
						if (entry->icon != NULL) {
							icon_name = g_strdup (entry->icon);
						} else {
							icon_name = g_strdup ("gnome-unknown.png");
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
						info = g_malloc (sizeof (CallbackData));
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
find_and_update_home_link (void)
{
	DIR *current_dir;
	struct dirent *this_entry;
	struct stat status;
	char *desktop_path, *link_path;
	char *home_link_name, *home_link_path, *home_dir_uri, *home_uri;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		g_free (desktop_path);
		return FALSE;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  The home link is at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a home link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);
				if (nautilus_link_local_is_home_link (link_path)) {

					/* Create the home link */					
					home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
					home_link_path = nautilus_make_path (desktop_path, home_link_name);
					
					home_dir_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
					home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, home_dir_uri);

					/* Make sure URI points to user specified home location */
					nautilus_link_local_set_link_uri (home_link_path, home_uri);

					g_free (home_link_path);
					g_free (home_link_name);
					g_free (desktop_path);
					g_free (home_dir_uri);
					g_free (home_uri);
					g_free (link_path);
										  
					return TRUE;
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);

	g_free (desktop_path);

	return FALSE;
}

/* place_home_directory
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
static void
place_home_directory (FMDesktopIconView *icon_view)
{
	char *desktop_path, *home_link_name, *home_link_path, *home_dir_uri, *home_uri;
	gboolean made_link;

	/* Check and see if there is a home link already.  If so, make */
	if (find_and_update_home_link ()) {
		return;
	}

	/* Create the home link */
	desktop_path = nautilus_get_desktop_directory ();
	home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
	home_link_path = nautilus_make_path (desktop_path, home_link_name);
	
	home_dir_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, home_dir_uri);
	made_link = nautilus_link_local_create
		(desktop_path, home_link_name, "temp-home.png", 
		 home_uri, NAUTILUS_LINK_HOME);	
	if (!made_link) {
		/* FIXME bugzilla.eazel.com 2526: Is a message to the console acceptable here? */
		g_message ("Unable to create home link");
	}
	
	g_free (home_link_path);
	g_free (home_link_name);
	g_free (desktop_path);
	g_free (home_dir_uri);
	g_free (home_uri);
}

/* Find a trash link and reset the name to Trash */	
static gboolean
find_and_rename_trash_link (void)
{
	DIR *current_dir;
	char *desktop_path;		
	struct dirent *this_entry;
	struct stat status;
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		return FALSE;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  The home link is at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a home link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);				
				if (nautilus_link_local_is_trash_link (link_path)) {
					/* Reset name */
					rename (this_entry->d_name, TRASH_LINK_NAME);
					nautilus_link_local_set_link_uri (link_path, NAUTILUS_TRASH_URI);
					return TRUE;
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);

	return FALSE;
}

static void
create_or_rename_trash (void)
{
	char *desktop_directory_path;

	/* Check for trash link */
	if (find_and_rename_trash_link ()) {
		return;
	}

	desktop_directory_path = nautilus_get_desktop_directory ();
	nautilus_link_local_create (desktop_directory_path,
				    TRASH_LINK_NAME,
				    "trash-empty.png", 
				    NAUTILUS_TRASH_URI,
				    NAUTILUS_LINK_TRASH);
	g_free (desktop_directory_path);
}

static void
remove_old_mount_links (void)
{
	DIR *current_dir;
	char *desktop_path;		
	struct dirent *this_entry;
	struct stat status;
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		return;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  Mount links are at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);
				if (nautilus_link_local_is_volume_link (link_path)) {
					unlink (this_entry->d_name);					
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);
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

static void
mount_or_unmount_removable_volume (BonoboUIComponent *component,
	       			   const char *path,
	       			   Bonobo_UIComponent_EventType type,
	       			   const char *state,
	       			   gpointer user_data)
{
	g_assert (BONOBO_IS_UI_COMPONENT (component));

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	nautilus_volume_monitor_mount_unmount_removable 
		(nautilus_volume_monitor_get (), (char *)user_data); 
}	       

/* Fill in the context menu with an item for each disk that is or could be mounted */
static void
update_disks_menu (FMDesktopIconView *view)
{
	GList *disk_list;
	GList *element;
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
			 nautilus_volume_monitor_volume_is_mounted (volume));
		g_free (command_path);

		bonobo_ui_component_add_listener_full
			(view->details->ui,
			 command_name,
			 mount_or_unmount_removable_volume,
			 g_strdup (volume->mount_path),
			 g_free);

		g_free (command_name);
	}
	g_list_free (disk_list);
}

static void
real_update_menus (FMDirectoryView *view)
{
	FMDesktopIconView *desktop_view;
	char *label;
	gboolean include_empty_trash;
	
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
		if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH, TRUE)) {
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
		BONOBO_UI_VERB ("Quit Desktop", quit_desktop_callback),
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
	/* Can't use auto-layout on the desktop, because doing so would cause all
	 * sorts of complications involving the fixed-size window.
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



