/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-monitor.c: file and directory change monitoring for nautilus
 
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
  
   Author: Seth Nickell <seth@eazel.com>
*/

#include <glib.h>

#if !HAVE_FAM_H
#include <fam.h>
#endif

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <stdio.h>
#include <string.h>

#include "nautilus-directory-notify.h"
#include "nautilus-file.h"
#include "nautilus-monitor.h"

#if !HAVE_FAM_H

static FAMConnection  *fam_connection;

static gboolean  tried_connection = FALSE;
static gboolean  connection_valid = TRUE;
static GList    *monitoring_list  = NULL;

typedef struct {
        char *uri;
        FAMRequest request;
} URIRequest;


/* FORWARD DECLARATIONS */
static void  nautilus_monitor_process_fam_notifications (gpointer data, gint fd, GdkInputCondition condition);


/* establish the initial connection with fam, called the first time the fam_connection */
/* object is "used" by one of the subfunctions                                         */
static gboolean
nautilus_monitor_establish_connection ()
{
        gboolean connection_valid;
        
        fam_connection = g_malloc0(sizeof(FAMConnection));

        /*  Connect to fam  */
        if (FAMOpen2(fam_connection, "Nautilus") != 0) {
                printf ("FAM_DEBUG: Nautilus failed to establish a link to the file alteration monitor\n");
                g_free(fam_connection);
                fam_connection = NULL;
                connection_valid = FALSE;
        } else {
                /* register our callback into the gtk loop */
                gdk_input_add(FAMCONNECTION_GETFD(fam_connection), GDK_INPUT_READ, 
                              nautilus_monitor_process_fam_notifications, NULL);
                connection_valid = TRUE;
        }

        return connection_valid;
}

/* singleton object, instantiate and connect if it doesn't already exist */
/* otherwise just pass back a reference to the existing fam_connection   */
static FAMConnection *
nautilus_monitor_get_fam ()
{
        if (tried_connection) {
                return fam_connection;
        } else {
                connection_valid = nautilus_monitor_establish_connection();
                tried_connection = TRUE;
                return fam_connection;
        }
}

/* give a request, find what path was used to create it */
static const char *
nautilus_monitor_find_path_from_request (GList *list, FAMRequest request) {
        URIRequest *uri_request = NULL;
        GList *p;
        gboolean found = FALSE;

        for (p = list; (p != NULL) && (!found); p = p->next) {
                uri_request = (URIRequest *)p->data;
                if (uri_request->request.reqnum == request.reqnum) {
                        found = TRUE;
                }
        }
        if (found) {
                return uri_request->uri;
        } else {
                return NULL;
        }
}

/* given a path, find the registered "request" that deals with it */
static const FAMRequest
nautilus_monitor_find_request_from_path (GList *list, const char *path) {
        URIRequest *uri_request = NULL;
        GList *p;
        gboolean found = FALSE;
        FAMRequest fr;

        for (p = list; (p != NULL) && (!found); p = p->next) {
                uri_request = (URIRequest *)p->data;
                if (strcmp (uri_request->uri, path) == 0) {
                        found = TRUE;
                }
        }
        if (found) {
                return uri_request->request;
        } else {
                fr.reqnum = -1;
                return fr;
        }
}

/* delete first entry found in list that matches path */
static void
nautilus_monitor_delete_first_request_found (GList *list, const char *path) {
        URIRequest *uri_request = NULL;
        GList *p, *last = NULL;
        gboolean found = FALSE;

        for (p = list; (p != NULL) && (!found); p = p->next) {
                uri_request = (URIRequest *)p->data;
                if (strcmp (uri_request->uri, path) == 0) {
                        found = TRUE;
                }
                last = p;
        }
        if (found) {
                g_free (uri_request->uri);
                g_free (uri_request);
                g_list_remove_link (list, last);
        }
}

static char *
nautilus_monitor_get_uri (char * file_name, FAMRequest fr)
{
        const char *base_uri;
        char *uri_string;

        /* FAM doesn't tell us when something is a full path, and when its just partial */
        /* so we have to look and see if it starts with a /                             */

        if (file_name[0] == '/') {
                uri_string = g_strdup_printf ("file://%s", file_name);
        } else {
                /* lookup the directory registry that was used for this file notification */
                /* and tack that on as a base uri fragment                                */
                base_uri = nautilus_monitor_find_path_from_request (monitoring_list, fr);
                uri_string = g_strdup_printf ("%s/%s", base_uri, file_name);
        }
        return uri_string;
}

static void
nautilus_monitor_process_fam_notifications (gpointer data, gint fd, GdkInputCondition condition)
{
        FAMConnection *fam;
        FAMEvent fam_event;     
        char *uri_string;
        GList *uri_list;

        /* get singleton object */
        fam = nautilus_monitor_get_fam ();

        /*  We want to read as many events as are available. */
        while (FAMPending(fam)) {

                if (FAMNextEvent(fam, &fam_event) != 1) {
                        printf ("FAM-DEBUG: Nautilus' link to fam died\n");
                        gdk_input_remove(fd);
                        FAMClose(fam);
                        g_free(fam);
                        connection_valid = FALSE;
                        return;
                }

		uri_string = nautilus_monitor_get_uri (fam_event.filename, fam_event.fr);
                
                switch (fam_event.code) {
                case FAMChanged:
                        printf ("FAMChanged : %s\n", uri_string);

                        /* FIXME: why doesn't the following work for updates?  */
                        /* file = nautilus_file_get (uri_string);              */
			/* nautilus_file_changed (file);                       */
                        /* nautilus_file_unref (file);                         */

                        uri_list = NULL;
                        uri_list = g_list_append (uri_list, uri_string);                        

                        nautilus_directory_notify_files_added (uri_list);

                        g_free (uri_string);
                        g_list_free (uri_list);

                        break;
                case FAMDeleted:
                        printf ("FAMDeleted : %s\n", uri_string);

                        uri_list = NULL;
                        uri_list = g_list_append (uri_list, uri_string);

                        nautilus_directory_notify_files_removed (uri_list);

                        g_free (uri_string);
                        g_list_free (uri_list);
                        break;
                case FAMCreated:                
                        printf ("FAMCreated : %s\n", uri_string);

                        uri_list = NULL;
                        uri_list = g_list_append (uri_list, uri_string);                        

                        nautilus_directory_notify_files_added (uri_list);
                        
                        g_free (uri_string);
                        g_list_free (uri_list);
                        break;
                case FAMStartExecuting:
			/* FAMStartExecuting is emitted when a file you are monitoring is
			   executed. This should work for both binaries and shell scripts. It
			   is unhandled because we don't do anything with this in Nautilus yet */
			break;
                case FAMStopExecuting:
			/* FamStopExecuting is emitted when a file you are monitoring ceases
			   execution. Unhandled because Nautilus doesn't do anything with execution
			   of files yet. */
			break;
                case FAMAcknowledge:
			/* Called in response to a successful CancelMonitor. Not sure why this
			   is included in FAM, and hence it is unhandled. */
			break;
                case FAMExists:
			/* FAMExists is emmitted when you start monitoring a directory. It tells you 
			   what's in the directory. Unhandled because we already handle this by
			   calling gnome_vfs_directory_load, which gives us more information
			   than merely the filename */
			break;
                case FAMEndExist:
			/* This event is emitted generated at the end of a FAMExists stream */
			break;
                case FAMMoved:
			/* FAMMoved is unhandled because FAM never seems to generate this
			   event on Linux systems (w/ or w/o IMON). Instead it generates a
			   FAMDeleted followed by a FAMCreated */
			break;
                }
        }
}

#endif /* !HAVE_FAM_H */



void
nautilus_monitor_add_file (const char *uri_string)
{

#if !HAVE_FAM_H

        const char *uri_scheme;
        char *path_name;
        GnomeVFSURI *uri;
        FAMConnection *fam;
        URIRequest *uri_request;
        
        /* get singleton object for connection */
        fam = nautilus_monitor_get_fam ();

        /* only try connecting once */
        if (!connection_valid) return;
        
        uri = gnome_vfs_uri_new (uri_string);
        uri_scheme = gnome_vfs_uri_get_scheme (uri);

        /* remove crufty entries registered for this URI */
        /* FIXME: this breaks multiple windows because we don't */
	/* currently have the clientid, we need to pass this in */
        /* nautilus_monitor_remove (uri_string); */

        /* we only know how to deal with things on the local filesystem for now */
        if (strcmp (uri_scheme, "file") == 0) {
                uri_request = g_new (URIRequest,1);
                path_name = strdup (gnome_vfs_uri_get_path (uri));
                FAMMonitorFile(fam, path_name, &uri_request->request, 0);

                printf ("ADDED : %d) file monitor for %s\n", uri_request->request.reqnum, uri_string);
                uri_request->uri = strdup (uri_string);
                /* add this URI to the "Things we watch" list */
                monitoring_list = g_list_append (monitoring_list, uri_request);

		g_free (path_name);
        }

        gnome_vfs_uri_unref (uri);

#endif /* !HAVE_FAM_H */

}



void
nautilus_monitor_add_directory (const char *uri_string)
{

#if !HAVE_FAM_H

        const char *uri_scheme;
        char *path_name;
        GnomeVFSURI *uri;
        FAMConnection *fam;
        URIRequest *uri_request;

        fam = nautilus_monitor_get_fam ();

        /* only try connection once */
        if (!connection_valid) return;
        
        uri = gnome_vfs_uri_new (uri_string);
        uri_scheme = gnome_vfs_uri_get_scheme (uri);

        /* remove crufty entries registered for this URI */
        /* FIXME: this breaks multiple windows because we don't */
	/* currently have the clientid, we need to pass this in */
        /* nautilus_monitor_remove (uri_string); */

        /* we only know how to deal with things on the local filesystem for now */
        if (strcmp (uri_scheme, "file") == 0) {
                uri_request = g_new (URIRequest, 1);
                path_name = strdup (gnome_vfs_uri_get_path (uri));
                FAMMonitorDirectory(fam, path_name, &uri_request->request, 0);

                printf ("ADDED : %d) directory monitor for %s\n", uri_request->request.reqnum, uri_string);
                uri_request->uri = strdup (uri_string);
                /* add this uri to our list of "things we watch" */
                monitoring_list = g_list_append (monitoring_list, uri_request);
		
		g_free (path_name);
        }

        gnome_vfs_uri_unref (uri);

#endif /* !HAVE_FAM_H */

}

void
nautilus_monitor_remove (const char *uri)
{       

#if !HAVE_FAM_H

        FAMConnection *fam;
        FAMRequest request, request2;
        GnomeVFSURI *real_uri;
        const char *uri_scheme;
        int code;

        fam = nautilus_monitor_get_fam ();

        /* only try connecting once */
        if (!connection_valid) return;

        real_uri = gnome_vfs_uri_new (uri);
        uri_scheme = gnome_vfs_uri_get_scheme (real_uri);
        gnome_vfs_uri_unref (real_uri);

        if (strcmp (uri_scheme, "file") == 0) {
                /* keep looking for entries and deleting them */
                request = nautilus_monitor_find_request_from_path (monitoring_list, uri);
                while (request.reqnum != -1) {
                        request2 = request;
                        code = FAMCancelMonitor (fam, &request);
                        printf ("REMOVED : %d) directory monitor for %s (return %d)\n", request2.reqnum, uri, code);
                        nautilus_monitor_delete_first_request_found (monitoring_list, uri);
                        request = nautilus_monitor_find_request_from_path (monitoring_list, uri);
                }
        }

#endif /* !HAVE_FAM_H */
}
