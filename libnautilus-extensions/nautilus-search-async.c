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

#include <libgnomevfs/gnome-vfs-types.h>
#include <pthread.h>
#include <nautilus-directory-private.h>

#include "nautilus-search-async.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* FIXME:  use class macros for search thread closure */
typedef struct {
	pthread_t *thread;
	pthread_attr_t thread_attributes;
	char *search_uri;
} NautilusSearchThreadClosure;




static void *  run_search                     (void *data);

/* this procedure is meant to mimic the behavior of the
   async load directory uri calls, so it takes
   the same arguments */
GnomeVFSResult
nautilus_async_medusa_search (GnomeVFSAsyncHandle **handle_return,
			      char *search_uri_text,
			      GnomeVFSAsyncDirectoryLoadCallback callback,
			      gpointer data)
{
	NautilusSearchThreadClosure *search_data;

	g_return_val_if_fail (handle_return != NULL, GNOME_VFS_ERROR_BADPARAMS);
	g_return_val_if_fail (search_uri_text != NULL, GNOME_VFS_ERROR_BADPARAMS);
	g_return_val_if_fail (nautilus_uri_is_search_uri (search_uri_text) == TRUE,
			      GNOME_VFS_ERROR_BADPARAMS);

	search_data = g_new (NautilusSearchThreadClosure, 1);
	
	pthread_attr_init (&search_data->thread_attributes);
	pthread_attr_setdetachstate (&search_data->thread_attributes,
				     PTHREAD_CREATE_DETACHED);
	search_data->search_uri = g_strdup (search_uri_text);
	if (pthread_create (search_data->thread, 
			    &search_data->thread_attributes,
			    run_search, search_data) != 0) {
		g_warning ("Impossible to allocate a new Search thread.");
		return GNOME_VFS_ERROR_INTERNAL;
	}
	/* FIXME:  This has to get freed somewhere */
	/* g_free (search_data); */
	return GNOME_VFS_OK;
}

static void *
run_search (void *data) 
{
	NautilusSearchThreadClosure *closure;

	
	closure = (NautilusSearchThreadClosure *) data;
	printf ("help!! I'm trying to run a search!\n");

	g_return_val_if_fail (closure->search_uri != NULL, NULL);
	g_return_val_if_fail (nautilus_uri_is_search_uri (closure->search_uri), NULL);

	return NULL;
}




