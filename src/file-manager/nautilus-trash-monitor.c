/* 
   nautilus-trash-monitor.c: Nautilus trash state watcher.
 
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
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include <gnome.h>
#include <glib.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>

#include "nautilus-trash-monitor.h"
#include "libnautilus-extensions/nautilus-directory.h"
#include "libnautilus-extensions/nautilus-gtk-macros.h"

struct NautilusTrashMonitorDetails {
	NautilusDirectory *trash_directory;
	gboolean empty;
};

enum {
	TRASH_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void nautilus_trash_monitor_initialize_class (NautilusTrashMonitorClass *klass);
static void nautilus_trash_monitor_initialize (gpointer object, gpointer klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashMonitor, nautilus_trash_monitor, GTK_TYPE_OBJECT)

static NautilusTrashMonitor *nautilus_trash_monitor;

static void
nautilus_trash_monitor_initialize_class (NautilusTrashMonitorClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	signals[TRASH_STATE_CHANGED] = gtk_signal_new ("trash_state_changed",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (NautilusTrashMonitorClass, trash_state_changed),
		    		gtk_marshal_NONE__BOOL,
		    		GTK_TYPE_NONE, 1,
		    		GTK_TYPE_BOOL);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_trash_files_changed_callback (NautilusDirectory *directory, GList *files, 
	gpointer callback_data)
{
	NautilusTrashMonitor *trash_monitor;
	gboolean old_empty_state;
	
	trash_monitor = callback_data;
	g_assert (NAUTILUS_IS_TRASH_MONITOR (trash_monitor));
	g_assert (trash_monitor->details->trash_directory == directory);

	/* Something about the Trash NautilusDirectory changed, find out if 
	 * it affected the empty state.
	 */
	old_empty_state = trash_monitor->details->empty;
	trash_monitor->details->empty = !nautilus_directory_is_not_empty (directory);

	if (old_empty_state != trash_monitor->details->empty) {
		/* trash got empty or full, notify everyone who cares */
		gtk_signal_emit (GTK_OBJECT (trash_monitor), 
			signals[TRASH_STATE_CHANGED], trash_monitor->details->empty);
	}
}

static void
nautilus_trash_metadata_ready_callback (NautilusDirectory *directory, GList *files,
	gpointer callback_data)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = callback_data;

	g_assert (NAUTILUS_IS_TRASH_MONITOR (trash_monitor));
	g_assert (files == NULL);
	g_assert (trash_monitor->details->trash_directory == directory);

	nautilus_directory_file_monitor_add (directory, trash_monitor,
					     NULL, FALSE, TRUE,
					     nautilus_trash_files_changed_callback, 
					     trash_monitor);

	/* Make sure we get notified about changes */
    	gtk_signal_connect (GTK_OBJECT (trash_monitor->details->trash_directory),
		 "files_added", GTK_SIGNAL_FUNC (nautilus_trash_files_changed_callback),
		 trash_monitor);
    	gtk_signal_connect (GTK_OBJECT (trash_monitor->details->trash_directory),
		 "files_changed", GTK_SIGNAL_FUNC (nautilus_trash_files_changed_callback),
		 trash_monitor);
}

static void
nautilus_trash_monitor_initialize (gpointer object, gpointer klass)
{
	NautilusTrashMonitor *trash_monitor;
	GnomeVFSURI *trash_dir_uri;
	char *trash_dir_uri_string;

	trash_monitor = NAUTILUS_TRASH_MONITOR (object);

	/* set up a NautilusDirectory for the Trash directory to monitor */
	/* FIXME:
	 * Add trash directories from all known volumes here
	 */
	gnome_vfs_find_directory (NULL, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777);
	g_assert (trash_dir_uri != NULL);
	trash_dir_uri_string = gnome_vfs_uri_to_string (trash_dir_uri, GNOME_VFS_URI_HIDE_NONE);

	trash_monitor->details = g_new0 (NautilusTrashMonitorDetails, 1);
	trash_monitor->details->trash_directory = nautilus_directory_get (trash_dir_uri_string);
	trash_monitor->details->empty = TRUE;

	g_free (trash_dir_uri_string);
	gnome_vfs_uri_unref (trash_dir_uri);

	nautilus_directory_call_when_ready
		(trash_monitor->details->trash_directory,
		 NULL, TRUE,
		 nautilus_trash_metadata_ready_callback, trash_monitor);
}

NautilusTrashMonitor *
nautilus_trash_monitor_get (void)
{
	if (nautilus_trash_monitor == NULL) {
		/* not running yet, start it up */
		nautilus_trash_monitor = gtk_type_new (NAUTILUS_TYPE_TRASH_MONITOR);
	}

	return nautilus_trash_monitor;
}

void
nautilus_trash_monitor_shutdown (void)
{
	if (nautilus_trash_monitor == NULL) {
		return;
	}

	nautilus_directory_file_monitor_remove (nautilus_trash_monitor->details->trash_directory, 
		nautilus_trash_monitor);
	gtk_object_unref (GTK_OBJECT (nautilus_trash_monitor->details->trash_directory));	
	g_free (nautilus_trash_monitor->details);
	g_free (nautilus_trash_monitor);
}

gboolean
nautilus_trash_monitor_is_empty (void)
{
	return nautilus_trash_monitor->details->empty;
}
