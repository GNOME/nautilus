/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-monitor.c: file and directory change monitoring for nautilus
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
  
   Authors: Seth Nickell <seth@eazel.com>
            Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-monitor.h"

#include <eel/eel-glib-extensions.h>

#ifdef HAVE_LIBFAM

#include "nautilus-file-changes-queue.h"
#include <fam.h>
#include <gdk/gdk.h>
#include <gmodule.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

struct NautilusMonitor {
	FAMRequest request;
};

static gboolean got_connection;

static gboolean process_fam_notifications (GIOChannel *channel,
					   GIOCondition cond,
					   gpointer callback_data);

/* singleton object, instantiate and connect if it doesn't already exist */
static FAMConnection *
get_fam_connection (void)
{
	static gboolean tried_connection;
	static FAMConnection connection;
	GIOChannel *ioc;
	
	/* Only try once. */
        if (tried_connection) {
		if (!got_connection) {
			return NULL;
		}
	} else {
                tried_connection = TRUE;
		if (FAMOpen2 (&connection, "Nautilus") != 0) {
			return NULL;
		}

		/* Make the main loop's select function watch the FAM
                 * connection's file descriptor for us.
		 */
		ioc = g_io_channel_unix_new (FAMCONNECTION_GETFD (&connection));
		g_io_add_watch (ioc, G_IO_IN | G_IO_HUP, process_fam_notifications, NULL);
		g_io_channel_unref (ioc);

		got_connection = TRUE;
	}
	return &connection;
}

static GHashTable * 
get_uri_hash_table (void)
{
	static GHashTable *table;

	if (table == NULL) {
		table = eel_g_hash_table_new_free_at_exit
			(NULL, NULL, "nautilus-monitor.c: FAM URI requests");
	}
	return table;
}

static GHashTable *
get_request_hash_table (void)
{
	static GHashTable *table;

	if (table == NULL) {
		table = eel_g_hash_table_new_free_at_exit
			(NULL, NULL, "nautilus-monitor.c: FAM requests");
	}
	return table;
}

static char *
get_event_uri (const FAMEvent *event)
{
        const char *base_path;
	char *path, *uri;

	/* For strange uri's like favorites: we do this stupid hack */
	uri = g_hash_table_lookup (get_uri_hash_table (),
				   GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&event->fr)));
	if (uri) {
		if (event->filename[0] == '/') {
			return g_strdup (uri);
		} else {
			return g_strconcat (uri, "///", event->filename, NULL);
		}
        }
	
        /* FAM doesn't tell us when something is a full path and when
	 * it's just partial so we have to look and see if it starts
	 * with a /.
	 */
        if (event->filename[0] == '/') {
                return gnome_vfs_get_uri_from_local_path (event->filename);
        }

	/* Look up the directory registry that was used for this file
	 * notification and tack that on.
	 */
	base_path = g_hash_table_lookup (get_request_hash_table (),
					 GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&event->fr)));
	g_return_val_if_fail (base_path != NULL, NULL);
	path = g_concat_dir_and_file (base_path, event->filename);
	uri = gnome_vfs_get_uri_from_local_path (path);
	g_free (path);
        return uri;
}

static gboolean
process_fam_notifications (GIOChannel *channel,
			   GIOCondition cond,
			   gpointer callback_data)
{
        FAMConnection *connection;
        FAMEvent event;
	char *uri;

        connection = get_fam_connection ();
	g_return_val_if_fail (connection != NULL, FALSE);

        /* Process all the pending events right now. */

        while (FAMPending (connection)) {
                if (FAMNextEvent (connection, &event) != 1) {
                        g_warning ("connection to FAM died");
                        FAMClose (connection);
                        got_connection = FALSE;
                        return FALSE;
                }

                switch (event.code) {
                case FAMChanged:
			uri = get_event_uri (&event);
			if (uri == NULL) {
				break;
			}
                        nautilus_file_changes_queue_file_changed (uri);
			g_free (uri);
                        break;

                case FAMDeleted:
			uri = get_event_uri (&event);
			if (uri == NULL) {
				break;
			}
                        nautilus_file_changes_queue_file_removed (uri);
			g_free (uri);
                        break;

                case FAMCreated:                
			uri = get_event_uri (&event);
			if (uri == NULL) {
				break;
			}
                        nautilus_file_changes_queue_file_added (uri);
			g_free (uri);
                        break;

                case FAMStartExecuting:
			/* Emitted when a file you are monitoring is
			 * executed. This should work for both
			 * binaries and shell scripts. Nautilus is not
			 * doing anything with this yet.
			 */
			break;

                case FAMStopExecuting:
			/* Emitted when a file you are monitoring
			 * ceases execution. Nautilus is not doing
			 * anything with this yet.
			 */
			break;

                case FAMAcknowledge:
			/* Called in response to a successful
			 * CancelMonitor. We don't need to do anything
			 * with this information.
			 */
			break;

                case FAMExists:
			/* Emitted when you start monitoring a
			 * directory. It tells you what's in the
			 * directory. Unhandled because Nautilus
			 * already handles this by calling
			 * gnome_vfs_directory_load, which gives us
			 * more information than merely the file name.
			 */
			break;

                case FAMEndExist:
			/* Emitted at the end of a FAMExists stream. */
			break;

                case FAMMoved:
			/* FAMMoved doesn't need to be handled because
			 * FAM never seems to generate this event on
			 * Linux systems (w/ or w/o IMON). Instead it
			 * generates a FAMDeleted followed by a
			 * FAMCreated.
			 */
			g_warning ("unexpected FAMMoved notification");
			break;
                }
        }

	nautilus_file_changes_consume_changes (TRUE);
	
	return TRUE;
}

#endif /* HAVE_LIBFAM */

gboolean
nautilus_monitor_active (void)
{
#ifndef HAVE_LIBFAM
	return FALSE;
#else
	return get_fam_connection () != NULL;
#endif
}

char * fixup_local_path (const char *uri_text);

char *
fixup_local_path (const char *uri_text)
{
	GnomeVFSURI *uri = gnome_vfs_uri_new (uri_text);
	gchar *path = NULL;
	/* Ugly hack to catch writes to favorites: */
	
	uri = gnome_vfs_uri_new (uri_text);
	if (strcmp (gnome_vfs_uri_get_scheme (uri), "favorites") == 0)
		path = g_strconcat (g_get_home_dir (),
				    "/.gnome/apps",
				    gnome_vfs_uri_get_path (uri),
				    NULL);

	return path;
}

NautilusMonitor *
nautilus_monitor_file (const char *uri)
{
#ifndef HAVE_LIBFAM
	return NULL;
#else
        FAMConnection *connection;
        char *path;
	NautilusMonitor *monitor;
	gboolean fixed = FALSE;

        connection = get_fam_connection ();
	if (connection == NULL) {
		return NULL;
	}

	path = gnome_vfs_get_local_path_from_uri (uri);
	if (path == NULL) {
		path = fixup_local_path (uri);
		fixed = TRUE;
	}
	if (path == NULL) {
		return NULL;
	}
        
	monitor = g_new0 (NautilusMonitor, 1);
	FAMMonitorFile (connection, path, &monitor->request, NULL);

	if (fixed) {
		GHashTable *uri_table = get_uri_hash_table ();

		g_hash_table_insert (uri_table,
				     GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&monitor->request)),
				     g_strdup (uri));
	}
	
	g_free (path);

	return monitor;
#endif
}



NautilusMonitor *
nautilus_monitor_directory (const char *uri)
{
#ifndef HAVE_LIBFAM
	return NULL;
#else
        FAMConnection *connection;
        char *path;
	NautilusMonitor *monitor;
	gboolean fixed = FALSE;

        connection = get_fam_connection ();
	if (connection == NULL) {
		return NULL;
	}

	path = gnome_vfs_get_local_path_from_uri (uri);
	if (path == NULL) {
		path = fixup_local_path (uri);
		fixed = TRUE;
	}
	if (path == NULL) {
		return NULL;
	}
        
	monitor = g_new0 (NautilusMonitor, 1);
	FAMMonitorDirectory (connection, path, &monitor->request, NULL);

	g_assert (g_hash_table_lookup (get_request_hash_table (),
				       GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&monitor->request))) == NULL);

	g_hash_table_insert (get_request_hash_table (),
			     GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&monitor->request)),
			     path);

	if (fixed) {
		GHashTable *uri_table = get_uri_hash_table ();

		g_hash_table_insert (uri_table,
				     GINT_TO_POINTER (FAMREQUEST_GETREQNUM (&monitor->request)),
				     g_strdup (uri));
	}

	
	return monitor;
#endif
}

void
nautilus_monitor_cancel (NautilusMonitor *monitor)
{       
#ifndef HAVE_LIBFAM
	g_return_if_fail (monitor == NULL);
#else
        FAMConnection *connection;
	int reqnum;
	char *path;
	char *uri;

	if (monitor == NULL) {
		return;
	}

	reqnum = FAMREQUEST_GETREQNUM (&monitor->request);
	path = g_hash_table_lookup (get_request_hash_table (),
				    GINT_TO_POINTER (reqnum));
	g_hash_table_remove (get_request_hash_table (),
			     GINT_TO_POINTER (reqnum));
	g_free (path);
	uri = g_hash_table_lookup (get_uri_hash_table (),
				   GINT_TO_POINTER (reqnum));
	if (uri) {
		g_hash_table_remove (get_uri_hash_table (),
				     GINT_TO_POINTER (reqnum));
		g_free (uri);
	}

        connection = get_fam_connection ();
	g_return_if_fail (connection != NULL);

	FAMCancelMonitor (connection, &monitor->request);
	g_free (monitor);
#endif
}
