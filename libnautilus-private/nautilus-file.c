/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-file.c: Nautilus file model.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-file.h"

#include "nautilus-directory-metafile.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-desktop-directory.h"
#include "nautilus-desktop-directory-file.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-link-desktop-file.h"
#include "nautilus-metadata.h"
#include "nautilus-module.h"
#include "nautilus-thumbnails.h"
#include "nautilus-trash-directory.h"
#include "nautilus-trash-file.h"
#include "nautilus-vfs-file.h"
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <grp.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libgnomevfs/gnome-vfs-drive.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libxml/parser.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Time in seconds to cache getpwuid results */
#define GETPWUID_CACHE_TIME (5*60)

#undef NAUTILUS_FILE_DEBUG_REF

#ifdef NAUTILUS_FILE_DEBUG_REF
extern void eazel_dump_stack_trace	(const char    *print_prefix,
					 int		num_levels);
/* from libleakcheck.so */
#endif

/* Files that start with these characters sort after files that don't. */
#define SORT_LAST_CHAR1 '.'
#define SORT_LAST_CHAR2 '#'

/* Name to use to tag metadata for the directory itself. */
#define FILE_NAME_FOR_DIRECTORY_METADATA "."

/* Name of Nautilus trash directories */
#define TRASH_DIRECTORY_NAME ".Trash"

typedef enum {
	SHOW_HIDDEN = 1 << 0,
	SHOW_BACKUP = 1 << 1
} FilterOptions;

typedef struct {
	NautilusFile *file;
	GnomeVFSAsyncHandle *handle;
	NautilusFileOperationCallback callback;
	gpointer callback_data;
	gboolean is_rename;
	gboolean use_slow_mime;
} Operation;

typedef GList * (* ModifyListFunction) (GList *list, NautilusFile *file);

enum {
	CHANGED,
	UPDATED_DEEP_COUNT_IN_PROGRESS,
	LAST_SIGNAL
};

static int date_format_pref;

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

static GHashTable *symbolic_links;

static void     nautilus_file_instance_init                  (NautilusFile          *file);
static void     nautilus_file_class_init                     (NautilusFileClass     *class);
static void     nautilus_file_info_iface_init                (NautilusFileInfoIface *iface);
static char *   nautilus_file_get_owner_as_string            (NautilusFile          *file,
							      gboolean               include_real_name);
static char *   nautilus_file_get_type_as_string             (NautilusFile          *file);
static gboolean update_info_and_name                         (NautilusFile          *file,
							      GnomeVFSFileInfo      *info,
							      gboolean info_has_slow_mime);
static char *   nautilus_file_get_display_name_nocopy        (NautilusFile          *file);
static char *   nautilus_file_get_display_name_collation_key (NautilusFile          *file);

GType
nautilus_file_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (NautilusFileClass),
			NULL, 
			NULL,
			(GClassInitFunc) nautilus_file_class_init,
			NULL,
			NULL,
			sizeof (NautilusFile),
			0,
			(GInstanceInitFunc) nautilus_file_instance_init,
		};
		
		static const GInterfaceInfo file_info_iface_info = {
			(GInterfaceInitFunc) nautilus_file_info_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_OBJECT,
					       "NautilusFile",
					       &info, 0);
		g_type_add_interface_static (type, 
					     NAUTILUS_TYPE_FILE_INFO,
					     &file_info_iface_info);
	}
	
	return type;
}

static void
nautilus_file_instance_init (NautilusFile *file)
{
	file->details = G_TYPE_INSTANCE_GET_PRIVATE ((file), NAUTILUS_TYPE_FILE, NautilusFileDetails);

	nautilus_file_invalidate_extension_info_internal (file);
}

static NautilusFile *
nautilus_file_new_from_relative_uri (NautilusDirectory *directory,
				     const char *relative_uri,
				     gboolean self_owned)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (relative_uri != NULL, NULL);
	g_return_val_if_fail (relative_uri[0] != '\0', NULL);

	if (self_owned && NAUTILUS_IS_TRASH_DIRECTORY (directory)) {
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_TRASH_FILE, NULL));
	} else if (NAUTILUS_IS_DESKTOP_DIRECTORY (directory)) {
		if (self_owned) {
			file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE, NULL));
		} else {
			file = NULL;
			g_assert_not_reached ();
		}
	} else {
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
	}

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	nautilus_directory_ref (directory);
	file->details->directory = directory;

	file->details->relative_uri = g_strdup (relative_uri);

	return file;
}

gboolean
nautilus_file_info_missing (NautilusFile *file, GnomeVFSFileInfoFields needed_mask)
{
	GnomeVFSFileInfo *info;

	if (file == NULL) {
		return TRUE;
	}
	
	info = file->details->info;
	if (info == NULL) {
		return TRUE;
	}
	return (info->valid_fields & needed_mask) != needed_mask;
}

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
	if (nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)) {
		return;
	}
	symlink_name = file->details->info->symlink_name;
	if (symlink_name == NULL) {
		return;
	}

	/* Creat the hash table first time through. */
	if (symbolic_links == NULL) {
		symbolic_links = eel_g_hash_table_new_free_at_exit
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

	file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));

#ifdef NAUTILUS_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	nautilus_directory_ref (directory);
	file->details->directory = directory;

	update_info_and_name (file, info, FALSE);
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
	char *canonical_uri, *directory_uri, *relative_uri, *file_name;
	const char *relative_uri_tmp;
	gboolean self_owned;
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	NautilusDirectory *directory;
	NautilusFile *file;

	g_return_val_if_fail (uri != NULL, NULL);

	/* Maybe we wouldn't need this if the gnome-vfs canonical
	 * stuff was strong enough.
	 */
	canonical_uri = eel_make_uri_canonical (uri);

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (canonical_uri);
	relative_uri = NULL;
	if (vfs_uri != NULL) {
		relative_uri = gnome_vfs_uri_extract_short_path_name (vfs_uri);

		/* Couldn't parse a name out of the URI: the URI must be bogus,
		 * so we'll treat it like the case where gnome_vfs_uri couldn't
		 * even create a URI.
		 */
		if (eel_str_is_empty (relative_uri)) {
			gnome_vfs_uri_unref (vfs_uri);
			vfs_uri = NULL;
			g_free (relative_uri);
			relative_uri = NULL;
		}
	}

	self_owned = FALSE;
	directory_uri = NULL;
	
	/* Make VFS version of directory URI. */
	if (vfs_uri == NULL) {
		if (eel_uri_is_desktop (uri) &&
		    !eel_uris_match (uri, EEL_DESKTOP_URI)) {
			directory_uri = g_strdup (EEL_DESKTOP_URI);
		}
	} else {
		directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
		if (directory_vfs_uri != NULL) {
			directory_uri = gnome_vfs_uri_to_string
				(directory_vfs_uri,
				 GNOME_VFS_URI_HIDE_NONE);
			gnome_vfs_uri_unref (directory_vfs_uri);
		} 
		gnome_vfs_uri_unref (vfs_uri);
	}
	
	if (directory_uri == NULL) {
		self_owned = TRUE;
		directory_uri = g_strdup (canonical_uri);
	}

	/* Get object that represents the directory. */
	directory = nautilus_directory_get_internal (directory_uri, create);
	g_free (directory_uri);

	/* Get the name for the file. */
	if (vfs_uri == NULL) {
		if (self_owned && directory != NULL) {
			file_name = nautilus_directory_get_name_for_self_as_new_file (directory);
			relative_uri = gnome_vfs_escape_string (file_name);
			g_free (file_name);
		} else if (eel_uri_is_desktop (uri)) {
			/* Special case desktop files here. They have no vfs_uri */
			relative_uri_tmp = uri + strlen (EEL_DESKTOP_URI);
			while (*relative_uri_tmp == '/') {
				relative_uri_tmp++;
			}
			relative_uri = strdup (relative_uri_tmp);
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
		file = nautilus_file_new_from_relative_uri (directory, relative_uri, self_owned);
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

gboolean
nautilus_file_is_self_owned (NautilusFile *file)
{
	return file->details->directory->details->as_file == file;
}

static void
finalize (GObject *object)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	char *uri;

	file = NAUTILUS_FILE (object);

	g_assert (file->details->operations_in_progress == NULL);

	if (file->details->is_thumbnailing) {
		uri = nautilus_file_get_uri (file);
		nautilus_thumbnail_remove_from_queue (uri);
		g_free (uri);
	}
	
	if (file->details->monitor != NULL) {
		nautilus_monitor_cancel (file->details->monitor);
	}

	nautilus_async_destroying_file (file);

	remove_from_link_hash_table (file);

	directory = file->details->directory;
	
	if (nautilus_file_is_self_owned (file)) {
		directory->details->as_file = NULL;
	} else {
		if (!file->details->is_gone) {
			nautilus_directory_remove_file (directory, file);
		}
	}

	nautilus_directory_unref (directory);
	g_free (file->details->relative_uri);
	g_free (file->details->cached_display_name);
	g_free (file->details->display_name_collation_key);
	g_free (file->details->guessed_mime_type);
	if (file->details->info != NULL) {
		gnome_vfs_file_info_unref (file->details->info);
	}
	g_free (file->details->top_left_text);
	g_free (file->details->display_name);
	g_free (file->details->custom_icon);
	g_free (file->details->activation_uri);
	g_free (file->details->compare_by_emblem_cache);
	
	eel_g_list_free_deep (file->details->mime_list);

	eel_g_list_free_deep (file->details->pending_extension_emblems);
	eel_g_list_free_deep (file->details->extension_emblems);	

	if (file->details->pending_extension_attributes) {
		g_hash_table_destroy (file->details->pending_extension_attributes);
	}
	
	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
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

	g_object_ref (file);
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

	g_object_unref (file);
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
	result = eel_format_uri_for_display (raw_uri);
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

NautilusFile *
nautilus_file_get_parent (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
		return NULL;
	}

	return nautilus_directory_get_corresponding_file (file->details->directory);
}

struct NautilusUserInfo {
	uid_t user_id;
	
	gboolean has_primary_group;
	gid_t primary_group;
	
	int num_supplementary_groups;
	gid_t supplementary_groups[NGROUPS_MAX];
};

/* Returns a pointer to the cached info, does not need freeing */
static struct NautilusUserInfo *
nautilus_file_get_user_info (void)
{
	static struct timeval cached_time;
	static struct NautilusUserInfo info;
	static gboolean has_cached_info = FALSE;
	struct passwd *password_info;
	struct timeval now;

	gettimeofday (&now, NULL);
	
	if (!has_cached_info ||
	    ((now.tv_sec - cached_time.tv_sec) > GETPWUID_CACHE_TIME)) {
		cached_time = now;
		has_cached_info = TRUE;

		info.user_id = geteuid ();
		
		info.has_primary_group = FALSE;
		/* No need to free result of getpwuid. */
		password_info = getpwuid (info.user_id);
		if (password_info) {
			info.has_primary_group = TRUE;
			info.primary_group = password_info->pw_gid;
		}
		info.num_supplementary_groups = getgroups (NGROUPS_MAX, info.supplementary_groups);
	}

	return &info;
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
	struct NautilusUserInfo *user_info;
	int i;

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

	/* This is called often. Cache the user information for five minutes */

	user_info = nautilus_file_get_user_info ();

	/* Check the user. */
	
	/* Root is not forbidden to do anything. */
	if (user_info->user_id == 0) {
		return FALSE;
	}

	/* File owner's access is governed by the owner bits. */
	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (user_info->user_id == (uid_t) file->details->info->uid) {
		return (file->details->info->permissions & owner_permission) == 0;
	}


	/* Group member's access is governed by the group bits. */
	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (user_info->has_primary_group
	    && user_info->primary_group == (gid_t) file->details->info->gid) {
		return (file->details->info->permissions & group_permission) == 0;
	}
	/* Check supplementary groups */
	for (i = 0; i < user_info->num_supplementary_groups; i++) {
		if ((gid_t) file->details->info->gid == user_info->supplementary_groups[i]) {
			return (file->details->info->permissions & group_permission) == 0;
		}
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

static gboolean
is_desktop_file (NautilusFile *file)
{
	return nautilus_file_is_mime_type (file, "application/x-desktop");
}

static gboolean
can_rename_desktop_file (NautilusFile *file)
{
	char *uri;
	gboolean res;

	uri = nautilus_file_get_uri (file);
	res = !eel_vfs_has_capability (uri, 
				       EEL_VFS_CAPABILITY_IS_REMOTE_AND_SLOW);
	g_free (uri);
	return res;
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
	gboolean can_rename;
	char *uri;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Nonexistent files can't be renamed. */
	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be renamed */
	if (nautilus_file_is_self_owned (file)) {
		return FALSE;
	}

	if (is_desktop_file (file) && !can_rename_desktop_file (file)) {
		return FALSE;
	}
	
	can_rename = TRUE;
	uri = nautilus_file_get_uri (file);

	/* Certain types of links can't be renamed */
	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		NautilusDesktopLink *link;

		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));

		can_rename = nautilus_desktop_link_can_rename (link);
		g_object_unref (link);
	}
	
	/* Nautilus trash directories cannot be renamed */
	if (eel_uri_is_trash_folder (uri)) {
		can_rename = FALSE;
	}

	g_free (uri);

	if (!can_rename) {
		return FALSE;
	}
	
	/* User must have write permissions for the parent directory. */
	parent = nautilus_file_get_parent (file);

	/* No parent directory for some reason (at root level?).
	 * Can't tell whether this file is renameable, so return TRUE.
	 */
	if (parent == NULL) {
		return TRUE;
	}
	
	can_rename = nautilus_file_can_write (parent);

	nautilus_file_unref (parent);

	return can_rename;
}

gboolean
nautilus_file_has_volume (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->has_volume;
}

gboolean
nautilus_file_has_drive (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->has_drive;
}

void
nautilus_file_set_volume (NautilusFile *file,
			  GnomeVFSVolume *volume)
{
	file->details->has_volume = volume != NULL;
	gnome_vfs_volume_ref (volume);
        g_object_set_data_full (G_OBJECT (file),
				"nautilus_file_volume",
				volume,
				(GDestroyNotify)gnome_vfs_volume_unref);
	
}

void
nautilus_file_set_drive (NautilusFile *file,
			 GnomeVFSDrive *drive)
{
	file->details->has_drive = drive != NULL;
	gnome_vfs_drive_ref (drive);
        g_object_set_data_full (G_OBJECT (file),
				"nautilus_file_drive",
				drive,
				(GDestroyNotify)gnome_vfs_drive_unref);
}


GnomeVFSVolume *
nautilus_file_get_volume (NautilusFile *file)
{
	return g_object_get_data (G_OBJECT (file),
				  "nautilus_file_volume");

}

GnomeVFSDrive *
nautilus_file_get_drive (NautilusFile *file)
{
	return g_object_get_data (G_OBJECT (file),
				  "nautilus_file_drive");
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

static gboolean
has_local_path (NautilusFile *file)
{
	return eel_str_has_prefix (file->details->directory->details->uri, "file:");
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

		update_info_and_name (op->file, new_info, op->use_slow_mime);

		/* Self-owned files store their metadata under the
		 * hard-code name "."  so there's no need to rename
		 * their metadata when they are renamed.
		 */
		if (!nautilus_file_is_self_owned (op->file)) {
			nautilus_directory_rename_file_metadata
				(directory, old_relative_uri, op->file->details->relative_uri);
		}

		g_free (old_relative_uri);

		new_uri = nautilus_file_get_uri (op->file);
		nautilus_directory_moved (old_uri, new_uri);
		g_free (new_uri);
		g_free (old_uri);


		/* the rename could have affected the display name if e.g.
		 * we're in a vfolder where the name comes from a desktop file
		 * and a rename affects the contents of the desktop file.
		 */
		if (op->file->details->display_name != NULL) {
			nautilus_file_invalidate_attributes (op->file,
							     NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
		}
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

static void
rename_guts (NautilusFile *file,
	     const char *new_name,
	     NautilusFileOperationCallback callback,
	     gpointer callback_data)
{
	Operation *op;
	GnomeVFSFileInfo *partial_file_info;
	GnomeVFSURI *vfs_uri;
	char *uri, *old_name;
	gboolean success;
	gboolean is_renameable_desktop_file;
	GnomeVFSFileInfoOptions options;
	
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (new_name != NULL);
	g_return_if_fail (callback != NULL);

	uri = nautilus_file_get_uri (file);
	is_renameable_desktop_file =
		is_desktop_file (file) && can_rename_desktop_file (file);
	
	/* Return an error for incoming names containing path separators.
	 * But not for .desktop files as '/' are allowed for them */
	if (strstr (new_name, "/") != NULL && !is_renameable_desktop_file) {
		(* callback) (file, GNOME_VFS_ERROR_NOT_PERMITTED, callback_data);
		g_free (uri);
		return;
	}
	
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
		g_free (uri);
		return;
	}

	/* Test the name-hasn't-changed case explicitly, for two reasons.
	 * (1) gnome_vfs_async_xfer returns an error if new & old are same.
	 * (2) We don't want to send file-changed signal if nothing changed.
	 */
	if (name_is (file, new_name)) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		g_free (uri);
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
		g_free (uri);
		return;
	}


	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		NautilusDesktopLink *link;

		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
		
		if (nautilus_desktop_link_rename (link, new_name)) {
			(* callback) (file, GNOME_VFS_OK, callback_data);
		} else {
			(* callback) (file, GNOME_VFS_ERROR_GENERIC, callback_data);
		}
		
		g_object_unref (link);
		g_free (uri);
		return;
	}
	
	if (is_renameable_desktop_file) {
		/* Don't actually change the name if the new name is the same.
		 * This helps for the vfolder method where this can happen and
		 * we want to minimize actual changes
		 */
		old_name = nautilus_link_desktop_file_local_get_text (uri);
		if (old_name != NULL && strcmp (new_name, old_name) == 0) {
			success = TRUE;
		} else {
			success = nautilus_link_desktop_file_local_set_text (uri, new_name);
		}
		g_free (old_name);
		g_free (uri);

		if (success) {
			nautilus_file_invalidate_attributes (file,
							     NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
			(* callback) (file, GNOME_VFS_OK, callback_data);
			return;
		} else {
			(* callback) (file, GNOME_VFS_ERROR_GENERIC, callback_data);
			return;
		}
	}
	g_free (uri);

	/* Set up a renaming operation. */
	op = operation_new (file, callback, callback_data);
	op->is_rename = TRUE;
	op->use_slow_mime = file->details->got_slow_mime_type;

	options = GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		| GNOME_VFS_FILE_INFO_FOLLOW_LINKS;
	if (op->use_slow_mime) {
		options |= GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE;
	}

	/* Do the renaming. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->name = g_strdup (new_name);
	vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       vfs_uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_NAME,
				       options,
				       GNOME_VFS_PRIORITY_DEFAULT,
				       rename_callback, op);
	gnome_vfs_file_info_unref (partial_file_info);
	gnome_vfs_uri_unref (vfs_uri);
}

void
nautilus_file_rename (NautilusFile *file,
		      const char *new_name,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	char *locale_name;
	gboolean utf8_filenames;
	const char *filename_charset;
	
	utf8_filenames = eel_get_filename_charset (&filename_charset);

	/* Note: Desktop file renaming wants utf8, even with G_BROKEN_FILENAMES */
	if (has_local_path (file) && !utf8_filenames &&
	    !is_desktop_file (file)) {
		locale_name = g_filename_from_utf8 (new_name, -1, NULL, NULL, NULL);
		if (locale_name == NULL) {
			(* callback) (file, GNOME_VFS_ERROR_NOT_PERMITTED, callback_data);
			return;
		}
		
		rename_guts (file, locale_name, callback, callback_data);
		g_free (locale_name);
		return;
	}
	
	rename_guts (file, new_name, callback, callback_data);
	return;
}

gboolean
nautilus_file_rename_in_progress (NautilusFile *file)
{
	GList *node;
	Operation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = node->next) {
		op = node->data;
		if (op->is_rename) {
			return TRUE;
		}
	}
	return FALSE;
}

void
nautilus_file_cancel (NautilusFile *file,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	GList *node, *next;
	Operation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = next) {
		next = node->next;
		op = node->data;

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

static void
update_link (NautilusFile *link_file, NautilusFile *target_file)
{
	g_assert (NAUTILUS_IS_FILE (link_file));
	g_assert (NAUTILUS_IS_FILE (target_file));
	g_assert (!nautilus_file_info_missing (link_file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME));

	/* FIXME bugzilla.gnome.org 42044: If we don't put any code
	 * here then the hash table is a waste of time.
	 */
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
		      gboolean update_name,
		      gboolean info_has_slow_mime)
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

	file->details->file_info_is_up_to_date = TRUE;
	file->details->got_slow_mime_type = info_has_slow_mime;

	if (!info_has_slow_mime || file->details->guessed_mime_type == NULL) {
		g_free (file->details->guessed_mime_type);
		file->details->guessed_mime_type = g_strdup (info->mime_type);
	}

	if (file->details->info != NULL
	    && gnome_vfs_file_info_matches (file->details->info, info)) {
		return FALSE;
	}


	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been renamed.
	 */

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
			nautilus_file_clear_cached_display_name (file);

			/* Since the name changes the old guessed mime type is now
			 * incorrect. This might be the slow mime type instead, but
			 * by now the user should have seen the warning dialog, so
			 * thats not a horrible mistake
			 */
			g_free (file->details->guessed_mime_type);
			file->details->guessed_mime_type = g_strdup (info->mime_type);
			
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
		      GnomeVFSFileInfo *info,
		      gboolean info_has_slow_mime)
{
	return update_info_internal (file, info, TRUE, info_has_slow_mime);
}

gboolean
nautilus_file_update_info (NautilusFile *file,
			   GnomeVFSFileInfo *info,
			   gboolean info_has_slow_mime)
{
	return update_info_internal (file, info, FALSE, info_has_slow_mime);
}

static gboolean
update_name_internal (NautilusFile *file,
		      const char *name,
		      gboolean in_directory)
{
	GList *node;
	char *new_relative_uri;

	g_assert (name != NULL);

	if (file->details->is_gone) {
		return FALSE;
	}

	if (name_is (file, name)) {
		return FALSE;
	}
	
	new_relative_uri = gnome_vfs_escape_string (name);

	if (file->details->info) {
		g_free (file->details->info->name);
		file->details->info->name = g_strdup (name);
	}

	node = NULL;
	if (in_directory) {
		node = nautilus_directory_begin_file_name_change
			(file->details->directory, file);
	}
	
	g_free (file->details->relative_uri);
	file->details->relative_uri = new_relative_uri;
	nautilus_file_clear_cached_display_name (file);
	
	if (in_directory) {
		nautilus_directory_end_file_name_change
			(file->details->directory, file, node);
	}


	return TRUE;
}


gboolean
nautilus_file_update_name (NautilusFile *file, const char *name)
{
	gboolean ret;
	
	ret = update_name_internal (file, name, TRUE);

	if (ret) {
		update_links_if_target (file);
	}

	return ret;
}

gboolean
nautilus_file_update_name_and_directory (NautilusFile *file, 
					 const char *name,
					 NautilusDirectory *new_directory)
{
	NautilusDirectory *old_directory;
	FileMonitors *monitors;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (file->details->directory), FALSE);
	g_return_val_if_fail (!file->details->is_gone, FALSE);
	g_return_val_if_fail (!nautilus_file_is_self_owned (file), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (new_directory), FALSE);

	old_directory = file->details->directory;
	if (old_directory == new_directory) {
		if (name) {
			return update_name_internal (file, name, TRUE);
		} else {
			return FALSE;
		}
	}

	nautilus_file_ref (file);

	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been moved.
	 */

	remove_from_link_hash_table (file);

	monitors = nautilus_directory_remove_file_monitors (old_directory, file);
	nautilus_directory_remove_file (old_directory, file);

	nautilus_directory_ref (new_directory);
	file->details->directory = new_directory;
	nautilus_directory_unref (old_directory);

	if (name) {
		update_name_internal (file, name, FALSE);
	}

	nautilus_directory_add_file (new_directory, file);
	nautilus_directory_add_file_monitors (new_directory, file, monitors);

	add_to_link_hash_table (file);

	update_links_if_target (file);

	nautilus_file_unref (file);

	return TRUE;
}

void
nautilus_file_set_directory (NautilusFile *file,
			     NautilusDirectory *new_directory)
{
	nautilus_file_update_name_and_directory (file, NULL, new_directory);
}

static Knowledge
get_item_count (NautilusFile *file,
		guint *count)
{
	gboolean known, unreadable;

	known = nautilus_file_get_directory_item_count
		(file, count, &unreadable);
	if (!known) {
		return UNKNOWN;
	}
	if (unreadable) {
		return UNKNOWABLE;
	}
	return KNOWN;
}

static Knowledge
get_size (NautilusFile *file,
	  GnomeVFSFileSize *size)
{
	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (file->details->info == NULL) {
		return UNKNOWN;
	}

	/* If we got info with no size in it, it means there is no
	 * such thing as a size as far as gnome-vfs is concerned,
	 * so "unknowable".
	 */
	if ((file->details->info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) == 0) {
		return UNKNOWABLE;
	}

	/* We have a size! */
	*size = file->details->info->size;
	return KNOWN;
}

static Knowledge
get_modification_time (NautilusFile *file,
		       time_t *modification_time)
{
	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (file->details->info == NULL) {
		return UNKNOWN;
	}

	/* If we got info with no modification time in it, it means
	 * there is no such thing as a modification time as far as
	 * gnome-vfs is concerned, so "unknowable".
	 */
	if ((file->details->info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) == 0) {
		return UNKNOWABLE;
	}

	/* We have a modification time. */
	*modification_time = file->details->info->mtime;
	return KNOWN;
}

static int
compare_directories_by_count (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with n items
	 *   Directories with 0 items
	 *   Directories with "unknowable" # of items
	 *   Directories with unknown # of items
	 */

	Knowledge count_known_1, count_known_2;
	guint count_1, count_2;

	count_known_1 = get_item_count (file_1, &count_1);
	count_known_2 = get_item_count (file_2, &count_2);

	if (count_known_1 < count_known_2) {
		return -1;
	}
	if (count_known_1 > count_known_2) {
		return +1;
	}

	/* count_known_1 and count_known_2 are equal now. Check if count
	 * details are UNKNOWABLE or UNKNOWN.
	 */ 
	if (count_known_1 == UNKNOWABLE || count_known_1 == UNKNOWN) {
		return 0;
	}

	if (count_1 > count_2) {
		return -1;
	}
	if (count_1 < count_2) {
		return +1;
	}

	return 0;
}

static int
compare_files_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Files with large sizes.
	 *   Files with smaller sizes.
	 *   Files with "unknowable" size.
	 *   Files with unknown size.
	 */

	Knowledge size_known_1, size_known_2;
	GnomeVFSFileSize size_1, size_2;

	size_known_1 = get_size (file_1, &size_1);
	size_known_2 = get_size (file_2, &size_2);

	if (size_known_1 < size_known_2) {
		return -1;
	}
	if (size_known_1 > size_known_2) {
		return +1;
	}

	/* size_known_1 and size_known_2 are equal now. Check if size 
	 * details are UNKNOWABLE or UNKNOWN 
	 */ 
	if (size_known_1 == UNKNOWABLE || size_known_1 == UNKNOWN) {
		return 0;
	}

	if (size_1 > size_2) {
		return -1;
	}
	if (size_1 < size_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with n items
	 *   Directories with 0 items
	 *   Directories with "unknowable" # of items
	 *   Directories with unknown # of items
	 *   Files with large sizes.
	 *   Files with smaller sizes.
	 *   Files with "unknowable" size.
	 *   Files with unknown size.
	 */

	gboolean is_directory_1, is_directory_2;

	is_directory_1 = nautilus_file_is_directory (file_1);
	is_directory_2 = nautilus_file_is_directory (file_2);

	if (is_directory_1 && !is_directory_2) {
		return -1;
	}
	if (is_directory_2 && !is_directory_1) {
		return +1;
	}

	if (is_directory_1) {
		return compare_directories_by_count (file_1, file_2);
	} else {
		return compare_files_by_size (file_1, file_2);
	}
}

static int
compare_by_display_name (NautilusFile *file_1, NautilusFile *file_2)
{
	char *name_1, *name_2;
	char *key_1, *key_2;
	gboolean sort_last_1, sort_last_2;
	int compare;

	name_1 = nautilus_file_get_display_name_nocopy (file_1);
	name_2 = nautilus_file_get_display_name_nocopy (file_2);

	sort_last_1 = name_1[0] == SORT_LAST_CHAR1 || name_1[0] == SORT_LAST_CHAR2;
	sort_last_2 = name_2[0] == SORT_LAST_CHAR1 || name_2[0] == SORT_LAST_CHAR2;

	if (sort_last_1 && !sort_last_2) {
		compare = +1;
	} else if (!sort_last_1 && sort_last_2) {
		compare = -1;
	} else {
		key_1 = nautilus_file_get_display_name_collation_key (file_1);
		key_2 = nautilus_file_get_display_name_collation_key (file_2);
		compare = strcmp (key_1, key_2);
	}

	return compare;
}

static int
compare_by_directory_name (NautilusFile *file_1, NautilusFile *file_2)
{
	char *directory_1, *directory_2;
	int compare;

	if (file_1->details->directory == file_2->details->directory) {
		return 0;
	}

	directory_1 = nautilus_file_get_parent_uri_for_display (file_1);
	directory_2 = nautilus_file_get_parent_uri_for_display (file_2);

	compare = eel_strcoll (directory_1, directory_2);

	g_free (directory_1);
	g_free (directory_2);

	return compare;
}

static gboolean
file_has_note (NautilusFile *file)
{
	char *note;
	gboolean res;

	note = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, NULL);
	res = note != NULL && note[0] != 0;
	g_free (note);

	return res;
}

static gboolean
file_is_desktop (NautilusFile *file)
{
	GnomeVFSURI *dir_vfs_uri;

	dir_vfs_uri = file->details->directory->details->vfs_uri;

	if (dir_vfs_uri == NULL ||
	    strcmp (dir_vfs_uri->method_string, "file") != 0) {
		return FALSE;
	}

	return nautilus_is_desktop_directory_file_escaped (dir_vfs_uri->text,
							   file->details->relative_uri);
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
	integer |= file_has_note (file);
	integer <<= 1;
	integer |= file_is_desktop (file);
	integer <<= 1;
#if TRASH_IS_FAST_ENOUGH
	integer |= nautilus_file_is_in_trash (file);
#endif

	return integer;
}

static GList *
prepend_automatic_emblem_names (NautilusFile *file,
				GList *names)
{
	/* Prepend in reverse order. */

#if TRASH_IS_FAST_ENOUGH
	if (nautilus_file_is_in_trash (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_TRASH));
	}
#endif
	if (file_is_desktop (file)) {
		names = g_list_prepend 
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_DESKTOP));
	}
	if (file_has_note (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_NOTE));
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

static void
fill_emblem_cache_if_needed (NautilusFile *file)
{
	GList *node, *keywords;
	char *scanner;
	size_t length;

	if (file->details->compare_by_emblem_cache != NULL) {
		/* Got a cache already. */
		return;
	}

	keywords = nautilus_file_get_keywords (file);

	/* Add up the keyword string lengths */
	length = 1;
	for (node = keywords; node != NULL; node = node->next) {
		length += strlen ((const char *) node->data) + 1;
	}

	/* Now that we know how large the cache struct needs to be, allocate it. */
	file->details->compare_by_emblem_cache = g_malloc (sizeof(NautilusFileSortByEmblemCache) + length);

	/* Copy them into the cache. */
	scanner = file->details->compare_by_emblem_cache->emblem_keywords;
	for (node = keywords; node != NULL; node = node->next) {
		length = strlen ((const char *) node->data) + 1;
		memcpy (scanner, (const char *) node->data, length);
		scanner += length;
	}

	/* Zero-terminate so we can tell where the list ends. */
	*scanner = 0;

	eel_g_list_free_deep (keywords);

	/* Chache the values of the automatic emblems. */
	file->details->compare_by_emblem_cache->automatic_emblems_as_integer
		= get_automatic_emblems_as_integer (file);	
}

static int
compare_by_emblems (NautilusFile *file_1, NautilusFile *file_2)
{
	const char *keyword_cache_1, *keyword_cache_2;
	size_t length;
	int compare_result;
 

	fill_emblem_cache_if_needed (file_1);
	fill_emblem_cache_if_needed (file_2);

	/* Compare automatic emblems. */
	if (file_1->details->compare_by_emblem_cache->automatic_emblems_as_integer <
		file_2->details->compare_by_emblem_cache->automatic_emblems_as_integer) {
		return +1;
	} else if (file_1->details->compare_by_emblem_cache->automatic_emblems_as_integer >
		file_2->details->compare_by_emblem_cache->automatic_emblems_as_integer) {
		return -1;
	}

	/* Compare each keyword. */
	compare_result = 0;
	keyword_cache_1 = file_1->details->compare_by_emblem_cache->emblem_keywords;
	keyword_cache_2 = file_2->details->compare_by_emblem_cache->emblem_keywords;
	for (; *keyword_cache_1 != '\0' && *keyword_cache_2 != '\0';) {
		compare_result = eel_strcoll (keyword_cache_1, keyword_cache_2);
		if (compare_result != 0) {
			return compare_result;
		}
		
		/* Advance to the next keyword */
		length = strlen (keyword_cache_1);
		keyword_cache_1 += length + 1;
		keyword_cache_2 += length + 1;
	}


	/* One or both is now NULL. */
	if (*keyword_cache_1 != '\0') {
		g_assert (*keyword_cache_2 == '\0');
		return -1;
	} else if (*keyword_cache_2 != '\0') {
		return +1;
	}

	return 0;	
}

static int
compare_by_type (NautilusFile *file_1, NautilusFile *file_2)
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
	    && eel_strcmp (file_1->details->info->mime_type,
				file_2->details->info->mime_type) == 0) {
		return 0;
	}

	type_string_1 = nautilus_file_get_type_as_string (file_1);
	type_string_2 = nautilus_file_get_type_as_string (file_2);

	result = eel_strcoll (type_string_1, type_string_2);

	g_free (type_string_1);
	g_free (type_string_2);

	return result;
}

static int
compare_by_modification_time (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Files with newer times.
	 *   Files with older times.
	 *   Files with "unknowable" times.
	 *   Files with unknown times.
	 */

	Knowledge time_known_1, time_known_2;
	time_t time_1, time_2;

	time_known_1 = get_modification_time (file_1, &time_1);
	time_known_2 = get_modification_time (file_2, &time_2);

	if (time_known_1 < time_known_2) {
		return -1;
	}
	if (time_known_1 > time_known_2) {
		return +1;
	}
	
	/* Now time_known_1 is equal to time_known_2. Check whether 
	 * we failed to get modification times for files
	 */
	if(time_known_1 == UNKNOWABLE || time_known_1 == UNKNOWN) {
		return 0;
	}
		
	if (time_1 > time_2) {
		return -1;
	}
	if (time_1 < time_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_full_path (NautilusFile *file_1, NautilusFile *file_2)
{
	int compare;

	compare = compare_by_directory_name (file_1, file_2);
	if (compare != 0) {
		return compare;
	}
	return compare_by_display_name (file_1, file_2);
}

static int
nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
					 NautilusFile *file_2,
					 gboolean directories_first)
{
	int compare;
	GnomeVFSDrive *drive1, *drive2;
	GnomeVFSVolume *volume1, *volume2;

	gboolean is_directory_1, is_directory_2;

	if (directories_first) {
		is_directory_1 = nautilus_file_is_directory (file_1);
		is_directory_2 = nautilus_file_is_directory (file_2);

		if (is_directory_1 && !is_directory_2) {
			return -1;
		}

		if (is_directory_2 && !is_directory_1) {
			return +1;
		}
	}

	/* Always sort drives/volumes separately: */
	if (file_1->details->has_drive != file_2->details->has_drive) {
		if (file_1->details->has_drive) {
			return -1;
		} else {
			return 1;
		}
	}
	if (file_1->details->has_drive) {
		drive1 = nautilus_file_get_drive (file_1);
		drive2 = nautilus_file_get_drive (file_2);
		compare = gnome_vfs_drive_compare (drive1, drive2);
		if (compare != 0) {
			return compare;
		}
	}
	
	if (file_1->details->has_volume != file_2->details->has_volume) {
		if (file_1->details->has_volume) {
			return -1;
		} else {
			return 1;
		}
	}
	if (file_1->details->has_volume) {
		volume1 = nautilus_file_get_volume (file_1);
		volume2 = nautilus_file_get_volume (file_2);
		compare = gnome_vfs_volume_compare (volume1, volume2);
		if (compare != 0) {
			return compare;
		}
	}

	return 0;
}

/**
 * nautilus_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * @directories_first: Put all directories before any non-directories
 * @reversed: Reverse the order of the items, except that
 * the directories_first flag is still respected.
 * 
 * Return value: int < 0 if @file_1 should come before file_2 in a
 * sorted list; int > 0 if @file_2 should come before file_1 in a
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort (NautilusFile *file_1,
				NautilusFile *file_2,
				NautilusFileSortType sort_type,
				gboolean directories_first,
				gboolean reversed)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}
	
	result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first);
	
	if (result == 0) {
		switch (sort_type) {
		case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
			result = compare_by_display_name (file_1, file_2);
			if (result == 0) {
				result = compare_by_directory_name (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_DIRECTORY:
			result = compare_by_full_path (file_1, file_2);
			break;
		case NAUTILUS_FILE_SORT_BY_SIZE:
			/* Compare directory sizes ourselves, then if necessary
			 * use GnomeVFS to compare file sizes.
			 */
			result = compare_by_size (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_TYPE:
			/* GnomeVFS doesn't know about our special text for certain
			 * mime types, so we handle the mime-type sorting ourselves.
			 */
			result = compare_by_type (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_MTIME:
			result = compare_by_modification_time (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_EMBLEMS:
			/* GnomeVFS doesn't know squat about our emblems, so
			 * we handle comparing them here, before falling back
			 * to tie-breakers.
			 */
			result = compare_by_emblems (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		default:
			g_return_val_if_reached (0);
		}
	}

	if (reversed) {
		result = -result;
	}
	
	return result;
}

int
nautilus_file_compare_for_sort_by_attribute     (NautilusFile                   *file_1,
						 NautilusFile                   *file_2,
						 const char                     *attribute,
						 gboolean                        directories_first,
						 gboolean                        reversed)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}

	/* Convert certain attributes into NautilusFileSortTypes and use
	 * nautilus_file_compare_for_sort()
	 */
	if (attribute == NULL || !strcmp (attribute, "name")) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
						       directories_first,
						       reversed);
	} else if (!strcmp (attribute, "size")) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_SIZE,
						       directories_first,
						       reversed);
	} else if (!strcmp (attribute, "type")) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_TYPE,
						       directories_first,
						       reversed);
	} else if (!strcmp (attribute, "modification_date") || !strcmp (attribute, "date_modified")) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_MTIME,
						       directories_first,
						       reversed);
	} else if (!strcmp (attribute, "emblems")) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_EMBLEMS,
						       directories_first,
						       reversed);
	}
	
	/* it is a normal attribute, compare by strings */

	result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first);
	
	if (result == 0) {
		char *value_1;
		char *value_2;
		
		value_1 = nautilus_file_get_string_attribute (file_1, 
							      attribute);
		value_2 = nautilus_file_get_string_attribute (file_2, 
							      attribute);

		result = strcmp (value_1, value_2);

		g_free (value_1);
		g_free (value_2);
	}

	if (reversed) {
		result = -result;
	}
	
	return result;
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
nautilus_file_compare_display_name (NautilusFile *file,
				    const char *pattern)
{
	char *name;
	int result;

	g_return_val_if_fail (pattern != NULL, -1);

	name = nautilus_file_get_display_name (file);
	result = eel_strcoll (name, pattern);
	g_free (name);
	return result;
}


gboolean
nautilus_file_is_hidden_file (NautilusFile *file)
{
	return nautilus_file_name_matches_hidden_pattern
		(file->details->relative_uri);
}

gboolean 
nautilus_file_is_backup_file (NautilusFile *file)
{
	return nautilus_file_name_matches_backup_pattern
		(file->details->relative_uri);
}

static gboolean
is_file_hidden (NautilusFile *file)
{
	return g_hash_table_lookup (file->details->directory->details->hidden_file_hash,
				    file->details->relative_uri) != NULL;
	
}

gboolean 
nautilus_file_should_show (NautilusFile *file, 
			   gboolean show_hidden,
			   gboolean show_backup)
{
	return (show_hidden || (!nautilus_file_is_hidden_file (file) && !is_file_hidden (file))) &&
		(show_backup || !nautilus_file_is_backup_file (file));

}

gboolean
nautilus_file_is_home (NautilusFile *file)
{
	GnomeVFSURI *dir_vfs_uri;

	dir_vfs_uri = file->details->directory->details->vfs_uri;

	if (dir_vfs_uri == NULL ||
	    strcmp (dir_vfs_uri->method_string, "file") != 0) {
		return FALSE;
	}

	return nautilus_is_home_directory_file_escaped (dir_vfs_uri->text,
							file->details->relative_uri);
}

gboolean
nautilus_file_is_in_desktop (NautilusFile *file)
{
	/* This handles visiting other people's desktops, but it can arguably
	 * be said that this might break and that we should lookup the passwd table.
	 */
	return strstr (file->details->directory->details->uri, "/Desktop") != NULL;
}

static gboolean
filter_hidden_and_backup_partition_callback (gpointer data,
					     gpointer callback_data)
{
	NautilusFile *file;
	FilterOptions options;

	file = NAUTILUS_FILE (data);
	options = GPOINTER_TO_INT (callback_data);

	return nautilus_file_should_show (file, 
					  options & SHOW_HIDDEN,
					  options & SHOW_BACKUP);
}

GList *
nautilus_file_list_filter_hidden_and_backup (GList    *files,
					     gboolean  show_hidden,
					     gboolean  show_backup)
{
	GList *filtered_files;
	GList *removed_files;

	/* FIXME bugzilla.gnome.org 40653: 
	 * Eventually this should become a generic filtering thingy. 
	 */

	filtered_files = nautilus_file_list_copy (files);
	filtered_files = eel_g_list_partition (filtered_files, 
					       filter_hidden_and_backup_partition_callback,
					       GINT_TO_POINTER ((show_hidden ? SHOW_HIDDEN : 0) |
								(show_backup ? SHOW_BACKUP : 0)),
					       &removed_files);
	nautilus_file_list_free (removed_files);

	return filtered_files;
}




/* We use the file's URI for the metadata for files in a directory,
 * but we use a hard-coded string for the metadata for the directory
 * itself.
 */
static const char *
get_metadata_name (NautilusFile *file)
{
	if (nautilus_file_is_self_owned (file)) {
		return FILE_NAME_FOR_DIRECTORY_METADATA;
	}
	return file->details->relative_uri;
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
		 get_metadata_name (file),
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
		 get_metadata_name (file),
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

	nautilus_directory_set_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
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

	nautilus_directory_set_file_metadata_list
		(file->details->directory,
		 get_metadata_name (file),
		 list_key,
		 list_subkey,
		 list);
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
		 get_metadata_name (file),
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
		 get_metadata_name (file),
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

	nautilus_directory_set_boolean_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
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

	nautilus_directory_set_integer_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
}

void
nautilus_file_clear_cached_display_name (NautilusFile *file)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	
	g_free (file->details->cached_display_name);
	file->details->cached_display_name = NULL;
	g_free (file->details->display_name_collation_key);
	file->details->display_name_collation_key = NULL;
}

static char *
nautilus_file_get_display_name_collation_key (NautilusFile *file)
{
	char *display_name;
	
	if (file == NULL) {
		return NULL;
	}
	
	if (file->details->display_name_collation_key != NULL) {
		return file->details->display_name_collation_key;
	}

	display_name = nautilus_file_get_display_name_nocopy (file);
	file->details->display_name_collation_key = g_utf8_collate_key (display_name, -1);

	return file->details->display_name_collation_key;
}

static char *
nautilus_file_get_display_name_nocopy (NautilusFile *file)
{
	char *name, *utf8_name, *short_name;
	gboolean validated;
	GnomeVFSURI *vfs_uri;
	const char *method;
	gboolean utf8_filenames;
	const char *filename_charset;

	if (file == NULL) {
		return NULL;
	}

	if (file->details->cached_display_name != NULL) {
		return file->details->cached_display_name;
	}

	validated = FALSE;
	
 	if (file->details->got_link_info && file->details->display_name != NULL) {
 		name = g_strdup (file->details->display_name);
	} else {
		name = nautilus_file_get_name (file);
		if (name == NULL) {
			/* Fall back to the escaped form if the unescaped form is no
			 * good. This is dangerous for people who code with the name,
			 * but convenient for people who just want to display it.
			 */
			name = g_strdup (file->details->relative_uri);
		} else {
			/* Support the G_BROKEN_FILENAMES feature of
			 * glib by using g_filename_to_utf8 to convert
			 * local filenames to UTF-8. Also do the same
			 * thing with any local filename that does not
			 * validate as good UTF-8.
			 */
			/* Keep in sync with nautilus_get_uri_shortname_for_display */
			if (has_local_path (file)) {
				utf8_filenames = eel_get_filename_charset (&filename_charset);
				if (utf8_filenames) {
					/* If not valid utf8, and filenames are utf8, test if converting
					   from the locale works */
					if (!g_utf8_validate (name, -1, NULL)) {
						utf8_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
						if (utf8_name != NULL) {
							g_free (name);
							name = utf8_name;
							/* Guaranteed to be correct utf8 here */
							validated = TRUE;
						}
					} else {
						/* name was valid, no need to re-validate */
						validated = TRUE;
					}
				} else {
					/* Try to convert from filename charset to utf8 */
					utf8_name = g_convert (name, -1, "UTF-8", filename_charset, NULL, NULL, NULL);
					if (utf8_name != NULL) {
						g_free (name);
						name = utf8_name;
						/* Guaranteed to be correct utf8 here */
						validated = TRUE;
					}
				}
			} else if (strcmp (name, "/") == 0) {
				/* Special-case the display name for roots that are not local files */
				g_free (name);
				
				vfs_uri = gnome_vfs_uri_new (file->details->directory->details->uri);
				method = nautilus_get_vfs_method_display_name (vfs_uri->method_string);
				if (method == NULL) {
					method = vfs_uri->method_string;
				}

				short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
				if (short_name == NULL ||
				    strcmp (short_name, GNOME_VFS_URI_PATH_STR) == 0) {
					name = g_strdup (method);
				} else {
					name = g_strdup_printf ("%s: %s", method, short_name);
				}
				g_free (short_name);
				
				gnome_vfs_uri_unref (vfs_uri);
			}
		}
	}

	if (!validated && !g_utf8_validate (name, -1, NULL)) {
		utf8_name = eel_make_valid_utf8 (name);
		g_free (name);
		name = utf8_name;
	}
	
	file->details->cached_display_name = name;
	return name;
}

char *
nautilus_file_get_display_name (NautilusFile *file)
{
	return g_strdup (nautilus_file_get_display_name_nocopy (file));
}

char *
nautilus_file_get_name (NautilusFile *file)
{
	return gnome_vfs_unescape_string (file->details->relative_uri, "/");
}
   
void             
nautilus_file_monitor_add (NautilusFile *file,
			   gconstpointer client,
			   NautilusFileAttributes attributes)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 monitor_add, (file, client, attributes));
}   
			   
void
nautilus_file_monitor_remove (NautilusFile *file,
			      gconstpointer client)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 monitor_remove, (file, client));
}			      


/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an desktop file or a nautilus
 * xml link file.
 */
char *
nautilus_file_get_activation_uri (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!file->details->got_link_info) {
		return NULL;
	}

	if (file->details->activation_uri != NULL) {
		return g_strdup (file->details->activation_uri);
	}
	
	return nautilus_file_get_uri (file);
}


char *
nautilus_file_get_drop_target_uri (NautilusFile *file)
{
	char *uri, *target_uri;
	NautilusDesktopLink *link;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
		
		uri = nautilus_desktop_link_get_activation_uri (link);
		g_object_unref (link);
		if (uri != NULL) {
			return uri;
		}
	}
	
	uri = nautilus_file_get_uri (file);
	
	/* Check for Nautilus link */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.gnome.org 43020: This does sync. I/O and works only locally. */
		if (!eel_vfs_has_capability (uri, EEL_VFS_CAPABILITY_IS_REMOTE_AND_SLOW)) {
			target_uri = nautilus_link_local_get_link_uri (uri);
			if (target_uri != NULL) {
				g_free (uri);
				uri = target_uri;
			}
		}
	}

	return uri;
}

char *
nautilus_file_get_custom_icon (NautilusFile *file)
{
	char *uri;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	
	uri = NULL;

	/* Metadata takes precedence */
	uri = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);

	if (uri == NULL && file->details->got_link_info) {
		uri = g_strdup (file->details->custom_icon);
	}

	return uri;
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
			    file->details->relative_uri,
			    NULL);
}

char *
nautilus_file_get_uri_scheme (NautilusFile *file)
{

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (file->details->directory == NULL || 
	    file->details->directory->details->uri == NULL) {
		return NULL;
	}

	return eel_uri_get_scheme (file->details->directory->details->uri);
}

gboolean
nautilus_file_get_date (NautilusFile *file,
			NautilusDateType date_type,
			time_t *date)
{
	if (date != NULL) {
		*date = 0;
	}

	g_return_val_if_fail (date_type == NAUTILUS_DATE_TYPE_CHANGED
			      || date_type == NAUTILUS_DATE_TYPE_ACCESSED
			      || date_type == NAUTILUS_DATE_TYPE_MODIFIED
			      || date_type == NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED, FALSE);

	if (file == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_date, (file, date_type, date));
}

static char *
nautilus_file_get_where_string (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_where_string, (file));
}

const char *TODAY_TIME_FORMATS [] = {
	/* Today, use special word.
	 * strftime patterns preceeded with the widest
	 * possible resulting string for that pattern.
	 *
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
	N_("today at 00:00:00 PM"),
	N_("today at %-I:%M:%S %p"),
	
	N_("today at 00:00 PM"),
	N_("today at %-I:%M %p"),
	
	N_("today, 00:00 PM"),
	N_("today, %-I:%M %p"),
	
	N_("today"),
	N_("today"),

	NULL
};

const char *YESTERDAY_TIME_FORMATS [] = {
	/* Yesterday, use special word.
	 * Note to localizers: Same issues as "today" string.
	 */
	N_("yesterday at 00:00:00 PM"),
	N_("yesterday at %-I:%M:%S %p"),
	
	N_("yesterday at 00:00 PM"),
	N_("yesterday at %-I:%M %p"),
	
	N_("yesterday, 00:00 PM"),
	N_("yesterday, %-I:%M %p"),
	
	N_("yesterday"),
	N_("yesterday"),

	NULL
};

const char *CURRENT_WEEK_TIME_FORMATS [] = {
	/* Current week, include day of week.
	 * Note to localizers: Same issues as "today" string.
	 * The width measurement templates correspond to
	 * the day/month name with the most letters.
	 */
	N_("Wednesday, September 00 0000 at 00:00:00 PM"),
	N_("%A, %B %-d %Y at %-I:%M:%S %p"),

	N_("Mon, Oct 00 0000 at 00:00:00 PM"),
	N_("%a, %b %-d %Y at %-I:%M:%S %p"),

	N_("Mon, Oct 00 0000 at 00:00 PM"),
	N_("%a, %b %-d %Y at %-I:%M %p"),
	
	N_("Oct 00 0000 at 00:00 PM"),
	N_("%b %-d %Y at %-I:%M %p"),
	
	N_("Oct 00 0000, 00:00 PM"),
	N_("%b %-d %Y, %-I:%M %p"),
	
	N_("00/00/00, 00:00 PM"),
	N_("%m/%-d/%y, %-I:%M %p"),

	N_("00/00/00"),
	N_("%m/%d/%y"),

	NULL
};

static char *
nautilus_file_fit_date_as_string (NautilusFile *file,
				  NautilusDateType date_type,
				  int width,
				  NautilusWidthMeasureCallback measure_callback,
				  NautilusTruncateCallback truncate_callback,
				  void *measure_context)
{
	time_t file_time_raw;
	struct tm *file_time;
	const char **formats;
	const char *width_template;
	const char *format;
	char *date_string;
	char *result;
	GDate *today;
	GDate *file_date;
	guint32 file_date_age;
	int i;

	if (!nautilus_file_get_date (file, date_type, &file_time_raw)) {
		return NULL;
	}

	file_time = localtime (&file_time_raw);

	if (date_format_pref == NAUTILUS_DATE_FORMAT_LOCALE) {
		return eel_strdup_strftime ("%c", file_time);
	} else if (date_format_pref == NAUTILUS_DATE_FORMAT_ISO) {
		return eel_strdup_strftime ("%Y-%m-%d %H:%M:%S", file_time);
	}
	
	file_date = eel_g_date_new_tm (file_time);
	
	today = g_date_new ();
	g_date_set_time (today, time (NULL));

	/* Overflow results in a large number; fine for our purposes. */
	file_date_age = (g_date_get_julian (today) -
			 g_date_get_julian (file_date));

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
		formats = TODAY_TIME_FORMATS;
	} else if (file_date_age == 1) {
		formats = YESTERDAY_TIME_FORMATS;
	} else if (file_date_age < 7) {
		formats = CURRENT_WEEK_TIME_FORMATS;
	} else {
		formats = CURRENT_WEEK_TIME_FORMATS;
	}

	/* Find the date format that just fits the required width. Instead of measuring
	 * the resulting string width directly, measure the width of a template that represents
	 * the widest possible version of a date in a given format. This is done by using M, m
	 * and 0 for the variable letters/digits respectively.
	 */
	format = NULL;
	
	for (i = 0; ; i += 2) {
		width_template = (formats [i] ? _(formats [i]) : NULL);
		if (width_template == NULL) {
			/* no more formats left */
			g_assert (format != NULL);
			
			/* Can't fit even the shortest format -- return an ellipsized form in the
			 * shortest format
			 */
			
			date_string = eel_strdup_strftime (format, file_time);

			if (truncate_callback == NULL) {
				return date_string;
			}
			
			result = (* truncate_callback) (date_string, width, measure_context);
			g_free (date_string);
			return result;
		}
		
		format = _(formats [i + 1]);

		if (measure_callback == NULL) {
			/* don't care about fitting the width */
			break;
		}

		if ((* measure_callback) (width_template, measure_context) <= width) {
			/* The template fits, this is the format we can fit. */
			break;
		}
	}
	
	return eel_strdup_strftime (format, file_time);

}

/**
 * nautilus_file_fit_modified_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date,
 * truncated to @width using the measuring and truncating callbacks.
 * @file: NautilusFile representing the file in question.
 * @width: The desired resulting string width.
 * @measure_callback: The callback used to measure the string width.
 * @truncate_callback: The callback used to truncate the string to a desired width.
 * @measure_context: Data neede when measuring and truncating.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
char *
nautilus_file_fit_modified_date_as_string (NautilusFile *file,
					   int width,
					   NautilusWidthMeasureCallback measure_callback,
					   NautilusTruncateCallback truncate_callback,
					   void *measure_context)
{
	return nautilus_file_fit_date_as_string (file, NAUTILUS_DATE_TYPE_MODIFIED,
		width, measure_callback, truncate_callback, measure_context);
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
	return nautilus_file_fit_date_as_string (file, date_type,
		0, NULL, NULL, NULL);
}

static NautilusSpeedTradeoffValue show_directory_item_count;
static NautilusSpeedTradeoffValue show_text_in_icons;

static void
show_text_in_icons_changed_callback (gpointer callback_data)
{
	show_text_in_icons = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS);
}

static void
show_directory_item_count_changed_callback (gpointer callback_data)
{
	show_directory_item_count = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS);
}

static gboolean
get_speed_tradeoff_preference_for_file (NautilusFile *file, NautilusSpeedTradeoffValue value)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	if (value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	return nautilus_file_is_local (file);
}

gboolean
nautilus_file_should_show_directory_item_count (NautilusFile *file)
{
	static gboolean show_directory_item_count_callback_added = FALSE;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Add the callback once for the life of our process */
	if (!show_directory_item_count_callback_added) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
						   show_directory_item_count_changed_callback,
						   NULL);
		show_directory_item_count_callback_added = TRUE;

		/* Peek for the first time */
		show_directory_item_count_changed_callback (NULL);
	}

	return get_speed_tradeoff_preference_for_file (file, show_directory_item_count);
}

gboolean
nautilus_file_should_get_top_left_text (NautilusFile *file)
{
	static gboolean show_text_in_icons_callback_added = FALSE;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Add the callback once for the life of our process */
	if (!show_text_in_icons_callback_added) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
						   show_text_in_icons_changed_callback,
						   NULL);
		show_text_in_icons_callback_added = TRUE;

		/* Peek for the first time */
		show_text_in_icons_changed_callback (NULL);
	}
	
	if (show_text_in_icons == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (show_text_in_icons == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	return get_speed_tradeoff_preference_for_file (file, show_text_in_icons);
}

/**
 * nautilus_file_get_directory_item_count
 * 
 * Get the number of items in a directory.
 * @file: NautilusFile representing a directory.
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
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	if (!nautilus_file_is_directory (file)) {
		return FALSE;
	}

	if (!nautilus_file_should_show_directory_item_count (file)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_item_count, (file, count, count_unreadable));
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

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NAUTILUS_REQUEST_DONE);

	if (!nautilus_file_should_show_directory_item_count (file)) {
		/* Set field so an existing value isn't treated as up-to-date
		 * when preference changes later.
		 */
		file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
		return file->details->deep_counts_status;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_deep_counts, (file,
				   directory_count,
				   file_count,
				   unreadable_directory_count,
				   total_size));
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

GnomeVFSFileInfo *
nautilus_file_peek_vfs_file_info (NautilusFile *file)
{
	return file->details->info;
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

	*mime_list = eel_g_str_list_copy (file->details->mime_list);
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
	return nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SIZE)
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
	return !nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS);
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
	if (user_id == (uid_t) file->details->info->uid) {
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
		nautilus_file_update_info (op->file, new_info, op->use_slow_mime);
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
	GnomeVFSFileInfoOptions options;

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
	op->use_slow_mime = file->details->got_slow_mime_type;

	options = GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		| GNOME_VFS_FILE_INFO_FOLLOW_LINKS;
	if (op->use_slow_mime) {
		options |= GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE;
	}
	/* Change the file-on-disk permissions. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->permissions = new_permissions;
	vfs_uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       vfs_uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_PERMISSIONS,
				       options,
				       GNOME_VFS_PRIORITY_DEFAULT,
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

static char *
get_real_name (struct passwd *user)
{
	char *locale_string, *part_before_comma, *capitalized_login_name, *real_name;

	if (user->pw_gecos == NULL) {
		return NULL;
	}

	locale_string = eel_str_strip_substring_and_after (user->pw_gecos, ",");
	if (!g_utf8_validate (locale_string, -1, NULL)) {
		part_before_comma = g_locale_to_utf8 (locale_string, -1, NULL, NULL, NULL);
		g_free (locale_string);
	} else {
		part_before_comma = locale_string;
	}

	if (!g_utf8_validate (user->pw_name, -1, NULL)) {
		locale_string = g_locale_to_utf8 (user->pw_name, -1, NULL, NULL, NULL);
	} else {
		locale_string = g_strdup (user->pw_name);
	}
	
	capitalized_login_name = eel_str_capitalize (locale_string);
	g_free (locale_string);

	if (capitalized_login_name == NULL) {
		real_name = part_before_comma;
	} else {
		real_name = eel_str_replace_substring
			(part_before_comma, "&", capitalized_login_name);
		g_free (part_before_comma);
	}


	if (eel_str_is_empty (real_name)
	    || eel_strcmp (user->pw_name, real_name) == 0
	    || eel_strcmp (capitalized_login_name, real_name) == 0) {
		g_free (real_name);
		real_name = NULL;
	}

	g_free (capitalized_login_name);

	return real_name;
}

static char *
get_user_and_real_name_from_id (uid_t uid)
{
	char *real_name, *user_and_real_name;
	struct passwd *password_info;
	
	/* No need to free result of getpwuid */
	password_info = getpwuid (uid);

	if (password_info == NULL) {
		return NULL;
	}

	real_name = get_real_name (password_info);
	if (real_name != NULL) {
		user_and_real_name = g_strdup_printf
			("%s - %s", password_info->pw_name, real_name);
	} else {
		user_and_real_name = g_strdup (password_info->pw_name);
	}
	g_free (real_name);

	return user_and_real_name;
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
get_id_from_digit_string (const char *digit_string, uid_t *id)
{
	long scanned_id;
	char c;

	g_assert (id != NULL);

	/* Only accept string if it has one integer with nothing
	 * afterwards.
	 */
	if (sscanf (digit_string, "%ld%c", &scanned_id, &c) != 1) {
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
	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	return !nautilus_file_info_missing (file, 0 /* FIXME bugzilla.gnome.org 40644: GNOME_VFS_FILE_INFO_FIELDS_UID */);
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
		nautilus_file_update_info (op->file, new_info, op->use_slow_mime);
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
	GnomeVFSFileInfoOptions options;
	
	/* Set up a owner-change operation. */
	op = operation_new (file, callback, callback_data);
	op->use_slow_mime = file->details->got_slow_mime_type;

	options = GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		| GNOME_VFS_FILE_INFO_FOLLOW_LINKS;
	if (op->use_slow_mime) {
		options |= GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE;
	}

	/* Change the file-on-disk owner. */
	partial_file_info = gnome_vfs_file_info_new ();
	partial_file_info->uid = owner;
	partial_file_info->gid = group;

	uri = nautilus_file_get_gnome_vfs_uri (file);
	gnome_vfs_async_set_file_info (&op->handle,
				       uri, partial_file_info, 
				       GNOME_VFS_SET_FILE_INFO_OWNER,
				       options,
				       GNOME_VFS_PRIORITY_DEFAULT,
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
	if (new_id == (uid_t) file->details->info->uid) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* FIXME bugzilla.gnome.org 42427: 
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
	char *real_name, *name;
	struct passwd *user;

	list = NULL;
	
	setpwent ();

	while ((user = getpwent ()) != NULL) {
		real_name = get_real_name (user);
		if (real_name != NULL) {
			name = g_strconcat (user->pw_name, "\n", real_name, NULL);
		} else {
			name = g_strdup (user->pw_name);
		}
		g_free (real_name);
		list = g_list_prepend (list, name);
	}

	endpwent ();

	return eel_g_str_list_alphabetize (list);
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
	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	return !nautilus_file_info_missing (file, 0 /* FIXME bugzilla.gnome.org 40644: GNOME_VFS_FILE_INFO_FIELDS_GID */);
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
	if (nautilus_file_info_missing (file, 0 /* FIXME bugzilla.gnome.org 40644: GNOME_VFS_FILE_INFO_FIELDS_GID */)) {
		return NULL;
	}

	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getgrgid */
	group_info = getgrgid ((gid_t) file->details->info->gid);

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
	if (user_id == (uid_t) file->details->info->uid) {
		return TRUE;
	}

	/* Root is also allowed to set group. */
	if (user_id == 0) {
		return TRUE;
	}

	/* Nobody else is allowed. */
	return FALSE;
}

/* Get a list of group names, filtered to only the ones
 * that contain the given username. If the username is
 * NULL, returns a list of all group names.
 */
static GList *
nautilus_get_group_names_for_user (void)
{
	GList *list;
	struct group *group;
	int count, i;
	gid_t gid_list[NGROUPS_MAX + 1];
	

	list = NULL;

	count = getgroups (NGROUPS_MAX + 1, gid_list);
	for (i = 0; i < count; i++) {
		group = getgrgid (gid_list[i]);
		if (group == NULL)
			break;
		
		list = g_list_prepend (list, g_strdup (group->gr_name));
	}

	return eel_g_str_list_alphabetize (list);
}

/**
 * nautilus_get_group_names:
 * 
 * Get a list of all group names.
 */
GList *
nautilus_get_all_group_names (void)
{
	GList *list;
	struct group *group;
	
	list = NULL;

	setgrent ();
	
	while ((group = getgrent ()) != NULL)
		list = g_list_prepend (list, g_strdup (group->gr_name));
	
	endgrent ();
	
	return eel_g_str_list_alphabetize (list);
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
	GList *result;

	if (!nautilus_file_can_set_group (file)) {
		return NULL;
	}	

	/* Check the user. */
	user_id = geteuid();

	if (user_id == 0) {
		/* Root is allowed to set group to anything. */
		result = nautilus_get_all_group_names ();
	} else if (user_id == (uid_t) file->details->info->uid) {
		/* Owner is allowed to set group to any that owner is member of. */
		result = nautilus_get_group_names_for_user ();
	} else {
		g_warning ("unhandled case in nautilus_get_settable_group_names");
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

	if (new_id == (gid_t) file->details->info->gid) {
		(* callback) (file, GNOME_VFS_OK, callback_data);
		return;
	}

	/* FIXME bugzilla.gnome.org 42427: We can't assume that the gid is already good/read,
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
	/* FIXME bugzilla.gnome.org 40644: 
	 * Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	if (nautilus_file_info_missing (file, 0 /* FIXME bugzilla.gnome.org 40644: GNOME_VFS_FILE_INFO_FIELDS_UID */)) {
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

	return g_strdup_printf (includes_directories
			? (includes_files 
			   ? ngettext ("%u item", "%u items", item_count) 
			   : ngettext ("%u folder", "%u folders", item_count))
			: ngettext ("%u file", "%u files", item_count), item_count);
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
	
	if (nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SIZE)) {
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
 * "date_permissions", "owner", "group", "permissions", "octal_permissions", "uri", "where",
 * "link_target", "volume", "free_space"
 * 
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute (NautilusFile *file, const char *attribute_name)
{
	char *extension_attribute;
	
	/* FIXME bugzilla.gnome.org 40646: 
	 * Use hash table and switch statement or function pointers for speed? 
	 */

	if (strcmp (attribute_name, "name") == 0) {
		return nautilus_file_get_display_name (file);
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
	if (strcmp (attribute_name, "where") == 0) {
		return nautilus_file_get_where_string (file);
	}
	if (strcmp (attribute_name, "link_target") == 0) {
		return nautilus_file_get_symbolic_link_target_path (file);
	}
	if (strcmp (attribute_name, "volume") == 0) {
		return nautilus_file_get_volume_name (file);
	}
	if (strcmp (attribute_name, "free_space") == 0) {
		return nautilus_file_get_volume_free_space (file);
	}

	extension_attribute = NULL;
	
	if (file->details->pending_extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->pending_extension_attributes, attribute_name);
	} 

	if (extension_attribute == NULL && file->details->extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->extension_attributes, attribute_name);
	}
		
	return g_strdup (extension_attribute);
}

/**
 * nautilus_file_get_string_attribute_with_default:
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
	if (result != NULL) {
		return result;
	}

	/* Supply default values for the ones we know about. */
	/* FIXME bugzilla.gnome.org 40646: 
	 * Use hash table and switch statement or function pointers for speed? 
	 */
	if (strcmp (attribute_name, "size") == 0) {
		if (!nautilus_file_should_show_directory_item_count (file)) {
			return g_strdup ("--");
		}
		count_unreadable = FALSE;
		if (nautilus_file_is_directory (file)) {
			nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable);
		}
		return g_strdup (count_unreadable ? _("? items") : "...");
	}
	if (strcmp (attribute_name, "deep_size") == 0) {
		status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL);
		if (status == NAUTILUS_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? bytes"));
		}
		return g_strdup ("...");
	}
	if (strcmp (attribute_name, "deep_file_count") == 0
	    || strcmp (attribute_name, "deep_directory_count") == 0
	    || strcmp (attribute_name, "deep_total_count") == 0) {
		status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL);
		if (status == NAUTILUS_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? items"));
		}
		return g_strdup ("...");
	}
	if (strcmp (attribute_name, "type") == 0) {
		return g_strdup (_("unknown type"));
	}
	if (strcmp (attribute_name, "mime_type") == 0) {
		return g_strdup (_("unknown MIME type"));
	}
	
	/* Fallback, use for both unknown attributes and attributes
	 * for which we have no more appropriate default.
	 */
	return g_strdup (_("unknown"));
}

/**
 * get_description:
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
	static GHashTable *warned = NULL;

	g_assert (NAUTILUS_IS_FILE (file));

	if (file->details->info == NULL) {
		return NULL;
	}
	
	mime_type = file->details->info->mime_type;
	if (eel_str_is_empty (mime_type)) {
		return NULL;
	}

	if (g_ascii_strcasecmp (mime_type, GNOME_VFS_MIME_TYPE_UNKNOWN) == 0
	    && nautilus_file_is_executable (file)) {
		return _("program");
	}

	description = gnome_vfs_mime_get_description (mime_type);
	if (!eel_str_is_empty (description)) {
		return description;
	}

	if (warned == NULL) {
		warned = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		eel_debug_call_at_shutdown_with_data ((GFreeFunc)g_hash_table_destroy, warned);
	}
	
	/* We want to update gnome-vfs/data/mime/gnome-vfs.keys to include 
	 * English (& localizable) versions of every mime type anyone ever sees.
	 */
	if (!g_hash_table_lookup (warned, mime_type)) {
		if (g_ascii_strcasecmp (mime_type, "x-directory/normal") == 0) {
			g_warning (_("Can't find description even for \"x-directory/normal\". This "
				     "probably means that your gnome-vfs.keys file is in the wrong place "
				     "or isn't being found for some other reason."));
		} else {
			g_warning (_("No description found for mime type \"%s\" (file is \"%s\"), "
				     "please tell the gnome-vfs mailing list."),
				   mime_type,
				   file->details->relative_uri);
		}
		g_hash_table_insert (warned, g_strdup (mime_type), GINT_TO_POINTER (1));
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
	if (file == NULL) {
		return GNOME_VFS_FILE_TYPE_UNKNOWN;
	}
	/* Don't use EEL_CALL_METHOD_WITH_RETURN_VALUE here, because the
	   typechecking was showing up a lot on the profiles, since this
	   is called from the sorting code */
	return ((NautilusFileClass*)G_OBJECT_GET_CLASS (file))->get_file_type (file);
}

gboolean
nautilus_file_needs_slow_mime_type (NautilusFile *file)
{
	return !file->details->got_slow_mime_type &&
		has_local_path (file);
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
	return g_strdup (GNOME_VFS_MIME_TYPE_UNKNOWN);
}

/**
 * nautilus_file_get_guessed_mime_type
 * 
 * Return the mime type that was guessed based on the extension.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The mime type.
 * 
 **/
char *
nautilus_file_get_guessed_mime_type (NautilusFile *file)
{
	if (file != NULL) {
		g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
		if (file->details->guessed_mime_type != NULL) {
			return g_strdup (file->details->guessed_mime_type);
		}
	}
	return g_strdup (GNOME_VFS_MIME_TYPE_UNKNOWN);
}

/**
 * nautilus_file_is_mime_type
 * 
 * Check whether a file is of a particular MIME type, or inherited
 * from it.
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
	
	if (file->details->info == NULL || file->details->info->mime_type == NULL) {
		return FALSE;
	}
	return (gnome_vfs_mime_type_get_equivalence (file->details->info->mime_type,
						     mime_type) != GNOME_VFS_MIME_UNRELATED);
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
	
	if (keywords != NULL) {
		keywords = eel_g_str_list_alphabetize (keywords);

		p = keywords;
		while (p->next != NULL) {
			if (strcmp ((const char *) p->data, (const char *) p->next->data) == 0) {
				duplicate_link = p->next;
				keywords = g_list_remove_link (keywords, duplicate_link);
				eel_g_list_free_deep (duplicate_link);
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
		(file, "keyword", "name");

	keywords = g_list_concat (keywords, eel_g_str_list_copy (file->details->extension_emblems));
	keywords = g_list_concat (keywords, eel_g_str_list_copy (file->details->pending_extension_emblems));

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

	/* Invalidate the emblem compare cache */
	g_free (file->details->compare_by_emblem_cache);
	file->details->compare_by_emblem_cache = NULL;
	
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	canonical_keywords = sort_keyword_list_and_remove_duplicates
		(g_list_copy (keywords));
	nautilus_file_set_metadata_list
		(file, "keyword", "name", canonical_keywords);
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
	return nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_FLAGS)
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
 * nautilus_file_get_volume_free_space
 * Get a nicely formatted char with free space on the file's volume
 * @file: NautilusFile representing the file in question.
 *
 * Returns: newly-allocated copy of file size in a formatted string
 */
char *
nautilus_file_get_volume_free_space (NautilusFile *file)
{
	char * file_uri;
	GnomeVFSFileSize free_space;
	GnomeVFSResult result;
	GnomeVFSURI * vfs_uri;

	file_uri = nautilus_file_get_uri (file);

	if (file_uri == NULL) {
		return NULL;
	}

	vfs_uri = gnome_vfs_uri_new (file_uri);
	g_free (file_uri);

	if (vfs_uri == NULL) {
		return NULL;
	}

	result = gnome_vfs_get_volume_free_space (vfs_uri, &free_space);
	gnome_vfs_uri_unref (vfs_uri);

	if (result == GNOME_VFS_OK) {
		return gnome_vfs_format_file_size_for_display (free_space);
	} else {
		return NULL;
	}
}

/**
 * nautilus_file_get_volume_name
 * Get the path of the volume the file resides on
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: newly-allocated copy of the volume name of the target file, 
 * if the volume name isn't set, it returns the mount path of the volume
 */ 
char *
nautilus_file_get_volume_name (NautilusFile *file)
{
	char *local_path;
	char *file_uri;
	char *volume_name;
	GnomeVFSVolume *volume;
	file_uri = nautilus_file_get_uri (file);
	
	local_path = gnome_vfs_get_local_path_from_uri (file_uri);
	volume = gnome_vfs_volume_monitor_get_volume_for_path (gnome_vfs_get_volume_monitor (), local_path);
	
	g_free (file_uri);
	g_free (local_path);

	if (volume != NULL) {
		volume_name = gnome_vfs_volume_get_display_name (volume);
		gnome_vfs_volume_unref (volume);
		return volume_name;
	} else {
		return NULL;
	}
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

        return nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)
                ? NULL
                : g_strdup (file->details->info->symlink_name);
}

/**
 * nautilus_file_get_symbolic_link_target_uri
 * 
 * Get the uri of the target of a symbolic link. It is an error 
 * to call this function on a file that isn't a symbolic link.
 * @file: NautilusFile representing the symbolic link in question.
 * 
 * Returns: newly-allocated copy of the uri of the target of the symbolic link.
 */
char *
nautilus_file_get_symbolic_link_target_uri (NautilusFile *file)
{
	char *file_uri;
	char *target;
	char *escaped_name;	
	
        g_return_val_if_fail (nautilus_file_is_symbolic_link (file), NULL);

	if (nautilus_file_info_missing (file, GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)) {
		return NULL;
	} else {
		file_uri = nautilus_file_get_uri (file);
		escaped_name = gnome_vfs_escape_path_string
			(file->details->info->symlink_name);

		target = gnome_vfs_uri_make_full_from_relative 
			(file_uri, escaped_name);
		g_free (file_uri);
		g_free (escaped_name);
		return target;
	}
}

/**
 * nautilus_file_is_nautilus_link
 * 
 * Check if this file is a "nautilus link", meaning a historical
 * nautilus xml link file or a desktop file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a nautilus link.
 * 
 **/
gboolean
nautilus_file_is_nautilus_link (NautilusFile *file)
{
	/* NOTE: I removed the historical link here, because i don't think we
	   even detect that mimetype anymore */
	return nautilus_file_is_mime_type (file, "application/x-desktop");
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
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return eel_uri_is_in_trash (file->details->directory->details->uri);
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
 * Returns: TRUE if @file has a text MIME type.
 * 
 **/
gboolean
nautilus_file_contains_text (NautilusFile *file)
{
	if (file == NULL) {
		return FALSE;
	}

	/* All text files inherit from text/plain */
	return nautilus_file_is_mime_type (file, "text/plain");
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
 * nautilus_file_peek_top_left_text
 * 
 * Peek at the text from the top left of the file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: NULL if there is no text readable, otherwise, the text.
 *          This string is owned by the file object and should not
 *          be kept around or freed.
 * 
 **/
char *
nautilus_file_peek_top_left_text (NautilusFile *file, gboolean *needs_loading)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!nautilus_file_should_get_top_left_text (file)) {
		if (needs_loading) {
			*needs_loading = FALSE;
		}
		return NULL;
	}
	
	if (needs_loading) {
		*needs_loading = !file->details->top_left_text_is_up_to_date;
	}

	/* Show " ..." in the file until we read the contents in. */
	if (!file->details->got_top_left_text) {
		
		if (nautilus_file_contains_text (file)) {
			return " ...";
		}
		return NULL;
	}
	
	/* Show what we read in. */
	return file->details->top_left_text;
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
	return g_strdup (nautilus_file_peek_top_left_text (file, NULL));
}


void
nautilus_file_mark_gone (NautilusFile *file)
{
	NautilusDirectory *directory;

	g_return_if_fail (!file->details->is_gone);

	file->details->is_gone = TRUE;

	update_links_if_target (file);

	/* Drop it from the symlink hash ! */
	remove_from_link_hash_table (file);

	/* Let the directory know it's gone. */
	directory = file->details->directory;
	if (!nautilus_file_is_self_owned (file)) {
		nautilus_directory_remove_file (directory, file);
	}

	/* Drop away all the old file information. */
	if (file->details->info != NULL) {
		gnome_vfs_file_info_unref (file->details->info);
		file->details->info = NULL;
	}

	/* FIXME bugzilla.gnome.org 42429: 
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
		nautilus_directory_emit_change_signals
			(file->details->directory, &fake_list);
	}
}

/**
 * nautilus_file_updated_deep_count_in_progress
 * 
 * Notify clients that a newer deep count is available for
 * the directory in question.
 */
void
nautilus_file_updated_deep_count_in_progress (NautilusFile *file) {
	GList *link_files, *node;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (nautilus_file_is_directory (file));

	/* Send out a signal. */
	g_signal_emit (file, signals[UPDATED_DEEP_COUNT_IN_PROGRESS], 0, file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (node = link_files; node != NULL; node = node->next) {
		nautilus_file_updated_deep_count_in_progress (NAUTILUS_FILE (node->data));
	}
	nautilus_file_list_free (link_files);	
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


	/* Invalidate the emblem compare cache. -- This is not the cleanest
	 * place to do it but it is the one guaranteed bottleneck through
	 * which all change notifications pass.
	 */
	g_free (file->details->compare_by_emblem_cache);
	file->details->compare_by_emblem_cache = NULL;

	/* Send out a signal. */
	g_signal_emit (file, signals[CHANGED], 0, file);

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
 * nautilus_file_is_not_yet_confirmed
 * 
 * Check if we're in a state where we don't know if a file really
 * exists or not, before the initial I/O is complete.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nautilus_file_is_not_yet_confirmed (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->info == NULL;
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
 * @file_attributes: A bit-mask with the desired information.
 * 
 * Return value: TRUE if all of the specified attributes are currently readable.
 */
gboolean
nautilus_file_check_if_ready (NautilusFile *file,
			      NautilusFileAttributes file_attributes)
{
	/* To be parallel with call_when_ready, return
	 * TRUE for NULL file.
	 */
	if (file == NULL) {
		return TRUE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 check_if_ready, (file, file_attributes));
}			      

void
nautilus_file_call_when_ready (NautilusFile *file,
			       NautilusFileAttributes file_attributes,
			       NautilusFileCallback callback,
			       gpointer callback_data)

{
	if (file == NULL) {
		(* callback) (file, callback_data);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 call_when_ready, (file, file_attributes, 
				   callback, callback_data));
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

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 cancel_call_when_ready, (file, callback, callback_data));
}

static void
invalidate_directory_count (NautilusFile *file)
{
	file->details->directory_count_is_up_to_date = FALSE;
}

static void
invalidate_deep_counts (NautilusFile *file)
{
	file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
}

static void
invalidate_mime_list (NautilusFile *file)
{
	file->details->mime_list_is_up_to_date = FALSE;
}

static void
invalidate_top_left_text (NautilusFile *file)
{
	file->details->top_left_text_is_up_to_date = FALSE;
}

static void
invalidate_file_info (NautilusFile *file)
{
	file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_slow_mime_type (NautilusFile *file)
{
	file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_link_info (NautilusFile *file)
{
	file->details->link_info_is_up_to_date = FALSE;
}

void
nautilus_file_invalidate_extension_info_internal (NautilusFile *file)
{
	file->details->pending_info_providers =
		nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_INFO_PROVIDER);
	if (!file->details->pending_extension_attributes) {
		file->details->pending_extension_attributes = 
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify)g_free, 
					       (GDestroyNotify)g_free);
	}
}

void
nautilus_file_invalidate_attributes_internal (NautilusFile *file,
					      NautilusFileAttributes file_attributes)
{
	Request request;

	if (file == NULL) {
		return;
	}

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		/* Desktop icon files are always up to date.
		 * If we invalidate their attributes they
		 * will lose data, so we just ignore them.
		 */
		return;
	}
	
	nautilus_directory_set_up_request (&request, file_attributes);

	if (request.directory_count) {
		invalidate_directory_count (file);
	}
	if (request.deep_count) {
		invalidate_deep_counts (file);
	}
	if (request.mime_list) {
		invalidate_mime_list (file);
	}
	if (request.file_info) {
		invalidate_file_info (file);
	}
	if (request.slow_mime_type) {
		invalidate_slow_mime_type (file);
	}
	if (request.top_left_text) {
		invalidate_top_left_text (file);
	}
	if (request.link_info) {
		invalidate_link_info (file);
	}
	if (request.extension_info) {
		nautilus_file_invalidate_extension_info_internal (file);
	}

	/* FIXME bugzilla.gnome.org 45075: implement invalidating metadata */
}

gboolean
nautilus_file_has_open_window (NautilusFile *file)
{
	return file->details->has_open_window;
}

void
nautilus_file_set_has_open_window (NautilusFile *file,
				   gboolean has_open_window)
{
	has_open_window = (has_open_window != FALSE);

	if (file->details->has_open_window != has_open_window) {
		file->details->has_open_window = has_open_window;
		nautilus_file_changed (file);
	}
}


gboolean
nautilus_file_is_thumbnailing (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->is_thumbnailing;
}

void
nautilus_file_set_is_thumbnailing (NautilusFile *file,
				   gboolean is_thumbnailing)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	
	file->details->is_thumbnailing = is_thumbnailing;
}


/**
 * nautilus_file_invalidate_attributes
 * 
 * Invalidate the specified attributes and force a reload.
 * @file: NautilusFile representing the file in question.
 * @file_attributes: attributes to froget.
 **/

void
nautilus_file_invalidate_attributes (NautilusFile *file,
				     NautilusFileAttributes file_attributes)
{
	/* Cancel possible in-progress loads of any of these attributes */
	nautilus_directory_cancel_loading_file_attributes (file->details->directory,
							   file,
							   file_attributes);
	
	/* Actually invalidate the values */
	nautilus_file_invalidate_attributes_internal (file, file_attributes);

	nautilus_directory_add_file_to_work_queue (file->details->directory, file);
	
	/* Kick off I/O if necessary */
	nautilus_directory_async_state_changed (file->details->directory);
}

NautilusFileAttributes 
nautilus_file_get_all_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI |
		NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES |
		NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON |
		NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES |
		NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY |
		NAUTILUS_FILE_ATTRIBUTE_METADATA |
		NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT |
		NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME |
		NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_VOLUMES;
}

void
nautilus_file_invalidate_all_attributes (NautilusFile *file)
{
	NautilusFileAttributes all_attributes;

	all_attributes = nautilus_file_get_all_attributes ();
	nautilus_file_invalidate_attributes (file, all_attributes);
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
	g_print ("uri: %s \n", uri);
	if (file->details->info == NULL) {
		g_print ("no file info \n");
	} else if (file->details->get_info_failed) {
		g_print ("failed to get file info \n");
	} else {
		g_print ("size: %ld \n", size);
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
		g_print ("kind: %s \n", file_kind);
		if (file->details->info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			g_print ("link to %s \n", file->details->info->symlink_name);
			/* FIXME bugzilla.gnome.org 42430: add following of symlinks here */
		}
		/* FIXME bugzilla.gnome.org 42431: add permissions and other useful stuff here */
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
	g_list_foreach (list, (GFunc) nautilus_file_unref, NULL);
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

static int
compare_by_display_name_cover (gconstpointer a, gconstpointer b)
{
	return compare_by_display_name (NAUTILUS_FILE (a), NAUTILUS_FILE (b));
}

/**
 * nautilus_file_list_sort_by_display_name
 * 
 * Sort the list of files by file name.
 * @list: GList of files.
 **/
GList *
nautilus_file_list_sort_by_display_name (GList *list)
{
	return g_list_sort (list, compare_by_display_name_cover);
}

typedef struct 
{
	GList *file_list;
	GList *remaining_files;
	NautilusFileListCallback callback;
	gpointer callback_data;
} FileListReadyData;

static void
file_list_file_ready_callback (NautilusFile *file,
			       gpointer user_data)
{
	FileListReadyData *data;
	
	data = user_data;
	data->remaining_files = g_list_remove (data->remaining_files, file);
	
	if (data->remaining_files == NULL) {
		if (data->callback) {
			(*data->callback) (data->file_list, data->callback_data);
		}
		
		nautilus_file_list_free (data->file_list);
		g_free (data);
	}
}

void
nautilus_file_list_call_when_ready (GList *file_list,
				    NautilusFileAttributes attributes,
				    NautilusFileListCallback callback,
				    gpointer callback_data)
{
	GList *l;
	FileListReadyData *data;
	
	g_return_if_fail (file_list != NULL);

	data = g_new0 (FileListReadyData, 1);
	data->file_list = nautilus_file_list_copy (file_list);
	data->remaining_files = g_list_copy (file_list);
	data->callback = callback;
	data->callback_data = callback_data;
	
	for (l = file_list; l != NULL; l = l->next) {
		nautilus_file_call_when_ready (NAUTILUS_FILE (l->data),
					       attributes,
					       file_list_file_ready_callback,
					       data);
	}
}

static char *
try_to_make_utf8 (const char *text, int *length)
{
	static const char *encodings_to_try[2];
	static int n_encodings_to_try = 0;
        gsize converted_length;
        GError *conversion_error;
	char *utf8_text;
	int i;
	
	if (n_encodings_to_try == 0) {
		const char *charset;
		gboolean charset_is_utf8;
		
		charset_is_utf8 = g_get_charset (&charset);
		if (!charset_is_utf8) {
			encodings_to_try[n_encodings_to_try++] = charset;
		}
        
		if (g_ascii_strcasecmp (charset, "ISO-8859-1") != 0) {
			encodings_to_try[n_encodings_to_try++] = "ISO-8859-1";
		}
	}

        utf8_text = NULL;
	for (i = 0; i < n_encodings_to_try; i++) {
		conversion_error = NULL;
		utf8_text = g_convert (text, *length, 
					   "UTF-8", encodings_to_try[i],
					   NULL, &converted_length, &conversion_error);
		if (utf8_text != NULL) {
			*length = converted_length;
			break;
		}
		g_error_free (conversion_error);
	}
	
	return utf8_text;
}



/* Extract the top left part of the read-in text. */
char *
nautilus_extract_top_left_text (const char *text,
				int length)
{
        GString* buffer;
	const gchar *in;
	const gchar *end;
	int line, i;
	gunichar c;
	char *text_copy;
	const char *utf8_end;
	gboolean validated;

        text_copy = NULL;
        if (text != NULL) {
		/* Might be a partial utf8 character at the end if we didn't read whole file */
		validated = g_utf8_validate (text, length, &utf8_end);
		if (!validated &&
		    !(length >= NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_BYTES &&
		      text + length - utf8_end < 6)) {
			text_copy = try_to_make_utf8 (text, &length);
			text = text_copy;
		} else if (!validated) {
			length = utf8_end - text;
		}
        }

	if (text == NULL || length == 0) {
		return NULL;
	}

	buffer = g_string_new ("");
	end = text + length; in = text;

	for (line = 0; line < NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES; line++) {
		/* Extract one line. */
		for (i = 0; i < NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE; ) {
			if (*in == '\n') {
				break;
			}
			
			c = g_utf8_get_char (in);
			
			if (g_unichar_isprint (c)) {
				g_string_append_unichar (buffer, c);
				i++;
			}
			
			in = g_utf8_next_char (in);
			if (in == end) {
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
		g_string_append_c(buffer, '\n');
	}
 done:
	g_free (text_copy);
 
	return g_string_free(buffer, FALSE);
}

static void
nautilus_file_class_init (NautilusFileClass *class)
{
	parent_class = g_type_class_peek_parent (class);

	G_OBJECT_CLASS (class)->finalize = finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusFileClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[UPDATED_DEEP_COUNT_IN_PROGRESS] =
		g_signal_new ("updated_deep_count_in_progress",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusFileClass, updated_deep_count_in_progress),
		              NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	
	g_type_class_add_private (class, sizeof (NautilusFileDetails));


	eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_DATE_FORMAT,
				       &date_format_pref);
}

static GnomeVFSFileInfo *
nautilus_file_get_vfs_file_info (NautilusFile *file)
{
	return gnome_vfs_file_info_dup (file->details->info);
}

static void
nautilus_file_add_emblem (NautilusFile *file,
			  const char *emblem_name)
{
	if (file->details->pending_info_providers) {
		file->details->pending_extension_emblems = g_list_prepend (file->details->pending_extension_emblems,
									   g_strdup (emblem_name));
	} else {
		file->details->extension_emblems = g_list_prepend (file->details->extension_emblems,
								   g_strdup (emblem_name));
	}

	nautilus_file_changed (file);
}

static void
nautilus_file_add_string_attribute (NautilusFile *file,
				    const char *attribute_name,
				    const char *value)
{
	if (file->details->pending_info_providers) {
		g_hash_table_insert (file->details->pending_extension_attributes,
				     g_strdup (attribute_name),
				     g_strdup (value));
	} else {
		g_hash_table_insert (file->details->extension_attributes,
				     g_strdup (attribute_name),
				     g_strdup (value));
	}

	nautilus_file_changed (file);
}

static void
nautilus_file_invalidate_extension_info (NautilusFile *file)
{
	nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO);
}

void
nautilus_file_info_providers_done (NautilusFile *file)
{
	eel_g_list_free_deep (file->details->extension_emblems);
	file->details->extension_emblems = file->details->pending_extension_emblems;
	file->details->pending_extension_emblems = NULL;

	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}
	
	file->details->extension_attributes = file->details->pending_extension_attributes;
	file->details->pending_extension_attributes = NULL;

	nautilus_file_changed (file);
}

static void     
nautilus_file_info_iface_init (NautilusFileInfoIface *iface)
{
	iface->is_gone = nautilus_file_is_gone;
	iface->get_name = nautilus_file_get_name;
	iface->get_uri = nautilus_file_get_uri;
	iface->get_parent_uri = nautilus_file_get_parent_uri;
	iface->get_uri_scheme = nautilus_file_get_uri_scheme;
	iface->get_mime_type = nautilus_file_get_mime_type;
	iface->is_mime_type = nautilus_file_is_mime_type;
	iface->is_directory = nautilus_file_is_directory;
	iface->get_vfs_file_info = nautilus_file_get_vfs_file_info;
	iface->add_emblem = nautilus_file_add_emblem;
	iface->get_string_attribute = nautilus_file_get_string_attribute;
	iface->add_string_attribute = nautilus_file_add_string_attribute;
	iface->invalidate_extension_info = nautilus_file_invalidate_extension_info;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file (void)
{
	NautilusFile *file_1;
	NautilusFile *file_2;
	GList *list;

        /* refcount checks */

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);

	file_1 = nautilus_file_get ("file:///home/");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1->details->directory)->ref_count, 1);
        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 1);

	nautilus_file_unref (file_1);

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

        list = NULL;
        list = g_list_prepend (list, file_1);
        list = g_list_prepend (list, file_2);

        nautilus_file_list_ref (list);
        
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 2);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 2);

	nautilus_file_list_unref (list);
        
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	nautilus_file_list_free (list);

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	

        /* name checks */
	file_1 = nautilus_file_get ("file:///home/");

	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home/") == file_1, TRUE);
	nautilus_file_unref (file_1);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home") == file_1, TRUE);
	nautilus_file_unref (file_1);

	nautilus_file_unref (file_1);

	file_1 = nautilus_file_get ("file:///home");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");
	nautilus_file_unref (file_1);

#if 0
	/* ALEX: I removed this, because it was breaking distchecks.
	 * It used to work, but when canonical uris changed from
	 * foo: to foo:/// it broke. I don't expect it to matter
	 * in real life */
	file_1 = nautilus_file_get (":");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), ":");
	nautilus_file_unref (file_1);
#endif

	file_1 = nautilus_file_get ("eazel:");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "eazel");
	nautilus_file_unref (file_1);

	file_1 = nautilus_file_get (EEL_TRASH_URI);
	EEL_CHECK_STRING_RESULT (nautilus_file_get_display_name (file_1), _("Trash"));
	nautilus_file_unref (file_1);

	/* sorting */
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE) < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, TRUE) > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, TRUE, FALSE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, TRUE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, TRUE, TRUE) == 0, TRUE);

	nautilus_file_unref (file_1);
	nautilus_file_unref (file_2);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
