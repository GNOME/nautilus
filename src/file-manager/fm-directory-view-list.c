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

#include <libnautilus/gtkflist.h>

#include "fm-directory-view.h"
#include "fm-directory-view-list.h"
#include "fm-icon-cache.h"

static FMDirectoryViewClass *parent_class = NULL;


/* forward declarations */
static GtkFList *create_flist (FMDirectoryViewList *view);
static gint display_flist_selection_info_idle_cb (gpointer data);
static void flist_activate_cb (GtkFList *flist,
			       gpointer entry_data,
			       gpointer data);
static void flist_selection_changed_cb (GtkFList *flist, gpointer data);
static void fm_directory_view_list_clear (FMDirectoryView *view);




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
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (class);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (class);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
	
	object_class->destroy = fm_directory_view_list_destroy;
	fm_directory_view_class->clear = fm_directory_view_list_clear;	
}

static void
fm_directory_view_list_init (FMDirectoryViewList *directory_view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (directory_view));

	/* FIXME: eventually get rid of set_mode call entirely. */
	fm_directory_view_set_mode (FM_DIRECTORY_VIEW (directory_view), 
				    FM_DIRECTORY_VIEW_MODE_DETAILED);

	g_assert (GTK_BIN (directory_view)->child == NULL);
	create_flist (directory_view);
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


static GtkFList *
create_flist (FMDirectoryViewList *view)
{
	GtkFList *flist;
	gchar *titles[] = {
		"Name",
		NULL
	};

	flist = GTK_FLIST (gtk_flist_new_with_titles (2, titles));
	gtk_clist_set_column_width (GTK_CLIST (flist), 0, 150); /* FIXME */
	GTK_WIDGET_SET_FLAGS (flist, GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (flist),
			    "activate",
			    GTK_SIGNAL_FUNC (flist_activate_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (flist_selection_changed_cb),
			    view);
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (flist));

	gtk_widget_show (GTK_WIDGET (flist));

	if (FM_DIRECTORY_VIEW (view)->directory_list != NULL) {
		GnomeVFSDirectoryListPosition *position;
		FMIconCache *icon_manager;

		icon_manager = fm_get_current_icon_cache();

		position = gnome_vfs_directory_list_get_first_position
			(FM_DIRECTORY_VIEW (view)->directory_list);

		gtk_clist_freeze (GTK_CLIST (flist));

		while (position != FM_DIRECTORY_VIEW (view)->current_position) {
			GnomeVFSFileInfo *info;

			info = gnome_vfs_directory_list_get
				(FM_DIRECTORY_VIEW (view)->directory_list, position);
			add_to_flist (icon_manager, flist, info);

			position = gnome_vfs_directory_list_position_next
				(position);
		}

		gtk_clist_thaw (GTK_CLIST (flist));
	}

	return flist;
}
 


static gint
display_flist_selection_info_idle_cb (gpointer data)
{
	FMDirectoryView *view;
	GtkFList *flist;
	GList *selection;

	view = FM_DIRECTORY_VIEW (data);
	flist = get_flist (view);

	selection = gtk_flist_get_selection (flist);
	display_selection_info (view, selection);
	g_list_free (selection);

	view->display_selection_idle_id = 0;

	return FALSE;
}

static void
flist_activate_cb (GtkFList *flist,
		   gpointer entry_data,
		   gpointer data)
{
	FMDirectoryView *directory_view;
	GnomeVFSURI *new_uri;
	GnomeVFSFileInfo *info;
	Nautilus_NavigationRequestInfo nri;

	info = (GnomeVFSFileInfo *) entry_data;
	directory_view = FM_DIRECTORY_VIEW (data);

	new_uri = gnome_vfs_uri_append_path (directory_view->uri, info->name);
	nri.requested_uri = gnome_vfs_uri_to_string(new_uri, 0);
	nri.new_window_default = nri.new_window_suggested = Nautilus_V_FALSE;
	nri.new_window_enforced = Nautilus_V_UNKNOWN;
	nautilus_view_frame_request_location_change(NAUTILUS_VIEW_FRAME(directory_view->view_frame),
						     &nri);
	g_free(nri.requested_uri);
	gnome_vfs_uri_unref (new_uri);
}

static void
flist_selection_changed_cb (GtkFList *flist,
			    gpointer data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);
	if (view->display_selection_idle_id == 0)
		view->display_selection_idle_id
			= gtk_idle_add (display_flist_selection_info_idle_cb,
					view);
}

static void
fm_directory_view_list_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	gtk_clist_clear(GTK_CLIST(get_flist(view)));
}

