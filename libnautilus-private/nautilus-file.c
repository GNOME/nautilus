/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-file.c: Nautilus file model.
 
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-file-private.h"

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>

#include <gtk/gtksignal.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-mime-info.h>
#include <libgnome/gnome-mime.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include <parser.h>

#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-string.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-directory-private.h"
#include "nautilus-directory-metafile.h"

typedef enum {
	NAUTILUS_DATE_TYPE_MODIFIED,
	NAUTILUS_DATE_TYPE_CHANGED,
	NAUTILUS_DATE_TYPE_ACCESSED,
	NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED
} NautilusDateType;

#define EMBLEM_NAME_SYMBOLIC_LINK "symbolic-link"
#define EMBLEM_NAME_CANT_READ "noread"
#define EMBLEM_NAME_CANT_WRITE "nowrite"

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void  nautilus_file_initialize_class          (NautilusFileClass    *klass);
static void  nautilus_file_initialize                (NautilusFile         *file);
static void  destroy                                 (GtkObject            *object);
static int   nautilus_file_compare_by_emblems        (NautilusFile         *file_1,
						      NautilusFile         *file_2);
static int   nautilus_file_compare_by_type           (NautilusFile         *file_1,
						      NautilusFile         *file_2);
static int   nautilus_file_compare_for_sort_internal (NautilusFile         *file_1,
						      NautilusFile         *file_2,
						      NautilusFileSortType  sort_type,
						      gboolean              reversed);
static char *nautilus_file_get_date_as_string        (NautilusFile         *file,
						      NautilusDateType      date_type);
static char *nautilus_file_get_owner_as_string       (NautilusFile         *file);
static char *nautilus_file_get_group_as_string       (NautilusFile         *file);
static char *nautilus_file_get_permissions_as_string (NautilusFile         *file);
static char *nautilus_file_get_size_as_string        (NautilusFile         *file);
static char *nautilus_file_get_type_as_string        (NautilusFile         *file);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFile, nautilus_file, GTK_TYPE_OBJECT)

static void
nautilus_file_initialize_class (NautilusFileClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = destroy;

	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusFileClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_file_initialize (NautilusFile *file)
{
	file->details = g_new0 (NautilusFileDetails, 1);
}

static NautilusFile *
nautilus_file_new_from_name (NautilusDirectory *directory,
			     const char *name)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (name[0] != '\0', NULL);

	file = gtk_type_new (NAUTILUS_TYPE_FILE);

	nautilus_file_ref (file);
	gtk_object_sink (GTK_OBJECT (file));

	nautilus_directory_ref (directory);

	file->details->directory = directory;
	file->details->name = g_strdup (name);

	return file;
}

NautilusFile *
nautilus_file_new_from_info (NautilusDirectory *directory,
			     GnomeVFSFileInfo *info)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	file = gtk_type_new (NAUTILUS_TYPE_FILE);

	nautilus_file_ref (file);
	gtk_object_sink (GTK_OBJECT (file));

	gnome_vfs_file_info_ref (info);
	nautilus_directory_ref (directory);

	file->details->directory = directory;
	file->details->info = info;
	file->details->name = info->name;

	return file;
}

/**
 * nautilus_file_get:
 * @uri: URI of file to get.
 *
 * Get a file given a uri.
 * Returns a referenced object. Unref when finished.
 * If two windows are viewing the same uri, the file object is shared.
 */
NautilusFile *
nautilus_file_get (const char *uri)
{
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	char *directory_uri;
	NautilusDirectory *directory;
	char *file_name_escaped, *file_name;
	NautilusFile *file;

	g_return_val_if_fail (uri != NULL, NULL);

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Make VFS version of directory URI. */
	directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
	if (directory_vfs_uri == NULL) {
		/* Use the directory itself if we have no parent. */
		gnome_vfs_uri_ref (vfs_uri);
		directory_vfs_uri = vfs_uri;
	}

	/* Make text version of directory URI. */
	directory_uri = gnome_vfs_uri_to_string (directory_vfs_uri,
						 GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (directory_vfs_uri);

	/* Get object that represents the directory. */
	directory = nautilus_directory_get (directory_uri);
	g_free (directory_uri);
	if (directory == NULL) {
		gnome_vfs_uri_unref (vfs_uri);
		return NULL;
	}

	/* Check to see if it's a file that's already known. */
	file_name_escaped = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	file_name = gnome_vfs_unescape_string (file_name_escaped, NULL);
	g_free (file_name_escaped);
	if (file_name == NULL) {
		return NULL;
	}
	file = nautilus_directory_find_file (directory, file_name);
	if (file != NULL) {
		nautilus_file_ref (file);
	} else {
		file = nautilus_file_new_from_name (directory, file_name);
		directory->details->files =
			g_list_prepend (directory->details->files, file);
	}
	g_free (file_name);

	nautilus_directory_unref (directory);
	
	return file;
}

static void
destroy (GtkObject *object)
{
	NautilusFile *file;
	GList **files;

	file = NAUTILUS_FILE (object);

	nautilus_async_destroying_file (file);
	
	files = &file->details->directory->details->files;
	if (file->details->is_gone) {
		g_assert (g_list_find (*files, file) == NULL);
	} else {
		g_assert (g_list_find (*files, file) != NULL);
		*files = g_list_remove (*files, file);
	}

	nautilus_directory_unref (file->details->directory);

	g_free (file->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusFile *
nautilus_file_ref (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	gtk_object_ref (GTK_OBJECT (file));
	return file;
}

void
nautilus_file_unref (NautilusFile *file)
{
	if (file == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	gtk_object_unref (GTK_OBJECT (file));
}

/**
 * nautilus_file_get_parent_uri_as_string:
 * 
 * Get the uri for the parent directory.
 * 
 * @file: The file in question.
 * 
 * Return value: A string representing the parent's location.
 * If the parent is NULL, returns the empty string.
 */
static char *
nautilus_file_get_parent_uri_as_string (NautilusFile *file) 
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (file->details->directory == NULL) {
		/* Callers expect an empty string, not a NULL. */
		return "";
	}

	return nautilus_directory_get_uri (file->details->directory);
}

static NautilusFile *
get_file_for_parent_directory (NautilusFile *file)
{
	char *parent_uri;
	NautilusFile *result;

	g_assert (NAUTILUS_IS_FILE (file));
	
	if (file->details->directory == NULL) {
		return NULL;
	}

	parent_uri = nautilus_directory_get_uri (file->details->directory);
	result = nautilus_file_get (parent_uri);
	g_free (parent_uri);

	return result;
}

/**
 * nautilus_file_denies_access_permission:
 * 
 * Check whether the current file does not have a given permission
 * for the current user. The sense is negative because the function
 * returns FALSE if permissions cannot be determined.
 * 
 * @file: The file to check.
 * @owner_permission: The USER version of the permission (e.g. GNOME_VFS_PERM_USER_READ).
 * @group_permission: The GROUP version of the permission (e.g. GNOME_VFS_PERM_GROUP_READ).
 * @other_permission: The OTHER version of the permission (e.g. GNOME_VFS_PERM_OTHER_READ).
 * 
 * Return value: TRUE if the current user definitely does not have
 * the specified permission. FALSE if the current user does have
 * permission, or if the permissions themselves are not queryable.
 */
static gboolean
nautilus_file_denies_access_permission (NautilusFile *file, 
				        GnomeVFSFilePermissions owner_permission,
				        GnomeVFSFilePermissions group_permission,
				        GnomeVFSFilePermissions other_permission)
{
	uid_t user_id;
	struct passwd *password_info;

	g_assert (NAUTILUS_IS_FILE (file));

	/* Once the file is gone, you are denied permission to do anything. */
	if (nautilus_file_is_gone (file)) {
		return TRUE;
	}

	/* File system does not provide permission bits.
	 * Can't determine specific permissions, do not deny permission at all.
	 */
	if (!nautilus_file_can_get_permissions (file)) {
		return FALSE;
	}

	/* Check the user. */
	user_id = geteuid ();

	/* Root is not forbidden to do anything. */
	if (user_id == 0) {
		return FALSE;
	}

	/* File owner's access is governed by the owner bits. */
	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (user_id == file->details->info->uid) {
		return (file->details->info->permissions & owner_permission) == 0;
	}

	/* No need to free result of getpwuid. */
	password_info = getpwuid (user_id);

	/* Group member's access is governed by the group bits. */
	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (password_info != NULL
	    && password_info->pw_gid == file->details->info->gid) {
		return (file->details->info->permissions & group_permission) == 0;
	}

	/* Other users' access is governed by the other bits. */
	return (file->details->info->permissions & other_permission) == 0;
}

/**
 * nautilus_file_can_read:
 * 
 * Check whether the user is allowed to read the contents of this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to read
 * the contents of the file. If the user has read permission, or
 * the code can't tell whether the user has read permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_read (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return !nautilus_file_denies_access_permission
		(file,
		 GNOME_VFS_PERM_USER_READ,
		 GNOME_VFS_PERM_GROUP_READ,
		 GNOME_VFS_PERM_OTHER_READ);
}

/**
 * nautilus_file_can_write:
 * 
 * Check whether the user is allowed to write to this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to write
 * to the file. If the user has write permission, or
 * the code can't tell whether the user has write permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_write (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return !nautilus_file_denies_access_permission
		(file, 
		 GNOME_VFS_PERM_USER_WRITE,
		 GNOME_VFS_PERM_GROUP_WRITE,
		 GNOME_VFS_PERM_OTHER_WRITE);
}

/**
 * nautilus_file_can_execute:
 * 
 * Check whether the user is allowed to execute this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to execute
 * the file. If the user has execute permission, or
 * the code can't tell whether the user has execute permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_execute (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return !nautilus_file_denies_access_permission
		(file, 
		 GNOME_VFS_PERM_USER_EXEC,
		 GNOME_VFS_PERM_GROUP_EXEC,
		 GNOME_VFS_PERM_OTHER_EXEC);
}

/**
 * nautilus_file_can_rename:
 * 
 * Check whether the user is allowed to change the name of the file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to change
 * the name of the file. If the user is allowed to change the name, or
 * the code can't tell whether the user is allowed to change the name,
 * returns TRUE (so rename failures must always be handled).
 */
gboolean
nautilus_file_can_rename (NautilusFile *file)
{
	NautilusFile *parent;
	gboolean result;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Nonexistent files can't be renamed. */
	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	/* User must have write permissions for the parent directory. */
	parent = get_file_for_parent_directory (file);

	/* 
	 * No parent directory for some reason (at root level?).
	 * Can't tell whether this file is renameable, so return TRUE.
	 */
	if (parent == NULL) {
		return TRUE;
	}

	result = nautilus_file_can_write (parent);

	nautilus_file_unref (parent);

	return result;
}

typedef struct {
	NautilusFile *file;
	GnomeVFSAsyncHandle *handle;
	NautilusFileOperationCallback callback;
	gpointer callback_data;

	/* Operation-specific data. */
	char *new_name; /* rename */
} FileOperationState;

static FileOperationState *
file_operation_state_new (NautilusFile *file,
			  NautilusFileOperationCallback callback,
			  gpointer callback_data)
{
	FileOperationState *state;

	nautilus_file_ref (file);

	state = g_new0 (FileOperationState, 1);
	state->file = file;
	state->callback = callback;
	state->callback_data = callback_data;

	return state;
}

static void
file_operation_state_free (FileOperationState *state)
{
	nautilus_file_unref (state->file);
	g_free (state->new_name);
	g_free (state);
}

static void
file_operation_state_complete (FileOperationState *state,
			       GnomeVFSResult result)
{
	/* Claim that something changed even if the operation failed.
	 * This makes it easier for some clients who see the "reverting"
	 * as "changing back".
	 */
	nautilus_file_changed (state->file);
	(* state->callback) (state->file, result, state->callback_data);
	file_operation_state_free (state);
}

static void
rename_update_info_and_metafile (FileOperationState *state)
{
	nautilus_directory_update_file_metadata
		(state->file->details->directory,
		 state->file->details->name,
		 state->new_name);
	
	g_free (state->file->details->name);
	state->file->details->name = g_strdup (state->new_name);
	if (state->file->details->info != NULL) {
		state->file->details->info->name = state->file->details->name;
	}
}

static int
rename_callback (GnomeVFSAsyncHandle *handle,
		 GnomeVFSXferProgressInfo *info,
		 gpointer callback_data)
{
	FileOperationState *state;

	state = callback_data;
	g_assert (handle == state->handle);
	g_assert (info != NULL);

	/* We aren't really interested in progress, but we do need to see
	 * when the transfer is done or fails.
	 */
	switch (info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
			/* Here's the case where we are done renaming. */
			if (info->vfs_status == GNOME_VFS_OK) {
				rename_update_info_and_metafile (state);
			}
			file_operation_state_complete (state, info->vfs_status);
		}
		break;
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		/* We have to handle this case because if you pass
		 * GNOME_VFS_ERROR_MODE_ABORT, you never get the
		 * error code for a failed rename.
		 * FIXME bugzilla.eazel.com 912: I believe this
		 * represents a bug in GNOME VFS.
		 */
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	default:
		break;
	}

	/* FIXME bugzilla.eazel.com 886: Pavel says I should return
	 * this, but he promises he will fix the API.
	 */
	return 1;
}

void
nautilus_file_rename (NautilusFile *file,
		      const char *new_name,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	char *directory_uri_text;
	GList *source_name_list, *target_name_list;
	GnomeVFSResult result;
	FileOperationState *state;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (new_name != NULL);
	g_return_if_fail (callback != NULL);
	
	/* FIXME bugzilla.eazel.com 645: 
	 * Make sure this returns an error for incoming names 
	 * containing path separators.
	 */

	/* Can't rename a file that's already gone.
	 * We need to check this here because there may be a new
	 * file with the same name.
	 */
	if (nautilus_file_is_gone (file)) {
	       	/* Claim that something changed even if the rename failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old name as "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_NOT_FOUND, callback_data);
		return;
	}

	/* Test the name-hasn't-changed case explicitly, for two reasons.
	 * (1) gnome_vfs_move returns an error if new & old are same.
	 * (2) We don't want to send file-changed signal if nothing changed.
	 */
	if (strcmp (new_name, file->details->name) == 0) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	state = file_operation_state_new (file, callback, callback_data);
	state->new_name = g_strdup (new_name);

	directory_uri_text = nautilus_directory_get_uri (file->details->directory);
	source_name_list = g_list_prepend (NULL, file->details->name);
	target_name_list = g_list_prepend (NULL, (char *) new_name);
	result = gnome_vfs_async_xfer
		(&state->handle,
		 directory_uri_text, source_name_list,
		 directory_uri_text, target_name_list,
		 GNOME_VFS_XFER_SAMEFS | GNOME_VFS_XFER_REMOVESOURCE,
		 GNOME_VFS_XFER_ERROR_MODE_QUERY,
		 GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
		 rename_callback, state,
		 NULL, NULL);
	g_free (directory_uri_text);
	g_list_free (source_name_list);
	g_list_free (target_name_list);

	if (result != GNOME_VFS_OK) {
		file_operation_state_free (state);

	       	/* Claim that something changed even if the rename failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old name as "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, result, callback_data);
		return;
	}
}

static GnomeVFSURI *
nautilus_file_get_gnome_vfs_uri (NautilusFile *file)
{
	if (file->details->name[0] == '/') {
		gnome_vfs_uri_ref (file->details->directory->details->uri);
		return file->details->directory->details->uri;
	}

	return gnome_vfs_uri_append_file_name (file->details->directory->details->uri,
					       file->details->name);
}

gboolean         
nautilus_file_matches_uri (NautilusFile *file, const char *uri_string)
{
	GnomeVFSURI *match_uri;
	GnomeVFSURI *file_uri;
	gboolean result;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (uri_string != NULL, FALSE);

	match_uri = gnome_vfs_uri_new (uri_string);
	if (match_uri == NULL) {
		return FALSE;
	}

	file_uri = nautilus_file_get_gnome_vfs_uri (file);
	result = gnome_vfs_uri_equal (file_uri, match_uri);
	gnome_vfs_uri_unref (file_uri);
	gnome_vfs_uri_unref (match_uri);

	return result;
}

gboolean
nautilus_file_is_local(NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return(nautilus_directory_is_local(file->details->directory));
}

gboolean
nautilus_file_update_info (NautilusFile *file, GnomeVFSFileInfo *info)
{
	if (file->details->is_gone) {
		return FALSE;
	}

	if (info == NULL) {
		nautilus_file_mark_gone (file);
		return TRUE;
	}

	if (file->details->info != NULL
	    && gnome_vfs_file_info_matches (file->details->info, info)) {
		return FALSE;
	}

	gnome_vfs_file_info_ref (info);

	if (file->details->info == NULL) {
		g_free (file->details->name);
	} else {
		gnome_vfs_file_info_unref (file->details->info);
	}

	file->details->info = info;
	file->details->name = info->name;

	return TRUE;
}

gboolean
nautilus_file_update_name (NautilusFile *file, const char *name)
{
	GnomeVFSFileInfo *info;

	g_assert (name != NULL);

	if (file->details->is_gone) {
		return FALSE;
	}

	/* Make a copy and update the file name in the copy. */
	if (strcmp (file->details->name, name) == 0) {
		return FALSE;
	}

	if (file->details->info == NULL) {
		g_free (file->details->name);
		file->details->name = g_strdup (name);
	} else {
		info = gnome_vfs_file_info_new ();
		gnome_vfs_file_info_copy (info, file->details->info);
		g_free (info->name);
		info->name = g_strdup (name);
		nautilus_file_update_info (file, info);
		gnome_vfs_file_info_unref (info);
	}

	return TRUE;
}

static int
nautilus_file_compare_directories_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with unkown # of items
	 *   Directories with 0 items
	 *   Directories with n items
	 *   All files.
	 * The files are sorted by size in a separate pass.
	 */
	gboolean is_directory_1, is_directory_2;
	gboolean count_known_1, count_known_2;
	gboolean count_unreadable_1, count_unreadable_2;
	guint item_count_1, item_count_2;

	is_directory_1 = nautilus_file_is_directory (file_1);
	is_directory_2 = nautilus_file_is_directory (file_2);

	if (is_directory_1 && !is_directory_2) {
		return -1;
	}
	if (is_directory_2 && !is_directory_1) {
		return +1;
	}

	if (!is_directory_1 && !is_directory_2) {
		return 0;
	}

	/* Both are directories, compare by item count. */

	count_known_1 = nautilus_file_get_directory_item_count (file_1,
								&item_count_1,
								&count_unreadable_1);
	count_known_2 = nautilus_file_get_directory_item_count (file_2,
								&item_count_2,
								&count_unreadable_2);

	if (!count_known_1 && count_known_2) {
		return -1;
	}
	if (count_known_1 && !count_known_2) {
		return +1;
	}

	if (!count_known_1 && !count_known_2) {
		/* Put unknowable before simply unknown. */
		if (count_unreadable_1 && !count_unreadable_2) {
			return -1;
		}

		if (!count_unreadable_1 && count_unreadable_2) {
			return +1;
		}

		return 0;
	}

	if (item_count_1 < item_count_2) {
		return -1;
	}
	if (item_count_2 < item_count_1) {
		return +1;
	}

	return 0;
}

/**
 * compare_emblem_names
 * 
 * Compare two emblem names by canonical order. Canonical order
 * is alphabetical, except the symbolic link name goes first. NULL
 * is allowed, and goes last.
 * @name_1: The first emblem name.
 * @name_2: The second emblem name.
 * 
 * Return value: 0 if names are equal, -1 if @name_1 should be
 * first, +1 if @name_2 should be first.
 */
static int
compare_emblem_names (const char *name_1, const char *name_2)
{
	int strcmp_result;

	strcmp_result = nautilus_strcmp (name_1, name_2);

	if (strcmp_result == 0) {
		return 0;
	}

	if (nautilus_strcmp (name_1, EMBLEM_NAME_SYMBOLIC_LINK) == 0) {
		return -1;
	}

	if (nautilus_strcmp (name_2, EMBLEM_NAME_SYMBOLIC_LINK) == 0) {
		return +1;
	}

	return strcmp_result;
}

static int
nautilus_file_compare_by_emblems (NautilusFile *file_1, NautilusFile *file_2)
{
	GList *emblem_names_1;
	GList *emblem_names_2;
	GList *p1;
	GList *p2;
	int compare_result;

	compare_result = 0;

	emblem_names_1 = nautilus_file_get_emblem_names (file_1);
	emblem_names_2 = nautilus_file_get_emblem_names (file_2);

	p1 = emblem_names_1;
	p2 = emblem_names_2;
	while (p1 != NULL && p2 != NULL) {
		compare_result = compare_emblem_names (p1->data, p2->data);
		if (compare_result != 0) {
			break;
		}

		p1 = p1->next;
		p2 = p2->next;
	}

	if (compare_result == 0) {
		/* One or both is now NULL. */
		if (p1 != NULL || p2 != NULL) {
			compare_result = p2 == NULL ? -1 : +1;
		}
	}

	nautilus_g_list_free_deep (emblem_names_1);
	nautilus_g_list_free_deep (emblem_names_2);

	return compare_result;	
}

static int
nautilus_file_compare_by_type (NautilusFile *file_1, NautilusFile *file_2)
{
	gboolean is_directory_1;
	gboolean is_directory_2;
	char *type_string_1;
	char *type_string_2;
	int result;

	/* Directories go first. Then, if mime types are identical,
	 * don't bother getting strings (for speed). This assumes
	 * that the string is dependent entirely on the mime type,
	 * which is true now but might not be later.
	 */
	is_directory_1 = nautilus_file_is_directory (file_1);
	is_directory_2 = nautilus_file_is_directory (file_2);
	
	if (is_directory_1 && is_directory_2) {
		return 0;
	}

	if (is_directory_1) {
		return -1;
	}

	if (is_directory_2) {
		return +1;
	}

	if (file_1->details->info != NULL
	    && file_2->details->info != NULL
	    && nautilus_strcmp (file_1->details->info->mime_type,
				file_2->details->info->mime_type) == 0) {
		return 0;
	}

	type_string_1 = nautilus_file_get_type_as_string (file_1);
	type_string_2 = nautilus_file_get_type_as_string (file_2);

	result = nautilus_strcmp (type_string_1, type_string_2);

	g_free (type_string_1);
	g_free (type_string_2);

	return result;
}

static int
nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
					 NautilusFile *file_2,
					 NautilusFileSortType sort_type,
					 gboolean reversed)
{
	GnomeVFSDirectorySortRule rules[3];
	int compare;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file_1), 0);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file_2), 0);

	switch (sort_type) {
	case NAUTILUS_FILE_SORT_BY_NAME:
		/* Note: This used to put directories first. I
		 * thought that was counterintuitive and removed it,
		 * but I can imagine discussing this further.
		 * John Sullivan <sullivan@eazel.com>
		 */
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_SIZE:
		/* Compare directory sizes ourselves, then if necessary
		 * use GnomeVFS to compare file sizes.
		 */
		compare = nautilus_file_compare_directories_by_size (file_1, file_2);
		if (compare != 0) {
			return compare;
		}
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYSIZE;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_TYPE:
		/* GnomeVFS doesn't know about our special text for certain
		 * mime types, so we handle the mime-type sorting ourselves.
		 */
		compare = nautilus_file_compare_by_type (file_1, file_2);
		if (compare != 0) {
			return compare;
		}
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_MTIME:
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYMTIME;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_EMBLEMS:
		/* GnomeVFS doesn't know squat about our emblems, so
		 * we handle comparing them here, before falling back
		 * to tie-breakers.
		 */
		compare = nautilus_file_compare_by_emblems (file_1, file_2);
		if (compare != 0) {
			return compare;
		}
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	default:
		g_return_val_if_fail (FALSE, 0);
	}

	if (file_1->details->info == NULL) {
		if (file_2->details->info == NULL) {
			compare = g_strcasecmp (file_1->details->name,
						file_2->details->name);
		} else {
			/* FIXME: We do have a name for file 2 to
                         * compare with, so we can probably do better
                         * than this for all cases.
			 */
			compare = -1;
		}
	} else if (file_2->details->info == NULL) {
		/* FIXME: We do have a name for file 1 to compare
                 * with, so we can probably do better than this for
		 * all cases.
		 */
		compare = +1;
	} else {
		compare = gnome_vfs_file_info_compare_for_sort
			(file_1->details->info, file_2->details->info, rules);
	}

	if (reversed) {
		compare = -compare;
	}

	return compare;
}

/**
 * nautilus_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * 
 * Return value: int < 0 if @file_1 should come before file_2 in a smallest-to-largest
 * sorted list; int > 0 if @file_2 should come before file_1 in a smallest-to-largest
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort (NautilusFile *file_1,
				NautilusFile *file_2,
				NautilusFileSortType sort_type)
{
	return nautilus_file_compare_for_sort_internal (file_1, file_2, sort_type, FALSE);
}

/**
 * nautilus_file_compare_for_sort_reversed:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * 
 * Return value: The opposite of nautilus_file_compare_for_sort: int > 0 if @file_1 
 * should come before file_2 in a smallest-to-largest sorted list; int < 0 if @file_2 
 * should come before file_1 in a smallest-to-largest sorted list; 0 if @file_1 
 * and @file_2 are equal for this sort criterion. Note that each named sort type 
 * may actually break ties several ways, with the name of the sort criterion 
 * being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort_reversed (NautilusFile *file_1,
					 NautilusFile *file_2,
					 NautilusFileSortType sort_type)
{
	return nautilus_file_compare_for_sort_internal (file_1, file_2, sort_type, TRUE);
}

/**
 * nautilus_file_compare_name:
 * @file: A file object
 * @pattern: A string we are comparing it with
 * 
 * Return value: result of a case-insensitive comparison of the file
 * name and the given pattern.
 **/
int
nautilus_file_compare_name (NautilusFile *file,
			    const char *pattern)
{
	return g_strcasecmp (file->details->name, pattern);
}

char *
nautilus_file_get_metadata (NautilusFile *file,
			    const char *key,
			    const char *default_metadata)
{
	g_return_val_if_fail (key != NULL, g_strdup (default_metadata));
	g_return_val_if_fail (key[0] != '\0', g_strdup (default_metadata));
	if (file == NULL) {
		return g_strdup (default_metadata);
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), g_strdup (default_metadata));

	return nautilus_directory_get_file_metadata
		(file->details->directory,
		 file->details->name,
		 key,
		 default_metadata);
}

GList *
nautilus_file_get_metadata_list (NautilusFile *file,
				 const char *list_key,
				 const char *list_subkey)
{
	g_return_val_if_fail (list_key != NULL, NULL);
	g_return_val_if_fail (list_key[0] != '\0', NULL);
	g_return_val_if_fail (list_subkey != NULL, NULL);
	g_return_val_if_fail (list_subkey[0] != '\0', NULL);
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	return nautilus_directory_get_file_metadata_list
		(file->details->directory,
		 file->details->name,
		 list_key,
		 list_subkey);
}

void
nautilus_file_set_metadata (NautilusFile *file,
			    const char *key,
			    const char *default_metadata,
			    const char *metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	if (nautilus_directory_set_file_metadata (file->details->directory,
						  file->details->name,
						  key,
						  default_metadata,
						  metadata)) {
		nautilus_file_changed (file);
	}
}

void
nautilus_file_set_metadata_list (NautilusFile *file,
				 const char *list_key,
				 const char *list_subkey,
				 GList *list)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (list_key != NULL);
	g_return_if_fail (list_key[0] != '\0');
	g_return_if_fail (list_subkey != NULL);
	g_return_if_fail (list_subkey[0] != '\0');

	if (nautilus_directory_set_file_metadata_list (file->details->directory,
						       file->details->name,
						       list_key,
						       list_subkey,
						       list)) {
		nautilus_file_changed (file);
	}
}

char *
nautilus_file_get_name (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	return g_strdup (file->details->name);
}
   
void             
nautilus_file_monitor_add (NautilusFile *file,
			   gconstpointer client,
			   GList *attributes,
			   gboolean monitor_metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	nautilus_directory_monitor_add_internal
		(file->details->directory, file,
		 client,
		 attributes, monitor_metadata,
		 NULL, NULL);
}   
			   
void
nautilus_file_monitor_remove (NautilusFile *file,
			      gconstpointer client)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	nautilus_directory_monitor_remove_internal
		(file->details->directory, file, client);
}			      


/* return the uri associated with the passed-in file, which may not be the actual uri if
   the file is an old-style gmc link or a nautilus xml file */
char *
nautilus_file_get_mapped_uri (NautilusFile *file)
{
	char* actual_uri;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char buffer[512];
	GnomeVFSFileSize bytes_read;

	/* first get the actual uri */
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
		
	actual_uri = nautilus_file_get_uri (file);
	if (actual_uri == NULL) {
		return NULL;
	}
		
	/* FIXME bugzilla.eazel.com 911: Need to use async. I/O. */
	/* see if it's a gmc style URI by reading the first part of the file */
	result = gnome_vfs_open (&handle, actual_uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_OK) {
		result = gnome_vfs_read (handle, buffer, sizeof (buffer), &bytes_read);
		if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {
			if (nautilus_str_has_prefix (buffer, "URL: ")) {
				char *eol = strchr (buffer, '\n');
				if (eol)
					*eol = '\0';
				if (strlen (buffer) <= bytes_read) {
					g_free (actual_uri);
					actual_uri = g_strdup (buffer + 5);
				}
			}
		}
		gnome_vfs_close (handle);
	}
	
	/* see if it's a nautilus link xml file - if so, open and parse the file to fetch the uri */
	if (nautilus_link_is_link_file (actual_uri)) {
		char *old_uri = actual_uri;
		actual_uri = nautilus_link_get_link_uri (actual_uri);
		g_free (old_uri);
	}
		
	/* all done so return the result */
	return actual_uri;
}

/* Return the actual uri associated with the passed-in file. */
char *
nautilus_file_get_uri (NautilusFile *file)
{
	GnomeVFSURI *uri;
	char *uri_text;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	uri = nautilus_file_get_gnome_vfs_uri (file);
	uri_text = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (uri);
	return uri_text;
}

static gboolean
info_missing (NautilusFile *file, GnomeVFSFileInfoFields needed_mask)
{
	GnomeVFSFileInfo *info;

	if (file == NULL) {
		return TRUE;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), TRUE);
	info = file->details->info;
	if (info == NULL) {
		return TRUE;
	}
	return (info->valid_fields & needed_mask) != needed_mask;
}

/**
 * nautilus_file_get_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date. 
 * The caller is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_date_as_string (NautilusFile *file, NautilusDateType date_type)
{	 
	struct tm *file_time;
	const char *format;
	GDate *today;
	GDate *file_date;
	guint32 file_date_age;

	g_return_val_if_fail (date_type == NAUTILUS_DATE_TYPE_CHANGED
			      || date_type == NAUTILUS_DATE_TYPE_ACCESSED
			      || date_type == NAUTILUS_DATE_TYPE_MODIFIED
			      || date_type == NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED, NULL);

	switch (date_type) {
	case NAUTILUS_DATE_TYPE_CHANGED:
		/* Before we have info on a file, the date is unknown. */
		if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_CTIME)) {
			return NULL;
		}
		file_time = localtime (&file->details->info->ctime);
		break;
	case NAUTILUS_DATE_TYPE_ACCESSED:
		/* Before we have info on a file, the date is unknown. */
		if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_ATIME)) {
			return NULL;
		}
		file_time = localtime (&file->details->info->atime);
		break;
	case NAUTILUS_DATE_TYPE_MODIFIED:
		/* Before we have info on a file, the date is unknown. */
		if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_MTIME)) {
			return NULL;
		}
		file_time = localtime (&file->details->info->mtime);
		break;
	case NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED:
		/* Before we have info on a file, the date is unknown. */
		if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_MTIME) ||
		    info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_CTIME)) {
			return NULL;
		}
		/* mtime is when the contents changed; ctime is when the
		 * contents or the permissions (inc. owner/group) changed.
		 * So we can only know when the permissions changed if mtime
		 * and ctime are different.
		 */
		if (file->details->info->mtime == file->details->info->ctime) {
			return NULL;
		}
		
		file_time = localtime (&file->details->info->ctime);
		break;
	default:
		g_assert_not_reached ();
	}

	file_date = nautilus_g_date_new_tm (file_time);
	
	today = g_date_new ();
	g_date_set_time (today, time (NULL));

	/* Overflow results in a large number; fine for our purposes. */
	file_date_age = g_date_julian (today) - g_date_julian (file_date);

	g_date_free (file_date);
	g_date_free (today);

	/* Format varies depending on how old the date is. This minimizes
	 * the length (and thus clutter & complication) of typical dates
	 * while providing sufficient detail for recent dates to make
	 * them maximally understandable at a glance. Keep all format
	 * strings separate rather than combining bits & pieces for
	 * internationalization's sake.
	 */

	if (file_date_age == 0)	{
		/* today, use special word */
		format = _("today %-I:%M %p");
	} else if (file_date_age == 1) {
		/* yesterday, use special word */
		format = _("yesterday %-I:%M %p");
	} else if (file_date_age < 7) {
		/* current week, include day of week */
		format = _("%A %-m/%-d/%y %-I:%M %p");
	} else {
		format = _("%-m/%-d/%y %-I:%M %p");
	}

	return nautilus_strdup_strftime (format, file_time);
}

/**
 * nautilus_file_get_directory_item_count
 * 
 * Get the number of items in a directory.
 * @file: NautilusFile representing a directory. It is an error to
 * call this function on a file that is not a directory.
 * @count: Place to put count.
 * @count_unreadable: Set to TRUE (if non-NULL) if permissions prevent
 * the item count from being read on this directory. Otherwise set to FALSE.
 * 
 * Returns: TRUE if count is available.
 * 
 **/
gboolean
nautilus_file_get_directory_item_count (NautilusFile *file, 
					guint *count,
					gboolean *count_unreadable)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (nautilus_file_is_directory (file), FALSE);
	g_return_val_if_fail (count != NULL, FALSE);

	if (count_unreadable != NULL) {
		*count_unreadable = file->details->directory_count_failed;
	}

	if (!file->details->got_directory_count) {
		return FALSE;
	}

	*count = file->details->directory_count;
	return TRUE;
}

/**
 * nautilus_file_get_deep_counts
 * 
 * Get the statistics about items inside a directory.
 * @file: NautilusFile representing a directory or file.
 * @directory_count: Place to put count of directories inside.
 * @files_count: Place to put count of files inside.
 * @unreadable_directory_count: Number of directories encountered
 * that were unreadable.
 * @total_size: Total size of all files and directories visited.
 * 
 * Returns: Status to indicate whether sizes are available.
 * 
 **/
NautilusRequestStatus
nautilus_file_get_deep_counts (NautilusFile *file,
			       guint *directory_count,
			       guint *file_count,
			       guint *unreadable_directory_count,
			       GnomeVFSFileSize *total_size)
{
	GnomeVFSFileType type;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (nautilus_file_is_directory (file), FALSE);

	if (file->details->deep_counts_status != NAUTILUS_REQUEST_NOT_STARTED) {
		if (directory_count != NULL) {
			*directory_count = file->details->deep_directory_count;
		}
		if (file_count != NULL) {
			*file_count = file->details->deep_file_count;
		}
		if (unreadable_directory_count != NULL) {
			*unreadable_directory_count = file->details->deep_unreadable_count;
		}
		if (total_size != NULL) {
			*total_size = file->details->deep_size;
		}
		return file->details->deep_counts_status;
	}

	if (directory_count != NULL) {
		*directory_count = 0;
	}
	if (file_count != NULL) {
		*file_count = 0;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		*total_size = 0;
	}

	/* For directories, or before we know the type, we haven't started. */
	type = nautilus_file_get_file_type (file);
	if (type == GNOME_VFS_FILE_TYPE_UNKNOWN
	    || type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		return NAUTILUS_REQUEST_NOT_STARTED;
	}

	/* For other types, we are done, and the zeros are permanent. */
	return NAUTILUS_REQUEST_DONE;
}

void
nautilus_file_recompute_deep_counts (NautilusFile *file)
{
	if (file->details->deep_counts_status != NAUTILUS_REQUEST_IN_PROGRESS) {
		file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
		if (file->details->directory != NULL) {
			nautilus_directory_async_state_changed (file->details->directory);
		}
	}
}

/**
 * nautilus_file_get_size
 * 
 * Get the file size.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Size in bytes.
 * 
 **/
GnomeVFSFileSize
nautilus_file_get_size (NautilusFile *file)
{
	/* Before we have info on the file, we don't know the size. */
	return info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SIZE)
		? 0 : file->details->info->size;
}

/**
 * nautilus_file_can_get_permissions:
 * 
 * Check whether the permissions for a file are determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the permissions are valid.
 */
gboolean
nautilus_file_can_get_permissions (NautilusFile *file)
{
	return !info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS);
}

/**
 * nautilus_file_can_set_permissions:
 * 
 * Check whether the current user is allowed to change
 * the permissions of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * permissions of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_permissions (NautilusFile *file)
{
	uid_t user_id;

	/* Not allowed to set the permissions if we can't
	 * even read them. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nautilus_file_can_get_permissions (file)) {
		return FALSE;
	}

	/* Check the user. */
	user_id = geteuid();

	/* Owner is allowed to set permissions. */
	if (user_id == file->details->info->uid) {
		return TRUE;
	}

	/* Root is also allowed to set permissions. */
	if (user_id == 0) {
		return TRUE;
	}

	/* Nobody else is allowed. */
	return FALSE;
}

GnomeVFSFilePermissions
nautilus_file_get_permissions (NautilusFile *file)
{
	g_return_val_if_fail (nautilus_file_can_get_permissions (file), 0);

	return file->details->info->permissions;
}

/**
 * nautilus_file_set_permissions:
 * 
 * Change a file's permissions. This should only be called if
 * nautilus_file_can_set_permissions returned TRUE.
 * 
 * @file: NautilusFile representing the file in question.
 * @new_permissions: New permissions value. This is the whole
 * set of permissions, not a delta.
 * 
 * Returns: GnomeVFSResult reporting the success or failure of
 * trying to change the file's permissions.
 * 
 **/
void
nautilus_file_set_permissions (NautilusFile *file, 
			       GnomeVFSFilePermissions new_permissions,
			       NautilusFileOperationCallback callback,
			       gpointer callback_data)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *partial_file_info;
	char *uri;

	if (!nautilus_file_can_set_permissions (file)) {
		/* Claim that something changed even if the permission change failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old permissions as "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_ACCESS_DENIED, callback_data);
		return;
	}
			       
	if (new_permissions == file->details->info->permissions) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* Change the file-on-disk permissions. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->permissions = new_permissions;
	uri = nautilus_file_get_uri (file);
	result = gnome_vfs_set_file_info (uri, partial_file_info, 
				 	  GNOME_VFS_SET_FILE_INFO_PERMISSIONS);
	gnome_vfs_file_info_unref (partial_file_info);
	g_free (uri);

	/* Update the permissions in our NautilusFile object. */
	if (result == GNOME_VFS_OK) {
		file->details->info->permissions = new_permissions;
	}

	/* Claim that something changed even if the permission change failed.
	 * This makes it easier for some clients who see the "reverting"
	 * to the old permissions as "changing back".
	 */
	nautilus_file_changed (file);
	(* callback) (file, result, callback_data);
}

/**
 * nautilus_file_get_octal_permissions_as_string:
 * 
 * Get a user-displayable string representing a file's permissions
 * as an octal number. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_octal_permissions_as_string (NautilusFile *file)
{
	GnomeVFSFilePermissions permissions;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!nautilus_file_can_get_permissions (file)) {
		return NULL;
	}

	permissions = file->details->info->permissions;
	return g_strdup_printf ("%03o", permissions);
}

/**
 * nautilus_file_get_permissions_as_string:
 * 
 * Get a user-displayable string representing a file's permissions. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_permissions_as_string (NautilusFile *file)
{
	GnomeVFSFilePermissions permissions;
	gboolean is_directory;
	gboolean is_link;
	gboolean suid, sgid, sticky;

	if (!nautilus_file_can_get_permissions (file)) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	permissions = file->details->info->permissions;
	is_directory = nautilus_file_is_directory (file);
	is_link = GNOME_VFS_FILE_INFO_SYMLINK (file->details->info);

	/* We use ls conventions for displaying these three obscure flags */
	suid = permissions & GNOME_VFS_PERM_SUID;
	sgid = permissions & GNOME_VFS_PERM_SGID;
	sticky = permissions & GNOME_VFS_PERM_STICKY;

	return g_strdup_printf ("%c%c%c%c%c%c%c%c%c%c",
				 is_link ? 'l' : is_directory ? 'd' : '-',
		 		 permissions & GNOME_VFS_PERM_USER_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_USER_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_USER_EXEC 
				 	? (suid ? 's' : 'x') 
				 	: (suid ? 'S' : '-'),
				 permissions & GNOME_VFS_PERM_GROUP_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_GROUP_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_GROUP_EXEC
				 	? (sgid ? 's' : 'x') 
				 	: (sgid ? 'S' : '-'),		
				 permissions & GNOME_VFS_PERM_OTHER_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_OTHER_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_OTHER_EXEC
				 	? (sticky ? 't' : 'x') 
				 	: (sticky ? 'T' : '-'));
}

/**
 * nautilus_file_get_owner_as_string:
 * 
 * Get a user-displayable string representing a file's owner. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_owner_as_string (NautilusFile *file)
{
	struct passwd *password_info;

	/* Before we have info on a file, the owner is unknown. */
	if (info_missing (file, 0 /* FIXME: GNOME_VFS_FILE_INFO_FIELDS_UID */)) {
		return NULL;
	}

	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getpwuid */
	password_info = getpwuid (file->details->info->uid);

	if (password_info == NULL) {
		return NULL;
	}
	
	return g_strdup (password_info->pw_name);
}

/**
 * nautilus_file_get_group_as_string:
 * 
 * Get a user-displayable string representing a file's group. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_group_as_string (NautilusFile *file)
{
	struct group *group_info;

	/* Before we have info on a file, the owner is unknown. */
	if (info_missing (file, 0 /* FIXME: GNOME_VFS_FILE_INFO_FIELDS_GID */)) {
		return NULL;
	}

	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getgrgid */
	group_info = getgrgid (file->details->info->gid);

	if (group_info == NULL) {
		return NULL;
	}
	
	return g_strdup (group_info->gr_name);
}

/**
 * nautilus_file_get_mime_type_as_string_attribute:
 * 
 * Get a user-displayable string representing a file's MIME type.
 * This string will be displayed in file manager views and thus
 * will not be blank even if the MIME type is unknown. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_mime_type_as_string_attribute (NautilusFile *file)
{
	char *mime_string;

	mime_string = nautilus_file_get_mime_type (file);
	if (mime_string != NULL) {
		return mime_string;
	}
	return NULL;
}

static char *
format_item_count_for_display (guint item_count, 
			       gboolean includes_directories, 
			       gboolean includes_files)
{
	g_return_val_if_fail (includes_directories || includes_files, NULL);

	if (item_count == 0) {
		return g_strdup (includes_directories
			? (includes_files ? _("0 items") : _("0 directories"))
			: _("0 files"));
	}
	if (item_count == 1) {
		return g_strdup (includes_directories
			? (includes_files ? _("1 item") : _("1 directory"))
			: _("1 file"));
	}
	return g_strdup_printf (includes_directories
		? (includes_files ? _("%u items") : _("%u directories"))
		: _("%u files"), item_count);
}

/**
 * nautilus_file_get_size_as_string:
 * 
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_size_as_string (NautilusFile *file)
{
	guint item_count;
	gboolean count_unreadable;

	if (file == NULL) {
		return NULL;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	
	if (nautilus_file_is_directory (file)) {
		if (!nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable)) {
			return NULL;
		}

		return format_item_count_for_display (item_count, TRUE, TRUE);
	}
	
	if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SIZE)) {
		return NULL;
	}
	return gnome_vfs_format_file_size_for_display (file->details->info->size);
}

static char *
nautilus_file_get_deep_count_as_string_internal (NautilusFile *file,
						 gboolean report_size,
						 gboolean report_directory_count,
						 gboolean report_file_count)
{
	NautilusRequestStatus status;
	guint directory_count;
	guint file_count;
	guint unreadable_count;
	guint total_count;
	GnomeVFSFileSize total_size;

	/* Must ask for size or some kind of count, but not both. */
	g_return_val_if_fail (!report_size || (!report_directory_count && !report_file_count), NULL);
	g_return_val_if_fail (report_size || report_directory_count || report_file_count, NULL);

	if (file == NULL) {
		return NULL;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	g_return_val_if_fail (nautilus_file_is_directory (file), NULL);

	status = nautilus_file_get_deep_counts 
		(file, &directory_count, &file_count, &unreadable_count, &total_size);

	/* Check whether any info is available. */
	if (status == NAUTILUS_REQUEST_NOT_STARTED) {
		return NULL;
	}

	total_count = file_count + directory_count;

	if (total_count == 0) {
		switch (status) {
		case NAUTILUS_REQUEST_IN_PROGRESS:
			/* Don't return confident "zero" until we're finished looking,
			 * because of next case.
			 */
			return NULL;
		case NAUTILUS_REQUEST_DONE:
			/* Don't return "zero" if we there were contents but we couldn't read them. */
			if (unreadable_count != 0) {
				return NULL;
			}
		default: break;
		}
	}

	/* Note that we don't distinguish the "everything was readable" case
	 * from the "some things but not everything was readable" case here.
	 * Callers can distinguish them using nautilus_file_get_deep_counts
	 * directly if desired.
	 */
	if (report_size) {
		return gnome_vfs_format_file_size_for_display (total_size);
	}

	return format_item_count_for_display (report_directory_count
		? (report_file_count ? total_count : directory_count)
		: file_count,
		report_directory_count, report_file_count);
}

/**
 * nautilus_file_get_deep_size_as_string:
 * 
 * Get a user-displayable string representing the size of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_size_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, TRUE, FALSE, FALSE);
}

/**
 * nautilus_file_get_deep_total_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_total_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, TRUE, TRUE);
}

/**
 * nautilus_file_get_deep_file_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * items, not including directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_file_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, FALSE, TRUE);
}

/**
 * nautilus_file_get_deep_directory_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_directory_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, TRUE, FALSE);
}

/**
 * nautilus_file_get_string_attribute:
 * 
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns NULL. You can call
 * nautilus_file_get_string_attribute_with_default if you want a non-NULL
 * default.
 * 
 * @file: NautilusFile representing the file in question.
 * @attribute_name: The name of the desired attribute. The currently supported
 * set includes "name", "type", "mime_type", "size", "deep_size", "deep_directory_count",
 * "deep_file_count", "deep_total_count", "date_modified", "date_changed", "date_accessed", 
 * "date_permissions", "owner", "group", "permissions", "octal_permissions", "uri", "parent_uri".
 * 
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute (NautilusFile *file, const char *attribute_name)
{
	/* FIXME bugzilla.eazel.com 646: 
	 * Use hash table and switch statement or function pointers for speed? 
	 */

	if (strcmp (attribute_name, "name") == 0) {
		return nautilus_link_get_display_name (nautilus_file_get_name (file));
	}

	if (strcmp (attribute_name, "type") == 0) {
		return nautilus_file_get_type_as_string (file);
	}

	if (strcmp (attribute_name, "mime_type") == 0) {
		return nautilus_file_get_mime_type_as_string_attribute (file);
	}

	if (strcmp (attribute_name, "size") == 0) {
		return nautilus_file_get_size_as_string (file);
	}

	if (strcmp (attribute_name, "deep_size") == 0) {
		return nautilus_file_get_deep_size_as_string (file);
	}

	if (strcmp (attribute_name, "deep_file_count") == 0) {
		return nautilus_file_get_deep_file_count_as_string (file);
	}

	if (strcmp (attribute_name, "deep_directory_count") == 0) {
		return nautilus_file_get_deep_directory_count_as_string (file);
	}

	if (strcmp (attribute_name, "deep_total_count") == 0) {
		return nautilus_file_get_deep_total_count_as_string (file);
	}

	if (strcmp (attribute_name, "date_modified") == 0) {
		return nautilus_file_get_date_as_string (file, 
							 NAUTILUS_DATE_TYPE_MODIFIED);
	}

	if (strcmp (attribute_name, "date_changed") == 0) {
		return nautilus_file_get_date_as_string (file, 
							 NAUTILUS_DATE_TYPE_CHANGED);
	}

	if (strcmp (attribute_name, "date_accessed") == 0) {
		return nautilus_file_get_date_as_string (file,
							 NAUTILUS_DATE_TYPE_ACCESSED);
	}

	if (strcmp (attribute_name, "date_permissions") == 0) {
		return nautilus_file_get_date_as_string (file,
							 NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED);
	}

	if (strcmp (attribute_name, "permissions") == 0) {
		return nautilus_file_get_permissions_as_string (file);
	}

	if (strcmp (attribute_name, "octal_permissions") == 0) {
		return nautilus_file_get_octal_permissions_as_string (file);
	}

	if (strcmp (attribute_name, "owner") == 0) {
		return nautilus_file_get_owner_as_string (file);
	}

	if (strcmp (attribute_name, "group") == 0) {
		return nautilus_file_get_group_as_string (file);
	}

	if (strcmp (attribute_name, "uri") == 0) {
		return nautilus_file_get_uri (file);
	}

	if (strcmp (attribute_name, "parent_uri") == 0) {
		return nautilus_file_get_parent_uri_as_string (file);
	}

	return NULL;
}

/**
 * nautilus_file_get_string_attribute:
 * 
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns a string representing
 * the unknown value, which varies with attribute. You can call
 * nautilus_file_get_string_attribute if you want NULL instead of a default
 * result.
 * 
 * @file: NautilusFile representing the file in question.
 * @attribute_name: The name of the desired attribute. See the description of
 * nautilus_file_get_string for the set of available attributes.
 * 
 * Returns: Newly allocated string ready to display to the user, or a string
 * such as "unknown" if the value is unknown or @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute_with_default (NautilusFile *file, const char *attribute_name)
{
	char *result;
	guint item_count;
	gboolean count_unreadable;
	NautilusRequestStatus status;

	result = nautilus_file_get_string_attribute (file, attribute_name);

	if (result == NULL) {
		/* Supply default values for the ones we know about. */
		/* FIXME bugzilla.eazel.com 646: 
		 * Use hash table and switch statement or function pointers for speed? 
		 */
		if (strcmp (attribute_name, "size") == 0) {
			count_unreadable = FALSE;
			if (nautilus_file_is_directory (file)) {
				nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable);
			}
			
			result = g_strdup (count_unreadable ? _("xxx") : _("--"));
		} else if (strcmp (attribute_name, "deep_size") == 0
			   || strcmp (attribute_name, "deep_file_count") == 0
			   || strcmp (attribute_name, "deep_directory_count") == 0
			   || strcmp (attribute_name, "deep_total_count") == 0) {
			status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL);
			if (status == NAUTILUS_REQUEST_DONE) {
				/* This means no contents at all were readable */
				return g_strdup (_("xxx"));
			}
			return g_strdup (_("--"));
		} else if (strcmp (attribute_name, "type") == 0) {
			result = g_strdup (_("unknown type"));
		} else if (strcmp (attribute_name, "mime_type") == 0) {
			result = g_strdup (_("unknown MIME type"));
		} else {
			/* Fallback, use for both unknown attributes and attributes
			 * for which we have no more appropriate default.
			 */
			result = g_strdup (_("unknown"));
		}
	}

	return result;
}

/**
 * nautilus_file_get_type_as_string:
 * 
 * Get a user-displayable string representing a file type. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_type_as_string (NautilusFile *file)
{
	const char *mime_type;
	const char *description;

	mime_type = info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		? NULL : file->details->info->mime_type;

	if (nautilus_strlen (mime_type) == 0) {
		/* No mime type, anything else interesting we can say about this? */
		if (nautilus_file_is_executable (file)) {
			return g_strdup (_("program"));
		}

		return NULL;
	}

	description = gnome_vfs_mime_description (mime_type);
	if (nautilus_strlen (description) > 0) {
		return g_strdup (description);
	}

	/* We want to update nautilus/data/nautilus.keys to include 
	 * English (& localizable) versions of every mime type anyone ever sees.
	 */
	if (strcasecmp (mime_type, "x-special/directory") == 0) {
		g_warning ("Can't find description even for \"x-special/directory\". This "
			   "probably means that your gnome-vfs.keys file is in the wrong place "
			   "or isn't being found for some other reason.");
			
	} else {
		g_warning ("No description found for mime type \"%s\" (file is \"%s\"), tell sullivan@eazel.com", 
			    mime_type,
			    file->details->name);
	}
	return g_strdup (mime_type);
}

/**
 * nautilus_file_get_file_type
 * 
 * Return this file's type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The type.
 * 
 **/
GnomeVFSFileType
nautilus_file_get_file_type (NautilusFile *file)
{
	return info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_TYPE)
		? GNOME_VFS_FILE_TYPE_UNKNOWN : file->details->info->type;
}

/**
 * nautilus_file_get_mime_type
 * 
 * Return this file's mime type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The mime type.
 * 
 **/
char *
nautilus_file_get_mime_type (NautilusFile *file)
{
	return info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		? NULL : g_strdup (file->details->info->mime_type);
}

/**
 * nautilus_file_is_mime_type
 * 
 * Check whether a file is of a particular MIME type.
 * @file: NautilusFile representing the file in question.
 * @mime_type: The MIME-type string to test (e.g. "text/plain")
 * 
 * Return value: TRUE if @mime_type exactly matches the
 * file's MIME type.
 * 
 **/
gboolean
nautilus_file_is_mime_type (NautilusFile *file, const char *mime_type)
{
	g_return_val_if_fail (mime_type != NULL, FALSE);

	if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)) {
		return FALSE;
	}
	return nautilus_strcmp (file->details->info->mime_type, mime_type) == 0;
}

/**
 * nautilus_file_get_emblem_names
 * 
 * Return the list of names of emblems that this file should display,
 * in canonical order.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: A list of emblem names.
 * 
 **/
GList *
nautilus_file_get_emblem_names (NautilusFile *file)
{
	GList *names;

	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	names = nautilus_file_get_keywords (file);
	
	if (!nautilus_file_can_read (file)) {
		names = g_list_prepend 
			(names, g_strdup (EMBLEM_NAME_CANT_READ));

	}

	if (!nautilus_file_can_write (file)) {
		names = g_list_prepend 
			(names, g_strdup (EMBLEM_NAME_CANT_WRITE));

	}
						    					    
	if (nautilus_file_is_symbolic_link (file)) {
		names = g_list_prepend 
			(names, g_strdup (EMBLEM_NAME_SYMBOLIC_LINK));
	}

	return names;
}

static GList *
sort_keyword_list_and_remove_duplicates (GList *keywords)
{
	GList *p;
	GList *duplicate_link;
	
	keywords = g_list_sort (keywords, (GCompareFunc) compare_emblem_names);

	if (keywords != NULL) {
		p = keywords;		
		while (p->next != NULL) {
			if (nautilus_strcmp (p->data, p->next->data) == 0) {
				duplicate_link = p->next;
				keywords = g_list_remove_link (keywords, duplicate_link);
				nautilus_g_list_free_deep (duplicate_link);
			} else {
				p = p->next;
			}
		}
	}
	
	return keywords;
}

/**
 * nautilus_file_get_keywords
 * 
 * Return this file's keywords.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: A list of keywords.
 * 
 **/
GList *
nautilus_file_get_keywords (NautilusFile *file)
{
	GList *keywords;

	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	/* Put all the keywords into a list. */
	keywords = nautilus_file_get_metadata_list
		(file, "KEYWORD", "NAME");
	
	return sort_keyword_list_and_remove_duplicates (keywords);
}

/**
 * nautilus_file_set_keywords
 * 
 * Change this file's keywords.
 * @file: NautilusFile representing the file in question.
 * @keywords: New set of keywords (a GList of strings).
 *
 **/
void
nautilus_file_set_keywords (NautilusFile *file, GList *keywords)
{
	GList *canonical_keywords;

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	canonical_keywords = sort_keyword_list_and_remove_duplicates
		(g_list_copy (keywords));
	nautilus_file_set_metadata_list
		(file, "KEYWORD", "NAME", canonical_keywords);
	g_list_free (canonical_keywords);
}

/**
 * nautilus_file_is_symbolic_link
 * 
 * Check if this file is a symbolic link.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a symbolic link.
 * 
 **/
gboolean
nautilus_file_is_symbolic_link (NautilusFile *file)
{
	return info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_FLAGS)
		? FALSE : GNOME_VFS_FILE_INFO_SYMLINK (file->details->info);
}

/**
 * nautilus_file_is_directory
 * 
 * Check if this file is a directory.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file is a directory.
 * 
 **/
gboolean
nautilus_file_is_directory (NautilusFile *file)
{
	if (file == NULL) {
		return FALSE;
	}
		
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return nautilus_file_get_file_type (file) == GNOME_VFS_FILE_TYPE_DIRECTORY;
}

/**
 * nautilus_file_contains_text
 * 
 * Check if this file contains text.
 * This is private and is used to decide whether or not to read the top left text.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file has a text MIME type or is a regular file with unknown MIME type.
 * 
 **/
gboolean
nautilus_file_contains_text (NautilusFile *file)
{
	char *mime_type, *uri;
	gboolean contains_text;

	if (file == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	mime_type = nautilus_file_get_mime_type (file);
	uri = nautilus_file_get_uri (file);
	
	/* see if it's a nautilus link xml file - if so, see if we need to handle specially */
	contains_text = (nautilus_str_has_prefix (mime_type, "text/")
		|| (mime_type == NULL && nautilus_file_get_file_type (file)
		    == GNOME_VFS_FILE_TYPE_REGULAR)) 
		&& !nautilus_link_is_link_file (uri);
		    
	g_free (mime_type);
	g_free (uri);
	
	return contains_text;
}

/**
 * nautilus_file_is_executable
 * 
 * Check if this file is executable at all.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if any of the execute bits are set. FALSE if
 * not, or if the permissions are unknown.
 * 
 **/
gboolean
nautilus_file_is_executable (NautilusFile *file)
{
	if (!nautilus_file_can_get_permissions (file)) {
		/* File's permissions field is not valid.
		 * Can't access specific permissions, so return FALSE.
		 */
		return FALSE;
	}

	return (file->details->info->permissions
		& (GNOME_VFS_PERM_USER_EXEC
		   | GNOME_VFS_PERM_GROUP_EXEC
		   | GNOME_VFS_PERM_OTHER_EXEC)) != 0;
}

/**
 * nautilus_file_get_top_left_text
 * 
 * Get the text from the top left of the file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: NULL if there is no text readable, otherwise, the text.
 * 
 **/
char *
nautilus_file_get_top_left_text (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	/* Show " --" in the file until we read the contents in. */
	if (!file->details->got_top_left_text) {
		if (nautilus_file_contains_text (file)) {
			return g_strdup (_(" --"));
		}
		return NULL;
	}

	/* Show what we read in. */
	return g_strdup (file->details->top_left_text);
}

/**
 * nautilus_file_delete
 * 
 * Delete this file.
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_delete (NautilusFile *file)
{
	char *text_uri;
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	/* Deleting a file that's already gone is easy. */
	if (file->details->is_gone) {
		return;
	}

	/* Do the actual deletion. */
	text_uri = nautilus_file_get_uri (file);
	if (nautilus_file_is_directory (file)) {
		result = gnome_vfs_remove_directory (text_uri);
	} else {
		result = gnome_vfs_unlink (text_uri);
	}
	g_free (text_uri);

	/* Mark the file gone. */
	if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_NOT_FOUND) {
		nautilus_file_ref (file);
		nautilus_file_mark_gone (file);
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
}

void
nautilus_file_mark_gone (NautilusFile *file)
{
	NautilusDirectory *directory;
	GList **files;

	file->details->is_gone = TRUE;
	
	/* Drop away all the old file information, but keep the name. */
	/* FIXME: Maybe we can get rid of the name too eventually, but
	 * for now that would probably require too many if statements
	 * everywhere anyone deals with the name. Maybe we can give it
	 * a hard-coded "<deleted>" name or something.
	 */
	if (file->details->info != NULL) {
		file->details->name = g_strdup (file->details->name);
		gnome_vfs_file_info_unref (file->details->info);
		file->details->info = NULL;
	}

	/* Let the directory know it's gone. */
	directory = file->details->directory;
	files = &directory->details->files;
	g_assert (g_list_find (*files, file) != NULL);
	*files = g_list_remove (*files, file);
	if (nautilus_directory_is_file_list_monitored (directory)) {
		nautilus_file_unref (file);
	}
}

/**
 * nautilus_file_changed
 * 
 * Notify the user that this file has changed.
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_changed (NautilusFile *file)
{
	GList fake_list;

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	fake_list.data = file;
	fake_list.next = NULL;
	fake_list.prev = NULL;
	nautilus_directory_emit_files_changed (file->details->directory, &fake_list);
}

/**
 * nautilus_file_emit_changed
 * 
 * Emit a file changed signal.
 * This can only be called by the directory, since the directory
 * also has to emit a files_changed signal.
 *
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_emit_changed (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));

	/* Send out a signal. */
	gtk_signal_emit (GTK_OBJECT (file), signals[CHANGED], file);
}

/**
 * nautilus_file_is_gone
 * 
 * Check if a file has already been deleted.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nautilus_file_is_gone (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->is_gone;
}

/**
 * nautilus_file_activate_custom
 *
 * This routine is called when a file is activated to have a chance to
 * do file-type specific activation. If it returns true, the framework
 * won't try to change location.
 *  
 * For now, the only thing we handle specially is link activation,
 * where we special case uris with a protocol of "command" to execute
 * shell commands, mainly to launch applications.
 *
 **/
/* FIXME: This does not belong here! It's a much higher-level
 * operation than we should have in Nautilusfile.
 */
gboolean
nautilus_file_activate_custom (NautilusFile *file, gboolean use_new_window)
{
	int result;
	char *uri, *old_uri, *command_str;
	uri = nautilus_file_get_uri (file);
	
	/* See if it's a nautilus link xml file - if so, see if we need to handle specially. */
	if (nautilus_link_is_link_file (uri)) {
		old_uri = uri;
		uri = nautilus_link_get_link_uri (uri);
		g_free (old_uri);
	
		if (nautilus_str_has_prefix (uri, "command:")) {
			/* FIXME: Hardcoded "+8"? */
			command_str = g_strdup_printf ("%s &", uri + 8);
			result = system (command_str);
			g_free (command_str);
			g_free (uri);
			return TRUE;
		}
	}
	g_free (uri);
	return FALSE;
}

/**
 * nautilus_file_check_if_ready
 *
 * Check whether the values for a set of file attributes are
 * currently available, without doing any additional work. This
 * is useful for callers that want to reflect updated information
 * when it is ready but don't want to force the work required to
 * obtain the information, which might be slow network calls, e.g.
 *
 * @file: The file being queried.
 * @file_attributes: A GList of the desired information.
 * 
 * Return value: TRUE if all of the specified attributes are currently readable.
 */
gboolean
nautilus_file_check_if_ready (NautilusFile *file,
			      GList *file_attributes)
{
	/* To be parallel with call_when_ready, return
	 * TRUE for NULL file.
	 */
	if (file == NULL) {
		return TRUE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return nautilus_directory_check_if_ready_internal
		(file->details->directory, file,
		 file_attributes);
}			      


void
nautilus_file_call_when_ready (NautilusFile *file,
			       GList *file_attributes,
			       gboolean wait_for_metadata,
			       NautilusFileCallback callback,
			       gpointer callback_data)
{
	g_return_if_fail (callback != NULL);

	if (file == NULL) {
		(* callback) (file, callback_data);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	nautilus_directory_call_when_ready_internal
		(file->details->directory, file,
		 file_attributes, wait_for_metadata,
		 NULL, callback, callback_data);
}

void
nautilus_file_cancel_callback (NautilusFile *file,
			       NautilusFileCallback callback,
			       gpointer callback_data)
{
	g_return_if_fail (callback != NULL);

	if (file == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	nautilus_directory_cancel_callback_internal
		(file->details->directory,
		 file,
		 NULL,
		 callback,
		 callback_data);
}

/**
 * nautilus_file_list_ref
 *
 * Ref all the files in a list.
 * @list: GList of files.
 **/
GList *
nautilus_file_list_ref (GList *list)
{
	return nautilus_gtk_object_list_ref (list);
}

/**
 * nautilus_file_list_unref
 *
 * Unref all the files in a list.
 * @list: GList of files.
 **/
void
nautilus_file_list_unref (GList *list)
{
	nautilus_gtk_object_list_unref (list);
}

/**
 * nautilus_file_list_free
 *
 * Free a list of files after unrefing them.
 * @list: GList of files.
 **/
void
nautilus_file_list_free (GList *list)
{
	nautilus_gtk_object_list_free (list);
}

/**
 * nautilus_file_list_copy
 *
 * Copy the list of files, making a new ref of each,
 * @list: GList of files.
 **/
GList *
nautilus_file_list_copy (GList *list)
{
	return nautilus_gtk_object_list_copy (list);
}

/* Extract the top left part of the read-in text. */
char *
nautilus_extract_top_left_text (const char *text,
				int length)
{
	char buffer[(NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE + 1)
		   * NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES + 1];
	const char *in, *end;
	char *out;
	int line, i;

	if (length == 0) {
		return NULL;
	}

	in = text;
	end = text + length;
	out = buffer;

	for (line = 0; line < NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES; line++) {
		/* Extract one line. */
		for (i = 0; i < NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE; ) {
			if (*in == '\n') {
				break;
			}
			if (isprint (*in)) {
				*out++ = *in;
				i++;
			}
			if (++in == end) {
				goto done;
			}
		}

		/* Skip the rest of the line. */
		while (*in != '\n') {
			if (++in == end) {
				goto done;
			}
		}
		if (++in == end) {
			goto done;
		}

		/* Put a new-line separator in. */
		*out++ = '\n';
	}
	
 done:
	/* Omit any trailing new-lines. */
	while (out != buffer && out[-1] == '\n') {
		out--;
	}

	/* Check again for special case of empty string. */
	if (out == buffer) {
		return NULL;
	}

	/* Allocate a copy to keep. */
	*out = '\0';
	return g_strdup (buffer);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file (void)
{
	NautilusFile *file_1;
	NautilusFile *file_2;
	GList *list;

        /* refcount checks */

        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);

	file_1 = nautilus_file_get ("file:///home/");

	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1)->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1->details->directory)->ref_count, 1);
        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 1);

	nautilus_file_unref (file_1);

        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

        list = NULL;
        list = g_list_append (list, file_1);
        list = g_list_append (list, file_2);

        nautilus_file_list_ref (list);
        
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1)->ref_count, 2);
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_2)->ref_count, 2);

	nautilus_file_list_unref (list);
        
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1)->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_2)->ref_count, 1);

	nautilus_file_list_free (list);

        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	

        /* name checks */
	file_1 = nautilus_file_get ("file:///home/");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home/") == file_1, TRUE);
	nautilus_file_unref (file_1);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home") == file_1, TRUE);
	nautilus_file_unref (file_1);

	nautilus_file_unref (file_1);

	file_1 = nautilus_file_get ("file:///home");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");
	nautilus_file_unref (file_1);

	/* sorting */
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1)->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_2)->ref_count, 1);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort_reversed (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_NAME) == 0, TRUE);

	nautilus_file_unref (file_1);
	nautilus_file_unref (file_2);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
