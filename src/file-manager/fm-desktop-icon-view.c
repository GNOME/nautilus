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
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
}

static void
fm_desktop_icon_view_quit_menu_item_callback (GtkMenuItem *item, gpointer callback_data)
{
	fm_directory_view_quit_nautilus (FM_DIRECTORY_VIEW (callback_data));
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

        menu_item = gtk_menu_item_new_with_label (_("Quit Nautilus"));
	gtk_signal_connect (GTK_OBJECT (menu_item),
			    "activate",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_quit_menu_item_callback),
			    view);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}
