/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-directory.h: GNOME file manager directory model
 
   Copyright (C) 1999 Eazel, Inc.
  
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

#ifndef FM_DIRECTORY_H
#define FM_DIRECTORY_H

#include <gtk/gtkobject.h>

/* FMDirectory is an abstract class that manages the model for a directory,
   real or virtual, for the GNOME file manager. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by GNOME VFS, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
       3) An interface that folds together the cached information that's
          kept in the metafile with "trustworthy" versions of the same
          information available from other means.

   In addition, none of the GnomeVFS interface is exposed directly, to
   help ensure that subclasses that are not based on GnomeVFS are possible.
*/

typedef struct _FMDirectory FMDirectory;
typedef struct _FMDirectoryClass FMDirectoryClass;

#define FM_TYPE_DIRECTORY \
	(fm_directory_get_type ())
#define FM_DIRECTORY(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY, FMDirectory))
#define FM_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY, FMDirectoryClass))
#define FM_IS_DIRECTORY(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY))
#define FM_IS_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_DIRECTORY))

typedef struct _FMFile FMFile;
typedef GList FMFileList;

typedef void (*FMFileListCallback) (FMDirectory *directory,
				    FMFileList  *files,
				    gpointer     data);

/* Basic GtkObject requirements. */
GtkType      fm_directory_get_type            (void);

/* Get a directory given a uri.
   Creates the appropriate subclass given the uri mappings.
   Returns a referenced object, not a floating one. Unref when finished.
   If two windows are viewing the same uri, the directory object is shared.
*/
FMDirectory *fm_directory_get                 (const char         *uri);

/* Get the current files.
   Instead of returning the list of files, this function uses a callback.
   The directory guarantees that signals won't be emitted while in the
   callback function.
*/
void         fm_directory_get_files           (FMDirectory        *directory,
					       FMFileListCallback  callback,
					       gpointer            callback_data);

/* Return true if the directory has enough information for layout.
   This will be false until the metafile is read to prevent a partial layout
   from being done.
*/
gboolean     fm_directory_is_ready_for_layout (FMDirectory        *directory);

/* Basic operations on file objects. */
void         fm_file_ref                      (FMFile             *file);
void         fm_file_unref                    (FMFile             *file);
char *       fm_file_get_name                 (FMFile             *file);

/* Return true if this file has already been deleted.
   This object will be unref'd after sending the files_removed signal,
   but it could hang around longer if someone ref'd it.
*/
gboolean     fm_file_is_gone                  (FMFile             *file);

typedef struct _FMDirectoryDetails FMDirectoryDetails;

struct _FMDirectory
{
	GtkObject object;

	/* Hidden details. */
	FMDirectoryDetails *details;
};

struct _FMDirectoryClass
{
	GtkObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added and files_removed signals are emitted as
	   the directory model discovers new files or discovers that
	   old files have been deleted. In the case of files_removed,
	   this is the last chance to forget about these file objects
	   which are about to be unref'd.
	*/
	void   (* files_added)     (FMDirectory        *directory,
				    FMFileList         *added_files);
	void   (* files_removed)   (FMDirectory        *directory,
				    FMFileList         *removed_files);

	/* The files_changed signal is emitted as changes occur to
	   existing files that are noticed by the synchronization framework.
	   The client must register which file attributes it is interested
	   in. Changes to other attributes are not reported via the signal.
	*/
	void   (* files_changed)    (FMDirectory       *directory,
				     FMFileList        *changed_files);

	/* The ready_for_layout signal is emitted when the directory
	   model judges that enough files are available for the layout
	   process to begin. For normal directories this is after the
	   metafile has been read. If there's no way to get the basic
	   layout information before getting the actual files, then
	   this signal need not be emitted as long as is_ready_for_layout
	   is already true.
	*/
	void   (* ready_for_layout) (FMDirectory       *directory);

	/*** Interface for FMDirectory subclasses to implement. ***/

	/* Implementation of fm_get_files. */
	void   (* get_files)        (FMDirectory       *directory,
				     FMFileListCallback callback,
				     gpointer           data);

	/* Implementation of destruction for an FMFile for an item
	   in this directory.
	*/
	void   (* finalize_file)    (FMFile            *file);
};

#endif /* FM_DIRECTORY_H */
