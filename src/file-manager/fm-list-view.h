/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.h - interface for list view of directory.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef FM_LIST_VIEW_H
#define FM_LIST_VIEW_H

/* This is not a general purpose class.
 * It has just enough generality to be reused by the search list view.
 * But for more use it would have to be refactored more.
 */

#include "fm-directory-view.h"

#define FM_TYPE_LIST_VIEW		(fm_list_view_get_type ())
#define FM_LIST_VIEW(obj)		(GTK_CHECK_CAST ((obj), FM_TYPE_LIST_VIEW, FMListView))
#define FM_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_LIST_VIEW, FMListViewClass))
#define FM_IS_LIST_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_LIST_VIEW))
#define FM_IS_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_LIST_VIEW))

typedef struct FMListViewDetails FMListViewDetails;
typedef struct FMListViewColumn FMListViewColumn;

typedef struct {
	FMDirectoryView parent_slot;
	FMListViewDetails *details;
} FMListView;

typedef struct {
	FMDirectoryViewClass parent_slot;

	void	 (* adding_file)		(FMListView	  *list_view,
						 NautilusFile	  *file);
	void	 (* removing_file)		(FMListView	  *list_view,
						 NautilusFile	  *file);
	int    	 (* get_number_of_columns)      (FMListView       *list_view);
	int    	 (* get_emblems_column)         (FMListView       *list_view);
	int    	 (* get_link_column)            (FMListView       *list_view);
	void   	 (* get_column_specification)   (FMListView       *list_view,
					         int               column_number,
					         FMListViewColumn *specification);
	char * 	 (* get_default_sort_attribute) (FMListView       *list_view);
} FMListViewClass;

/* GtkObject support */
GtkType fm_list_view_get_type (void);

#endif /* FM_LIST_VIEW_H */
