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

typedef void (*NautilusFileListCallback) (NautilusDirectory *directory,
					  GList             *files,
					  gpointer           callback_data);

/* Basic GtkObject requirements. */
GtkType            nautilus_directory_get_type             (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NautilusDirectory *nautilus_directory_get                  (const char               *uri);

/* Convenience functions, since we do a lot of ref'ing and unref'ing. */
void               nautilus_directory_ref                  (NautilusDirectory        *directory);
void               nautilus_directory_unref                (NautilusDirectory        *directory);

/* Access to a URI. */
char *             nautilus_directory_get_uri              (NautilusDirectory        *directory);

/* Simple preliminary interface for getting and setting metadata. */
char *             nautilus_directory_get_metadata         (NautilusDirectory        *directory,
							    const char               *tag,
							    const char               *default_metadata);
void               nautilus_directory_set_metadata         (NautilusDirectory        *directory,
							    const char               *tag,
							    const char               *default_metadata,
							    const char               *metadata);
gboolean           nautilus_directory_get_boolean_metadata (NautilusDirectory        *directory,
							    const char               *tag,
							    gboolean                  default_metadata);
void               nautilus_directory_set_boolean_metadata (NautilusDirectory        *directory,
							    const char               *tag,
							    gboolean                  default_metadata,
							    gboolean                  metadata);
int                nautilus_directory_get_integer_metadata (NautilusDirectory        *directory,
							    const char               *tag,
							    int                       default_metadata);
void               nautilus_directory_set_integer_metadata (NautilusDirectory        *directory,
							    const char               *tag,
							    int                       default_metadata,
							    int                       metadata);

/* Monitor the files in a directory. */
void               nautilus_directory_monitor_files_ref    (NautilusDirectory        *directory,
							    NautilusFileListCallback  initial_files_callback,
							    gpointer                  callback_data);
void               nautilus_directory_monitor_files_unref  (NautilusDirectory        *directory);

/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           nautilus_directory_are_all_files_seen   (NautilusDirectory        *directory);

/* Return true if the directory metadata has been loaded.
 * Until this is true, get_metadata calls will return defaults.
 * (We could have another way to indicate "don't know".)
 */
gboolean           nautilus_directory_metadata_loaded      (NautilusDirectory        *directory);

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

	/* The files_added and files_removed signals are emitted as
	   the directory model discovers new files or discovers that
	   old files have been deleted. In the case of files_removed,
	   this is the last chance to forget about these file objects
	   which are about to be unref'd.
	*/
	void   (* files_added)      (NautilusDirectory        *directory,
				     GList                    *added_files);
	void   (* files_removed)    (NautilusDirectory        *directory,
				     GList                    *removed_files);

	/* The files_changed signal is emitted as changes occur to
	   existing files that are noticed by the synchronization framework.
	   The client must register which file attributes it is interested
	   in. Changes to other attributes are not reported via the signal.
	*/
	void   (* files_changed)    (NautilusDirectory       *directory,
				     GList                   *changed_files);

	/* The metadata_changed signal is emitted when changes to the metadata
	 * for the directory itself are made.
	 */
	void   (* metadata_changed) (NautilusDirectory       *directory);
};

#endif /* NAUTILUS_DIRECTORY_H */
