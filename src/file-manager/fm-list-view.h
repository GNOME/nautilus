/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.h - interface for list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001 Anders Carlsson <andersca@gnu.org>
   
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
            Anders Carlsson <andersca@gnu.org>
*/

#ifndef FM_LIST_VIEW_H
#define FM_LIST_VIEW_H

#include "fm-directory-view.h"

#define FM_TYPE_LIST_VIEW		(fm_list_view_get_type ())
#define FM_LIST_VIEW(obj)		(GTK_CHECK_CAST ((obj), FM_TYPE_LIST_VIEW, FMListView))
#define FM_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_LIST_VIEW, FMListViewClass))
#define FM_IS_LIST_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_LIST_VIEW))
#define FM_IS_LIST_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_LIST_VIEW))

#define FM_LIST_VIEW_ID "OAFIID:Nautilus_File_Manager_List_View"

typedef struct FMListViewDetails FMListViewDetails;

typedef struct {
	FMDirectoryView parent_instance;
	FMListViewDetails *details;
} FMListView;

typedef struct {
	FMDirectoryViewClass parent_class;
} FMListViewClass;

GType fm_list_view_get_type (void);
void  fm_list_view_register (void);

#endif /* FM_LIST_VIEW_H */
