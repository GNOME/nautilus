/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

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

   Authors: Mike Engber <engber@eazel.com>
*/

#include <config.h>
#include "fm-desktop-icon-view.h"
#include "fm-icon-view.h"

#include <gnome.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

static void fm_desktop_icon_view_initialize		(FMDesktopIconView        *desktop_icon_view);
static void fm_desktop_icon_view_initialize_class	(FMDesktopIconViewClass   *klass);

static void fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu);

static char *   fm_desktop_icon_view_get_directory_sort_by       (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_sort_by       (FMIconView *icon_view, NautilusDirectory *directory, const char* sort_by);
static gboolean fm_desktop_icon_view_get_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory, gboolean sort_reversed);
static gboolean fm_desktop_icon_view_get_directory_auto_layout   (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_auto_layout   (FMIconView *icon_view, NautilusDirectory *directory, gboolean auto_layout);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView, fm_desktop_icon_view, FM_TYPE_ICON_VIEW);

static void
fm_desktop_icon_view_initialize_class (FMDesktopIconViewClass *klass)
{
	GtkObjectClass		*object_class;
	FMDirectoryViewClass	*fm_directory_view_class;
	FMIconViewClass		*fm_icon_view_class;

	object_class		= GTK_OBJECT_CLASS (klass);
	fm_directory_view_class	= FM_DIRECTORY_VIEW_CLASS (klass);
	fm_icon_view_class	= FM_ICON_VIEW_CLASS (klass);

        fm_directory_view_class->create_background_context_menu_items = fm_desktop_icon_view_create_background_context_menu_items;

        fm_icon_view_class->get_directory_sort_by       = fm_desktop_icon_view_get_directory_sort_by;
        fm_icon_view_class->set_directory_sort_by       = fm_desktop_icon_view_set_directory_sort_by;
        fm_icon_view_class->get_directory_sort_reversed = fm_desktop_icon_view_get_directory_sort_reversed;
        fm_icon_view_class->set_directory_sort_reversed = fm_desktop_icon_view_set_directory_sort_reversed;
        fm_icon_view_class->get_directory_auto_layout   = fm_desktop_icon_view_get_directory_auto_layout;
        fm_icon_view_class->set_directory_auto_layout   = fm_desktop_icon_view_set_directory_auto_layout;
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
}

static void
fm_desktop_icon_view_close_desktop_menu_item_callback (GtkMenuItem *item, gpointer callback_data)
{
	fm_directory_view_close_desktop (FM_DIRECTORY_VIEW (callback_data));
}

static void
fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu)
{
	GtkWidget *menu_item;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_background_context_menu_items, 
		 (view, menu));

        menu_item = gtk_menu_item_new_with_label (_("Close Nautilus Desktop"));
	gtk_signal_connect (GTK_OBJECT (menu_item),
			    "activate",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_close_desktop_menu_item_callback),
			    view);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

static char *
fm_desktop_icon_view_get_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory)
{
	return g_strdup("name");
}

static void
fm_desktop_icon_view_set_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory, const char* sort_by)
{
	/* do nothing - the desktop always uses the same sort_by */
}

static gboolean
fm_desktop_icon_view_get_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory, gboolean sort_reversed)
{
	/* do nothing - the desktop always uses sort_reversed == FALSE */
}

static gboolean
fm_desktop_icon_view_get_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory, gboolean auto_layout)
{
	/* do nothing - the desktop always uses auto_layout == FALSE */
}
