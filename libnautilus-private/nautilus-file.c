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

#include "nautilus-directory-metafile.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-string.h"
#include <ctype.h>
#include <gnome-xml/parser.h>
#include <grp.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-mime-info.h>
#include <libgnome/gnome-mime.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#undef NAUTILUS_FILE_DEBUG_REF

#ifdef NAUTILUS_FILE_DEBUG_REF
extern void eazel_dump_stack_trace	(const char    *print_prefix,
					 int		num_levels);
/* from libleakcheck.so */
#endif

typedef enum {
	NAUTILUS_DATE_TYPE_MODIFIED,
	NAUTILUS_DATE_TYPE_CHANGED,
	NAUTILUS_DATE_TYPE_ACCESSED,
	NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED
} NautilusDateType;

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GHashTable *symbolic_links;

static void     nautilus_file_initialize_class    (NautilusFileClass *klass);
static void     nautilus_file_initialize          (NautilusFile      *file);
static void     destroy                           (GtkObject         *object);
static char *   nautilus_file_get_owner_as_string (NautilusFile      *file,
						   gboolean           include_real_name);
static char *   nautilus_file_get_type_as_string  (NautilusFile      *file);
static gboolean update_info_and_name              (NautilusFile      *file,
						   GnomeVFSFileInfo  *info);

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
nautilus_file_new_from_relative_uri (NautilusDirectory *directory,
				     const char *relative_uri)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (relative_uri != NULL, NULL);
	g_return_val_if_fail (relative_uri[0] != '\0', NULL);

	file = NAUTILUS_FILE (gtk_object_new (NAUTILUS_TYPE_FILE, NULL));
	gtk_object_ref (GTK_OBJECT (file));
	gtk_object_sink (GTK_OBJECT (file));

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	nautilus_directory_ref (directory);

	file->details->directory = directory;
	file->details->relative_uri = g_strdup (relative_uri);

	return file;
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

typedef GList * (* ModifyListFunction) (GList *list, NautilusFile *file);

static void
modify_link_hash_table (NautilusFile *file,
			ModifyListFunction modify_function)
{
	const char *symlink_name;
	gboolean found;
	gpointer original_key;
	gpointer original_value;
	GList *list;

	/* Check if there is a symlink name. If none, we are OK. */
	if (info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)) {
		return;
	}
	symlink_name = file->details->info->symlink_name;
	if (symlink_name == NULL) {
		return;
	}

	/* Creat the hash table first time through. */
	if (symbolic_links == NULL) {
		symbolic_links = nautilus_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal, "nautilus-file.c: symbolic_links");
	}

	/* Find the old contents of the hash table. */
	found = g_hash_table_lookup_extended
		(symbolic_links, symlink_name,
		 &original_key, &original_value);
	if (!found) {
		list = NULL;
	} else {
		g_hash_table_remove (symbolic_links, symlink_name);
		g_free (original_key);
		list = original_value;
	}
	list = (* modify_function) (list, file);
	if (list != NULL) {
		g_hash_table_insert (symbolic_links, g_strdup (symlink_name), list);
	}
}

static GList *
add_to_link_hash_table_list (GList *list, NautilusFile *file)
{
	g_assert (g_list_find (list, file) == NULL);
	return g_list_prepend (list, file);
}

static void
add_to_link_hash_table (NautilusFile *file)
{
	modify_link_hash_table (file, add_to_link_hash_table_list);
}

static GList *
remove_from_link_hash_table_list (GList *list, NautilusFile *file)
{
	g_assert (g_list_find (list, file) != NULL);
	return g_list_remove (list, file);
}

static void
remove_from_link_hash_table (NautilusFile *file)
{
	modify_link_hash_table (file, remove_from_link_hash_table_list);
}

NautilusFile *
nautilus_file_new_from_info (NautilusDirectory *directory,
			     GnomeVFSFileInfo *info)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	file = NAUTILUS_FILE (gtk_object_new (NAUTILUS_TYPE_FILE, NULL));

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	gtk_object_ref (GTK_OBJECT (file));
	gtk_object_sink (GTK_OBJECT (file));

	nautilus_directory_ref (directory);
	file->details->directory = directory;

	update_info_and_name (file, info);
	return file;
}

/**
 * nautilus_file_get_internal:
 * @uri: URI of file to get.
 *
 * Get a file given a uri.
 * Returns a referenced object. Unref when finished.
 * If two windows are viewing the same uri, the file object is shared.
 */
static NautilusFile *
nautilus_file_get_internal (const char *uri, gboolean create)
{
	char *canonical_uri, *directory_uri, *relative_uri;
	gboolean self_owned;
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	NautilusDirectory *directory;
	NautilusFile *file;

	g_return_val_if_fail (uri != NULL, NULL);

	/* Maybe we wouldn't need this if the gnome-vfs canonical
	 * stuff was strong enough.
	 */
	canonical_uri = nautilus_make_uri_canonical (uri);

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (canonical_uri);
	relative_uri = NULL;
	if (vfs_uri != NULL) {
		relative_uri = gnome_vfs_uri_extract_short_path_name (vfs_uri);

		/* Couldn't parse a name out of the URI: the URI must be bogus,
		 * so we'll treat it like the case where gnome_vfs_uri couldn't
		 * even create a URI.
		 */
		if (nautilus_str_is_empty (relative_uri)) {
			gnome_vfs_uri_unref (vfs_uri);
			vfs_uri = NULL;
			g_free (relative_uri);
			relative_uri = NULL;
		}
	}

	/* Make VFS version of directory URI. */
	if (vfs_uri == NULL) {
		directory_vfs_uri = NULL;
	} else {
		directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
		gnome_vfs_uri_unref (vfs_uri);
	}

	self_owned = directory_vfs_uri == NULL;
	if (self_owned) {
		/* Use the item itself if we have no parent. */
		directory_uri = g_strdup (canonical_uri);
	} else {
		/* Make text version of directory URI. */
		directory_uri = gnome_vfs_uri_to_string
			(directory_vfs_uri,
			 GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (directory_vfs_uri);
	}

	/* Get object that represents the directory. */
	directory = nautilus_directory_get_internal (directory_uri, create);
	g_free (directory_uri);

	/* Get the name for the file. */
	if (vfs_uri == NULL) {
		g_assert (self_owned);
		if (directory != NULL) {
			relative_uri = gnome_vfs_escape_string
				(nautilus_directory_get_name_for_self_as_new_file (directory));
		}
	}

	/* Check to see if it's a file that's already known. */
	if (directory == NULL) {
		file = NULL;
	} else if (self_owned) {
		file = directory->details->as_file;
	} else {
		file = nautilus_directory_find_file_by_relative_uri (directory, relative_uri);
	}

	/* Ref or create the file. */
	if (file != NULL) {
		nautilus_file_ref (file);
	} else if (create) {
		file = nautilus_file_new_from_relative_uri (directory, relative_uri);
		if (self_owned) {
			g_assert (directory->details->as_file == NULL);
			directory->details->as_file = file;
		} else {
			nautilus_directory_add_file (directory, file);
		}
	}

	g_free (canonical_uri);
	g_free (relative_uri);
	nautilus_directory_unref (directory);

	return file;
}

NautilusFile *
nautilus_file_get_existing (const char *uri)
{
	return nautilus_file_get_internal (uri, FALSE);
}

NautilusFile *
nautilus_file_get (const char *uri)
{
	return nautilus_file_get_internal (uri, TRUE);
}

static void
destroy (GtkObject *object)
{
	NautilusDirectory *directory;
	NautilusFile *file;

	file = NAUTILUS_FILE (object);

	g_assert (file->details->operations_in_progress == NULL);

	nautilus_async_destroying_file (file);
	
	remove_from_link_hash_table (file);

	directory = file->details->directory;
	
	if (directory->details->as_file == file) {
		directory->details->as_file = NULL;
	} else {
		if (!file->details->is_gone) {
			nautilus_directory_remove_file (directory, file);
		}
	}

	nautilus_directory_unref (directory);
	g_free (file->details->relative_uri);
	if (file->details->info != NULL) {
		gnome_vfs_file_info_unref (file->details->info);
	}
	g_free (file->details->top_left_text);
	g_free (file->details->activation_uri);

	g_list_foreach (file->details->mime_list, (GFunc) g_free, NULL);
	g_list_free (file->details->mime_list);

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

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

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

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p unref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	gtk_object_unref (GTK_OBJECT (file));
}

static gboolean
nautilus_file_is_self_owned (NautilusFile *file)
{
	return file->details->directory->details->as_file == file;
}

/**
 * nautilus_file_get_parent_uri_for_display:
 * 
 * Get the uri for the parent directory.
 * 
 * @file: The file in question.
 * 
 * Return value: A string representing the parent's location,
 * formatted for user display (including stripping "file://").
 * If the parent is NULL, returns the empty string.
 */ 
char *
nautilus_file_get_parent_uri_for_display (NautilusFile *file) 
{
	char *raw_uri;
	char *result;

	g_assert (NAUTILUS_IS_FILE (file));
	
	raw_uri = nautilus_file_get_parent_uri (file);
	result = nautilus_format_uri_for_display (raw_uri);
	g_free (raw_uri);

	return result;
}

/**
 * nautilus_file_get_parent_uri:
 * 
 * Get the uri for the parent directory.
 * 
 * @file: The file in question.
 * 
 * Return value: A string for the parent's location, in "raw URI" form.
 * Use nautilus_file_get_parent_uri_for_display instead if the
 * result is to be displayed on-screen.
 * If the parent is NULL, returns the empty string.
 */ 
char *
nautilus_file_get_parent_uri (NautilusFile *file) 
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
		/* Callers expect an empty string, not a NULL. */
		return g_strdup ("");
	}

	return nautilus_directory_get_uri (file->details->directory);
}

static NautilusFile *
get_file_for_parent_directory (NautilusFile *file)
{
	char *parent_uri;
	NautilusFile *result;

	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
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

	/* Self-owned files can't be renamed */
	if (nautilus_file_is_self_owned (file)) {
		return FALSE;
	}

	/* Certain types of links can't be renamed */
	if (nautilus_file_is_nautilus_link (file)) {
		return FALSE;
	}
	
	/* User must have write permissions for the parent directory. */
	parent = get_file_for_parent_directory (file);

	/* No parent directory for some reason (at root level?).
	 * Can't tell whether this file is renameable, so return TRUE.
	 */
	if (parent == NULL) {
		return TRUE;
	}
	
	result = nautilus_file_can_write (parent);

	nautilus_file_unref (parent);

	return result;
}

static GnomeVFSURI *
nautilus_file_get_gnome_vfs_uri (NautilusFile *file)
{
	GnomeVFSURI *vfs_uri;

	vfs_uri = file->details->directory->details->vfs_uri;
	if (vfs_uri == NULL) {
		return NULL;
	}

	if (nautilus_file_is_self_owned (file)) {
		gnome_vfs_uri_ref (vfs_uri);
		return vfs_uri;
	}

	return gnome_vfs_uri_append_string
		(vfs_uri, file->details->relative_uri);
}

typedef struct {
	NautilusFile *file;
	GnomeVFSAsyncHandle *handle;
	NautilusFileOperationCallback callback;
	gpointer callback_data;
} Operation;

static Operation *
operation_new (NautilusFile *file,
	       NautilusFileOperationCallback callback,
	       gpointer callback_data)
{
	Operation *op;

	nautilus_file_ref (file);

	op = g_new0 (Operation, 1);
	op->file = file;
	op->callback = callback;
	op->callback_data = callback_data;

	op->file->details->operations_in_progress = g_list_prepend
		(op->file->details->operations_in_progress, op);

	return op;
}

static void
operation_remove (Operation *op)
{
	op->file->details->operations_in_progress = g_list_remove
		(op->file->details->operations_in_progress, op);

}

static void
operation_free (Operation *op)
{
	operation_remove (op);
	nautilus_file_unref (op->file);
	g_free (op);
}

static void
operation_complete (Operation *op,
		    GnomeVFSResult result)
{
	/* Claim that something changed even if the operation failed.
	 * This makes it easier for some clients who see the "reverting"
	 * as "changing back".
	 */
	operation_remove (op);
	nautilus_file_changed (op->file);
	(* op->callback) (op->file, result, op->callback_data);
	operation_free (op);
}

static void
operation_cancel (Operation *op)
{
	/* Cancel the operation if it's still in progress. */
	g_assert (op->handle != NULL);
	gnome_vfs_async_cancel (op->handle);

	/* Claim that something changed even though the operation was
	 * canceled in case some work was partly done, but don't call
	 * the callback.
	 */
	nautilus_file_changed (op->file);
	operation_free (op);
}

static void
rename_callback (GnomeVFSAsyncHandle *handle,
		 GnomeVFSResult result,
		 GnomeVFSFileInfo *new_info,
		 gpointer callback_data)
{
	Operation *op;
	NautilusDirectory *directory;
	NautilusFile *existing_file;
	char *old_relative_uri;
	char *old_uri;
	char *new_uri;
	NautilusDirectory *renamed_directory;


	op = callback_data;
	g_assert (handle == op->handle);

	if (result == GNOME_VFS_OK && new_info != NULL) {
		directory = op->file->details->directory;
		
		/* If there was another file by the same name in this
		 * directory, mark it gone.
		 */
		existing_file = nautilus_directory_find_file_by_name (directory, new_info->name);
		if (existing_file != NULL) {
			nautilus_file_mark_gone (existing_file);
			nautilus_file_changed (existing_file);
		}
		
		old_uri = nautilus_file_get_uri (op->file);
		old_relative_uri = g_strdup (op->file->details->relative_uri);
		update_info_and_name (op->file, new_info);
		nautilus_directory_rename_file_metadata
			(directory, old_relative_uri, op->file->details->relative_uri);

		renamed_directory = nautilus_directory_get_existing (old_uri);
		if (renamed_directory != NULL) {
			new_uri = nautilus_file_get_uri (op->file);
			nautilus_directory_handle_directory_moved (renamed_directory,
								   new_uri);
			g_free (new_uri);
		}
		g_free (old_uri);
	}
	operation_complete (op, result);
}

static gboolean
name_is (NautilusFile *file, const char *new_name)
{
	char *old_name;
	gboolean equal;

	old_name = nautilus_file_get_name (file);
	equal = strcmp (new_name, old_name) == 0;
	g_free (old_name);
	return equal;
}

void
nautilus_file_rename (NautilusFile *file,
		      const char *new_name,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	Operation *op;
	GnomeVFSFileInfo *partial_file_info;
	GnomeVFSURI *vfs_uri;

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
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_NOT_FOUND, callback_data);
		return;
	}

	/* Test the name-hasn't-changed case explicitly, for two reasons.
	 * (1) gnome_vfs_async_xfer returns an error if new & old are same.
	 * (2) We don't want to send file-changed signal if nothing changed.
	 */
	if (name_is (file, new_name)) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* Self-owned files can't be renamed. Test the name-not-actually-changing
	 * case before this case.
	 */
	if (nautilus_file_is_self_owned (file)) {
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_NOT_SUPPORTED, callback_data);
		return;
	}

	/* Set up a renaming operation. */
	op = operation_new (file, callback, callback_data);

	/* Do the renaming. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->name = g_strdup (new_name);
	vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       vfs_uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_NAME,
				       (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
					| GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
				       rename_callback, op);
	gnome_vfs_file_info_unref (partial_file_info);
	gnome_vfs_uri_unref (vfs_uri);
}

void
nautilus_file_cancel (NautilusFile *file,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	GList *p, *next;
	Operation *op;

	for (p = file->details->operations_in_progress; p != NULL; p = next) {
		next = p->next;
		op = p->data;

		g_assert (op->file == file);
		if (op->callback == callback
		    && op->callback_data == callback_data) {
			operation_cancel (op);
		}
	}
}

gboolean         
nautilus_file_matches_uri (NautilusFile *file, const char *match_uri)
{
	GnomeVFSURI *match_vfs_uri, *file_vfs_uri;
	char *file_uri;
	gboolean result;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (match_uri != NULL, FALSE);

	match_vfs_uri = gnome_vfs_uri_new (match_uri);
	file_vfs_uri = nautilus_file_get_gnome_vfs_uri (file);

	if (match_vfs_uri == NULL || file_vfs_uri == NULL) {
		file_uri = nautilus_file_get_uri (file);
		result = strcmp (match_uri, file_uri) == 0;
	} else {
		result = gnome_vfs_uri_equal (file_vfs_uri, match_vfs_uri);
	}

	if (file_vfs_uri != NULL) {
		gnome_vfs_uri_unref (file_vfs_uri);
	}
	if (match_vfs_uri != NULL) {
		gnome_vfs_uri_unref (match_vfs_uri);
	}

	return result;
}

gboolean
nautilus_file_is_local (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return nautilus_directory_is_local (file->details->directory);
}

gboolean
nautilus_file_should_get_top_left_text (NautilusFile *file)
{
	NautilusSpeedTradeoffValue preference_value;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	preference_value = nautilus_preferences_get_enum
		(NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS, 
		 NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);

	if (preference_value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (preference_value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (preference_value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	return nautilus_file_is_local (file);
}

static void
update_link (NautilusFile *link_file, NautilusFile *target_file)
{
	g_assert (NAUTILUS_IS_FILE (link_file));
	g_assert (NAUTILUS_IS_FILE (target_file));
	g_assert (!info_missing (link_file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME));
}

static GList *
get_link_files (NautilusFile *target_file)
{
	char *uri;
	GList *link_files;
	
	if (symbolic_links == NULL) {
		link_files = NULL;
	} else {
		uri = nautilus_file_get_uri (target_file);
		link_files = g_hash_table_lookup (symbolic_links, uri);
		g_free (uri);
	}
	return nautilus_file_list_copy (link_files);
}

static void
update_links_if_target (NautilusFile *target_file)
{
	GList *link_files, *p;
	
	link_files = get_link_files (target_file);
	for (p = link_files; p != NULL; p = p->next) {
		update_link (NAUTILUS_FILE (p->data), target_file);
	}
	nautilus_file_list_free (link_files);
}

static gboolean
update_info_internal (NautilusFile *file,
		      GnomeVFSFileInfo *info,
		      gboolean update_name)
{
	GList *node;
	GnomeVFSFileInfo *info_copy;
	char *new_relative_uri;

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

	remove_from_link_hash_table (file);

	info_copy = gnome_vfs_file_info_dup (info);
	if (file->details->info != NULL) {
		gnome_vfs_file_info_unref (file->details->info);
	}
	file->details->info = info_copy;

	if (update_name) {
		new_relative_uri = gnome_vfs_escape_string (info->name);
		if (file->details->relative_uri != NULL
		    && strcmp (file->details->relative_uri, new_relative_uri) == 0) {
			g_free (new_relative_uri);
		} else {
			node = nautilus_directory_begin_file_name_change
				(file->details->directory, file);
			g_free (file->details->relative_uri);
			file->details->relative_uri = new_relative_uri;
			nautilus_directory_end_file_name_change
				(file->details->directory, file, node);
		}
	}

	add_to_link_hash_table (file);

	update_links_if_target (file);

	return TRUE;
}

static gboolean
update_info_and_name (NautilusFile *file,
		      GnomeVFSFileInfo *info)
{
	return update_info_internal (file, info, TRUE);
}

gboolean
nautilus_file_update_info (NautilusFile *file,
			   GnomeVFSFileInfo *info)
{
	return update_info_internal (file, info, FALSE);
}

gboolean
nautilus_file_update_name (NautilusFile *file, const char *name)
{
	GnomeVFSFileInfo *info;
	GList *node;

	g_assert (name != NULL);

	if (file->details->is_gone) {
		return FALSE;
	}

	if (name_is (file, name)) {
		return FALSE;
	}

	if (file->details->info == NULL) {
		node = nautilus_directory_begin_file_name_change
			(file->details->directory, file);
		g_free (file->details->relative_uri);
		file->details->relative_uri = gnome_vfs_escape_string (name);
		nautilus_directory_end_file_name_change
			(file->details->directory, file, node);
	} else {
		/* Make a copy and update the file name in the copy. */
		info = gnome_vfs_file_info_new ();
		gnome_vfs_file_info_copy (info, file->details->info);
		g_free (info->name);
		info->name = g_strdup (name);
		update_info_and_name (file, info);
		gnome_vfs_file_info_unref (info);
	}

	return TRUE;
}

static int
nautilus_file_compare_directories_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with n items
	 *   Directories with 0 items
	 *   Directories with unknown # of items
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
		return +1;
	}
	if (count_known_1 && !count_known_2) {
		return -1;
	}

	if (!count_known_1 && !count_known_2) {
		/* Put unknowable after simply unknown. */
		if (count_unreadable_1 && !count_unreadable_2) {
			return +1;
		}

		if (!count_unreadable_1 && count_unreadable_2) {
			return -1;
		}

		return 0;
	}

	if (item_count_1 > item_count_2) {
		return -1;
	}
	if (item_count_2 > item_count_1) {
		return +1;
	}

	return 0;
}

static int
nautilus_file_compare_by_name (NautilusFile *file_1, NautilusFile *file_2)
{
	char *name_1, *name_2;
	int compare;

	name_1 = nautilus_file_get_name (file_1);
	name_2 = nautilus_file_get_name (file_2);

	compare = nautilus_strcmp_case_breaks_ties (name_1, name_2);

	g_free (name_1);
	g_free (name_2);

	return compare;
}

static int
nautilus_file_compare_by_directory_name (NautilusFile *file_1, NautilusFile *file_2)
{
	char *directory_1, *directory_2;
	int compare;

	directory_1 = nautilus_file_get_parent_uri_for_display (file_1);
	directory_2 = nautilus_file_get_parent_uri_for_display (file_2);

	compare = nautilus_strcmp_case_breaks_ties (directory_1, directory_2);

	g_free (directory_1);
	g_free (directory_2);

	return compare;
}

static int
get_automatic_emblems_as_integer (NautilusFile *file)
{
	int integer;

	/* Keep in proper order for sorting. */

	integer = nautilus_file_is_symbolic_link (file);
	integer <<= 1;
	integer |= !nautilus_file_can_read (file);
	integer <<= 1;
	integer |= !nautilus_file_can_write (file);
	integer <<= 1;
	integer |= nautilus_file_is_in_trash (file);

	return integer;
}

static GList *
prepend_automatic_emblem_names (NautilusFile *file,
				GList *names)
{
	/* Prepend in reverse order. */

	if (nautilus_file_is_in_trash (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_TRASH));
	}
	if (!nautilus_file_can_write (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE));
	}
	if (!nautilus_file_can_read (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_READ));
	}
	if (nautilus_file_is_symbolic_link (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_SYMBOLIC_LINK));
	}

	return names;
}

static int
nautilus_file_compare_by_emblems (NautilusFile *file_1, NautilusFile *file_2)
{
	int auto_1, auto_2;
	GList *keywords_1, *keywords_2;
	GList *node_1, *node_2;
	int compare_result;
 
	auto_1 = get_automatic_emblems_as_integer (file_1);
	auto_2 = get_automatic_emblems_as_integer (file_2);
	if (auto_1 < auto_2) {
		return +1;
	} else if (auto_1 > auto_2) {
		return -1;
	}

	keywords_1 = nautilus_file_get_keywords (file_1);
	keywords_2 = nautilus_file_get_keywords (file_2);

	/* Compare each keyword. */
	compare_result = 0;
	for (node_1 = keywords_1, node_2 = keywords_2;
	     node_1 != NULL && node_2 != NULL;
	     node_1 = node_1->next, node_2 = node_2->next) {
		
		compare_result = nautilus_strcmp_case_breaks_ties (node_1->data, node_2->data);
		if (compare_result != 0) {
			break;
		}
	}

	if (compare_result == 0) {
		/* One or both is now NULL. */
		if (node_1 != NULL) {
			compare_result = -1;
		} else if (node_2 != NULL) {
			compare_result = +1;
		}
	}

	nautilus_g_list_free_deep (keywords_1);
	nautilus_g_list_free_deep (keywords_2);

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

	result = nautilus_strcmp_case_breaks_ties (type_string_1, type_string_2);

	g_free (type_string_1);
	g_free (type_string_2);

	return result;
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
	GnomeVFSDirectorySortRule rules[3];
	int compare;
	char *name_1, *name_2;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file_1), 0);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file_2), 0);

	/* FIXME: If we didn't use
	 * GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE, we could free
	 * the names in the file info (in update_info_and_name) and
	 * save some storage per-file. We might need to stop using it
	 * anyway to make international sorting work right.
	 */

	switch (sort_type) {
	case NAUTILUS_FILE_SORT_BY_NAME:
		/* Note: This used to put directories first. I
		 * thought that was counterintuitive and removed it,
		 * but I can imagine discussing this further.
		 * John Sullivan <sullivan@eazel.com>
		 */
		compare = nautilus_file_compare_by_name (file_1, file_2);
		if (compare != 0) {
			return compare;
		}
		return nautilus_file_compare_by_directory_name (file_1, file_2);
	case NAUTILUS_FILE_SORT_BY_DIRECTORY:
		compare = nautilus_file_compare_by_directory_name (file_1, file_2);
		if (compare != 0) {
			return compare;
		}
		return nautilus_file_compare_by_name (file_1, file_2);
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
			name_1 = nautilus_file_get_name (file_1);
			name_2 = nautilus_file_get_name (file_2);
			compare = nautilus_strcmp_case_breaks_ties (name_1, name_2);
			g_free (name_1);
			g_free (name_2);
		} else {
			/* FIXME bugzilla.eazel.com 2426: 
			 * We do have a name for file 2 to
                         * compare with, so we can probably do better
                         * than this for all cases.
			 */
			compare = -1;
		}
	} else if (file_2->details->info == NULL) {
		/* FIXME bugzilla.eazel.com 2426: 
		 * We do have a name for file 1 to compare
                 * with, so we can probably do better than this for
		 * all cases.
		 */
		compare = +1;
	} else {
		compare = gnome_vfs_file_info_compare_for_sort
			(file_1->details->info, file_2->details->info, rules);
	}

	return compare;
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
	return - nautilus_file_compare_for_sort (file_1, file_2, sort_type);
}

/**
 * nautilus_file_compare_name:
 * @file: A file object
 * @pattern: A string we are comparing it with
 * 
 * Return value: result of a comparison of the file name and the given pattern,
 * using the same sorting order as sort by name.
 **/
int
nautilus_file_compare_name (NautilusFile *file,
			    const char *pattern)
{
	char *name;
	int result;

	g_return_val_if_fail (pattern != NULL, -1);

	name = nautilus_file_get_name (file);
	result = nautilus_strcmp_case_breaks_ties (name, pattern);
	g_free (name);
	return result;
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
		 file->details->relative_uri,
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
		 file->details->relative_uri,
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
						  file->details->relative_uri,
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
						       file->details->relative_uri,
						       list_key,
						       list_subkey,
						       list)) {
		nautilus_file_changed (file);
	}
}


gboolean
nautilus_file_get_boolean_metadata (NautilusFile *file,
				    const char   *key,
				    gboolean      default_metadata)
{
	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);
	if (file == NULL) {
		return default_metadata;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), default_metadata);

	return nautilus_directory_get_boolean_file_metadata
		(file->details->directory,
		 file->details->relative_uri,
		 key,
		 default_metadata);
}

int
nautilus_file_get_integer_metadata (NautilusFile *file,
				    const char   *key,
				    int           default_metadata)
{
	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);
	if (file == NULL) {
		return default_metadata;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), default_metadata);

	return nautilus_directory_get_integer_file_metadata
		(file->details->directory,
		 file->details->relative_uri,
		 key,
		 default_metadata);
}


void
nautilus_file_set_boolean_metadata (NautilusFile *file,
				    const char   *key,
				    gboolean      default_metadata,
				    gboolean      metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	if (nautilus_directory_set_boolean_file_metadata (file->details->directory,
							  file->details->relative_uri,
							  key,
							  default_metadata,
							  metadata)) {
		nautilus_file_changed (file);
	}
}

void
nautilus_file_set_integer_metadata (NautilusFile *file,
				    const char   *key,
				    int           default_metadata,
				    int           metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	if (nautilus_directory_set_integer_file_metadata (file->details->directory,
							  file->details->relative_uri,
							  key,
							  default_metadata,
							  metadata)) {
		nautilus_file_changed (file);
	}
}




char *
nautilus_file_get_name (NautilusFile *file)
{
	char *name;
	
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	name = gnome_vfs_unescape_string (file->details->relative_uri, "/");
	if (name != NULL) {
		return name;
	}

	/* Fall back to the escaped form if the unescaped form is no
	 * good. This is dangerous for people who code with the name,
	 * but convenient for people who just want to display it.
	 */
	return g_strdup (file->details->relative_uri);
}
   
void             
nautilus_file_monitor_add (NautilusFile *file,
			   gconstpointer client,
			   GList *attributes)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	nautilus_directory_monitor_add_internal
		(file->details->directory, file,
		 client,
		 attributes);
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


/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an old-style gmc link or a nautilus
 * xml link file.
 */
char *
nautilus_file_get_activation_uri (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!file->details->got_activation_uri) {
		return NULL;
	}
	return file->details->activation_uri == NULL
		? nautilus_file_get_uri (file)
		: g_strdup (file->details->activation_uri);
}

/* Return the actual uri associated with the passed-in file. */
char *
nautilus_file_get_uri (NautilusFile *file)
{
	GnomeVFSURI *vfs_uri;
	char *uri;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (nautilus_file_is_self_owned (file)) {
		return g_strdup (file->details->directory->details->uri);
	}

	vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	if (vfs_uri != NULL) {
		uri = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (vfs_uri);
		return uri;
	}

	return g_strconcat (file->details->directory->details->uri,
			    "/",
			    file->details->relative_uri,
			    NULL);
}

char *
nautilus_file_get_uri_scheme (NautilusFile *file)
{
	char *colon;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (file->details->directory == NULL || 
	    file->details->directory->details->uri == NULL) {
		return NULL;
	}

	colon = strchr (file->details->directory->details->uri, ':');

	if (colon == NULL) {
		return NULL;
	}

	return g_strndup (file->details->directory->details->uri, 
			  colon - file->details->directory->details->uri);
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
	struct tm *file_time = NULL;
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
		/* Today, use special word.
		 * Note to localizers: You can look at man strftime
		 * for details on the format, but you should only use
		 * the specifiers from the C standard, not extensions.
		 * These include "%" followed by one of
		 * "aAbBcdHIjmMpSUwWxXyYZ". There are two extensions
		 * in the Nautilus version of strftime that can be
		 * used (and match GNU extensions). Putting a "-"
		 * between the "%" and any numeric directive will turn
		 * off zero padding, and putting a "_" there will use
		 * space padding instead of zero padding.
		 */
		format = _("today %-I:%M %p");
	} else if (file_date_age == 1) {
		/* Yesterday, use special word.
		 * Note to localizers: Same issues as "today" string.
		 */
		format = _("yesterday %-I:%M %p");
	} else if (file_date_age < 7) {
		/* Current week, include day of week.
		 * Note to localizers: Same issues as "today" string.
		 */
		format = _("%A %-m/%-d/%y %-I:%M %p");
	} else {
		/* Other dates.
		 * Note to localizers: Same issues as "today" string.
		 */
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
 * nautilus_file_get_directory_item_mime_types
 * 
 * Get the list of mime-types present in a directory.
 * @file: NautilusFile representing a directory. It is an error to
 * call this function on a file that is not a directory.
 * @mime_list: Place to put the list of mime-types.
 * 
 * Returns: TRUE if mime-type list is available.
 * 
 **/
gboolean
nautilus_file_get_directory_item_mime_types (NautilusFile *file,
					     GList **mime_list)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_list != NULL, FALSE);

	if (!nautilus_file_is_directory (file)
	    || !file->details->got_mime_list) {
		*mime_list = NULL;
		return FALSE;
	}

	*mime_list = nautilus_g_str_list_copy (file->details->mime_list);
	return TRUE;
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

static void
set_permissions_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GnomeVFSFileInfo *new_info,
			  gpointer callback_data)
{
	Operation *op;

	op = callback_data;
	g_assert (handle == op->handle);

	if (result == GNOME_VFS_OK && new_info != NULL) {
		nautilus_file_update_info (op->file, new_info);
	}
	operation_complete (op, result);
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
 **/
void
nautilus_file_set_permissions (NautilusFile *file, 
			       GnomeVFSFilePermissions new_permissions,
			       NautilusFileOperationCallback callback,
			       gpointer callback_data)
{
	Operation *op;
	GnomeVFSURI *vfs_uri;
	GnomeVFSFileInfo *partial_file_info;

	if (!nautilus_file_can_set_permissions (file)) {
		/* Claim that something changed even if the permission change failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old permissions as "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_ACCESS_DENIED, callback_data);
		return;
	}
			       
	/* Test the permissions-haven't-changed case explicitly
	 * because we don't want to send the file-changed signal if
	 * nothing changed.
	 */
	if (new_permissions == file->details->info->permissions) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* Set up a permission change operation. */
	op = operation_new (file, callback, callback_data);

	/* Change the file-on-disk permissions. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->permissions = new_permissions;
	vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       vfs_uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_PERMISSIONS,
				       (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
					| GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
				       set_permissions_callback, op);
	gnome_vfs_file_info_unref (partial_file_info);
	gnome_vfs_uri_unref (vfs_uri);
}

static char *
get_user_name_from_id (uid_t uid)
{
	struct passwd *password_info;
	
	/* No need to free result of getpwuid */
	password_info = getpwuid (uid);

	if (password_info == NULL) {
		return NULL;
	}
	
	return g_strdup (password_info->pw_name);
}

static gboolean
user_has_real_name (struct passwd *user)
{
	if (nautilus_str_is_empty (user->pw_gecos)) {
		return FALSE;
	}
	
	return nautilus_strcmp (user->pw_name, user->pw_gecos) != 0;
}

static char *
get_user_and_real_name_from_id (uid_t uid)
{
	struct passwd *password_info;
	
	/* No need to free result of getpwuid */
	password_info = getpwuid (uid);

	if (password_info == NULL) {
		return NULL;
	}

	if (user_has_real_name (password_info)) {
		return g_strdup_printf ("%s - %s", password_info->pw_name, password_info->pw_gecos);
	} else {
		return g_strdup (password_info->pw_name);
	}	
}

static gboolean
get_group_id_from_group_name (const char *group_name, uid_t *gid)
{
	struct group *group;

	g_assert (gid != NULL);

	group = getgrnam (group_name);

	if (group == NULL) {
		return FALSE;
	}

	*gid = group->gr_gid;

	return TRUE;
}

static gboolean
get_ids_from_user_name (const char *user_name, uid_t *uid, uid_t *gid)
{
	struct passwd *password_info;

	g_assert (uid != NULL || gid != NULL);

	password_info = getpwnam (user_name);

	if (password_info == NULL) {
		return FALSE;
	}

	if (uid != NULL) {
		*uid = password_info->pw_uid;
	}

	if (gid != NULL) {
		*gid = password_info->pw_gid;
	}

	return TRUE;
}

static gboolean
get_user_id_from_user_name (const char *user_name, uid_t *id)
{
	return get_ids_from_user_name (user_name, id, NULL);
}

static gboolean
get_group_id_from_user_name (const char *user_name, uid_t *id)
{
	return get_ids_from_user_name (user_name, NULL, id);
}

static gboolean
get_id_from_digit_string (const char *digit_string, uid_t *id)
{
	long scanned_id;

	g_assert (id != NULL);

	/* Only accept string if it has one integer with nothing
	 * afterwards.
	 */
	if (sscanf (digit_string, "%ld%*s", &scanned_id) != 1) {
		return FALSE;
	}
	*id = scanned_id;
	return TRUE;
}

/**
 * nautilus_file_can_get_owner:
 * 
 * Check whether the owner a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the owner is valid.
 */
gboolean
nautilus_file_can_get_owner (NautilusFile *file)
{
	/* Before we have info on a file, the owner is unknown. */
	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	return !info_missing (file, 0 /* FIXME bugzilla.eazel.com 644: GNOME_VFS_FILE_INFO_FIELDS_UID */);
}

/**
 * nautilus_file_get_owner_name:
 * 
 * Get the user name of the file's owner. If the owner has no
 * name, returns the userid as a string. The caller is responsible
 * for g_free-ing this string.
 * 
 * @file: The file in question.
 * 
 * Return value: A newly-allocated string.
 */
char *
nautilus_file_get_owner_name (NautilusFile *file)
{
	return nautilus_file_get_owner_as_string (file, FALSE);
}

/**
 * nautilus_file_can_set_owner:
 * 
 * Check whether the current user is allowed to change
 * the owner of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * owner of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_owner (NautilusFile *file)
{
	/* Not allowed to set the owner if we can't
	 * even read it. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nautilus_file_can_get_owner (file)) {
		return FALSE;
	}

	/* Only root is also allowed to set the owner. */
	return geteuid() == 0;
}

static void
set_owner_and_group_callback (GnomeVFSAsyncHandle *handle,
			      GnomeVFSResult result,
			      GnomeVFSFileInfo *new_info,
			      gpointer callback_data)
{
	Operation *op;

	op = callback_data;
	g_assert (handle == op->handle);

	if (result == GNOME_VFS_OK && new_info != NULL) {
		nautilus_file_update_info (op->file, new_info);
	}
	operation_complete (op, result);
}

static void
set_owner_and_group (NautilusFile *file, 
		     uid_t owner,
		     uid_t group,
		     NautilusFileOperationCallback callback,
		     gpointer callback_data)
{
	Operation *op;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *partial_file_info;
	
	/* Set up a owner-change operation. */
	op = operation_new (file, callback, callback_data);

	/* Change the file-on-disk owner. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->uid = owner;
	partial_file_info->gid = group;

	uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_OWNER,
				       (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
					| GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
				       set_owner_and_group_callback, op);
	gnome_vfs_file_info_unref (partial_file_info);
	gnome_vfs_uri_unref (uri);
}

/**
 * nautilus_file_set_owner:
 * 
 * Set the owner of a file. This will only have any effect if
 * nautilus_file_can_set_owner returns TRUE.
 * 
 * @file: The file in question.
 * @user_name_or_id: The user name to set the owner to.
 * If the string does not match any user name, and the
 * string is an integer, the owner will be set to the
 * userid represented by that integer.
 * @callback: Function called when asynch owner change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nautilus_file_set_owner (NautilusFile *file, 
			 const char *user_name_or_id,
			 NautilusFileOperationCallback callback,
			 gpointer callback_data)
{
	uid_t new_id;

	if (!nautilus_file_can_set_owner (file)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_ACCESS_DENIED, callback_data);
		return;
	}

	/* If no match treating user_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_user_id_from_user_name (user_name_or_id, &new_id)
	    && !get_id_from_digit_string (user_name_or_id, &new_id)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_BAD_PARAMETERS, callback_data);
		return;		
	}

	/* Test the owner-hasn't-changed case explicitly because we
	 * don't want to send the file-changed signal if nothing
	 * changed.
	 */
	if (new_id == file->details->info->uid) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* FIXME bugzilla.eazel.com 2427: 
	 * We can't assume that the gid is already good/read,
	 * can we? Maybe we have to precede the set_file_info with a
	 * get_file_info to fix this?
	 */
	set_owner_and_group (file,
			     new_id,
			     file->details->info->gid,
			     callback, callback_data);
}

/**
 * nautilus_get_user_names:
 * 
 * Get a list of user names. For users with a different associated 
 * "real name", the real name follows the standard user name, separated 
 * by a carriage return. The caller is responsible for freeing this list 
 * and its contents.
 */
GList *
nautilus_get_user_names (void)
{
	GList *list;
	char *name;
	struct passwd *user;

	list = NULL;
	
	setpwent ();

	while ((user = getpwent ()) != NULL) {
		if (user_has_real_name (user)) {
			name = g_strconcat (user->pw_name, "\n", user->pw_gecos, NULL);
		} else {
			name = g_strdup (user->pw_name);
		}
		list = g_list_prepend (list, name);
	}

	endpwent ();

	return nautilus_g_str_list_sort (list);
}

/**
 * nautilus_file_can_get_group:
 * 
 * Check whether the group a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the group is valid.
 */
gboolean
nautilus_file_can_get_group (NautilusFile *file)
{
	/* Before we have info on a file, the group is unknown. */
	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	return !info_missing (file, 0 /* FIXME bugzilla.eazel.com 644: GNOME_VFS_FILE_INFO_FIELDS_GID */);
}

/**
 * nautilus_file_get_group_name:
 * 
 * Get the name of the file's group. If the group has no
 * name, returns the groupid as a string. The caller is responsible
 * for g_free-ing this string.
 * 
 * @file: The file in question.
 * 
 * Return value: A newly-allocated string.
 **/
char *
nautilus_file_get_group_name (NautilusFile *file)
{
	struct group *group_info;

	/* Before we have info on a file, the owner is unknown. */
	if (info_missing (file, 0 /* FIXME bugzilla.eazel.com 644: GNOME_VFS_FILE_INFO_FIELDS_GID */)) {
		return NULL;
	}

	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getgrgid */
	group_info = getgrgid (file->details->info->gid);

	if (group_info != NULL) {
		return g_strdup (group_info->gr_name);
	}
	
	/* In the oddball case that the group name has been set to an id for which
	 * there is no defined group, return the id in string form.
	 */
	return g_strdup_printf ("%d", file->details->info->gid);
}

/**
 * nautilus_file_can_set_group:
 * 
 * Check whether the current user is allowed to change
 * the group of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * group of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_group (NautilusFile *file)
{
	uid_t user_id;

	/* Not allowed to set the permissions if we can't
	 * even read them. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nautilus_file_can_get_group (file)) {
		return FALSE;
	}

	/* Check the user. */
	user_id = geteuid();

	/* Owner is allowed to set group (with restrictions). */
	if (user_id == file->details->info->uid) {
		return TRUE;
	}

	/* Root is also allowed to set group. */
	if (user_id == 0) {
		return TRUE;
	}

	/* Nobody else is allowed. */
	return FALSE;
}

static gboolean
group_includes_user (struct group *group, const char *username)
{
	char *member;
	int member_index;
	uid_t user_gid;

	/* Check whether user is in group. Check not only member list,
	 * but also group id of username, since user is not explicitly
	 * listed in member list of own group.
	 */
	if (get_group_id_from_user_name (username, &user_gid)) {
		if (user_gid == group->gr_gid) {
			return TRUE;
		}
	}

	member_index = 0;
	while ((member = (group->gr_mem [member_index++])) != NULL) {
		if (strcmp (member, username) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

/* Get a list of group names, filtered to only the ones
 * that contain the given username. If the username is
 * NULL, returns a list of all group names.
 */
static GList *
nautilus_get_group_names_including (const char *username)
{
	GList *list;
	struct group *group;

	list = NULL;

	setgrent ();

	while ((group = getgrent ()) != NULL) {
		if (username != NULL && !group_includes_user (group, username)) {
			continue;
		}

		list = g_list_prepend (list, g_strdup (group->gr_name));
	}

	endgrent ();

	return nautilus_g_str_list_sort (list);
}

/**
 * nautilus_get_group_names:
 * 
 * Get a list of all group names.
 */
GList *
nautilus_get_group_names (void)
{
	return nautilus_get_group_names_including (NULL);
}

/**
 * nautilus_file_get_settable_group_names:
 * 
 * Get a list of all group names that the current user
 * can set the group of a specific file to.
 * 
 * @file: The NautilusFile in question.
 */
GList *
nautilus_file_get_settable_group_names (NautilusFile *file)
{
	uid_t user_id;
	char *user_name_string;
	GList *result;

	if (!nautilus_file_can_set_group (file)) {
		return NULL;
	}	

	/* Check the user. */
	user_id = geteuid();

	if (user_id == 0) {
		/* Root is allowed to set group to anything. */
		result = nautilus_get_group_names_including (NULL);
	} else if (user_id == file->details->info->uid) {
		/* Owner is allowed to set group to any that owner is member of. */
		user_name_string = get_user_name_from_id (user_id);
		result = nautilus_get_group_names_including (user_name_string);
		g_free (user_name_string);
	} else {
		g_warning ("Unhandled case in nautilus_get_settable_group_names");
		result = NULL;
	}

	return result;
}

/**
 * nautilus_file_set_group:
 * 
 * Set the group of a file. This will only have any effect if
 * nautilus_file_can_set_group returns TRUE.
 * 
 * @file: The file in question.
 * @group_name_or_id: The group name to set the owner to.
 * If the string does not match any group name, and the
 * string is an integer, the group will be set to the
 * group id represented by that integer.
 * @callback: Function called when asynch group change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nautilus_file_set_group (NautilusFile *file, 
			 const char *group_name_or_id,
			 NautilusFileOperationCallback callback,
			 gpointer callback_data)
{
	uid_t new_id;

	if (!nautilus_file_can_set_group (file)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_ACCESS_DENIED, callback_data);
		return;
	}

	/* If no match treating group_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_group_id_from_group_name (group_name_or_id, &new_id)
	    && !get_id_from_digit_string (group_name_or_id, &new_id)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		(* callback) (file, GNOME_VFS_ERROR_BAD_PARAMETERS, callback_data);
		return;		
	}

	if (new_id == file->details->info->gid) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* FIXME bugzilla.eazel.com 2427: We can't assume that the gid is already good/read,
	 * can we? Maybe we have to precede the set_file_info with a
	 * get_file_info to fix this?
	 */
	set_owner_and_group (file,
			     file->details->info->uid,
			     new_id,
			     callback, callback_data);
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
	is_link = nautilus_file_is_symbolic_link (file);

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
 * @include_real_name: Whether or not to append the real name (if any)
 * for this user after the user name.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_owner_as_string (NautilusFile *file, gboolean include_real_name)
{
	char *user_name;

	/* Before we have info on a file, the owner is unknown. */
	/* FIXME bugzilla.eazel.com 644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (info_missing (file, 0 /* FIXME bugzilla.eazel.com 644: GNOME_VFS_FILE_INFO_FIELDS_UID */)) {
		return NULL;
	}

	if (include_real_name) {
		user_name = get_user_and_real_name_from_id (file->details->info->uid);
	} else {
		user_name = get_user_name_from_id (file->details->info->uid);
	}

	if (user_name != NULL) {
		return user_name;
	}

	/* In the oddball case that the user name has been set to an id for which
	 * there is no defined user, return the id in string form.
	 */
	return g_strdup_printf ("%d", file->details->info->uid);
}

static char *
format_item_count_for_display (guint item_count, 
			       gboolean includes_directories, 
			       gboolean includes_files)
{
	g_return_val_if_fail (includes_directories || includes_files, NULL);

	if (item_count == 0) {
		return g_strdup (includes_directories
			? (includes_files ? _("0 items") : _("0 folders"))
			: _("0 files"));
	}
	if (item_count == 1) {
		return g_strdup (includes_directories
			? (includes_files ? _("1 item") : _("1 folder"))
			: _("1 file"));
	}
	return g_strdup_printf (includes_directories
		? (includes_files ? _("%u items") : _("%u folders"))
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
		return nautilus_file_get_name (file);
	}
	if (strcmp (attribute_name, "type") == 0) {
		return nautilus_file_get_type_as_string (file);
	}
	if (strcmp (attribute_name, "mime_type") == 0) {
		return nautilus_file_get_mime_type (file);
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
		return nautilus_file_get_owner_as_string (file, TRUE);
	}
	if (strcmp (attribute_name, "group") == 0) {
		return nautilus_file_get_group_name (file);
	}
	if (strcmp (attribute_name, "uri") == 0) {
		return nautilus_file_get_uri (file);
	}
	if (strcmp (attribute_name, "parent_uri") == 0) {
		return nautilus_file_get_parent_uri_for_display (file);
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
			
			result = g_strdup (count_unreadable ? _("? items") : "...");
		} else if (strcmp (attribute_name, "deep_size") == 0) {
			status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL);
			if (status == NAUTILUS_REQUEST_DONE) {
				/* This means no contents at all were readable */
				return g_strdup ("? bytes");
			}
			return g_strdup ("...");
		} else if (strcmp (attribute_name, "deep_file_count") == 0
			   || strcmp (attribute_name, "deep_directory_count") == 0
			   || strcmp (attribute_name, "deep_total_count") == 0) {
			status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL);
			if (status == NAUTILUS_REQUEST_DONE) {
				/* This means no contents at all were readable */
				return g_strdup (_("? items"));
			}
			return g_strdup ("...");
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

static const char *
get_description (NautilusFile *file)
{
	const char *mime_type, *description;

	g_assert (NAUTILUS_IS_FILE (file));

	if (file->details->info == NULL) {
		mime_type = NULL;
	} else {
		mime_type = file->details->info->mime_type;
	}

	/* FIXME: When this code was originally written, unknown types
	 * were represented by NULL, but now they are represented by
	 * "application/octet-string". Perhaps we want to check for
	 * that here, so "program" will appear more often.
	 */
	if (nautilus_strlen (mime_type) == 0) {
		/* No MIME type, anything else interesting we can say about this? */
		/* FIXME: Maybe we should always return NULL when the
		 * MIME type is unknown.
		 */
		/* If it's a directory, call it "folder" before
		 * looking at the executable bit, since the executable
		 * bit means something else for directories.
		 */
		if (nautilus_file_is_directory (file)) {
			return _("folder");
		}
		if (nautilus_file_is_executable (file)) {
			return _("program");
		}

		return NULL;
	}

	description = gnome_vfs_mime_get_description (mime_type);
	if (nautilus_strlen (description) > 0) {
		return description;
	}

	/* We want to update gnome-vfs/data/mime/gnome-vfs.keys to include 
	 * English (& localizable) versions of every mime type anyone ever sees.
	 */
	if (g_strcasecmp (mime_type, "x-directory/normal") == 0) {
		g_warning ("Can't find description even for \"x-directory/normal\". This "
			   "probably means that your gnome-vfs.keys file is in the wrong place "
			   "or isn't being found for some other reason.");
	} else {
		g_warning ("No description found for mime type \"%s\" (file is \"%s\"), tell sullivan@eazel.com", 
			   mime_type,
			   file->details->relative_uri);
	}
	return mime_type;
}

static char *
update_description_for_link (NautilusFile *file, const char *string)
{
	if (nautilus_file_is_symbolic_link (file)) {
		g_assert (!nautilus_file_is_broken_symbolic_link (file));
		if (string == NULL) {
			return g_strdup (_("link"));
		}
		/* Note to localizers: convert file type string for file 
		 * (e.g. "folder", "plain text") to file type for symbolic link 
		 * to that kind of file (e.g. "link to folder").
		 */
		return g_strdup_printf (_("link to %s"), string);
	}

	return g_strdup (string);
}

static char *
nautilus_file_get_type_as_string (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	if (nautilus_file_is_broken_symbolic_link (file)) {
		return g_strdup (_("link (broken)"));
	}
	
	return update_description_for_link (file, get_description (file));
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
 * Return this file's default mime type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The mime type.
 * 
 **/
char *
nautilus_file_get_mime_type (NautilusFile *file)
{
	if (file != NULL) {
		g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
		if (file->details->info != NULL
		    && file->details->info->mime_type != NULL) {
			return g_strdup (file->details->info->mime_type);
		}
	}
	return g_strdup ("application/octet-stream");
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
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);
	
	if (file->details->info == NULL) {
		return FALSE;
	}
	return nautilus_strcasecmp (file->details->info->mime_type,
				    mime_type) == 0;
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
	if (file == NULL) {
		return NULL;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	
	return prepend_automatic_emblem_names
		(file, nautilus_file_get_keywords (file));
}

static GList *
sort_keyword_list_and_remove_duplicates (GList *keywords)
{
	GList *p;
	GList *duplicate_link;
	
	keywords = g_list_sort (keywords, (GCompareFunc) nautilus_strcmp_case_breaks_ties);

	if (keywords != NULL) {
		p = keywords;
		while (p->next != NULL) {
			if (strcmp (p->data, p->next->data) == 0) {
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
		? FALSE : (file->details->info->flags & GNOME_VFS_FILE_FLAGS_SYMLINK);
}

/**
 * nautilus_file_is_broken_symbolic_link
 * 
 * Check if this file is a symbolic link with a missing target.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a symbolic link with a missing target.
 * 
 **/
gboolean
nautilus_file_is_broken_symbolic_link (NautilusFile *file)
{
	if (file == NULL) {
		return FALSE;
	}
		
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Non-broken symbolic links return the target's type for get_file_type. */
	return nautilus_file_get_file_type (file) == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK;
}

/**
 * nautilus_file_get_symbolic_link_target_path
 * 
 * Get the file path of the target of a symbolic link. It is an error 
 * to call this function on a file that isn't a symbolic link.
 * @file: NautilusFile representing the symbolic link in question.
 * 
 * Returns: newly-allocated copy of the file path of the target of the symbolic link.
 */
char *
nautilus_file_get_symbolic_link_target_path (NautilusFile *file)
{
	g_return_val_if_fail (nautilus_file_is_symbolic_link (file), NULL);

	return info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)
		? NULL
		: g_strdup (file->details->info->symlink_name);
}

/**
 * nautilus_file_is_nautilus_link
 * 
 * Check if this file is a nautilus link.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a nautilus link.
 * 
 **/
gboolean
nautilus_file_is_nautilus_link (NautilusFile *file)
{
	return nautilus_file_is_mime_type (file, "application/x-nautilus-link");
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
 * nautilus_file_is_in_trash
 * 
 * Check if this file is a file in trash.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file is in a trash.
 * 
 **/
gboolean
nautilus_file_is_in_trash (NautilusFile *file)
{
	GnomeVFSURI *file_vfs_uri, *trash_vfs_uri;
	gboolean result;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Use a check for the actual trash first so that the trash
	 * itself will be "in trash". There are fancier ways to do
	 * this, but lets start with this.
	 */
	if (nautilus_uri_is_trash (file->details->directory->details->uri)) {
		return TRUE;
	}

        file_vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	if (file_vfs_uri == NULL) {
		return FALSE;
	}

	result = gnome_vfs_find_directory
		(file_vfs_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		 &trash_vfs_uri, FALSE, FALSE, 0777)
		== GNOME_VFS_OK;
        if (result) {
		result = gnome_vfs_uri_equal (trash_vfs_uri, file_vfs_uri)
			|| gnome_vfs_uri_is_parent (trash_vfs_uri, file_vfs_uri, TRUE);
		gnome_vfs_uri_unref (trash_vfs_uri);
        }

        gnome_vfs_uri_unref (file_vfs_uri);

	return result;
}

GnomeVFSResult
nautilus_file_get_file_info_result (NautilusFile *file)
{
	if (!file->details->get_info_failed) {
		return GNOME_VFS_OK;
	}

	return file->details->get_info_error;
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
	char *mime_type;
	gboolean contains_text;
	
	if (file == NULL) {
		return FALSE;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	mime_type = nautilus_file_get_mime_type (file);
	contains_text = nautilus_istr_has_prefix (mime_type, "text/")
		|| (g_strcasecmp (mime_type, "application/octet-stream") == 0
		    && nautilus_file_get_file_type (file) == GNOME_VFS_FILE_TYPE_REGULAR);
	g_free (mime_type);
	
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

	if (!nautilus_file_should_get_top_left_text (file)) {
		return NULL;
	}

	/* Show " ..." in the file until we read the contents in. */
	if (!file->details->got_top_left_text) {
		if (nautilus_file_contains_text (file)) {
			return g_strdup (" ...");
		}
		return NULL;
	}

	/* Show what we read in. */
	return g_strdup (file->details->top_left_text);
}

void
nautilus_file_mark_gone (NautilusFile *file)
{
	NautilusDirectory *directory;

	file->details->is_gone = TRUE;

	update_links_if_target (file);

	/* Let the directory know it's gone. */
	directory = file->details->directory;
	if (directory->details->as_file != file) {
		nautilus_directory_remove_file (directory, file);
	}
	
	/* Drop away all the old file information. */
	if (file->details->info != NULL) {
		gnome_vfs_file_info_unref (file->details->info);
		file->details->info = NULL;
	}

	/* FIXME bugzilla.eazel.com 2429: 
	 * Maybe we can get rid of the name too eventually, but
	 * for now that would probably require too many if statements
	 * everywhere anyone deals with the name. Maybe we can give it
	 * a hard-coded "<deleted>" name or something.
	 */
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

	if (nautilus_file_is_self_owned (file)) {
		nautilus_file_emit_changed (file);
	} else {
		fake_list.data = file;
		fake_list.next = NULL;
		fake_list.prev = NULL;
		nautilus_directory_emit_change_signals_deep
			(file->details->directory, &fake_list);
	}
}

/**
 * nautilus_file_forget_activation_uri
 * 
 * Invalidate the activation URI and force a reload.
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_forget_activation_uri (NautilusFile *file)
{
	char *file_uri;
	NautilusDirectory *directory;
	
	g_free (file->details->activation_uri);
	file->details->activation_uri = NULL;

	file->details->got_activation_uri = FALSE;

	/* Inform directory that its state has changed */
	file_uri = nautilus_file_get_uri (file);
	if (file_uri != NULL) {
		directory = nautilus_directory_get (file_uri);
		if (directory != NULL) {
			nautilus_directory_async_state_changed (directory);
			nautilus_directory_unref (directory);
		}
		g_free (file_uri);
	}
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
	GList *link_files, *p;

	g_assert (NAUTILUS_IS_FILE (file));

	/* Send out a signal. */
	gtk_signal_emit (GTK_OBJECT (file), signals[CHANGED], file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (p = link_files; p != NULL; p = p->next) {
		nautilus_file_changed (NAUTILUS_FILE (p->data));
	}
	nautilus_file_list_free (link_files);
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
		(file->details->directory,
		 file,
		 file_attributes);
}			      

void
nautilus_file_call_when_ready (NautilusFile *file,
			       GList *file_attributes,
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
		 file_attributes, NULL, callback, callback_data);
}

void
nautilus_file_cancel_call_when_ready (NautilusFile *file,
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
 * nautilus_file_dump
 *
 * Debugging call, prints out the contents of the file
 * fields.
 *
 * @file: file to dump.
 **/
void
nautilus_file_dump (NautilusFile *file)
{
	long size = file->details->deep_size;
	char *uri;
	const char *file_kind;

	uri = nautilus_file_get_uri (file);
	g_print("uri: %s \n", uri);
	if (file->details->info == NULL) {
		g_print("no file info \n");
	} else if (file->details->get_info_failed) {
		g_print("failed to get file info \n");
	} else {
		g_print("size: %ld \n", size);
		switch (file->details->info->type) {
			case GNOME_VFS_FILE_TYPE_REGULAR:
				file_kind = "regular file";
				break;
			case GNOME_VFS_FILE_TYPE_DIRECTORY:
				file_kind = "folder";
				break;
			case GNOME_VFS_FILE_TYPE_FIFO:
				file_kind = "fifo";
				break;
			case GNOME_VFS_FILE_TYPE_SOCKET:
				file_kind = "socket";
				break;
			case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
				file_kind = "character device";
				break;
			case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
				file_kind = "block device";
				break;
			case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
				file_kind = "symbolic link";
				break;
			case GNOME_VFS_FILE_TYPE_UNKNOWN:
			default:
				file_kind = "unknown";
				break;
		}
		g_print("kind: %s \n", file_kind);
		if (file->details->info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			g_print("link to %s \n", file->details->info->symlink_name);
			/* FIXME bugzilla.eazel.com 2430: add following of symlinks here */
		}
		/* FIXME bugzilla.eazel.com 2431: add permissions and other useful stuff here */
	}
	g_free (uri);
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
	g_list_foreach (list, (GFunc) nautilus_file_ref, NULL);
	return list;
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
	nautilus_g_list_safe_for_each (list, (GFunc) nautilus_file_unref, NULL);
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
	nautilus_file_list_unref (list);
	g_list_free (list);
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
	return g_list_copy (nautilus_file_list_ref (list));
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
