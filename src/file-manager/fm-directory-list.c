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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-directory-list.h"

#include "fm-icon-cache.h"

/*
 * Prototypes for private FMDirectoryListEntry functions
 */
static FMDirectoryListEntry* fm_directory_list_entry_new           (GnomeVFSFileInfo *info, GdkPixbuf *icon);

/*
 * FMDirectoryList object
 */

static void stop_load (FMDirectoryList *dlist);

/* GtkObject implementation details */

static void fm_directory_list_class_init (FMDirectoryListClass *class);
static void fm_directory_list_init       (FMDirectoryList      *dlist);
static void fm_directory_list_destroy    (GtkObject          *object);
static void fm_directory_list_finalize   (GtkObject          *object);

enum {
        ENTRIES_LOADED,
        FINISHED_LOAD,
        ENTRY_ICON_CHANGED,
        ENTRY_NAME_CHANGED,
        LAST_SIGNAL
};

static GtkObjectClass *parent_class;
static guint dlist_signals[LAST_SIGNAL];

GtkType
fm_directory_list_get_type (void)
{
	static GtkType fm_directory_list_type = 0;

	if (!fm_directory_list_type) {
		GtkTypeInfo fm_directory_list_info = {
			"FMDirectoryList",
			sizeof (FMDirectoryList),
			sizeof (FMDirectoryListClass),
			(GtkClassInitFunc) fm_directory_list_class_init,
			(GtkObjectInitFunc) fm_directory_list_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		fm_directory_list_type = gtk_type_unique (gtk_object_get_type (), &fm_directory_list_info);
	}

	return fm_directory_list_type;
}

static void
fm_directory_list_class_init (FMDirectoryListClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

        dlist_signals[ENTRIES_LOADED] =
                gtk_signal_new ("entries_loaded",
                                GTK_RUN_FIRST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (FMDirectoryListClass,
                                                   entries_loaded),
                                gtk_marshal_NONE__POINTER,
                                GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

        dlist_signals[FINISHED_LOAD] =
                gtk_signal_new ("finished_load",
                                GTK_RUN_FIRST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (FMDirectoryListClass,
                                                   finished_load),
                                gtk_marshal_NONE__NONE,
                                GTK_TYPE_NONE, 0);
        
        dlist_signals[ENTRY_ICON_CHANGED] =
                gtk_signal_new ("entry_icon_changed",
                                GTK_RUN_FIRST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (FMDirectoryListClass,
                                                   entry_icon_changed),
                                gtk_marshal_NONE__POINTER,
                                GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

        dlist_signals[ENTRY_NAME_CHANGED] =
                gtk_signal_new ("entry_name_changed",
                                GTK_RUN_FIRST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (FMDirectoryListClass,
                                                   entry_name_changed),
                                gtk_marshal_NONE__POINTER,
                                GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
        
	gtk_object_class_add_signals (object_class,
                                      dlist_signals, LAST_SIGNAL);
        
        object_class->destroy = fm_directory_list_destroy;
        object_class->finalize = fm_directory_list_finalize;
}

static void
fm_directory_list_init (FMDirectoryList *dlist)
{
        dlist->directory_list = NULL;
        dlist->vfs_async_handle = NULL;
        dlist->uri = NULL;
}

static void
fm_directory_list_destroy (GtkObject *object)
{
        FMDirectoryList *dlist;
        
        g_return_if_fail(object != NULL);
        g_return_if_fail(FM_IS_DIRECTORY_LIST(object));

        dlist = FM_DIRECTORY_LIST(object);

        if (dlist->directory_list != NULL)
                stop_load(dlist);

        if (dlist->uri) {
                gnome_vfs_uri_unref(dlist->uri);
                dlist->uri = NULL;
        }
        
        (*  GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
fm_directory_list_finalize (GtkObject *object)
{
        FMDirectoryList *canvas;

        g_return_if_fail(object != NULL);
        g_return_if_fail(FM_IS_DIRECTORY_LIST(object));

        
        
        (* GTK_OBJECT_CLASS(parent_class)->finalize) (object);
}


FMDirectoryList*
fm_directory_list_new (void)
{
        FMDirectoryList *dlist;

        dlist = gtk_type_new(fm_directory_list_get_type());

        return dlist;
}

/*
 * The meat of FMDirectoryList
 */

#define ENTRIES_PER_CB 1 /* FIXME 1 for debugging, but for performance
                            more would be better */

static void
stop_load (FMDirectoryList *dlist)
{
        GSList *iter;

        iter = dlist->entries;
        while (iter != NULL) {
                fm_directory_list_entry_unref(iter->data);
                
                iter = g_slist_next(iter);
        }

        dlist->entries = NULL;
        dlist->entries_tail = NULL;
        
	if (dlist->vfs_async_handle != NULL) {
                /* Destroys the async handle and the directory list */
		gnome_vfs_async_cancel (dlist->vfs_async_handle);
		dlist->vfs_async_handle = NULL;
	}
        
	dlist->directory_list = NULL;

        gtk_signal_emit(GTK_OBJECT(dlist),
                        dlist_signals[FINISHED_LOAD]);
}

static void
directory_load_cb (GnomeVFSAsyncHandle *handle,
		   GnomeVFSResult result,
		   GnomeVFSDirectoryList *list,
		   guint entries_read,
		   gpointer callback_data)
{
	FMDirectoryList *dlist;

	g_assert(entries_read <= ENTRIES_PER_CB);
        
	dlist = FM_DIRECTORY_LIST (callback_data);

	if (dlist->directory_list == NULL) {
		if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {

			dlist->directory_list = list;
                        
                } else if (entries_read == 0) {
                        g_warning("our error handling here is not implemented, FIXME: %s", gnome_vfs_result_to_string(result));
			/*
			gtk_signal_emit (GTK_OBJECT (dlist),
					 signals[OPEN_FAILED]);
			*/
		}
	}

        /* OK, the directory list is supposed to always be positioned
           at the start of the NEW entries we haven't seen yet.
        */
        {
                GSList *new_entries = NULL;
                guint i;

                i = 0;
                while (i < entries_read) {
                        GnomeVFSFileInfo *info;
                        FMDirectoryListEntry *entry;
                        GdkPixbuf            *pixbuf;
                        
                        info = gnome_vfs_directory_list_current (dlist->directory_list);

                        g_assert(info != NULL);

                        pixbuf = fm_icon_cache_get_icon(fm_get_current_icon_cache(),
                                                        info);

                        g_assert(pixbuf != NULL);
                        
                        entry = fm_directory_list_entry_new(info, pixbuf);

                        new_entries = g_slist_prepend(new_entries, entry);

                        gdk_pixbuf_unref(pixbuf);
                        /* The info doesn't need an unref, the directory
                           list owns it */
                        
                        gnome_vfs_directory_list_next (dlist->directory_list);
                        ++i;
                }

                new_entries = g_slist_reverse(new_entries);

                gtk_signal_emit(GTK_OBJECT(dlist),
                                dlist_signals[ENTRIES_LOADED],
                                new_entries);

                if (dlist->entries == NULL) {
                        dlist->entries = new_entries;
                        dlist->entries_tail = g_slist_last(dlist->entries);
                } else {
                        g_assert(dlist->entries_tail != NULL);
                        dlist->entries_tail->next = new_entries;
                        dlist->entries_tail = g_slist_last(new_entries);
                }
	}

                
	if (result == GNOME_VFS_ERROR_EOF) {

		stop_load (dlist);

	} else if (result != GNOME_VFS_OK) {

                stop_load (dlist);
                
                /* FIXME error */
	}
}


void
fm_directory_list_load_uri (FMDirectoryList *dlist,
                            const gchar *uri)
{
        GnomeVFSResult result;
        static GnomeVFSDirectorySortRule sort_rules[] = {
		GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST,
		GNOME_VFS_DIRECTORY_SORT_BYNAME,
		GNOME_VFS_DIRECTORY_SORT_NONE
	};			/* FIXME */

        if (dlist->uri) {
                /* stop current */
                stop_load(dlist);
                gnome_vfs_uri_unref(dlist->uri);
        }
        
        /* start new */
        dlist->uri = gnome_vfs_uri_new(uri);

        g_assert(dlist->uri);

        result = gnome_vfs_async_load_directory_uri
		(&dlist->vfs_async_handle, 		/* handle */
		 dlist->uri,				/* uri */
		 (GNOME_VFS_FILE_INFO_GETMIMETYPE	/* options */
		  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
		 NULL, 					/* meta_keys */
		 sort_rules, 				/* sort_rules */
		 FALSE, 				/* reverse_order */
		 GNOME_VFS_DIRECTORY_FILTER_NONE, 	/* filter_type */
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR  /* filter_options */
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL, 					/* filter_pattern */
		 ENTRIES_PER_CB,			/* items_per_notification */
		 directory_load_cb,	 		/* callback */
		 dlist);		 	       	/* callback_data */
        
	g_return_if_fail(result == GNOME_VFS_OK);
}

/*
 * Directory list entry
 */

struct _FMDirectoryListEntry {
        guint refcount;
        GnomeVFSFileInfo *info;
        GdkPixbuf *icon;
};

FMDirectoryListEntry*
fm_directory_list_entry_new           (GnomeVFSFileInfo *info, GdkPixbuf *icon)
{
        FMDirectoryListEntry *entry;

        g_return_val_if_fail(info != NULL, NULL);
        g_return_val_if_fail(icon != NULL, NULL);
        
        entry = g_new(FMDirectoryListEntry, 1);

        entry->refcount = 1;
        entry->info = info;
        entry->icon = icon;

        gnome_vfs_file_info_ref(entry->info);
        gdk_pixbuf_ref(entry->icon);
        
        return entry;
}

void
fm_directory_list_entry_ref           (FMDirectoryListEntry *entry)
{
        g_return_if_fail(entry != NULL);
        
        entry->refcount += 1;
}

void
fm_directory_list_entry_unref         (FMDirectoryListEntry *entry)
{
        g_return_if_fail(entry != NULL);
        g_return_if_fail(entry->refcount > 0);

        entry->refcount -= 1;

        if (entry->refcount == 0) {
                if (entry->info)
                        gnome_vfs_file_info_unref(entry->info);
                if (entry->icon)
                        gdk_pixbuf_unref(entry->icon);
                g_free(entry);
        }
}

GnomeVFSFileInfo*
fm_directory_list_entry_get_file_info (FMDirectoryListEntry *entry)
{
        g_return_val_if_fail(entry != NULL, NULL);
        g_return_val_if_fail(entry->info != NULL, NULL);
        
        return entry->info;
}

GdkPixbuf*
fm_directory_list_entry_get_icon      (FMDirectoryListEntry *entry)
{
        g_return_val_if_fail(entry != NULL, NULL);
        g_return_val_if_fail(entry->icon != NULL, NULL);
        
        return entry->icon;
}


const gchar*
fm_directory_list_entry_get_name      (FMDirectoryListEntry *entry)
{
        g_return_val_if_fail(entry != NULL, NULL);
        g_return_val_if_fail(entry->info != NULL, NULL);

        return entry->info->name;
}

