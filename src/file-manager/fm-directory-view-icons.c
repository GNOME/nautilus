/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view-icons.c - implementation of icon view of directory.

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

#include <libnautilus/nautilus-gtk-macros.h>

#include "fm-directory-view.h"
#include "fm-directory-view-icons.h"

static FMDirectoryViewClass *parent_class = NULL;


/* forward declarations */
static GnomeIconContainer *create_icon_container (FMDirectoryViewIcons *view);
static gint display_icon_container_selection_info_idle_cb (gpointer data);
static void fm_directory_view_icons_clear (FMDirectoryView *view);
static void icon_container_activate_cb (GnomeIconContainer *icon_container,
					const gchar *name,
					gpointer icon_data,
					gpointer data);
static void icon_container_selection_changed_cb (GnomeIconContainer *container,
						 gpointer data);



/* GtkObject methods.  */

static void
fm_directory_view_icons_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


static void
fm_directory_view_icons_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
	
	object_class->destroy = fm_directory_view_icons_destroy;
	fm_directory_view_class->clear = fm_directory_view_icons_clear;
}

static void
fm_directory_view_icons_initialize (gpointer object, gpointer klass)
{
	GnomeIconContainer *icon_container;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (object));

	/* FIXME: eventually get rid of set_mode call entirely. */
	fm_directory_view_set_mode (FM_DIRECTORY_VIEW (object), 
				    FM_DIRECTORY_VIEW_MODE_ICONS);

	g_assert (GTK_BIN (object)->child == NULL);
	icon_container = create_icon_container (object);
	gnome_icon_container_set_icon_mode
		(icon_container, GNOME_ICON_CONTAINER_NORMAL_ICONS);
}

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (FMDirectoryViewIcons, fm_directory_view_icons, FM_TYPE_DIRECTORY_VIEW);

GtkWidget *
fm_directory_view_icons_new (void)
{
	return gtk_widget_new (fm_directory_view_icons_get_type (), NULL);
}


static GnomeIconContainer *
create_icon_container (FMDirectoryViewIcons *view)
{
	GnomeIconContainer *icon_container;

	icon_container = GNOME_ICON_CONTAINER (gnome_icon_container_new ());
	GTK_WIDGET_SET_FLAGS (icon_container, GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "activate",
			    GTK_SIGNAL_FUNC (icon_container_activate_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (icon_container_selection_changed_cb),
			    view);

	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (icon_container));

	gtk_widget_show (GTK_WIDGET (icon_container));
	load_icon_container (FM_DIRECTORY_VIEW (view), icon_container);

	return icon_container;
}

static gint
display_icon_container_selection_info_idle_cb (gpointer data)
{
	FMDirectoryView *view;
	GnomeIconContainer *icon_container;
	GList *selection;

	view = FM_DIRECTORY_VIEW (data);
	icon_container = get_icon_container (view);

	selection = gnome_icon_container_get_selection (icon_container);
	display_selection_info (view, selection);
	g_list_free (selection);

	view->display_selection_idle_id = 0;

	return FALSE;
}

static void
fm_directory_view_icons_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	gnome_icon_container_clear (get_icon_container (view));
}


/* WARNING WARNING WARNING

   These two functions actually do completely different things, although they
   have similiar name.  (Actually, maybe I should change these names: FIXME.)

   The `get' function retrieves the current *actual* layout from the icon
   container.  The `set' function, instead, specifies the layout that will be
   used when adding new files to the view.  */

GnomeIconContainerLayout *
fm_directory_view_icons_get_icon_layout (FMDirectoryViewIcons *view)
{
	GnomeIconContainer *icon_container;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view), NULL);

	icon_container = get_icon_container (FM_DIRECTORY_VIEW (view));
	return gnome_icon_container_get_layout (icon_container);
}

void
fm_directory_view_icons_set_icon_layout (FMDirectoryViewIcons *view,
				   const GnomeIconContainerLayout *layout)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	FM_DIRECTORY_VIEW (view)->icon_layout = layout;
}

void
fm_directory_view_icons_line_up_icons (FMDirectoryViewIcons *view)
{
	GnomeIconContainer *container;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));
	/* FIXME: Get rid of FM_DIRECTORY_VIEW_ICONS casts after
	 * get_icon_container is moved into this subclass.
	 */
	g_return_if_fail (get_icon_container (FM_DIRECTORY_VIEW (view)) != NULL);

	gnome_icon_container_line_up (get_icon_container (FM_DIRECTORY_VIEW (view)));
}

static void
icon_container_activate_cb (GnomeIconContainer *icon_container,
			    const gchar *name,
			    gpointer icon_data,
			    gpointer data)
{
	FMDirectoryView *directory_view;
	GnomeVFSURI *new_uri;
	GnomeVFSFileInfo *info;
	Nautilus_NavigationRequestInfo nri;

	info = (GnomeVFSFileInfo *) icon_data;
	directory_view = FM_DIRECTORY_VIEW (data);

	new_uri = gnome_vfs_uri_append_path (directory_view->uri, name);

	nri.requested_uri = gnome_vfs_uri_to_string(new_uri, 0);
	nri.new_window_default = nri.new_window_suggested = Nautilus_V_FALSE;
	nri.new_window_enforced = Nautilus_V_UNKNOWN;
	nautilus_view_frame_request_location_change(NAUTILUS_VIEW_FRAME(directory_view->view_frame),
						     &nri);
	g_free(nri.requested_uri);
	gnome_vfs_uri_unref (new_uri);
}

static void
icon_container_selection_changed_cb (GnomeIconContainer *container,
				     gpointer data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);
	if (view->display_selection_idle_id == 0)
		view->display_selection_idle_id = gtk_idle_add
			(display_icon_container_selection_info_idle_cb,
			 view);
}

