/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.c - implementation of icon view of directory.

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

#include <config.h>
#include "fm-icon-view.h"

#include "fm-icon-text-window.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-icon-container.h>

/* Paths to use when creating & referring to bonobo menu items */
#define MENU_PATH_BEFORE_STRETCH_SEPARATOR   "/Settings/Before Stretch Separator"
#define MENU_PATH_STRETCH_ICON   "/Settings/Stretch"
#define MENU_PATH_RESTORE_STRETCHED_ICONS   "/Settings/Restore"
#define MENU_PATH_AFTER_STRETCH_SEPARATOR   "/Settings/After Stretch Separator"
#define MENU_PATH_CUSTOMIZE_ICON_TEXT   "/Settings/Icon Text"

/* forward declarations */
static void                   add_icon_at_free_position                         (FMIconView             *icon_view,
										 NautilusFile           *file);
static void                   add_icon_if_already_positioned                    (FMIconView             *icon_view,
										 NautilusFile           *file);
static NautilusIconContainer *create_icon_container                             (FMIconView             *icon_view);
static void                   display_icons_not_already_positioned              (FMIconView             *icon_view);
static void                   fm_icon_view_icon_changed_callback                (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 int                     x,
										 int                     y,
										 double                  scale_x,
										 double                  scale_y,
										 FMIconView             *icon_view);
static void                   fm_icon_view_add_file                             (FMDirectoryView        *view,
										 NautilusFile           *file);
static void                   fm_icon_view_file_changed                         (FMDirectoryView        *view,
										 NautilusFile           *file);
static void                   fm_icon_view_append_background_context_menu_items (FMDirectoryView        *view,
										 GtkMenu                *menu);
static void                   fm_icon_view_append_selection_context_menu_items  (FMDirectoryView        *view,
										 GtkMenu                *menu,
										 GList                  *files);
static void                   fm_icon_view_begin_loading                        (FMDirectoryView        *view);
static void                   fm_icon_view_bump_zoom_level                      (FMDirectoryView        *view,
										 int                     zoom_increment);
static gboolean               fm_icon_view_can_zoom_in                          (FMDirectoryView        *view);
static gboolean               fm_icon_view_can_zoom_out                         (FMDirectoryView        *view);
static void                   fm_icon_view_clear                                (FMDirectoryView        *view);
static void                   fm_icon_view_destroy                              (GtkObject              *view);
static void                   fm_icon_view_done_adding_files                    (FMDirectoryView        *view);
static char *                 fm_icon_view_get_icon_text_attribute_names        (FMIconView             *view);
static GList *                fm_icon_view_get_selection                        (FMDirectoryView        *view);
static NautilusZoomLevel      fm_icon_view_get_zoom_level                       (FMIconView             *view);
static void                   fm_icon_view_initialize                           (FMIconView             *icon_view);
static void                   fm_icon_view_initialize_class                     (FMIconViewClass        *klass);
static void                   fm_icon_view_merge_menus                          (FMDirectoryView        *view);
static gboolean               fm_icon_view_react_to_icon_change_idle_callback   (gpointer                data);
static void                   fm_icon_view_select_all                           (FMDirectoryView        *view);
static void                   fm_icon_view_set_selection                        (FMDirectoryView        *view,
										 GList                  *selection);
static void                   fm_icon_view_set_zoom_level                       (FMIconView             *view,
										 NautilusZoomLevel       new_level);
static void                   fm_icon_view_update_menus                         (FMDirectoryView        *view);
static NautilusIconContainer *get_icon_container                                (FMIconView             *icon_view);
static void                   icon_container_activate_callback                  (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 FMIconView             *icon_view);
static void                   icon_container_selection_changed_callback         (NautilusIconContainer  *container,
										 FMIconView             *icon_view);
static void                   icon_container_context_click_selection_callback   (NautilusIconContainer  *container,
										 FMIconView             *icon_view);
static void                   icon_container_context_click_background_callback  (NautilusIconContainer  *container,
										 FMIconView             *icon_view);
static NautilusScalableIcon * get_icon_images_callback                          (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 GList                 **emblem_icons,
										 const char             *modifier,
										 FMIconView             *icon_view);
static char *                 get_icon_uri_callback                             (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 FMIconView             *icon_view);
static char *                 get_icon_text_callback                            (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 FMIconView             *icon_view);
static char *                 get_icon_property_callback                        (NautilusIconContainer  *container,
										 NautilusFile           *icon_data,
										 const char             *property_name,
										 FMIconView             *icon_view);
static void                   text_attribute_names_changed_callback             (NautilusPreferences    *preferences,
										 const char             *name,
										 gconstpointer           value,
										 gpointer                user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMIconView, fm_icon_view, FM_TYPE_DIRECTORY_VIEW);

struct FMIconViewDetails
{
	GList *icons_not_positioned;
	NautilusZoomLevel default_zoom_level;
	char *text_attribute_names;

	guint react_to_icon_change_idle_id;
};

/* GtkObject methods.  */

static void
fm_icon_view_initialize_class (FMIconViewClass *klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

	object_class->destroy = fm_icon_view_destroy;
	
	fm_directory_view_class->add_file = fm_icon_view_add_file;
	fm_directory_view_class->begin_loading = fm_icon_view_begin_loading;
	fm_directory_view_class->bump_zoom_level = fm_icon_view_bump_zoom_level;	
	fm_directory_view_class->can_zoom_in = fm_icon_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_icon_view_can_zoom_out;
	fm_directory_view_class->clear = fm_icon_view_clear;
	fm_directory_view_class->done_adding_files = fm_icon_view_done_adding_files;	
	fm_directory_view_class->file_changed = fm_icon_view_file_changed;
	fm_directory_view_class->get_selection = fm_icon_view_get_selection;
	fm_directory_view_class->select_all = fm_icon_view_select_all;
	fm_directory_view_class->set_selection = fm_icon_view_set_selection;
        fm_directory_view_class->append_background_context_menu_items = fm_icon_view_append_background_context_menu_items;
        fm_directory_view_class->append_selection_context_menu_items = fm_icon_view_append_selection_context_menu_items;
        fm_directory_view_class->merge_menus = fm_icon_view_merge_menus;
        fm_directory_view_class->update_menus = fm_icon_view_update_menus;

	/* Set the text attribute preferences info.  We do this so that we can later get a sane default value.
	 *
	 * We might want to change things such that no default has to be installed
	 * for this preference.  If so, then the code that fetches the preference
	 * would have to deal with either a NULL return value (or "").
	 */
	nautilus_preferences_set_info (nautilus_preferences_get_global_preferences (),
				       NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES,
				       NULL,
				       NAUTILUS_PREFERENCE_STRING,
				       "name|size|date_modified|type",
				       NULL);
}

static void
fm_icon_view_initialize (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;
        
        g_return_if_fail (GTK_BIN (icon_view)->child == NULL);

	icon_view->details = g_new0 (FMIconViewDetails, 1);
	icon_view->details->default_zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
	icon_view->details->text_attribute_names
		= nautilus_preferences_get_string (nautilus_preferences_get_global_preferences (),
					       	   NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES);	

	nautilus_preferences_add_string_callback (nautilus_preferences_get_global_preferences (),
						  NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES,
						  text_attribute_names_changed_callback,
						  icon_view);	
	
	icon_container = create_icon_container (icon_view);
}

static void
fm_icon_view_destroy (GtkObject *object)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (object);

	nautilus_preferences_remove_callback (nautilus_preferences_get_global_preferences (),
					      NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES,
					      text_attribute_names_changed_callback,
					      icon_view);

        if (icon_view->details->react_to_icon_change_idle_id != 0) {
                gtk_idle_remove (icon_view->details->react_to_icon_change_idle_id);
        }

	g_free (icon_view->details->text_attribute_names);
	nautilus_file_list_free (icon_view->details->icons_not_positioned);
	g_free (icon_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static NautilusIconContainer *
create_icon_container (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;
	FMDirectoryView *directory_view;

	icon_container = NAUTILUS_ICON_CONTAINER (nautilus_icon_container_new ());
	directory_view = FM_DIRECTORY_VIEW (icon_view);

	GTK_WIDGET_SET_FLAGS (icon_container, GTK_CAN_FOCUS);
	
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "activate",
			    GTK_SIGNAL_FUNC (icon_container_activate_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "context_click_selection",
			    GTK_SIGNAL_FUNC (icon_container_context_click_selection_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "context_click_background",
			    GTK_SIGNAL_FUNC (icon_container_context_click_background_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "icon_changed",
			    GTK_SIGNAL_FUNC (fm_icon_view_icon_changed_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (icon_container_selection_changed_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_icon_images",
			    GTK_SIGNAL_FUNC (get_icon_images_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_icon_uri",
			    GTK_SIGNAL_FUNC (get_icon_uri_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_icon_text",
			    GTK_SIGNAL_FUNC (get_icon_text_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_icon_property",
			    GTK_SIGNAL_FUNC (get_icon_property_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "move_copy_items",
			    GTK_SIGNAL_FUNC (fm_directory_view_move_copy_items),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_container_uri",
			    GTK_SIGNAL_FUNC (fm_directory_view_get_container_uri),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "can_accept_item",
			    GTK_SIGNAL_FUNC (fm_directory_view_can_accept_item),
			    directory_view);

	gtk_container_add (GTK_CONTAINER (icon_view), GTK_WIDGET (icon_container));

	gtk_widget_show (GTK_WIDGET (icon_container));

	return icon_container;
}

static NautilusIconContainer *
get_icon_container (FMIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
add_icon_if_already_positioned (FMIconView *icon_view,
				NautilusFile *file)
{
	NautilusDirectory *directory;
	char *position_string, *scale_string;
	gboolean position_good, scale_good;
	int x, y;
	double scale_x, scale_y;

	/* Get the current position of this icon from the metadata. */
	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view));
	position_string = nautilus_file_get_metadata (file, 
						      NAUTILUS_METADATA_KEY_ICON_POSITION, 
						      "");
	position_good = sscanf (position_string, " %d , %d %*s", &x, &y) == 2;
	g_free (position_string);

	/* Get the scale of the icon from the metadata. */
	scale_string = nautilus_file_get_metadata (file,
						   NAUTILUS_METADATA_KEY_ICON_SCALE,
						   "1");
	scale_good = sscanf (scale_string, " %lf %*s", &scale_x) == 1;
	if (scale_good) {
		scale_y = scale_x;
	} else {
		scale_good = sscanf (scale_string, " %lf %lf %*s", &scale_x, &scale_y) == 2;
		if (!scale_good) {
			scale_x = 1.0;
			scale_y = 1.0;
		}
	}
	g_free (scale_string);

	if (!position_good) {
		nautilus_file_ref (file);
		icon_view->details->icons_not_positioned =
			g_list_prepend (icon_view->details->icons_not_positioned, file);
		return;
	}

	nautilus_file_ref (file);
	nautilus_icon_container_add (get_icon_container (icon_view),
				     NAUTILUS_ICON_CONTAINER_ICON_DATA (file),
				     x, y, scale_x, scale_y);
}

static void
add_icon_at_free_position (FMIconView *icon_view,
			   NautilusFile *file)
{
	nautilus_file_ref (file);
	nautilus_icon_container_add_auto (get_icon_container (icon_view),
					  NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
show_stretch_handles_callback (gpointer ignored, gpointer view)
{
	g_assert (FM_IS_ICON_VIEW (view));

	nautilus_icon_container_show_stretch_handles
		(get_icon_container (FM_ICON_VIEW (view)));

        /* Update menus because Restore item may have changed state */
	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
unstretch_icons_callback (gpointer ignored, gpointer view)
{
	g_assert (FM_IS_ICON_VIEW (view));

	nautilus_icon_container_unstretch
		(get_icon_container (FM_ICON_VIEW (view)));

        /* Update menus because Stretch item may have changed state */
	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
fm_icon_view_compute_menu_item_info (FMIconView *view, 
				     GList *files, 
				     const char *menu_path,
				     gboolean include_accelerator_underbars,
				     char **return_name,
				     gboolean *sensitive_return)
{
	NautilusIconContainer *icon_container;
	char *name, *stripped;

	g_assert (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (view);

	if (strcmp (MENU_PATH_STRETCH_ICON, menu_path) == 0) {
                name = g_strdup (_("_Stretch Icon"));
		/* Current stretching UI only works on one item at a time, so we'll
		 * desensitize the menu item if that's not the case.
		 */
        	*sensitive_return = g_list_length (files) == 1
        	                    && !nautilus_icon_container_has_stretch_handles (icon_container);
	} else if (strcmp (MENU_PATH_RESTORE_STRETCHED_ICONS, menu_path) == 0) {
                if (g_list_length (files) > 1) {
                        name = g_strdup (_("_Restore Icons to Unstretched Size"));
                } else {
                        name = g_strdup (_("_Restore Icon to Unstretched Size"));
                }

        	*sensitive_return = nautilus_icon_container_is_stretched (icon_container);

	} else if (strcmp (MENU_PATH_CUSTOMIZE_ICON_TEXT, menu_path) == 0) {
                name = g_strdup (_("Customize _Icon Text..."));
        	*sensitive_return = TRUE;
	} else {
                g_assert_not_reached ();
	}

	if (!include_accelerator_underbars) {
                stripped = nautilus_str_strip_chr (name, '_');
		g_free (name);
		name = stripped;
        }

	*return_name = name;
}           

static void
append_one_context_menu_item (FMIconView *view,
                              GtkMenu *menu,
                              GList *files,
                              const char *menu_path,
                              GtkSignalFunc callback)
{
	GtkWidget *menu_item;
	char *label;
	gboolean sensitive;
        
        fm_icon_view_compute_menu_item_info (view, 
                                                        files, 
                                                        menu_path, 
                                                        FALSE, 
                                                        &label, 
                                                        &sensitive); 
        menu_item = gtk_menu_item_new_with_label (label);
        g_free (label);
        gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate", callback, view);
	gtk_menu_append (menu, menu_item);
}

static void
fm_icon_view_append_selection_context_menu_items (FMDirectoryView *view,
							     GtkMenu *menu,
							     GList *files)
{
	g_assert (FM_IS_ICON_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, 
				    append_selection_context_menu_items, 
				    (view, menu, files));

        append_one_context_menu_item (FM_ICON_VIEW (view), menu, files, 
                                      MENU_PATH_STRETCH_ICON, 
                                      GTK_SIGNAL_FUNC (show_stretch_handles_callback));
        append_one_context_menu_item (FM_ICON_VIEW (view), menu, files, 
                                      MENU_PATH_RESTORE_STRETCHED_ICONS, 
                                      GTK_SIGNAL_FUNC (unstretch_icons_callback));
}

/* Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
customize_icon_text_callback (gpointer ignored1, gpointer ignored2)
{
	nautilus_gtk_window_present (fm_icon_text_window_get_or_create ());
}

static void
fm_icon_view_append_background_context_menu_items (FMDirectoryView *view,
						   GtkMenu *menu)
{
	GtkWidget *menu_item;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, 
				    append_background_context_menu_items, 
				    (view, menu));

	/* Put a separator before this item, since previous items are
	 * window-specific and this one is global.
	 */
	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

        append_one_context_menu_item (FM_ICON_VIEW (view), menu, NULL, 
                                      MENU_PATH_CUSTOMIZE_ICON_TEXT, 
                                      GTK_SIGNAL_FUNC (customize_icon_text_callback));
}

static void
display_icons_not_already_positioned (FMIconView *view)
{
	GList *p;

	/* FIXME: This will block if there are many files.  */
	for (p = view->details->icons_not_positioned; p != NULL; p = p->next) {
		add_icon_at_free_position (view, p->data);
	}

	nautilus_file_list_free (view->details->icons_not_positioned);
	view->details->icons_not_positioned = NULL;
}

static void
fm_icon_view_clear (FMDirectoryView *view)
{
	NautilusIconContainer *icon_container;
	
	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));

	/* Clear away the existing icons. */
	nautilus_icon_container_clear (icon_container);
}

static void
fm_icon_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	add_icon_if_already_positioned (FM_ICON_VIEW (view), file);
}

static void
fm_icon_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	/* This handles both changes to an existing file and the existing file going away. */
	/* FIXME: We really want to ask if it is gone from this directory,
	 * not if it has been deleted.
	 */
	if (nautilus_file_is_gone (file)) {
		if (nautilus_icon_container_remove (get_icon_container (FM_ICON_VIEW (view)),
						    NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
			nautilus_file_unref (file);
		}
	} else {
		nautilus_icon_container_request_update (get_icon_container (FM_ICON_VIEW (view)),
							NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
	}
}

static void
fm_icon_view_done_adding_files (FMDirectoryView *view)
{
	display_icons_not_already_positioned (FM_ICON_VIEW (view));
}

static void
fm_icon_view_begin_loading (FMDirectoryView *view)
{
	FMIconView *icon_view;
	NautilusDirectory *directory;
	int level;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	directory = fm_directory_view_get_model (view);

	nautilus_connect_background_to_directory_metadata (GTK_WIDGET (get_icon_container (icon_view)),
							   directory);

	/* Set up the zoom level from the metadata. */
	level = nautilus_directory_get_integer_metadata (directory, 
							 NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
							 icon_view->details->default_zoom_level);
	fm_icon_view_set_zoom_level (icon_view, level);

}

static NautilusZoomLevel
fm_icon_view_get_zoom_level (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);
	return nautilus_icon_container_get_zoom_level (get_icon_container (view));
}

static void
fm_icon_view_set_zoom_level (FMIconView *view,
			     NautilusZoomLevel new_level)
{
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	icon_container = get_icon_container (view);
	if (nautilus_icon_container_get_zoom_level (icon_container) == new_level) {
		return;
	}

	nautilus_directory_set_integer_metadata
		(fm_directory_view_get_model (FM_DIRECTORY_VIEW (view)), 
		 NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
		 view->details->default_zoom_level,
		 new_level);

	nautilus_icon_container_set_zoom_level (icon_container, new_level);

	/* Reset default to new level; this way any change in zoom level
	 * will "stick" until the user visits a directory that had its zoom
	 * level set explicitly earlier.
	 */
	view->details->default_zoom_level = new_level;
}


static void
fm_icon_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
	FMIconView *icon_view;
	NautilusZoomLevel new_level;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	new_level = fm_icon_view_get_zoom_level (icon_view) + zoom_increment;
	fm_icon_view_set_zoom_level(icon_view, new_level);
}

static gboolean 
fm_icon_view_can_zoom_in (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return fm_icon_view_get_zoom_level (FM_ICON_VIEW (view)) 
		< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
fm_icon_view_can_zoom_out (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return fm_icon_view_get_zoom_level (FM_ICON_VIEW (view)) 
		> NAUTILUS_ZOOM_LEVEL_SMALLEST;
}

/**
 * fm_icon_view_get_icon_text_attribute_names:
 *
 * Get a string representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Use g_free to free the result.
 * @view: FMIconView to query.
 * 
 * Return value: A |-delimited string comprising attribute names, e.g. "name|size".
 * 
 **/
static char *
fm_icon_view_get_icon_text_attribute_names (FMIconView *view)
{
	char * all_names;
	char * result;
	char * c;
	int pieces_so_far;
	int piece_count;
	const int pieces_by_level[] = {
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALLEST */
		1,	/* NAUTILUS_ZOOM_LEVEL_SMALLER */
		1,	/* NAUTILUS_ZOOM_LEVEL_SMALL */
		2,	/* NAUTILUS_ZOOM_LEVEL_STANDARD */
		3,	/* NAUTILUS_ZOOM_LEVEL_LARGE */
		3,	/* NAUTILUS_ZOOM_LEVEL_LARGER */
		4	/* NAUTILUS_ZOOM_LEVEL_LARGEST */
	};

	piece_count = pieces_by_level[fm_icon_view_get_zoom_level (view)];

	all_names = view->details->text_attribute_names;
	pieces_so_far = 0;

	for (c = all_names; *c != '\0'; ++c)
	{
		if (pieces_so_far == piece_count) {
			break;
		}

		if (*c == '|') {
			++pieces_so_far;
		}
	}

	/* Return an initial substring of the full set */
	result = g_strndup (all_names, (c - all_names));
	
	return result;
}

static GList *
fm_icon_view_get_selection (FMDirectoryView *view)
{
	GList *list;

	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

	list = nautilus_icon_container_get_selection
		(get_icon_container (FM_ICON_VIEW (view)));
	nautilus_file_list_ref (list);
	return list;
}

static void
append_bonobo_menu_item (FMIconView *view, 
                         BonoboUIHandler *ui_handler,
                         GList *files,
                         const char *path,
                         const char *hint,
                         BonoboUIHandlerCallbackFunc callback,
                         gpointer callback_data)
{
        char *label;
        gboolean sensitive;
        
        fm_icon_view_compute_menu_item_info (view, files, path, TRUE, &label, &sensitive);
        bonobo_ui_handler_menu_new_item (ui_handler, path, label, hint, 
                                         -1,                            /* Position, -1 means at end */
                                         BONOBO_UI_HANDLER_PIXMAP_NONE, /* Pixmap type */
                                         NULL,                          /* Pixmap data */
                                         0,                             /* Accelerator key */
                                         0,                             /* Modifiers for accelerator */
                                         callback, callback_data);
        g_free (label);
        bonobo_ui_handler_menu_set_sensitivity (ui_handler, path, sensitive);
}

static void
fm_icon_view_merge_menus (FMDirectoryView *view)
{
        GList *selection;
        BonoboUIHandler *ui_handler;

        g_assert (FM_IS_ICON_VIEW (view));

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

        selection = fm_directory_view_get_selection (view);
        ui_handler = fm_directory_view_get_bonobo_ui_handler (view);

        bonobo_ui_handler_menu_new_separator (ui_handler,
                                              MENU_PATH_BEFORE_STRETCH_SEPARATOR,
                                              -1); 

        append_bonobo_menu_item (FM_ICON_VIEW (view), ui_handler, selection,
                                 MENU_PATH_STRETCH_ICON,
                                 _("Make the selected icon stretchable"),
                                 (BonoboUIHandlerCallbackFunc) show_stretch_handles_callback, view);
        append_bonobo_menu_item (FM_ICON_VIEW (view), ui_handler, selection,
                                 MENU_PATH_RESTORE_STRETCHED_ICONS,
                                 _("Restore each selected icon to its original size"),
                                 (BonoboUIHandlerCallbackFunc) unstretch_icons_callback, view);

        bonobo_ui_handler_menu_new_separator (ui_handler,
                                              MENU_PATH_AFTER_STRETCH_SEPARATOR,
                                              -1); 

        append_bonobo_menu_item (FM_ICON_VIEW (view), ui_handler, selection,
                                 MENU_PATH_CUSTOMIZE_ICON_TEXT,
                                 _("Choose which information appears beneath each icon's name"),
                                 (BonoboUIHandlerCallbackFunc) customize_icon_text_callback, view);

        nautilus_file_list_free (selection);
}

static void
update_bonobo_menu_item (FMIconView *view, 
                         BonoboUIHandler *ui_handler,
                         GList *files,
                         const char *menu_path)
{
	char *label;
	gboolean sensitive;

        fm_icon_view_compute_menu_item_info (view, files, menu_path, TRUE, &label, &sensitive);
        bonobo_ui_handler_menu_set_sensitivity (ui_handler, menu_path, sensitive);
        bonobo_ui_handler_menu_set_label (ui_handler, menu_path, label);
        g_free (label);
}

static void
fm_icon_view_update_menus (FMDirectoryView *view)
{
        GList *selection;
        BonoboUIHandler *ui_handler;

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));

        ui_handler = fm_directory_view_get_bonobo_ui_handler (view);
        selection = fm_directory_view_get_selection (view);
        
        update_bonobo_menu_item (FM_ICON_VIEW (view), ui_handler, selection, 
                                 MENU_PATH_STRETCH_ICON);
        update_bonobo_menu_item (FM_ICON_VIEW (view), ui_handler, selection, 
                                 MENU_PATH_RESTORE_STRETCHED_ICONS);
        nautilus_file_list_free (selection);
}

static void
fm_icon_view_select_all (FMDirectoryView *view)
{
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));
        nautilus_icon_container_select_all(icon_container);
}

static void
fm_icon_view_set_selection (FMDirectoryView *view, GList *selection)
{
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));
	nautilus_icon_container_set_selection (icon_container, selection);
}

static void
icon_container_activate_callback (NautilusIconContainer *container,
				  NautilusFile *file,
				  FMIconView *icon_view)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (file != NULL);

	fm_directory_view_activate_file (FM_DIRECTORY_VIEW (icon_view), file, FALSE);
}

static void
icon_container_selection_changed_callback (NautilusIconContainer *container,
					   FMIconView *icon_view)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (icon_view));
}

static void
icon_container_context_click_selection_callback (NautilusIconContainer *container,
						 FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	fm_directory_view_pop_up_selection_context_menu (FM_DIRECTORY_VIEW (icon_view));
}

static void
icon_container_context_click_background_callback (NautilusIconContainer *container,
						  FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	fm_directory_view_pop_up_background_context_menu (FM_DIRECTORY_VIEW (icon_view));
}

static gboolean
fm_icon_view_react_to_icon_change_idle_callback (gpointer data) 
{        
        FMIconView *icon_view;
        
        g_assert (FM_IS_ICON_VIEW (data));
        
        icon_view = FM_ICON_VIEW (data);
        icon_view->details->react_to_icon_change_idle_id = 0;
        
	/* Rebuild the menus since some of them (e.g. Restore Stretched Icons)
	 * may be different now.
	 */
	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (icon_view));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
fm_icon_view_icon_changed_callback (NautilusIconContainer *container,
				    NautilusFile *file,
				    int x, int y, double scale_x, double scale_y,
				    FMIconView *icon_view)
{
	NautilusDirectory *directory;
	char *position_string;
	char *scale_string;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (file != NULL);

	/* Schedule updating menus for the next idle. Doing it directly here
	 * noticeably slows down icon stretching.  The other work here to
	 * store the icon position and scale does not seem to noticeably
	 * slow down icon stretching. It would be trickier to move to an
	 * idle call, because we'd have to keep track of potentially multiple
	 * sets of file/geometry info.
	 */
	if (icon_view->details->react_to_icon_change_idle_id == 0) {
                icon_view->details->react_to_icon_change_idle_id
                        = gtk_idle_add (fm_icon_view_react_to_icon_change_idle_callback,
                                        icon_view);
	}

	/* Store the new position of the icon in the metadata.
	 * FIXME: Is a comma acceptable in locales where it is the decimal separator?
	 */
	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (icon_view));
	position_string = g_strdup_printf ("%d,%d", x, y);
	nautilus_file_set_metadata (file, 
				    NAUTILUS_METADATA_KEY_ICON_POSITION, 
				    NULL, 
				    position_string);

	/* FIXME: %.2f is not a good format for the scale factor. We'd like it to
	 * say "2" or "2x" instead of "2.00".
	 * FIXME: scale_x == scale_y is too strict a test. It would be better to
	 * check if the string representations match instead of the FP values.
	 * FIXME: Is a comma acceptable in locales where it is the decimal separator?
	 */
	if (scale_x == scale_y) {
		scale_string = g_strdup_printf ("%.2f", scale_x);
	} else {
		scale_string = g_strdup_printf ("%.2f,%.2f", scale_x, scale_y);
	}
	nautilus_file_set_metadata (file,
				    NAUTILUS_METADATA_KEY_ICON_SCALE,
				    "1.00",
				    scale_string);

	g_free (position_string);
	g_free (scale_string);
}

static NautilusScalableIcon *
get_icon_images_callback (NautilusIconContainer *container,
			  NautilusFile *file,
			  GList **emblem_icons,
			  const char *modifier,
			  FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (emblem_icons != NULL);
	g_assert (FM_IS_ICON_VIEW (icon_view));

	/* Get the appropriate images for the file. */
	if (emblem_icons != NULL) {
		*emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file);
	}
	return nautilus_icon_factory_get_icon_for_file (file, modifier);
}

static char *
get_icon_uri_callback (NautilusIconContainer *container,
		       NautilusFile *file,
		       FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	return nautilus_file_get_uri (file);
}

static char *
get_icon_text_callback (NautilusIconContainer *container,
			NautilusFile *file,
			FMIconView *icon_view)
{
	char *attribute_names;
	char **text_array;
	char *result;
	int i;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	attribute_names = fm_icon_view_get_icon_text_attribute_names
		(icon_view);
	text_array = g_strsplit (attribute_names, "|", 0);
	g_free (attribute_names);

	for (i = 0; text_array[i] != NULL; i++)	{
		char *attribute_string;

		attribute_string = nautilus_file_get_string_attribute
			(file, text_array[i]);
		
		/* Unknown attributes get turned into blank lines (also note that
		 * leaving a NULL in text_array would cause it to be incompletely
		 * freed).
		 */
		if (attribute_string == NULL) {
			attribute_string = g_strdup ("");
		}

		/* Replace each attribute name in the array with its string value */
		g_free (text_array[i]);
		text_array[i] = attribute_string;
	}

	result = g_strjoinv ("\n", text_array);

	g_strfreev (text_array);

	return result;
}

static char *
get_icon_property_callback (NautilusIconContainer *container,
			    NautilusFile *file,
			    const char *property_name,
			    FMIconView *icon_view)
{
	const char *mime_type;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (property_name != NULL);
	g_assert (FM_IS_ICON_VIEW (icon_view));
	
	if (strcmp (property_name, "contents_as_text") == 0) {
		mime_type = nautilus_file_get_mime_type (file);
		if (mime_type == NULL || nautilus_str_has_prefix (mime_type, "text/")) {
			return nautilus_file_get_uri (file);
		}
	}
	
	/* nothing applied, so return nothing */
	return NULL;		
}

static void
text_attribute_names_changed_callback (NautilusPreferences *preferences,
         			       const char *name,
         			       gconstpointer value,
         			       gpointer user_data)

{
	FMIconView *icon_view;

	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (strcmp (name, NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES) == 0);
	g_assert (value != NULL);
	g_assert (FM_IS_ICON_VIEW (user_data));

	icon_view = FM_ICON_VIEW (user_data);

	g_free (icon_view->details->text_attribute_names);
	icon_view->details->text_attribute_names = g_strdup ((char *) value);

	nautilus_icon_container_request_update_all (get_icon_container (icon_view));	
}
