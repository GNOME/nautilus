/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-list-column-title.h: List column title widget for interacting with list columns

   Copyright (C) 2000 Eazel, Inc.

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
  
   Authors: Pavel Cisler <pavel@eazel.com>

*/

#ifndef __NAUTILUS_LIST_COLUMN_TITLE__
#define __NAUTILUS_LIST_COLUMN_TITLE__

#include <gdk/gdktypes.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_LIST_COLUMN_TITLE	\
	(nautilus_list_column_title_get_type ())
#define NAUTILUS_LIST_COLUMN_TITLE(obj)	\
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LIST_COLUMN_TITLE, NautilusListColumnTitle))
#define NAUTILUS_LIST_COLUMN_TITLE_CLASS(klass)	\
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LIST_COLUMN_TITLE, NautilusListColumnTitleClass))
#define NAUTILUS_IS_LIST_COLUMN_TITLE(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LIST_COLUMN_TITLE))
#define NAUTILUS_IS_LIST_COLUMN_TITLE_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LIST_COLUMN_TITLE))

typedef struct NautilusListColumnTitle		NautilusListColumnTitle;
typedef struct NautilusListColumnTitleClass  	NautilusListColumnTitleClass;
typedef struct NautilusListColumnTitleDetails	NautilusListColumnTitleDetails;


struct NautilusListColumnTitle
{
	GtkBin bin;
	NautilusListColumnTitleDetails *details;
};

struct NautilusListColumnTitleClass
{
	GtkBinClass parent_class;
};

GtkType	   			nautilus_list_column_title_get_type	(void);
NautilusListColumnTitle	        *nautilus_list_column_title_new		(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __NAUTILUS_LIST_COLUMN_TITLE__ */
