/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef FM_DIRECTORY_VIEW_H
#define FM_DIRECTORY_VIEW_H

#include <gtk/gtkscrolledwindow.h>
#include <libnautilus/ntl-content-view-frame.h>
#include <libnautilus/nautilus-directory.h>



enum _FMDirectoryViewSortType {
	FM_DIRECTORY_VIEW_SORT_NONE,
	FM_DIRECTORY_VIEW_SORT_BYNAME,
	FM_DIRECTORY_VIEW_SORT_BYSIZE,
	FM_DIRECTORY_VIEW_SORT_BYTYPE,
	FM_DIRECTORY_VIEW_SORT_BYMTIME
};
typedef enum _FMDirectoryViewSortType FMDirectoryViewSortType;


typedef struct _FMDirectoryView      FMDirectoryView;
typedef struct _FMDirectoryViewClass FMDirectoryViewClass;

#define FM_TYPE_DIRECTORY_VIEW			(fm_directory_view_get_type ())
#define FM_DIRECTORY_VIEW(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW, FMDirectoryView))
#define FM_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW, FMDirectoryViewClass))
#define FM_IS_DIRECTORY_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))
#define FM_IS_DIRECTORY_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))

typedef struct _FMDirectoryViewDetails FMDirectoryViewDetails;

struct _FMDirectoryView {
	GtkScrolledWindow parent;
	FMDirectoryViewDetails *details;
};

struct _FMDirectoryViewClass {
	GtkScrolledWindowClass parent_class;

	/* The 'clear' signal is emitted to empty the view of its contents.
	 * It must be replaced by each subclass.
	 */
	void 	(* clear) 		 (FMDirectoryView *view);
	
	/* The 'begin_adding_entries' signal is emitted before a set of entries
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary preparation for a set of new entries. The default
	 * implementation does nothing.
	 */
	void 	(* begin_adding_entries) (FMDirectoryView *view);
	
	/* The 'add_entry' signal is emitted to add one entry to the view.
	 * It must be replaced by each subclass.
	 */
	void 	(* add_entry) 		 (FMDirectoryView *view, 
					  NautilusFile *file);

	/* The 'done_adding_entries' signal is emitted after a set of entries
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary cleanup (typically, cleanup for code in begin_adding_entries).
	 * The default implementation does nothing.
	 */
	void 	(* done_adding_entries)  (FMDirectoryView *view);
	
	/* The 'begin_loading' signal is emitted before any of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary preparation to start dealing with a
	 * new directory. The default implementation does nothing.
	 */
	void 	(* begin_loading) 	 (FMDirectoryView *view);

	/* get_selection is not a signal; it is just a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NautilusFile pointers.
	 */
	NautilusFileList *
		(* get_selection) 	 (FMDirectoryView *view);

        /* bump_zoom_level is a function pointer that subclasses override to
         * change the zoom level of an object */
       void     (* bump_zoom_level)   (FMDirectoryView *view, gint zoom_increment);
};



/* GtkObject support */
GtkType                   fm_directory_view_get_type                 (void);

/* Component embedding support */
NautilusContentViewFrame *fm_directory_view_get_view_frame           (FMDirectoryView         *view);

/* URI handling */
void                      fm_directory_view_load_uri                 (FMDirectoryView         *view,
								      const char              *uri);

/* Functions callable from the user interface and elsewhere. */
GList *                   fm_directory_view_get_selection            (FMDirectoryView         *view);
void                      fm_directory_view_stop                     (FMDirectoryView         *view);
void                      fm_directory_view_sort                     (FMDirectoryView         *view,
								      FMDirectoryViewSortType  sort_type,
								      gboolean                 reverse_sort);

/* Wrappers for signal emitters. These are normally called 
 * only by FMDirectoryView itself. They have corresponding signals
 * that observers might want to connect with.
 */
void                      fm_directory_view_clear                    (FMDirectoryView         *view);
void                      fm_directory_view_begin_adding_entries     (FMDirectoryView         *view);
void                      fm_directory_view_add_entry                (FMDirectoryView         *view,
								      NautilusFile            *file);
void                      fm_directory_view_done_adding_entries      (FMDirectoryView         *view);
void                      fm_directory_view_begin_loading            (FMDirectoryView         *view);				       			 
/* Hooks for subclasses to call. These are normally called only by 
 * FMDirectoryView and its subclasses 
 */
void                      fm_directory_view_activate_entry           (FMDirectoryView         *view,
								      NautilusFile            *file);
void                      fm_directory_view_bump_zoom_level          (FMDirectoryView         *view, gint zoom_increment);
void                      fm_directory_view_notify_selection_changed (FMDirectoryView         *view);
NautilusDirectory *       fm_directory_view_get_model                (FMDirectoryView         *view);
void                      fm_directory_view_popup_background_context_menu  (FMDirectoryView *view);
void                      fm_directory_view_popup_item_context_menu  (FMDirectoryView *view,
								      NautilusFile *file); 

#endif /* FM_DIRECTORY_VIEW_H */
