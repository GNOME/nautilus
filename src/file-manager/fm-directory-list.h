/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * GNOME File Manager (Directory list object)
 * 
 * Copyright (C) 1999, 2000 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * Developed by: Havoc Pennington
 */
#ifndef __FM_DIRECTORY_LIST_H__
#define __FM_DIRECTORY_LIST_H__

#include <libgnome/gnome-defs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

BEGIN_GNOME_DECLS

/* An entry in the directory list */

typedef struct _FMDirectoryListEntry FMDirectoryListEntry;

void                  fm_directory_list_entry_ref           (FMDirectoryListEntry *entry);
void                  fm_directory_list_entry_unref         (FMDirectoryListEntry *entry);
GnomeVFSFileInfo*     fm_directory_list_entry_get_file_info (FMDirectoryListEntry *entry);
GdkPixbuf*            fm_directory_list_entry_get_icon      (FMDirectoryListEntry *entry);
const gchar*          fm_directory_list_entry_get_name      (FMDirectoryListEntry *entry);



/* FMDirectoryList represents the non-graphical part of a
   directory listing; two different kinds of view
   currently exist, the FMDirectoryView and the
   DesktopCanvas
*/

typedef struct _FMDirectoryList      FMDirectoryList;
typedef struct _FMDirectoryListClass FMDirectoryListClass;

#define FM_TYPE_DIRECTORY_LIST			(fm_directory_list_get_type ())
#define FM_DIRECTORY_LIST(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_LIST, FMDirectoryList))
#define FM_DIRECTORY_LIST_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_LIST, FMDirectoryListClass))
#define FM_IS_DIRECTORY_LIST(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_LIST))
#define FM_IS_DIRECTORY_LIST_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_LIST))

struct _FMDirectoryList {
        GtkObject object;

	GnomeVFSDirectoryList *directory_list;

	GnomeVFSAsyncHandle *vfs_async_handle;
	GnomeVFSURI *uri;

        GSList *entries;
        GSList *entries_tail;
};

struct _FMDirectoryListClass {
        GtkObjectClass object_class;

        /* Eventually we might want signals like file_deleted,
           file_added, blah blah; for now there's just the
           stuff for loading a directory. */

        /* You must add a refcount to these FMDirectoryListEntry
           objects if you want to keep hold of them outside of the
           signal handler. */
        
        /* Each entry loaded after calling
           fm_directory_list_load_uri() will be passed out via this
           signal exactly ONE time; there is no guarantee about how
           many each time, you may get all the entries in one signal
           or one entry at a time. */
        void (* entries_loaded)     (FMDirectoryList *list,
                                     GSList          *entries);

        /* Emitted when all entries have been loaded; icon/name
           changed signals can come after this, but no more
           entries_loaded signals */
        void (* finished_load) (FMDirectoryList *list);
        
        /* These are called if the icon or name for a directory
           changes.  In particular, icon_changed will be used if we
           are doing slow MIME magic in an idle handler, and need to
           update the icons to the more accurate ones. */
        void (* entry_icon_changed) (FMDirectoryList      *list,
                                     FMDirectoryListEntry *entry);

        void (* entry_name_changed) (FMDirectoryList      *list,
                                     FMDirectoryListEntry *entry);
};



GtkType          fm_directory_list_get_type (void);
FMDirectoryList *fm_directory_list_new      (void);
void             fm_directory_list_load_uri (FMDirectoryList *dlist,
                                             const gchar     *uri);


END_GNOME_DECLS

#endif

