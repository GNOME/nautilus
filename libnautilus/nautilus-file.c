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

#include <grp.h>
#include <pwd.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <stdlib.h>
#include <xmlmemory.h>

#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"
#include "nautilus-directory-private.h"

typedef enum {
	NAUTILUS_DATE_TYPE_MODIFIED,
	NAUTILUS_DATE_TYPE_CHANGED,
	NAUTILUS_DATE_TYPE_ACCESSED
} NautilusDateType;

/* FIXME: This hack needs to die eventually. See comments with function */
static int   get_directory_item_count_hack           (NautilusFile         *file,
						      gboolean              ignore_invisible_items);

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
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	char *directory_uri;
	NautilusDirectory *directory;
	NautilusFile *file;

	/* Get info on the file. */
	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, file_info,
					  GNOME_VFS_FILE_INFO_GETMIMETYPE
					  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  			  | GNOME_VFS_FILE_INFO_FOLLOWLINKS, NULL);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Make VFS version of directory URI. */
	directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	if (directory_vfs_uri == NULL) {
		return NULL;
	}

	/* Make text version of directory URI. */
	directory_uri = gnome_vfs_uri_to_string (directory_vfs_uri,
						 GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (directory_vfs_uri);

	/* Get object that represents the directory. */
	directory = nautilus_directory_get (directory_uri);
	g_free (directory_uri);
	if (directory == NULL) {
		return NULL;
	}

	file = nautilus_directory_find_file (directory, file_info->name);
	if (file == NULL) {
		file = nautilus_directory_new_file (directory, file_info);
		directory->details->files =
			g_list_append (directory->details->files, file);
	}

	gnome_vfs_file_info_unref (file_info);
	nautilus_file_ref (file);
	gtk_object_unref (GTK_OBJECT (directory));
	
	return file;
}

void
nautilus_file_ref (NautilusFile *file)
{
	g_return_if_fail (file != NULL);

	g_assert (file->ref_count < G_MAXINT);
	g_assert (file->directory != NULL);

	/* Increment the ref count. */
	if (file->ref_count++ != 0) {
		return;
	}

	/* As soon as someone other than the directory holds a ref, 
	 * we need to hold the directory too. */
	gtk_object_ref (GTK_OBJECT (file->directory));
}

void
nautilus_file_unref (NautilusFile *file)
{
        gboolean goner;

	g_return_if_fail (file != NULL);

	g_assert (file->ref_count != 0);
	g_assert (file->directory != NULL);

	/* Decrement the ref count. */
	if (--file->ref_count != 0) {
		return;
	}

        goner = file->is_gone;
        
	/* No references left, so it's time to release our hold on the directory. */
	gtk_object_unref (GTK_OBJECT (file->directory));

	/* Files that were deleted aren't referenced by the directory,
	 * and need to be freed explicitly.
	 */
	if (goner) {
		nautilus_file_free (file);
	}
}

void
nautilus_file_free (NautilusFile *file)
{
	g_assert (file->ref_count == 0);

	/* Destroy the file object. */
	gnome_vfs_file_info_unref (file->info);
	g_free (file);
}

static int
nautilus_file_compare_by_size_with_directories (NautilusFile *file_1, NautilusFile *file_2)
{
	gboolean is_directory_1;
	gboolean is_directory_2;
	int item_count_1;
	int item_count_2;

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
	/* FIXME: get_directory_item_count_hack is slow, and calling
	 * it for every pairwise comparison here is nasty. Need to
	 * change this to (not-yet-existent) architecture where the
	 * item count can be calculated once in a deferred way, and
	 * then stored or cached.
	 */
	item_count_1 = get_directory_item_count_hack (file_1, FALSE);
	item_count_2 = get_directory_item_count_hack (file_2, FALSE);

	if (item_count_1 < item_count_2) {
		return -1;
	}

	if (item_count_2 < item_count_1) {
		return +1;
	}

	return 0;
}

static int
nautilus_file_compare_by_type (NautilusFile *file_1, NautilusFile *file_2)
{
	gboolean is_directory_1;
	gboolean is_directory_2;
	char * type_string_1;
	char * type_string_2;
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

	if (nautilus_strcmp (file_1->info->mime_type, file_2->info->mime_type) == 0) {
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

	g_return_val_if_fail (file_1 != NULL, 0);
	g_return_val_if_fail (file_2 != NULL, 0);
	g_return_val_if_fail (sort_type != NAUTILUS_FILE_SORT_NONE, 0);

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
		compare = nautilus_file_compare_by_size_with_directories (file_1, file_2);
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
	default:
		g_assert_not_reached ();
		return 0;
	}

	if (reversed) {
		return gnome_vfs_file_info_compare_for_sort_reversed (file_1->info,
								      file_2->info,
								      rules);
	} else {
		return gnome_vfs_file_info_compare_for_sort (file_1->info,
							     file_2->info,
							     rules);
	}
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

char *
nautilus_file_get_metadata (NautilusFile *file,
			    const char *tag,
			    const char *default_metadata)
{
	g_return_val_if_fail (file != NULL, NULL);

	return nautilus_directory_get_file_metadata (file->directory,
						     file->info->name,
						     tag,
						     default_metadata);
}

void
nautilus_file_set_metadata (NautilusFile *file,
			    const char *tag,
			    const char *default_metadata,
			    const char *metadata)
{
	g_return_if_fail (file != NULL);

	nautilus_directory_set_file_metadata (file->directory,
					      file->info->name,
					      tag,
					      default_metadata,
					      metadata);
}

char *
nautilus_file_get_name (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	g_assert (file->directory == NULL || NAUTILUS_IS_DIRECTORY (file->directory));
	g_assert (file->info->name != NULL);
	g_assert (file->info->name[0] != '\0');

	return g_strdup (file->info->name);
}

char *
nautilus_file_get_uri (NautilusFile *file)
{
	GnomeVFSURI *uri;
	char *uri_text;

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->directory != NULL, NULL);

	uri = gnome_vfs_uri_append_path (file->directory->details->uri, file->info->name);
	uri_text = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (uri);
	return uri_text;
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

	g_return_val_if_fail (file != NULL, NULL);

	switch (date_type)
	{
		case NAUTILUS_DATE_TYPE_CHANGED:
			file_time = localtime(&file->info->ctime);
			break;
		case NAUTILUS_DATE_TYPE_ACCESSED:
			file_time = localtime(&file->info->atime);
			break;
		case NAUTILUS_DATE_TYPE_MODIFIED:
			file_time = localtime(&file->info->mtime);
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
 * @ignore_invisible_items: TRUE if invisible items should not be
 * included in count.
 * 
 * Returns: item count for this directory.
 * 
 **/
guint
nautilus_file_get_directory_item_count (NautilusFile *file, 
					gboolean ignore_invisible_items)
{
	g_return_val_if_fail (nautilus_file_is_directory (file), 0);

	return get_directory_item_count_hack (file, ignore_invisible_items);
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
	g_return_val_if_fail (file != NULL, 0);

	return file->info->size;
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

	permissions = file->info->permissions;
	is_directory = nautilus_file_is_directory (file);
	is_link = GNOME_VFS_FILE_INFO_SYMLINK(file->info);
	
	return g_strdup_printf ("%c%c%c%c%c%c%c%c%c%c",
				 is_link ? 'l' : is_directory ? 'd' : '-',
		 		 permissions & GNOME_VFS_PERM_USER_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_USER_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_USER_EXEC ? 'x' : '-',
				 permissions & GNOME_VFS_PERM_GROUP_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_GROUP_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_GROUP_EXEC ? 'x' : '-',
				 permissions & GNOME_VFS_PERM_OTHER_READ ? 'r' : '-',
				 permissions & GNOME_VFS_PERM_OTHER_WRITE ? 'w' : '-',
				 permissions & GNOME_VFS_PERM_OTHER_EXEC ? 'x' : '-');
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

	/* FIXME: Can we trust the uid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getpwuid */
	password_info = getpwuid (file->info->uid);

	g_print ("pointer to password info is %p\n", password_info);

	if (password_info == NULL) {
		return g_strdup (_("unknown owner"));
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

	/* FIXME: Can we trust the gid in the file info? Might
	 * there be garbage there? What will it do for non-local files?
	 */
	/* No need to free result of getgrgid */
	group_info = getgrgid (file->info->gid);

	if (group_info == NULL) {
		return g_strdup (_("unknown group"));
	}
	
	return g_strdup (group_info->gr_name);
}


/* This #include is part of the following hack, and should be removed with it */
#include <dirent.h>

static int
get_directory_item_count_hack (NautilusFile *file, gboolean ignore_invisible_items)
{
 	/* Code borrowed from Gnomad and hacked into here for now */

	char * uri;
	char * path;
	DIR* directory;
	int count;
	struct dirent * entry;
	
	g_assert (nautilus_file_is_directory (file));
	
	uri = nautilus_file_get_uri (file);
	if (nautilus_str_has_prefix (uri, "file://")) {
		path = uri + 7;
	} else {
		path = uri;
	}
	
	directory = opendir (path);
	
	g_free (uri);
	
	if (!directory) {
		return 0;
	}
        
	count = 0;
	
	while ((entry = readdir(directory)) != NULL) {
		// Only count invisible items if requested.
		if (!ignore_invisible_items || entry->d_name[0] != '.') {
			count += 1;
		}
	}
	
	closedir(directory);
	
	/* This way of getting the count includes . and .., so we subtract those out */
	if (!ignore_invisible_items) {
		count -= 2;
	}

	return count;
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
	g_return_val_if_fail (file != NULL, NULL);

	if (nautilus_file_is_directory (file)) {
		/* FIXME: Since computing the item count is slow, we
		 * want to do it in a deferred way. However, that
		 * architecture doesn't exist yet, so we're hacking
		 * it in for now.
		 */
		int item_count;

		item_count = get_directory_item_count_hack (file, FALSE);
		if (item_count == 0) {
			return g_strdup (_("0 items"));
		} else if (item_count == 1) {
			return g_strdup (_("1 item"));
		} else {
			return g_strdup_printf (_("%d items"), item_count);
		}
	}

	return gnome_vfs_file_size_to_string (file->info->size);
}

/**
 * nautilus_file_get_string_attribute:
 * 
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string.
 * 
 * @file: NautilusFile representing the file in question.
 * @attribute_name: The name of the desired attribute. The currently supported
 * set includes "name", "type", "size", "date_modified", "date_changed",
 * "date_accessed", "owner", "group", "permissions".
 * 
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute (NautilusFile *file, const char *attribute_name)
{
	/* FIXME: Use hash table and switch statement or function pointers for speed? */

	if (strcmp (attribute_name, "name") == 0) {
		return nautilus_file_get_name (file);
	}

	if (strcmp (attribute_name, "type") == 0) {
		return nautilus_file_get_type_as_string (file);
	}

	if (strcmp (attribute_name, "size") == 0) {
		return nautilus_file_get_size_as_string (file);
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

	if (strcmp (attribute_name, "permissions") == 0) {
		return nautilus_file_get_permissions_as_string (file);
	}

	if (strcmp (attribute_name, "owner") == 0) {
		return nautilus_file_get_owner_as_string (file);
	}

	if (strcmp (attribute_name, "group") == 0) {
		return nautilus_file_get_group_as_string (file);
	}

	return NULL;
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
	g_return_val_if_fail (file != NULL, NULL);

	if (nautilus_file_is_directory (file)) {
		/* Special-case this so it isn't "special/directory".
		 * FIXME: Should this be "folder" instead?
		 */		
		return g_strdup (_("directory"));
	}

	if (nautilus_strlen (file->info->mime_type) == 0) {
		return g_strdup (_("unknown type"));
	}

	return g_strdup (file->info->mime_type);
}

/**
 * nautilus_file_get_type
 * 
 * Return this file's type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The type.
 * 
 **/
GnomeVFSFileType
nautilus_file_get_type (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return file->info->type;
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
const char *
nautilus_file_get_mime_type (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return file->info->mime_type;
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
	xmlNode *file_node, *child;
	xmlChar *property;

	g_return_val_if_fail (file != NULL, NULL);

	keywords = NULL;

	/* Put all the keywords into a list. */
	file_node = nautilus_directory_get_file_metadata_node (file->directory,
							       file->info->name,
							       FALSE);
	if (file_node != NULL) {
		for (child = file_node->childs; child != NULL; child = child->next) {
			if (strcmp (child->name, "KEYWORD") == 0) {
				property = xmlGetProp (child, "NAME");
				if (property != NULL) {
					keywords = g_list_prepend (keywords,
								   g_strdup (property));
					xmlFree (property);
				}
			}
		}
	}

	return g_list_reverse (keywords);
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
	xmlNode *file_node, *child, *next;
	GList *p;
	gboolean need_write;
	xmlChar *property;

	g_return_if_fail (file != NULL);

	/* Put all the keywords into a list. */
	file_node = nautilus_directory_get_file_metadata_node (file->directory,
							       file->info->name,
							       keywords != NULL);
	need_write = FALSE;
	if (file_node != NULL) {
		p = keywords;

		/* Remove any nodes except the ones we expect. */
		for (child = file_node->childs; child != NULL; child = next) {
			next = child->next;
			if (strcmp (child->name, "KEYWORD") == 0) {
				property = xmlGetProp (child, "NAME");
				if (property != NULL && p != NULL
				    && strcmp (property, (char *) p->data) == 0) {
					p = p->next;
				} else {
					xmlUnlinkNode (child);
					xmlFreeNode (child);
					need_write = TRUE;
				}
				xmlFree (property);
			}
		}

		/* Add any additional nodes needed. */
		for (; p != NULL; p = p->next) {
			child = xmlNewChild (file_node, NULL, "KEYWORD", NULL);
			xmlSetProp (child, "NAME", p->data);
			need_write = TRUE;
		}
	}

	if (need_write) {
		/* Since we changed the tree, arrange for it to be written. */
		nautilus_directory_request_write_metafile (file->directory);
		nautilus_file_changed (file);
	}
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
	g_return_val_if_fail (file != NULL, FALSE);

	return GNOME_VFS_FILE_INFO_SYMLINK (file->info);
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
	g_return_val_if_fail (file != NULL, FALSE);

	return nautilus_file_get_type (file) == GNOME_VFS_FILE_TYPE_DIRECTORY;
}

/**
 * nautilus_file_is_executable
 * 
 * Check if this file is executable at all.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if any of the execute bits are set.
 * 
 **/
gboolean
nautilus_file_is_executable (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return (file->info->flags & (GNOME_VFS_PERM_USER_EXEC
				     | GNOME_VFS_PERM_GROUP_EXEC
				     | GNOME_VFS_PERM_OTHER_EXEC)) != 0;
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
	GList *removed_files;

	g_return_if_fail (file != NULL);

	/* Deleting a file that's already gone is easy. */
	if (file->is_gone) {
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
	if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_NOTFOUND) {
		file->is_gone = TRUE;

		/* Let the directory know it's gone. */
		if (file->directory != NULL) {
		        file->directory->details->files
			     = g_list_remove (file->directory->details->files, file);

			/* Send out a signal. */
			removed_files = g_list_prepend (NULL, file);
			nautilus_directory_files_removed (file->directory, removed_files);
			g_list_free (removed_files);
		}
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
	GList *changed_files;

	/* Send out a signal. */
	changed_files = g_list_prepend (NULL, file);
	nautilus_directory_files_changed (file->directory, changed_files);
	g_list_free (changed_files);
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
	g_return_val_if_fail (file != NULL, FALSE);

	return file->is_gone;
}

/**
 * nautilus_file_list_ref
 *
 * Ref all the files in a list.
 * @file_list: GList of files.
 **/
void
nautilus_file_list_ref (GList *file_list)
{
	g_list_foreach (file_list, (GFunc) nautilus_file_ref, NULL);
}

/**
 * nautilus_file_list_unref
 *
 * Unref all the files in a list.
 * @file_list: GList of files.
 **/
void
nautilus_file_list_unref (GList *file_list)
{
	g_list_foreach (file_list, (GFunc) nautilus_file_unref, NULL);
}

/**
 * nautilus_file_list_free
 *
 * Free a list of files after unrefing them.
 * @file_list: GList of files.
 **/
void
nautilus_file_list_free (GList *file_list)
{
	nautilus_file_list_unref (file_list);
	g_list_free (file_list);
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

	NAUTILUS_CHECK_INTEGER_RESULT (file_1->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (GTK_OBJECT (file_1->directory)->ref_count, 1);
        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 1);

	nautilus_file_unref (file_1);

        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

        list = NULL;    /* let's be explicit here */
        list = g_list_append (list, file_1);
        list = g_list_append (list, file_2);

        nautilus_file_list_ref (list);
        
	NAUTILUS_CHECK_INTEGER_RESULT (file_1->ref_count, 2);
	NAUTILUS_CHECK_INTEGER_RESULT (file_2->ref_count, 2);

	nautilus_file_list_unref (list);
        
	NAUTILUS_CHECK_INTEGER_RESULT (file_1->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (file_2->ref_count, 1);

	nautilus_file_list_free (list);

        NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	

        /* name checks */
	file_1 = nautilus_file_get ("file:///home/");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home/") == file_1, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_get ("file:///home") == file_1, TRUE);

	nautilus_file_unref (file_1);
	nautilus_file_unref (file_1);

	file_1 = nautilus_file_get ("file:///home");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");
	nautilus_file_unref (file_1);

	/* sorting */
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

	NAUTILUS_CHECK_INTEGER_RESULT (file_1->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (file_2->ref_count, 1);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort_reversed (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_NAME) == 0, TRUE);

	nautilus_file_unref (file_1);
	nautilus_file_unref (file_2);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
