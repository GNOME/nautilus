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
#include "fm-icon-cache.h"


/* forward declarations */
static void add_to_icon_container 		 (FMDirectoryViewIcons *icon_view,
		       				  FMIconCache *icon_manager,
		       				  GnomeIconContainer *icon_container,
		       				  GnomeVFSFileInfo *info,
		       				  gboolean with_layout);
static GnomeIconContainer *create_icon_container (FMDirectoryViewIcons *icon_view);
static void display_icons_not_in_layout 	 (FMDirectoryViewIcons *icon_view);
static void fm_directory_view_icons_add_entry    (FMDirectoryView *view, 
					          GnomeVFSFileInfo *info);
static void fm_directory_view_icons_done_adding_entries 
				          	 (FMDirectoryView *view);
static void fm_directory_view_icons_begin_loading
				          	 (FMDirectoryView *view);
static void fm_directory_view_icons_clear 	 (FMDirectoryView *view);
static GList * fm_directory_view_icons_get_selection
						 (FMDirectoryView *view);
static GnomeIconContainer *get_icon_container 	 (FMDirectoryViewIcons *icon_view);
static void icon_container_activate_cb 		 (GnomeIconContainer *ignored,
						  const gchar *name,
						  gpointer icon_data,
						  gpointer data);
static void icon_container_selection_changed_cb  (GnomeIconContainer *container,
						  gpointer data);

static void set_up_base_uri			 (FMDirectoryViewIcons *icon_view);



static FMDirectoryViewClass *parent_class = NULL;

struct _FMDirectoryViewIconsDetails
{
	const GnomeIconContainerLayout *icon_layout;
	GList *icons_not_in_layout;
};


/* GtkObject methods.  */

static void
fm_directory_view_icons_destroy (GtkObject *object)
{
	FMDirectoryViewIcons *icon_view;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (object));

	icon_view = FM_DIRECTORY_VIEW_ICONS (object);
	g_list_free (icon_view->details->icons_not_in_layout);
	icon_view->details->icons_not_in_layout = NULL;

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
	
	fm_directory_view_class->clear 
		= fm_directory_view_icons_clear;
	fm_directory_view_class->add_entry 
		= fm_directory_view_icons_add_entry;
	fm_directory_view_class->done_adding_entries 
		= fm_directory_view_icons_done_adding_entries;	
	fm_directory_view_class->begin_loading 
		= fm_directory_view_icons_begin_loading;
	fm_directory_view_class->get_selection 
		= fm_directory_view_icons_get_selection;	
}

static void
fm_directory_view_icons_initialize (gpointer object, gpointer klass)
{
	FMDirectoryViewIcons *directory_view_icons;
	GnomeIconContainer *icon_container;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (object));
	g_return_if_fail (GTK_BIN (object)->child == NULL);

	directory_view_icons = FM_DIRECTORY_VIEW_ICONS (object);
	
	icon_container = create_icon_container (directory_view_icons);
	gnome_icon_container_set_icon_mode
		(icon_container, GNOME_ICON_CONTAINER_NORMAL_ICONS);

	directory_view_icons->details = g_new0 (FMDirectoryViewIconsDetails, 1);
}

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (FMDirectoryViewIcons, fm_directory_view_icons, FM_TYPE_DIRECTORY_VIEW);

/**
 * fm_directory_view_icons_new:
 *
 * Create a new FMDirectoryViewIcons.
 * 
 * Return value: The newly-allocated FMDirectoryViewIcons.
 **/
GtkWidget *
fm_directory_view_icons_new (void)
{
	return gtk_widget_new (fm_directory_view_icons_get_type (), NULL);
}


static GnomeIconContainer *
create_icon_container (FMDirectoryViewIcons *icon_view)
{
	GnomeIconContainer *icon_container;

	icon_container = GNOME_ICON_CONTAINER (gnome_icon_container_new ());
	GTK_WIDGET_SET_FLAGS (icon_container, GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "activate",
			    GTK_SIGNAL_FUNC (icon_container_activate_cb),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (icon_container_selection_changed_cb),
			    icon_view);

	gtk_container_add (GTK_CONTAINER (icon_view), GTK_WIDGET (icon_container));

	gtk_widget_show (GTK_WIDGET (icon_container));
	
	fm_directory_view_populate (FM_DIRECTORY_VIEW (icon_view));

	return icon_container;
}

static void
display_icons_not_in_layout (FMDirectoryViewIcons *view)
{
	FMIconCache *icon_manager;
	GnomeIconContainer *icon_container;
	GList *p;

	if (view->details->icons_not_in_layout == NULL)
		return;

	icon_manager = fm_get_current_icon_cache();

	icon_container = get_icon_container (view);
	g_return_if_fail (icon_container != NULL);

	/* FIXME: This will block if there are many files.  */

	for (p = view->details->icons_not_in_layout; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		add_to_icon_container (view, icon_manager,
				       icon_container, info, FALSE);
	}

	g_list_free (view->details->icons_not_in_layout);
	view->details->icons_not_in_layout = NULL;
}


static GnomeIconContainer *
get_icon_container (FMDirectoryViewIcons *icon_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icon_view), NULL);
	g_return_val_if_fail (GNOME_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return GNOME_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
add_to_icon_container (FMDirectoryViewIcons *icon_view,
		       FMIconCache *icon_manager,
		       GnomeIconContainer *icon_container,
		       GnomeVFSFileInfo *info,
		       gboolean with_layout)
{
	GdkPixbuf *image;

	g_return_if_fail(info);

	image = fm_icon_cache_get_icon (icon_manager, info);

	if (! with_layout || icon_view->details->icon_layout == NULL) {
		gnome_icon_container_add_pixbuf_auto (icon_container,
						     image,
						     info->name,
						     info);
	} else {
		gboolean result;

		result = gnome_icon_container_add_pixbuf_with_layout
			(icon_container, image, info->name, info,
			 icon_view->details->icon_layout);
		if (! result)
			icon_view->details->icons_not_in_layout = g_list_prepend
				(icon_view->details->icons_not_in_layout, info);
	}
}

/* Set up the base URI for Drag & Drop operations.  */
static void
set_up_base_uri (FMDirectoryViewIcons *icon_view)
{
	gchar *txt_uri;

	g_return_if_fail (get_icon_container(icon_view) != NULL);

	txt_uri = gnome_vfs_uri_to_string (fm_directory_view_get_uri (FM_DIRECTORY_VIEW (icon_view)), 0);
	if (txt_uri == NULL)
		return;

	gnome_icon_container_set_base_uri (get_icon_container (icon_view), txt_uri);

	g_free (txt_uri);
}

static void
fm_directory_view_icons_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	gnome_icon_container_clear (get_icon_container (FM_DIRECTORY_VIEW_ICONS (view)));
}

static void
fm_directory_view_icons_add_entry (FMDirectoryView *view, GnomeVFSFileInfo *info)
{
	FMDirectoryViewIcons *icon_view;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	icon_view = FM_DIRECTORY_VIEW_ICONS (view);

	add_to_icon_container (icon_view, 
			       fm_get_current_icon_cache(), 
			       get_icon_container (icon_view), 
			       info, 
			       TRUE);
}

static void
fm_directory_view_icons_done_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	display_icons_not_in_layout (FM_DIRECTORY_VIEW_ICONS (view));
}

static void
fm_directory_view_icons_begin_loading (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	set_up_base_uri (FM_DIRECTORY_VIEW_ICONS (view));
}

static GList *
fm_directory_view_icons_get_selection (FMDirectoryView *view)
{
	FMDirectoryViewIcons *icon_view;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view), NULL);

	icon_view = FM_DIRECTORY_VIEW_ICONS (view);
	return gnome_icon_container_get_selection (get_icon_container (icon_view));
}


/* WARNING WARNING WARNING

   These two functions actually do completely different things, although they
   have similiar name.  (Actually, maybe I should change these names: FIXME.)

   The `get' function retrieves the current *actual* layout from the icon
   container.  The `set' function, instead, specifies the layout that will be
   used when adding new files to the view.  

   These two functions might become entirely obsolete. They are currently unused.
*/

/**
 * fm_directory_view_get_icon_layout:
 *
 * Get a GnomeIconContainerLayout representing how icons are 
 * currently positioned in this view. The caller is responsible for destroying
 * this object.
 * @view: FMDirectoryViewIcons in question.
 * 
 * Return value: A newly-allocated GnomeIconContainerLayout object specifying
 * positions for the currently-displayed set of icons.
 * 
 **/
GnomeIconContainerLayout *
fm_directory_view_icons_get_icon_layout (FMDirectoryViewIcons *view)
{
	GnomeIconContainer *icon_container;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view), NULL);

	icon_container = get_icon_container (view);
	return gnome_icon_container_get_layout (icon_container);
}

/**
 * fm_directory_view_set_icon_layout:
 *
 * Supply a GnomeIconContainerLayout object to use for positioning
 * icons that are added to this view in the future.
 * @view: FMDirectoryViewIcons in question.
 * @layout: GnomeIconContainerLayout to use in future.
 * 
 **/
void
fm_directory_view_icons_set_icon_layout (FMDirectoryViewIcons *view,
				   const GnomeIconContainerLayout *layout)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	view->details->icon_layout = layout;
}

/**
 * fm_directory_view_icons_line_up_icons:
 *
 * Line up the icons in this view.
 * @icon_view: FMDirectoryViewIcons whose ducks should be in a row.
 * 
 **/
void
fm_directory_view_icons_line_up_icons (FMDirectoryViewIcons *icon_view)
{
	GnomeIconContainer *container;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));
	g_return_if_fail (get_icon_container (icon_view) != NULL);

	gnome_icon_container_line_up (get_icon_container (icon_view));
}

static void
icon_container_activate_cb (GnomeIconContainer *ignored,
			    const gchar *name,
			    gpointer entry_data,
			    gpointer data)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (data));
	g_return_if_fail (entry_data != NULL);
	/* Name from icon container had better match name in file info */
	g_return_if_fail (strcmp (name, ((GnomeVFSFileInfo *)entry_data)->name) == 0);

	fm_directory_view_activate_entry (FM_DIRECTORY_VIEW (data),
					  (GnomeVFSFileInfo *) entry_data);
}

static void
icon_container_selection_changed_cb (GnomeIconContainer *container,
				     gpointer data)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (data));
	g_return_if_fail (container == get_icon_container (FM_DIRECTORY_VIEW_ICONS (data)));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (data));
}

