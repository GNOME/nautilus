/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-async.c :  The search process
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Rebecca Schulman <rebecka@eazel.com>
*/

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <gtk/gtkmain.h>
#include <gdk/gdk.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <nautilus-directory-private.h>
#include <medusa-search-service.h>
#include <glib.h>

#include "nautilus-search-async.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif



static GnomeVFSResult   request_search                    (char *search_uri);
static int              initialize_socket                 (struct sockaddr_un *daemon_address);
static int              get_key_from_cookie               (void);
void                    parse_results                     (char *transmission);
static void             search_results_received_callback  (GIOChannel *source,
							   GIOCondition condition,
							   gpointer data);

/* this procedure is meant to mimic the behavior of the
   async load directory uri calls, so it takes
   the same arguments */
GnomeVFSResult
nautilus_async_medusa_search (GnomeVFSAsyncHandle **handle_return,
			      char *search_uri_text,
			      GnomeVFSAsyncDirectoryLoadCallback callback,
			      gpointer data)
{
	GnomeVFSResult result;

	g_return_val_if_fail (handle_return != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (search_uri_text != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (nautilus_uri_is_search_uri (search_uri_text) == TRUE,
			      GNOME_VFS_ERROR_BAD_PARAMETERS);

	/* the sending of the request is done synchronously,
	   and the nautilus receives the results over a channel. */
	result = request_search (search_uri_text);
	
	return result;
}

static GnomeVFSResult
request_search (char *search_uri) 
{
	int search_request_port;
	struct sockaddr_un *server_address;
	char cookie_request[MAX_LINE];
	char request_field[MAX_LINE];
	int key;
	GIOChannel *result_channel;

	
	printf ("help!! I'm trying to run a search!\n");
	/* FIXME:  This crap will be replaced by
	   the new medusa search service API */
	g_return_val_if_fail (search_uri != NULL, GNOME_VFS_ERROR_INVALID_URI);
	g_return_val_if_fail (nautilus_uri_is_search_uri (search_uri), 
			      GNOME_VFS_ERROR_INVALID_URI);


	/* For now run a dummy search */
	search_request_port = initialize_socket (server_address);


	/* Send request for cookie */
	sprintf (cookie_request, "%s\t%d\t%d\n", COOKIE_REQUEST, getuid (), getpid());
	printf ("Sending %s\n", cookie_request);
	g_return_val_if_fail (write (search_request_port, cookie_request, 
				     strlen (cookie_request)) > 0,
			      GNOME_VFS_ERROR_DIRECTORY_BUSY);

	

	key = get_key_from_cookie ();
	printf ("Got cookie %d from cookie file\n", key);
	sprintf (request_field, "%d %d %d\tFile_Name ^ tmp\n", getuid (), getpid (), key);
	
	printf ("Sending %s", request_field);
	g_return_val_if_fail (write (search_request_port, request_field, 
				     strlen(request_field)) > 0, 
			      GNOME_VFS_ERROR_DIRECTORY_BUSY);
	
	memset (request_field, 0, MAX_LINE);
	sprintf (request_field,"%d %d %d\tDONE\n", getuid (), getpid (), key);
	g_return_val_if_fail (write (search_request_port, request_field, 
				     strlen(request_field)) > 0, 
			      GNOME_VFS_ERROR_DIRECTORY_BUSY);
	
	/* Results are gotten in a different place */
	/* Set up a watch on the result socket */
	/* FIXME:  Is there a right macro here for these function casts? */
	result_channel = g_io_channel_unix_new (search_request_port);
	g_io_add_watch (result_channel,
			G_IO_IN,
			(GIOFunc) search_results_received_callback,
			search_uri);

	return GNOME_VFS_OK;
}




static int
initialize_socket (struct sockaddr_un *daemon_address)
{
  int search_request_port;
    
  search_request_port = socket (AF_LOCAL, SOCK_STREAM, 0);
  g_return_val_if_fail (search_request_port != -1, 3);

  daemon_address->sun_family = AF_LOCAL;
  /* FIXME:  This number (108) sucks, but it has no #define in the header.
     What to do? (POSIX requires 100 bytes at least, here)  */
  snprintf (daemon_address->sun_path, 100, "%s", SEARCH_SOCKET_PATH);

  g_return_val_if_fail (connect (search_request_port, (struct sockaddr *) daemon_address,
			     SUN_LEN (daemon_address)) != -1, -1);
  
  

  return search_request_port;
}


static int
get_key_from_cookie ()
{
  char file_name[MAX_LINE];
  int cookie_fd, key;
  /* Go look for cookie */
  sprintf (file_name, "%s/%d_%d", COOKIE_PATH, getuid (), getpid ());
  printf ("Looking in cookie file %s\n", file_name);

  cookie_fd = open (file_name, O_RDONLY);
  /* Keep looking if cookie file isn't created yet */
  while (cookie_fd == -1) {
    cookie_fd = open (file_name, O_RDONLY);
  }
  read (cookie_fd, &key, sizeof (int));
  close (cookie_fd);
  
  return key;
}



void
parse_results (char *transmission)
{
  char *line;
  line = transmission;
#ifdef SEARCH_DAEMON_DEBUG
  /* printf ("Received %s\n", line); */
#endif
  while ((line - 1) != NULL && *line != 0) {
    if (strncmp (line, SEARCH_FILE_TRANSMISSION, strlen (SEARCH_FILE_TRANSMISSION)) == 0) {
      printf ("next file is %s", line);
    }
    else if (strncmp (line, SEARCH_END_TRANSMISSION, strlen (SEARCH_END_TRANSMISSION)) == 0) {
      exit (1);
    }
    line = strchr (line, 0);
    line++;
  }
}

static void
search_results_received_callback (GIOChannel *source,
				  GIOCondition condition,
				  gpointer data)
{
	
	g_assert (condition == G_IO_IN);
	/* Make sure source is a valid file descriptor */
	printf ("Received search results\n");

}

