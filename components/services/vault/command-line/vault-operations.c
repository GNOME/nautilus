/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ian McKellar <yakk@yakk.net.au>
 *
 */

#include <config.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include "vault-operations.h"

static GnomeVFSResult vault_list(GList *args, gchar *uri, gboolean debug, gchar **error_context);
static GnomeVFSResult vault_upload(GList *args, gchar *uri, gboolean debug, gchar **error_context);
static GnomeVFSResult vault_download(GList *args, gchar *uri, gboolean debug, gchar **error_context);
static GnomeVFSResult vault_move(GList *args, gchar *uri, gboolean debug, gchar **error_context);
static GnomeVFSResult vault_delete(GList *args, gchar *uri, gboolean debug, gchar **error_context);

struct VaultOperation vault_operations[] = {
	{"list", 	"list [<remote path>]", 			0, 1, vault_list},
	{"ls", 		"ls [<remote path>]", 				0, 1, vault_list},
	{"upload", 	"upload <local path> [<remote path>]", 		1, 2, vault_upload},
	{"download", 	"download <remote path> [<local path>]", 	1, 2, vault_download},
	{"move", 	"move <from> <to>", 				2, 2, vault_move},
	{"mv", 		"mv <from> <to>", 				2, 2, vault_move},
	{"rename", 	"rename <from> <to>", 				2, 2, vault_move},
	{"delete", 	"delete <file>", 				1, 1, vault_delete},
	{NULL, NULL, 0, 0, NULL}
};


static GnomeVFSResult vault_list(GList *args, gchar *uri, gboolean debug, gchar **error_context) {
	GnomeVFSResult result;
	GnomeVFSDirectoryList *list;
	GnomeVFSFileInfo *info;
	gchar *text_uri;

	if(args != NULL) {
		text_uri = g_strconcat(uri, args->data, NULL);
	} else {
		text_uri = uri;
	}

	result = gnome_vfs_directory_list_load (&list, text_uri, (GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE | GNOME_VFS_FILE_INFO_FOLLOW_LINKS), NULL);

	if(result == GNOME_VFS_OK) {

		info = gnome_vfs_directory_list_first(list);

		while( info != NULL ) {
			g_print("%s %10d %20s %s\n", 
					info->type==GNOME_VFS_FILE_TYPE_DIRECTORY?"Folder":"File  ",
					(gint)info->size,
					info->mime_type,
					info->name);

			info = gnome_vfs_directory_list_next(list);
		}
	}

	return result;
}



static gchar *make_local_uri(gchar *fn, gchar *remote) {
	gchar buffer[PATH_MAX+1];

	g_assert(fn || remote);

	if(fn == NULL && remote != NULL) fn = g_basename(remote);
	if(fn[0] != '/') {
		getcwd(buffer, PATH_MAX);
		fn = g_strconcat(buffer, "/", fn, NULL);
	}

	if(fn[strlen(fn)-1] == '/' && remote)
		fn = g_strconcat(fn, g_basename(remote), NULL);

	return  g_strconcat("file://", fn, NULL);
}

static gchar *make_remote_uri(gchar *uri, gchar *fn, gchar *local) {
	if(fn)
		uri = g_strconcat(uri, fn, NULL);

	if(uri[strlen(uri)-1] == '/' && local)
		uri = g_strconcat(uri, g_basename(local), NULL);

	return uri;
}


static GnomeVFSResult vault_upload(GList *args, gchar *uri, gboolean debug, gchar **error_context) {
	GnomeVFSResult result;
	GnomeVFSHandle *whandle, *rhandle;
	gchar *remote_uri;
	gchar *local_uri;
	gchar buffer[1025];

	local_uri = make_local_uri(args->data, NULL);

	args = args->next;

	remote_uri = make_remote_uri(uri, args?args->data:NULL, local_uri);

	g_print("local: %s\n", local_uri);
	g_print("remote: %s\n", remote_uri);

	result = gnome_vfs_open (&rhandle, local_uri, GNOME_VFS_OPEN_READ);
	if(result != GNOME_VFS_OK) return result;

	result = gnome_vfs_open (&whandle, remote_uri, GNOME_VFS_OPEN_WRITE);
	if(result != GNOME_VFS_OK) return result;

	while(result == GNOME_VFS_OK) {
		GnomeVFSFileSize bytes = 1024, bytes_read;

		result = gnome_vfs_read (rhandle, buffer, bytes, &bytes_read);
		if(result != GNOME_VFS_OK) return result;

		if(bytes_read == 0) break;

		g_print("%d=\n", (gint)bytes_read);

		result = gnome_vfs_write (whandle, buffer, bytes_read, &bytes);
		if(result != GNOME_VFS_OK) return result;

		g_print("%d-\n", (gint)bytes);
	}

	result = gnome_vfs_close(rhandle);
	result = gnome_vfs_close(whandle); /* we especially need to do this for HTTP */


	return result;
}


static GnomeVFSResult vault_download(GList *args, gchar *uri, gboolean debug, gchar **error_context) {
	GnomeVFSResult result;
	GnomeVFSHandle *whandle, *rhandle;
	gchar *remote_uri;
	gchar *local_uri;
	gchar buffer[1025];

	remote_uri = make_remote_uri(uri, args->data, NULL);

	args = args->next;

	local_uri = make_local_uri(args?args->data:NULL, remote_uri);

	g_print("local: %s\n", local_uri);
	g_print("remote: %s\n", remote_uri);

	result = gnome_vfs_open (&rhandle, remote_uri, GNOME_VFS_OPEN_READ);
	if(result != GNOME_VFS_OK) {
		*error_context = remote_uri;
		return result;
	}

	result = gnome_vfs_create (&whandle, local_uri, GNOME_VFS_OPEN_WRITE, TRUE, 0600);
	if(result != GNOME_VFS_OK) {
		*error_context = local_uri;
		return result;
	}

	while(result == GNOME_VFS_OK) {
		GnomeVFSFileSize bytes = 1024, bytes_read;

		result = gnome_vfs_read (rhandle, buffer, bytes, &bytes_read);
		if(result != GNOME_VFS_OK) {
			*error_context = "read";
			return result;
		}

		if(bytes_read == 0) break;

		g_print("%d=\n", (gint)bytes_read);

		result = gnome_vfs_write (whandle, buffer, bytes_read, &bytes);
		if(result != GNOME_VFS_OK) {
			*error_context = "write";
			return result;
		}

		g_print("%d-\n", (gint)bytes);
	}

	result = gnome_vfs_close(rhandle);
	result = gnome_vfs_close(whandle); /* we especially need to do this for HTTP */


	return result;
}


static GnomeVFSResult vault_move(GList *args, gchar *uri, gboolean debug, gchar **error_context) {
	GnomeVFSResult result;
	gchar *uri1, *uri2;

	uri1 = make_remote_uri(uri, args->data, NULL);

	args = args->next;

	uri2 = make_remote_uri(uri, args->data, NULL);

	g_print("from: %s\n", uri1);
	g_print("to: %s\n", uri2);

	result = gnome_vfs_move (uri1, uri2, TRUE);
	if(result != GNOME_VFS_OK) {
		*error_context = g_strdup_printf("move %s to %s", uri1, uri2);
		return result;
	}
	return result;
}


static GnomeVFSResult vault_delete(GList *args, gchar *uri, gboolean debug, gchar **error_context) {
	GnomeVFSResult result;
	gchar *uri1;

	uri1 = make_remote_uri(uri, args->data, NULL);

	g_print("deleting: %s\n", uri1);

	result = gnome_vfs_unlink (uri1);
	if(result != GNOME_VFS_OK) {
		*error_context = uri1;
		return result;
	}
	return result;
}
