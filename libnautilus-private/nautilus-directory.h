/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.h: Nautilus directory model.
 
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

#ifndef NAUTILUS_DIRECTORY_H
#define NAUTILUS_DIRECTORY_H

#include <gtk/gtkobject.h>

/* NautilusDirectory is a class that manages the model for a directory,
   real or virtual, for Nautilus, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gnome-vfs, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
       3) An interface that folds together the cached information that's
          kept in the metafile with "trustworthy" versions of the same
          information available from other means.
*/

typedef struct NautilusDirectory NautilusDirectory;
typedef struct NautilusDirectoryClass NautilusDirectoryClass;

#define NAUTILUS_TYPE_DIRECTORY \
	(nautilus_directory_get_type ())
#define NAUTILUS_DIRECTORY(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DIRECTORY, NautilusDirectory))
#define NAUTILUS_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DIRECTORY, NautilusDirectoryClass))
#define NAUTILUS_IS_DIRECTORY(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DIRECTORY))
#define NAUTILUS_IS_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DIRECTORY))

/* NautilusFile is defined both here and in nautilus-file.h. */
#ifndef NAUTILUS_FILE_DEFINED
#define NAUTILUS_FILE_DEFINED
typedef struct NautilusFile NautilusFile;
#endif

typedef void (*NautilusDirectoryCallback) (NautilusDirectory *directory,
					   GList             *files,
					   gpointer           callback_data);

/* Basic GtkObject requirements. */
GtkType            nautilus_directory_get_type             (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NautilusDirectory *nautilus_directory_get                  (const char                *uri);

/* Convenience functions, since we do a lot of ref'ing and unref'ing. */
void               nautilus_directory_ref                  (NautilusDirectory         *directory);
void               nautilus_directory_unref                (NautilusDirectory         *directory);

/* Access to a URI. */
char *             nautilus_directory_get_uri              (NautilusDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           nautilus_directory_contains_file        (NautilusDirectory         *directory,
							    NautilusFile              *file);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 * If any file attributes or metadata keys are passed, it won't call
 * until all the files are seen.
 */
void               nautilus_directory_call_when_ready      (NautilusDirectory         *directory,
							    GList                     *file_attributes,
							    gboolean                   wait_for_metadata,
							    NautilusDirectoryCallback  callback,
							    gpointer                   callback_data);
GList *            nautilus_directory_wait_until_ready     (NautilusDirectory         *directory,
							    GList                     *file_attributes,
							    gboolean                   wait_for_metadata);
void               nautilus_directory_cancel_callback      (NautilusDirectory         *directory,
							    NautilusDirectoryCallback  callback,
							    gpointer                   callback_data);

/* Getting and setting metadata. */
char *             nautilus_directory_get_metadata         (NautilusDirectory         *directory,
							    const char                *key,
							    const char                *default_metadata);
GList             *nautilus_directory_get_metadata_list    (NautilusDirectory         *directory,
							    const char                *list_key,
							    const char                *list_subkey);
void               nautilus_directory_set_metadata         (NautilusDirectory         *directory,
							    const char                *key,
							    const char                *default_metadata,
							    const char                *metadata);
void               nautilus_directory_set_metadata_list    (NautilusDirectory         *directory,
							    const char                *list_key,
							    const char                *list_subkey,
							    GList                     *list);

/* Covers for common data types. */
gboolean           nautilus_directory_get_boolean_metadata (NautilusDirectory         *directory,
							    const char                *key,
							    gboolean                   default_metadata);
void               nautilus_directory_set_boolean_metadata (NautilusDirectory         *directory,
							    const char                *key,
							    gboolean                   default_metadata,
							    gboolean                   metadata);
int                nautilus_directory_get_integer_metadata (NautilusDirectory         *directory,
							    const char                *key,
							    int                        default_metadata);
void               nautilus_directory_set_integer_metadata (NautilusDirectory         *directory,
							    const char                *key,
							    int                        default_metadata,
							    int                        metadata);

/* Monitor the files in a directory. */
void               nautilus_directory_file_monitor_add     (NautilusDirectory         *directory,
							    gconstpointer              client,
							    GList                     *monitor_attributes,
							    gboolean                   monitor_metadata,
							    gboolean                   force_reload,
							    NautilusDirectoryCallback  initial_files_callback,
							    gpointer                   callback_data);
void               nautilus_directory_file_monitor_remove  (NautilusDirectory         *directory,
							    gconstpointer              client);

/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           nautilus_directory_are_all_files_seen   (NautilusDirectory         *directory);

/* Return true if the directory metadata has been loaded.
 * Until this is true, get_metadata calls will return defaults.
 * (We could have another way to indicate "don't know".)
 */
gboolean           nautilus_directory_metadata_loaded      (NautilusDirectory         *directory);

/* Return true if the directory is local. */
gboolean           nautilus_directory_is_local             (NautilusDirectory         *directory);

gboolean           nautilus_directory_is_search_directory  (NautilusDirectory         *directory);

/* Return false if directory contains anything besides a nautilus metafile.
 * Only valid if directory is monitored.
 * Used by the Trash monitor
 */
gboolean	   nautilus_directory_is_not_empty	   (NautilusDirectory	      *directory);

typedef struct NautilusDirectoryDetails NautilusDirectoryDetails;

struct NautilusDirectory
{
	GtkObject object;
	NautilusDirectoryDetails *details;
};

struct NautilusDirectoryClass
{
	GtkObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added signal is emitted as the directory model 
	 * discovers new files.
	 */
	void   (* files_added)      (NautilusDirectory        *directory,
				     GList                    *added_files);

	/* The files_changed signal is emitted as changes occur to
	 * existing files that are noticed by the synchronization framework,
	 * including when an old file has been deleted. When an old file
	 * has been deleted, this is the last chance to forget about these
	 * file objects, which are about to be unref'd. Use a call to
	 * nautilus_file_is_gone () to test for this case.
	 */
	void   (* files_changed)    (NautilusDirectory       *directory,
				     GList                   *changed_files);

	/* The metadata_changed signal is emitted when changes to the metadata
	 * for the directory itself are made. Changes to file metadata just
	 * result in calls to files_changed.
	 */
	void   (* metadata_changed) (NautilusDirectory       *directory);


	/* The done_loading signal is emitted when a directory load
	 * request completes. This is needed because, at least in the
	 * case where the directory is empty, the caller will receive
	 * no kind of notification at all when a directory load
	 * initiated by `nautilus_directory_file_monitor_add' completes.
	 */

	void   (* done_loading) (NautilusDirectory       *directory);
};

#endif /* NAUTILUS_DIRECTORY_H */
