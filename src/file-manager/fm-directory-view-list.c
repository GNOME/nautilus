/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view-list.c - implementation of list view of directory.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnome.h>

#include "fm-directory-view.h"
#include "fm-directory-view-list.h"

static FMDirectoryViewClass *parent_class = NULL;


/* GtkObject methods.  */

static void
fm_directory_view_list_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
fm_directory_view_list_class_init (FMDirectoryViewListClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
	object_class->destroy = fm_directory_view_list_destroy;
}

static void
fm_directory_view_list_init (FMDirectoryViewList *directory_view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (directory_view));

	fm_directory_view_set_mode (FM_DIRECTORY_VIEW (directory_view), 
				    FM_DIRECTORY_VIEW_MODE_DETAILED);
}

GtkType
fm_directory_view_list_get_type (void)
{
	static GtkType directory_view_list_type = 0;

	if (directory_view_list_type == 0) {
		static GtkTypeInfo directory_view_list_info = {
			"FMDirectoryViewList",
			sizeof (FMDirectoryViewList),
			sizeof (FMDirectoryViewListClass),
			(GtkClassInitFunc) fm_directory_view_list_class_init,
			(GtkObjectInitFunc) fm_directory_view_list_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		directory_view_list_type
			= gtk_type_unique (fm_directory_view_get_type (),
					   &directory_view_list_info);
	}

	return directory_view_list_type;
}

GtkWidget *
fm_directory_view_list_new (void)
{
	return gtk_widget_new (fm_directory_view_list_get_type (), NULL);
}
