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
#include <libgnomevfs/gnome-vfs-types.h>

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

typedef struct _NautilusDirectory NautilusDirectory;
typedef struct _NautilusDirectoryClass NautilusDirectoryClass;

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

typedef struct _NautilusFile NautilusFile;
typedef GList NautilusFileList;

#define NAUTILUS_FILE(file) ((NautilusFile *)(file))

typedef void (*NautilusFileListCallback) (NautilusDirectory *directory,
					  NautilusFileList  *files,
					  gpointer           data);

/* Basic GtkObject requirements. */
GtkType            nautilus_directory_get_type            (void);

/* Get a directory given a uri.
   Creates the appropriate subclass given the uri mappings.
   Returns a referenced object, not a floating one. Unref when finished.
   If two windows are viewing the same uri, the directory object is shared.
*/
NautilusDirectory *nautilus_directory_get                 (const char               *uri);
char *             nautilus_directory_get_uri             (NautilusDirectory        *directory);

/* Simple preliminary interface for getting and setting metadata. */
char *             nautilus_directory_get_metadata        (NautilusDirectory        *directory,
							   const char               *tag,
							   const char               *default_metadata);
void               nautilus_directory_set_metadata        (NautilusDirectory        *directory,
							   const char               *tag,
							   const char               *default_metadata,
							   const char               *metadata);

/* Get the current files.
   Instead of returning the list of files, this function uses a callback.
   The directory guarantees that signals won't be emitted while in the
   callback function.
*/
void               nautilus_directory_get_files           (NautilusDirectory        *directory,
							   NautilusFileListCallback  callback,
							   gpointer                  callback_data);

/* Return true if the directory has enough information for layout.
   This will be false until the metafile is read to prevent a partial layout
   from being done.
*/
gboolean           nautilus_directory_is_ready_for_layout (NautilusDirectory        *directory);

/* Temporary interface for NautilusFile while we are phasing it in. */
NautilusFile *     nautilus_directory_new_file            (NautilusDirectory        *directory,
							   GnomeVFSFileInfo         *info);

/* Basic operations on file objects. */
void               nautilus_file_ref                      (NautilusFile             *file);
void               nautilus_file_unref                    (NautilusFile             *file);

/* Basic attributes for file objects. */
char *             nautilus_file_get_name                 (NautilusFile             *file);
char *             nautilus_file_get_uri                  (NautilusFile             *file);
GnomeVFSFileSize   nautilus_file_get_size                 (NautilusFile             *file);
GnomeVFSFileType   nautilus_file_get_type                 (NautilusFile             *file);
const char *	   nautilus_file_get_mime_type            (NautilusFile             *file);
gboolean           nautilus_file_is_symbolic_link	  (NautilusFile             *file);
gboolean           nautilus_file_is_executable            (NautilusFile             *file);

/* Simple getting and setting top-level metadata. */
char *             nautilus_file_get_metadata             (NautilusFile             *file,
							   const char               *tag,
							   const char               *default_metadata);
void               nautilus_file_set_metadata             (NautilusFile             *file,
							   const char               *tag,
							   const char               *default_metadata,
							   const char               *metadata);

/* Utility functions for formatting file-related information. */
char *             nautilus_file_get_date_as_string       (NautilusFile             *file);
char *             nautilus_file_get_size_as_string       (NautilusFile             *file);
char *             nautilus_file_get_type_as_string       (NautilusFile             *file);

/* Return true if this file has already been deleted.
   This object will be unref'd after sending the files_removed signal,
   but it could hang around longer if someone ref'd it.
*/
gboolean           nautilus_file_is_gone                  (NautilusFile             *file);

typedef struct _NautilusDirectoryDetails NautilusDirectoryDetails;

struct _NautilusDirectory
{
	GtkObject object;

	/* Hidden details. */
	NautilusDirectoryDetails *details;
};

struct _NautilusDirectoryClass
{
	GtkObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added and files_removed signals are emitted as
	   the directory model discovers new files or discovers that
	   old files have been deleted. In the case of files_removed,
	   this is the last chance to forget about these file objects
	   which are about to be unref'd.
	*/
	void   (* files_added)     (NautilusDirectory        *directory,
				    NautilusFileList         *added_files);
	void   (* files_removed)   (NautilusDirectory        *directory,
				    NautilusFileList         *removed_files);

	/* The files_changed signal is emitted as changes occur to
	   existing files that are noticed by the synchronization framework.
	   The client must register which file attributes it is interested
	   in. Changes to other attributes are not reported via the signal.
	*/
	void   (* files_changed)    (NautilusDirectory       *directory,
				     NautilusFileList        *changed_files);

	/* The ready_for_layout signal is emitted when the directory
	   model judges that enough files are available for the layout
	   process to begin. For normal directories this is after the
	   metafile has been read. If there's no way to get the basic
	   layout information before getting the actual files, then
	   this signal need not be emitted as long as is_ready_for_layout
	   is already true.
	*/
	void   (* ready_for_layout) (NautilusDirectory       *directory);
};

#endif /* NAUTILUS_DIRECTORY_H */
