/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view-list.h - interface for list view of directory.

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

#ifndef __FM_DIRECTORY_VIEW_LIST_H__
#define __FM_DIRECTORY_VIEW_LIST_H__



typedef struct _FMDirectoryViewList      FMDirectoryViewList;
typedef struct _FMDirectoryViewListClass FMDirectoryViewListClass;

#define FM_TYPE_DIRECTORY_VIEW_LIST			(fm_directory_view_list_get_type ())
#define FM_DIRECTORY_VIEW_LIST(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW_LIST, FMDirectoryViewList))
#define FM_DIRECTORY_VIEW_LIST_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW_LIST, FMDirectoryViewListClass))
#define FM_IS_DIRECTORY_VIEW_LIST(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_LIST))
#define FM_IS_DIRECTORY_VIEW_LIST_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_LIST))

struct _FMDirectoryViewList {
	FMDirectoryView parent;
};

struct _FMDirectoryViewListClass {
	FMDirectoryViewClass parent_class;
};


/* GtkObject support */
GtkType    fm_directory_view_list_get_type (void);
GtkWidget *fm_directory_view_list_new      (void);

#endif /* __FM_DIRECTORY_VIEW_LIST_H__ */
