/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-search-list-view.h - interface for list view of a virtual directory.

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

   Authors: Rebecca Schulman <rebecka@eazel.com>
*/

#ifndef FM_SEARCH_LIST_VIEW_H
#define FM_SEARCH_LIST_VIEW_H

#include "fm-list-view.h"

#define FM_TYPE_SEARCH_LIST_VIEW		(fm_search_list_view_get_type ())
#define FM_SEARCH_LIST_VIEW(obj)		(GTK_CHECK_CAST ((obj), FM_TYPE_SEARCH_LIST_VIEW, FMSearchListView))
#define FM_SEARCH_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_SEARCH_LIST_VIEW, FMSearch_ListViewClass))
#define FM_IS_SEARCH_LIST_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_SEARCH_LIST_VIEW))
#define FM_IS_SEARCH_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_SEARCH_LIST_VIEW))

typedef struct FMSearchListViewDetails FMSearchListViewDetails;

typedef struct {
	FMListView parent_slot;
	FMSearchListViewDetails *details;
} FMSearchListView;

typedef struct {
	FMListViewClass parent_slot;
} FMSearchListViewClass;

/* GtkObject support */
GtkType fm_search_list_view_get_type (void);

#endif /* FM_SEARCH_LIST_VIEW_H */
