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

#include "fm-directory-view-icons.h"

#include "fm-icon-cache.h"
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-string.h>
#include <libnautilus/nautilus-background.h>

#include <ctype.h>
#include <errno.h>

#define DEFAULT_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"


/* forward declarations */
static void add_icon_at_free_position		 (FMDirectoryViewIcons *icon_view,
		       				  NautilusFile *file);
static void add_icon_if_already_positioned	 (FMDirectoryViewIcons *icon_view,
		       				  NautilusFile *file);
static GnomeIconContainer *create_icon_container (FMDirectoryViewIcons *icon_view);
static void display_icons_not_already_positioned (FMDirectoryViewIcons *icon_view);
static void fm_directory_view_icons_icon_moved_cb (GnomeIconContainer *container,
						   const char *icon_name,
						   NautilusFile *icon_data,
						   int x, int y,
						   FMDirectoryViewIcons *icon_view);
static void fm_directory_view_icons_add_entry    (FMDirectoryView *view, 
					          NautilusFile *file);
static void fm_directory_view_icons_background_changed_cb (NautilusBackground *background,
							   FMDirectoryViewIcons *icon_view);
static void fm_directory_view_icons_begin_loading
				          	 (FMDirectoryView *view);
static void fm_directory_view_icons_clear 	 (FMDirectoryView *view);
static void fm_directory_view_icons_destroy      (GtkObject *view);
static void fm_directory_view_icons_done_adding_entries 
				          	 (FMDirectoryView *view);
static NautilusFileList * fm_directory_view_icons_get_selection
						 (FMDirectoryView *view);
static void fm_directory_view_icons_initialize   (FMDirectoryViewIcons *icon_view);
static void fm_directory_view_icons_initialize_class (FMDirectoryViewIconsClass *klass);
static GnomeIconContainer *get_icon_container 	 (FMDirectoryViewIcons *icon_view);
static void icon_container_activate_cb 		 (GnomeIconContainer *container,
						  const gchar *icon_name,
						  NautilusFile *icon_data,
						  FMDirectoryViewIcons *icon_view);
static void icon_container_selection_changed_cb  (GnomeIconContainer *container,
						  FMDirectoryViewIcons *icon_view);

static void set_up_base_uri			 (FMDirectoryViewIcons *icon_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryViewIcons, fm_directory_view_icons, FM_TYPE_DIRECTORY_VIEW);


struct _FMDirectoryViewIconsDetails
{
	NautilusFileList *icons_not_positioned;
};


/* GtkObject methods.  */

static void
fm_directory_view_icons_initialize_class (FMDirectoryViewIconsClass *klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

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
fm_directory_view_icons_initialize (FMDirectoryViewIcons *directory_view_icons)
{
	GnomeIconContainer *icon_container;
	
	g_return_if_fail (GTK_BIN (directory_view_icons)->child == NULL);

	directory_view_icons->details = g_new0 (FMDirectoryViewIconsDetails, 1);

	icon_container = create_icon_container (directory_view_icons);
	gnome_icon_container_set_icon_mode
		(icon_container, GNOME_ICON_CONTAINER_NORMAL_ICONS);
}

static void
fm_directory_view_icons_destroy (GtkObject *object)
{
	FMDirectoryViewIcons *icon_view;

	icon_view = FM_DIRECTORY_VIEW_ICONS (object);

	g_list_free (icon_view->details->icons_not_positioned);
	icon_view->details->icons_not_positioned = NULL;

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
			    "icon_moved",
			    GTK_SIGNAL_FUNC (fm_directory_view_icons_icon_moved_cb),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (icon_container_selection_changed_cb),
			    icon_view);

	gtk_signal_connect (GTK_OBJECT (nautilus_get_widget_background (GTK_WIDGET (icon_container))),
			    "changed",
			    GTK_SIGNAL_FUNC (fm_directory_view_icons_background_changed_cb),
			    icon_view);

	gtk_container_add (GTK_CONTAINER (icon_view), GTK_WIDGET (icon_container));

	gtk_widget_show (GTK_WIDGET (icon_container));
	
	fm_directory_view_populate (FM_DIRECTORY_VIEW (icon_view));

	return icon_container;
}

static GnomeIconContainer *
get_icon_container (FMDirectoryViewIcons *icon_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icon_view), NULL);
	g_return_val_if_fail (GNOME_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return GNOME_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
add_icon_if_already_positioned (FMDirectoryViewIcons *icon_view,
				NautilusFile *file)
{
	NautilusDirectory *directory;
	char *position_string;
	gboolean position_good;
	int x, y;
	GdkPixbuf *image;
	char *name;

	/* Get the current position of this icon from the metadata. */
	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view));
	position_string = nautilus_file_get_metadata (file, "ICON_POSITION", "");
	position_good = sscanf (position_string, " %d , %d %*s", &x, &y) == 2;
	g_free (position_string);

	if (!position_good) {
		icon_view->details->icons_not_positioned =
			g_list_prepend (icon_view->details->icons_not_positioned, file);
		return;
	}

	/* Get the appropriate image and name for the file. For the moment,
	 * we always use the standard size of icons.
	 */
	image = fm_icon_cache_get_icon_for_file (fm_get_current_icon_cache (), 		
						 file,
						 NAUTILUS_ICON_SIZE_STANDARD);
	name = nautilus_file_get_name (file);
	gnome_icon_container_add_pixbuf (get_icon_container (icon_view),
					 image, name, x, y, file);
	g_free (name);
}

static void
add_icon_at_free_position (FMDirectoryViewIcons *icon_view,
			   NautilusFile *file)
{
	GdkPixbuf *image;
	char *name;

	/* Get the appropriate image and name for the file. */
	image = fm_icon_cache_get_icon_for_file (fm_get_current_icon_cache (), 		
						 file,
						 NAUTILUS_ICON_SIZE_STANDARD);
	name = nautilus_file_get_name (file);
	gnome_icon_container_add_pixbuf_auto (get_icon_container (icon_view),
					      image, name, file);
	g_free (name);
}

static void
display_icons_not_already_positioned (FMDirectoryViewIcons *view)
{
	GList *p;

	/* FIXME: This will block if there are many files.  */
	for (p = view->details->icons_not_positioned; p != NULL; p = p->next)
		add_icon_at_free_position (view, p->data);

	g_list_free (view->details->icons_not_positioned);
	view->details->icons_not_positioned = NULL;
}

/* Set up the base URI for Drag & Drop operations.  */
static void
set_up_base_uri (FMDirectoryViewIcons *icon_view)
{
	char *txt_uri;

	g_return_if_fail (get_icon_container(icon_view) != NULL);

	txt_uri = nautilus_directory_get_uri
		(fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view)));
	if (txt_uri == NULL)
		return;

	gnome_icon_container_set_base_uri (get_icon_container (icon_view), txt_uri);

	g_free (txt_uri);
}

static void
fm_directory_view_icons_clear (FMDirectoryView *view)
{
	GnomeIconContainer *icon_container;
	char *background_color;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));

	icon_container = get_icon_container (FM_DIRECTORY_VIEW_ICONS (view));

	/* Clear away the existing icons. */
	gnome_icon_container_clear (icon_container);

	/* Set up the background color from the metadata. */
	background_color = nautilus_directory_get_metadata (fm_directory_view_get_model (view),
							    "ICON_VIEW_BACKGROUND_COLOR",
							    DEFAULT_BACKGROUND_COLOR);
	nautilus_background_set_color (nautilus_get_widget_background (GTK_WIDGET (icon_container)),
				       background_color);
	g_free (background_color);
}

static void
fm_directory_view_icons_add_entry (FMDirectoryView *view, NautilusFile *file)
{
	add_icon_if_already_positioned (FM_DIRECTORY_VIEW_ICONS (view), file);
}

static void
fm_directory_view_icons_done_adding_entries (FMDirectoryView *view)
{
	display_icons_not_already_positioned (FM_DIRECTORY_VIEW_ICONS (view));
}

static void
fm_directory_view_icons_begin_loading (FMDirectoryView *view)
{
	set_up_base_uri (FM_DIRECTORY_VIEW_ICONS (view));
}

static GList *
fm_directory_view_icons_get_selection (FMDirectoryView *view)
{
	return gnome_icon_container_get_selection
		(get_icon_container (FM_DIRECTORY_VIEW_ICONS (view)));
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
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));

	gnome_icon_container_line_up (get_icon_container (icon_view));
}

static void
icon_container_activate_cb (GnomeIconContainer *container,
			    const gchar *name,
			    NautilusFile *file,
			    FMDirectoryViewIcons *icon_view)
{
	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (file != NULL);

	fm_directory_view_activate_entry (FM_DIRECTORY_VIEW (icon_view), file);
}

static void
icon_container_selection_changed_cb (GnomeIconContainer *container,
				     FMDirectoryViewIcons *icon_view)
{
	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));
	g_assert (container == get_icon_container (icon_view));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (icon_view));
}

static void
fm_directory_view_icons_background_changed_cb (NautilusBackground *background,
					       FMDirectoryViewIcons *icon_view)
{
	NautilusDirectory *directory;
	char *color_spec;

	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));
	g_assert (background == nautilus_get_widget_background
		  (GTK_WIDGET (get_icon_container (icon_view))));

	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view));
	if (directory == NULL)
		return;
	
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (directory,
					 "ICON_VIEW_BACKGROUND_COLOR",
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);
}

static void
fm_directory_view_icons_icon_moved_cb (GnomeIconContainer *container,
				       const char *name,
				       NautilusFile *file,
				       int x, int y,
				       FMDirectoryViewIcons *icon_view)
{
	NautilusDirectory *directory;
	char *position_string;

	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (file != NULL);

	/* Store the new position of the icon in the metadata. */
	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view));
	position_string = g_strdup_printf ("%d,%d", x, y);
	nautilus_file_set_metadata (file, "ICON_POSITION", NULL, position_string);
	g_free (position_string);
}
