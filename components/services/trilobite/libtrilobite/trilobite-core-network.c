/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 *  trilobite-core-network: functions for retrieving files from the
 *  network and parsing XML documents
 *
 *  Copyright (C) 2000 Eazel, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: J Shane Culpepper <pepper@eazel.com>
 *	     Robey Pointer <robey@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgnomevfs/gnome-vfs.h>
#include "trilobite-core-utils.h"
#include "trilobite-core-network.h"


/* function for lazy bastards who can't be bothered to figure out the format of the xml they're parsing:
 * it checks for a property with the name, and then if there isn't one, then it tries to find a child
 * with that name instead.
 */
char *
trilobite_xml_get_string (xmlNode *node, const char *name)
{
	char *ret;
	char *tmp;
	xmlNode *child;

	ret = xmlGetProp (node, name);
	if (ret) {
		goto good;
	}
	child = node->xmlChildrenNode;
	while (child) {
		if (g_strcasecmp (child->name, name) == 0) {
			ret = xmlNodeGetContent (child);
			if (ret) {
				goto good;
			}
		}
		child = child->next;
	}
	return NULL;

good:
	tmp = g_strdup (ret);
	xmlFree (ret);
	return tmp;
}

static GnomeVFSHandle *
trilobite_open_uri (const char *uri_text)
{
	GnomeVFSResult err;
	GnomeVFSURI *uri;
	GnomeVFSHandle *handle = NULL;

	if (! gnome_vfs_initialized ()) {
		setenv ("GNOME_VFS_HTTP_USER_AGENT", trilobite_get_useragent_string (NULL), 1);

		if (! gnome_vfs_init ()) {
			g_warning ("cannot initialize gnome-vfs!");
			return NULL;
		}
	}

	uri = gnome_vfs_uri_new (uri_text);
	if (uri == NULL) {
		trilobite_debug ("fetch-uri: invalid uri");
		return NULL;
	}

	err = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	if (err != GNOME_VFS_OK) {
		trilobite_debug ("fetch-uri on '%s': open failed: %s", 
				 uri_text, 
				 gnome_vfs_result_to_string (err));
		handle = NULL;
	}

	gnome_vfs_uri_unref (uri);
	return handle;
}

/* fetch a file from an url, using gnome-vfs
 * (using gnome-vfs allows urls of the type "eazel-auth:/etc" to work)
 * generally this will be used to fetch XML files.
 * on success, the body will be null-terminated (this helps work around bugs in libxml,
 * and also makes it easy to manipulate a small body using string operations).
 */
gboolean
trilobite_fetch_uri (const char *uri_text, char **body, int *length)
{
	GnomeVFSResult err;
	GnomeVFSHandle *handle;
	int buffer_size;
	GnomeVFSFileSize bytes;

	handle = trilobite_open_uri (uri_text);
	if (handle == NULL) {
		return FALSE;
	}

	/* start the buffer at a reasonable size */
	buffer_size = 4096;
	*body = g_malloc (buffer_size);
	*length = 0;

	while (1) {
		/* i think this is probably pretty loser: */
		g_main_iteration (FALSE);
		err = gnome_vfs_read (handle, (*body) + (*length), buffer_size - (*length), &bytes);
		if ((bytes == 0) || (err != GNOME_VFS_OK)) {
			break;
		}
		*length += bytes;
		if (*length >= buffer_size - 64) {
			/* expando time! */
			buffer_size *= 4;
			*body = g_realloc (*body, buffer_size);
		}
	}

	/* EOF is now an "error" :) */
	if ((err != GNOME_VFS_OK) && (err != GNOME_VFS_ERROR_EOF)) {
		g_free (*body);
		*body = NULL;
		goto fail;
	}

	(*body)[*length] = 0;
	gnome_vfs_close (handle);
	return TRUE;

fail:
	trilobite_debug ("fetch-uri on %s: %s (%d)", uri_text, gnome_vfs_result_to_string (err), err);
	gnome_vfs_close (handle);
	return FALSE;
}

gboolean
trilobite_fetch_uri_to_file (const char *uri_text, const char *filename)
{
	GnomeVFSResult err;
	GnomeVFSHandle *handle;
	FILE *file;
	char buffer[1024];
	GnomeVFSFileSize bytes;

	file = fopen (filename, "w");
	if (file == NULL) {
		return FALSE;
	}

	handle = trilobite_open_uri (uri_text);
	if (handle == NULL) {
		fclose (file);
		return FALSE;
	}

	while (1) {
		g_main_iteration (FALSE);
		err = gnome_vfs_read (handle, buffer, sizeof(buffer), &bytes);
		if ((bytes == 0) || (err != GNOME_VFS_OK)) {
			break;
		}
		fwrite (buffer, bytes, 1, file);
	}

	gnome_vfs_close (handle);
	fclose (file);

	return (err == GNOME_VFS_OK);
}
