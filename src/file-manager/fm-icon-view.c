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

#include "fm-desktop-icon-view.h"
#include "fm-error-reporting.h"
#include "fm-icon-text-window.h"
#include <bonobo/bonobo-ui-util.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-container.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-sound.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Paths to use when creating & referring to Bonobo menu items */
#define MENU_PATH_RENAME 			"/menu/File/File Items Placeholder/Rename"
#define MENU_PATH_CUSTOMIZE_ICON_TEXT 		"/menu/Edit/Global Edit Items Placeholder/Icon Text"
#define MENU_PATH_STRETCH_ICON 			"/menu/Edit/Edit Items Placeholder/Stretch"
#define MENU_PATH_UNSTRETCH_ICONS 		"/menu/Edit/Edit Items Placeholder/Unstretch"
#define MENU_PATH_LAY_OUT			"/menu/View/View Items Placeholder/Lay Out"
#define MENU_PATH_MANUAL_LAYOUT 		"/menu/View/View Items Placeholder/Lay Out/Manual Layout"
#define MENU_PATH_TIGHTER_LAYOUT 		"/menu/View/View Items Placeholder/Lay Out/Tighter Layout"
#define MENU_PATH_SORT_REVERSED			"/menu/View/View Items Placeholder/Lay Out/Reversed Order"
#define MENU_PATH_CLEAN_UP			"/menu/View/View Items Placeholder/Clean Up"

#define COMMAND_PREFIX                          "/commands/"
#define COMMAND_RENAME 				"/commands/Rename"
#define COMMAND_STRETCH_ICON 			"/commands/Stretch"
#define COMMAND_UNSTRETCH_ICONS 		"/commands/Unstretch"
#define COMMAND_TIGHTER_LAYOUT 			"/commands/Tighter Layout"
#define COMMAND_SORT_REVERSED			"/commands/Reversed Order"
#define COMMAND_CLEAN_UP			"/commands/Clean Up"

#define ID_MANUAL_LAYOUT                        "Manual Layout"
#define ID_TIGHTER_LAYOUT                       "Tighter Layout"
#define ID_SORT_REVERSED                        "Reversed Order"


/* forward declarations */
static void     create_icon_container                     (FMIconView        *icon_view);
static void     fm_icon_view_initialize                   (FMIconView        *icon_view);
static void     fm_icon_view_initialize_class             (FMIconViewClass   *klass);
static void     fm_icon_view_set_directory_sort_by        (FMIconView        *icon_view,
							   NautilusFile      *file,
							   const char        *sort_by);
static void     fm_icon_view_set_zoom_level               (FMIconView        *view,
							   NautilusZoomLevel  new_level,
							   gboolean           always_set_level);
gboolean        fm_icon_view_supports_auto_layout         (FMIconView        *view);
static void     fm_icon_view_update_icon_container_fonts  (FMIconView        *icon_view);
static void     fm_icon_view_update_click_mode            (FMIconView        *icon_view);
static void     fm_icon_view_update_smooth_graphics_mode  (FMIconView        *icon_view);
static gboolean fm_icon_view_get_directory_tighter_layout (FMIconView        *icon_view,
							   NautilusFile      *file);
static void     fm_icon_view_set_directory_tighter_layout (FMIconView        *icon_view,
							   NautilusFile      *file,
							   gboolean           tighter_layout);
static gboolean real_supports_auto_layout                 (FMIconView        *view);
static void     set_sort_criterion_by_id                  (FMIconView        *icon_view,
							   const char        *id);
static gboolean set_sort_reversed                         (FMIconView        *icon_view,
							   gboolean           new_value);
static void     switch_to_manual_layout                   (FMIconView        *view);
static void     preview_sound                             (NautilusFile      *file,
							   gboolean           start_flag);
static void     update_layout_menus                       (FMIconView        *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMIconView,
				   fm_icon_view,
				   FM_TYPE_DIRECTORY_VIEW)

typedef struct {
	NautilusFileSortType sort_type;
	const char *metadata_text;
	const char *id;
	const char *menu_label;
	const char *menu_hint;
} SortCriterion;

typedef enum {
	MENU_ITEM_TYPE_STANDARD,
	MENU_ITEM_TYPE_CHECK,
	MENU_ITEM_TYPE_RADIO,
	MENU_ITEM_TYPE_TREE
} MenuItemType;

/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] = {
	{
		NAUTILUS_FILE_SORT_BY_NAME,
		"name",
		"Sort by Name",
		N_("by _Name"),
		N_("Keep icons sorted by name in rows")
	},
	{
		NAUTILUS_FILE_SORT_BY_SIZE,
		"size",
		"Sort by Size",
		N_("by _Size"),
		N_("Keep icons sorted by size in rows")
	},
	{
		NAUTILUS_FILE_SORT_BY_TYPE,
		"type",
		"Sort by Type",
		N_("by _Type"),
		N_("Keep icons sorted by type in rows")
	},
	{
		NAUTILUS_FILE_SORT_BY_MTIME,
		"modification date",
		"Sort by Modification Date",
		N_("by Modification _Date"),
		N_("Keep icons sorted by modification date in rows")
	},
	{
		NAUTILUS_FILE_SORT_BY_EMBLEMS,
		"emblems",
		"Sort by Emblems",
		N_("by _Emblems"),
		N_("Keep icons sorted by emblems in rows")
	}
};

/* some state variables used for sound previewing */

static int timeout = -1;

struct FMIconViewDetails
{
	GList *icons_not_positioned;
	NautilusZoomLevel default_zoom_level;

	guint react_to_icon_change_idle_id;
	gboolean menus_ready;

	const SortCriterion *sort;
	gboolean sort_reversed;

	/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
	gboolean updating_toggle_menu_item;

	BonoboUIComponent *ui;
};

static void
fm_icon_view_destroy (GtkObject *object)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (object);

	/* don't try to update menus during the destroy process */
	icon_view->details->menus_ready = FALSE;

	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui);
		bonobo_object_unref (BONOBO_OBJECT (icon_view->details->ui));
	}

        if (icon_view->details->react_to_icon_change_idle_id != 0) {
                gtk_idle_remove (icon_view->details->react_to_icon_change_idle_id);
        }

	/* kill any sound preview process that is ongoing */
	preview_sound (NULL, FALSE);

	nautilus_file_list_free (icon_view->details->icons_not_positioned);
	g_free (icon_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static NautilusIconContainer *
get_icon_container (FMIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static gboolean
get_stored_icon_position_callback (NautilusIconContainer *container,
				   NautilusFile *file,
				   NautilusIconPosition *position,
				   FMIconView *icon_view)
{
	char *position_string, *scale_string;
	gboolean position_good, scale_good;
	char *locale;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (position != NULL);
	g_assert (FM_IS_ICON_VIEW (icon_view));

	/* Doing parsing in the "C" locale instead of the one set
	 * by the user ensures that data in the metafile is not in
	 * a locale-specific format. It's only necessary for floating
	 * point values since there aren't locale-specific formats for
	 * integers in C stdio.
	 */
	locale = setlocale (LC_NUMERIC, "C");

	/* Get the current position of this icon from the metadata. */
	position_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_POSITION, "");
	position_good = sscanf
		(position_string, " %d , %d %*s",
		 &position->x, &position->y) == 2;
	g_free (position_string);

	/* Get the scale of the icon from the metadata. */
	scale_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_SCALE, "1");
	scale_good = sscanf
		(scale_string, " %lf %*s",
		 &position->scale_x) == 1;
	if (scale_good) {
		position->scale_y = position->scale_x;
	} else {
		scale_good = sscanf
			(scale_string, " %lf %lf %*s",
			 &position->scale_x,
			 &position->scale_y) == 2;
		if (!scale_good) {
			position->scale_x = 1.0;
			position->scale_y = 1.0;
		}
	}
	g_free (scale_string);

	setlocale (LC_NUMERIC, locale);

	return position_good;
}

static gboolean
set_sort_criterion (FMIconView *icon_view, const SortCriterion *sort)
{
	if (sort == NULL) {
		return FALSE;
	}
	if (icon_view->details->sort == sort) {
		return FALSE;
	}
	icon_view->details->sort = sort;

	/* Store the new sort setting. */
	fm_icon_view_set_directory_sort_by (icon_view,
					    fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view)),
					    sort->metadata_text);
	
	/* Update the layout menus to match the new sort setting. */
	update_layout_menus (icon_view);

	return TRUE;
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

        /* Update menus because Stretch and Unstretch items have changed state */
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

        /* Update menus because Stretch and Unstretch items have changed state */
	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
fm_icon_view_clean_up (FMIconView *icon_view)
{
	NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view, clean_up, (icon_view));
}

static void
fm_icon_view_real_clean_up (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;
	gboolean saved_sort_reversed;

	icon_container = get_icon_container (icon_view);

	/* Hardwire Clean Up to always be by name, in forward order */
	saved_sort_reversed = icon_view->details->sort_reversed;
	
	set_sort_reversed (icon_view, FALSE);
	set_sort_criterion (icon_view, &sort_criteria[0]);
	
	nautilus_icon_container_sort (icon_container);
	nautilus_icon_container_freeze_icon_positions (icon_container);

	set_sort_reversed (icon_view, saved_sort_reversed);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
clean_up_callback (gpointer ignored, gpointer view)
{
	fm_icon_view_clean_up (FM_ICON_VIEW (view));
}

	

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
rename_icon_callback (gpointer ignored, gpointer view)
{
	g_assert (FM_IS_ICON_VIEW (view));
  		
	nautilus_icon_container_start_renaming_selected_item
		(get_icon_container (FM_ICON_VIEW (view)));

	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
tighter_layout_state_changed_callback (BonoboUIComponent   *component,
				       const char          *path,
				       Bonobo_UIComponent_EventType type,
				       const char          *state,
				       gpointer            user_data)
{
	NautilusIconContainer *icon_container;
	NautilusFile *file;
	gboolean is_tighter_layout;
	FMIconView *view;

	g_assert (strcmp (path, ID_TIGHTER_LAYOUT) == 0);

	view = FM_ICON_VIEW (user_data);

	/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
	if (FM_ICON_VIEW (view)->details->updating_toggle_menu_item)
		return;


	icon_container = get_icon_container (FM_ICON_VIEW (view));
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));
	is_tighter_layout = fm_icon_view_get_directory_tighter_layout (FM_ICON_VIEW (view), file);
	
	fm_icon_view_set_directory_tighter_layout (FM_ICON_VIEW (view), file, !is_tighter_layout);
	nautilus_icon_container_set_tighter_layout (icon_container, !is_tighter_layout);
}


static gboolean
fm_icon_view_using_auto_layout (FMIconView *icon_view)
{
	return nautilus_icon_container_is_auto_layout 
		(get_icon_container (icon_view));
}

static gboolean
fm_icon_view_using_tighter_layout (FMIconView *icon_view)
{
	return nautilus_icon_container_is_tighter_layout 
		(get_icon_container (icon_view));
}

static void
compute_menu_item_info (FMIconView *view, 
			GList *selection, 
			const char *menu_path,
			gboolean include_accelerator_underbars,
			char **return_name,
			gboolean *sensitive_return,
			MenuItemType *menu_item_type_return)
{
	NautilusIconContainer *icon_container;
	char *name, *stripped;
	gboolean sensitive;
	MenuItemType type;

	g_assert (FM_IS_ICON_VIEW (view));
	g_assert (menu_path != NULL);
	g_assert (return_name != NULL);

	icon_container = get_icon_container (view);
	sensitive = TRUE;
	type = MENU_ITEM_TYPE_STANDARD;
	
	if (strcmp (MENU_PATH_STRETCH_ICON, menu_path) == 0) {
                name = g_strdup (_("_Stretch Icon"));
		/* Current stretching UI only works on one item at a time, so we'll
		 * desensitize the menu item if that's not the case.
		 */
        	sensitive = nautilus_g_list_exactly_one_item (selection)
			&& !nautilus_icon_container_has_stretch_handles (icon_container);
	} else if (strcmp (MENU_PATH_UNSTRETCH_ICONS, menu_path) == 0) {
                if (nautilus_g_list_more_than_one_item (selection)) {
                        name = g_strdup (_("_Restore Icons' Original Sizes"));
                } else {
                        name = g_strdup (_("_Restore Icon's Original Size"));
                }
        	sensitive = nautilus_icon_container_is_stretched (icon_container);
	} else if (strcmp (MENU_PATH_CUSTOMIZE_ICON_TEXT, menu_path) == 0) {
                name = g_strdup (_("_Icon Captions..."));
	} else if (strcmp (MENU_PATH_RENAME, menu_path) == 0) {
		/* Modify file name. We only allow this on a single file selection. */
		name = g_strdup (_("_Rename"));
		sensitive = nautilus_g_list_exactly_one_item (selection)
			&& nautilus_file_can_rename (selection->data);
	} else if (strcmp (MENU_PATH_CLEAN_UP, menu_path) == 0) {
		name = g_strdup (_("_Clean Up by Name"));
		sensitive = !fm_icon_view_using_auto_layout (view);
	} else if (strcmp (MENU_PATH_TIGHTER_LAYOUT, menu_path) == 0) {
		name = g_strdup (_("_Use Tighter Layout"));
		sensitive = fm_icon_view_using_auto_layout (view);
		type = MENU_ITEM_TYPE_CHECK;
	} else if (strcmp (MENU_PATH_SORT_REVERSED, menu_path) == 0) {
		name = g_strdup (_("Re_versed Order"));
		sensitive = fm_icon_view_using_auto_layout (view);
		type = MENU_ITEM_TYPE_CHECK;
	} else if (strcmp (MENU_PATH_LAY_OUT, menu_path) == 0) {
		name = g_strdup (_("_Lay out items"));
		type = MENU_ITEM_TYPE_TREE;
	} else if (strcmp (MENU_PATH_MANUAL_LAYOUT, menu_path) == 0) {
		name = g_strdup (_("_manually"));
		type = MENU_ITEM_TYPE_RADIO;
	} else {
		name = NULL;
		g_message ("Unknown menu path\"%s\" in compute_menu_item_info", menu_path);
	}

	if (!include_accelerator_underbars) {
                stripped = nautilus_str_strip_chr (name, '_');
		g_free (name);
		name = stripped;
        }

        if (sensitive_return != NULL) {
		*sensitive_return = sensitive;
        }

        if (menu_item_type_return != NULL) {
		*menu_item_type_return = type;
        }

	*return_name = name;
}           

static void
handle_radio_item (FMIconView *view,
		   const char *id)
{
	/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
	if (view->details->updating_toggle_menu_item) {
		return;
	}

	if (strcmp (id, ID_MANUAL_LAYOUT) == 0) {
		switch_to_manual_layout (view);
	} else {
		set_sort_criterion_by_id (view, id);
	}
}

static void
context_menu_layout_radio_item_callback (GtkWidget *menu_item, gpointer user_data)
{
	FMIconView *icon_view;
	const char *id;

	icon_view = FM_ICON_VIEW (user_data);

	id = (const char *) gtk_object_get_data (GTK_OBJECT (menu_item), "id");
	g_assert (id != NULL);
	handle_radio_item (icon_view, id);
}

static GtkMenuItem *
append_one_context_menu_layout_item (FMIconView *view,
                              	     GtkMenu *menu,
                              	     GtkRadioMenuItem *item_in_group,
                              	     const char *id,
                              	     const char *menu_label)
{
	GtkWidget *menu_item;
	char *label_for_display;

	label_for_display = nautilus_str_strip_chr (menu_label, '_');
        menu_item = gtk_radio_menu_item_new_with_label (
        	item_in_group ? gtk_radio_menu_item_group (item_in_group) : NULL,
        	label_for_display);
        g_free (label_for_display);

	/* Match appearance of Bonobo menu items, where unchosen items are still marked. */
       	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_item), TRUE);

        /* Attach id to menu item so we can check it in the callback. */
        gtk_object_set_data_full (GTK_OBJECT (menu_item), "id",
				  g_strdup (id), g_free);
        
	gtk_widget_show (menu_item);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate", context_menu_layout_radio_item_callback, view);
	gtk_menu_append (menu, menu_item);

	return GTK_MENU_ITEM (menu_item);
}

static GtkMenuItem *
insert_one_context_menu_item (FMIconView *view,
                              GtkMenu *menu,
                              GList *selection,
                              const char *menu_path,
                              gint position,
                              GtkSignalFunc callback)
{
	GtkWidget *menu_item;
	char *label;
	gboolean sensitive;
	MenuItemType type;
        
        compute_menu_item_info (view, selection, menu_path, FALSE, &label, &sensitive, &type);

        switch (type) {
        case MENU_ITEM_TYPE_CHECK:
        	menu_item = gtk_check_menu_item_new_with_label (label);
        	/* Match appearance of Bonobo toggle items, where inactive
        	 * check items are still marked.
        	 */
        	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
        	break;

        case MENU_ITEM_TYPE_TREE:
        case MENU_ITEM_TYPE_STANDARD:
		menu_item = gtk_menu_item_new_with_label (label);
		break;

	case MENU_ITEM_TYPE_RADIO:	/* Not handled here due to required group parameter */
        default:
        	g_assert_not_reached ();
		menu_item = NULL;
        }

        g_free (label);
        gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate", callback, view);
	gtk_menu_insert (menu, menu_item, position);

	return GTK_MENU_ITEM (menu_item);
}

static GtkMenuItem *
append_one_context_menu_item (FMIconView *view,
                              GtkMenu *menu,
                              GList *selection,
                              const char *menu_path,
                              GtkSignalFunc callback)
{
	return insert_one_context_menu_item (view, menu, selection, menu_path, -1, callback);
}

/* special_link_in_selection
 * 
 * Return TRUE is one of our special links is the selection.
 * Special links include the following: 
 *	 NAUTILUS_LINK_TRASH, NAUTILUS_LINK_HOME, NAUTILUS_LINK_MOUNT
 */
 
static gboolean
special_link_in_selection (FMDirectoryView *view)
{
	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_TRASH)) {
		return TRUE;
	}

	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_HOME)) {
		return TRUE;
	}

	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_MOUNT)) {
		return TRUE;
	}

	return FALSE;
}

static void
fm_icon_view_create_selection_context_menu_items (FMDirectoryView *view,
						  GtkMenu *menu,
						  GList *selection)
{
	gint position;

	g_assert (FM_IS_ICON_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_selection_context_menu_items,
		 (view, menu, selection));

        append_one_context_menu_item
		(FM_ICON_VIEW (view), menu, selection,
		 MENU_PATH_STRETCH_ICON,
		 GTK_SIGNAL_FUNC (show_stretch_handles_callback));
        append_one_context_menu_item
		(FM_ICON_VIEW (view), menu, selection,
		 MENU_PATH_UNSTRETCH_ICONS,
		 GTK_SIGNAL_FUNC (unstretch_icons_callback));
	
	/* The Rename item is inserted directly after the
	 * Duplicate item created by the FMDirectoryView.
	 */
	if (!special_link_in_selection (view)) {
		position = fm_directory_view_get_context_menu_index
				(menu, FM_DIRECTORY_VIEW_COMMAND_DUPLICATE) + 1;
     		insert_one_context_menu_item
				(FM_ICON_VIEW (view), menu, selection, 
		 		 MENU_PATH_RENAME, position,
		 		 GTK_SIGNAL_FUNC (rename_icon_callback));
	}
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
fm_icon_view_create_background_context_menu_items (FMDirectoryView *view,
						   GtkMenu *menu)
{
	FMIconView *icon_view;
	char *manual_item_label;
	GtkMenuItem *lay_out_item;
	GtkMenuItem *toggle_item;
	GtkRadioMenuItem *manual_item;
	GtkMenu *layout_submenu;
	gboolean is_auto_layout;
	int position;
	int i;

	g_assert (GTK_IS_MENU (menu));

	icon_view = FM_ICON_VIEW (view);

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_background_context_menu_items, 
		 (view, menu));

	position = fm_directory_view_get_context_menu_index
		(menu, FM_DIRECTORY_VIEW_COMMAND_NEW_FOLDER) + 1;

	nautilus_gtk_menu_insert_separator (menu, position++);

	if (fm_icon_view_supports_auto_layout (icon_view)) {

		/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
		icon_view->details->updating_toggle_menu_item = TRUE;

		is_auto_layout = fm_icon_view_using_auto_layout (icon_view);

	     	lay_out_item = GTK_MENU_ITEM (insert_one_context_menu_item
			(icon_view, menu, NULL, 
			 MENU_PATH_LAY_OUT, position++,
			 NULL));
		layout_submenu = GTK_MENU (gtk_menu_new ());

		/* Compute label the standard bonobo_or_gtk way, to avoid
		 * duplicating string constant.
		 */
		compute_menu_item_info (icon_view, 
					NULL, 
					MENU_PATH_MANUAL_LAYOUT,
					FALSE, 
					&manual_item_label, 
					NULL, 
					NULL);
		manual_item = GTK_RADIO_MENU_ITEM (append_one_context_menu_layout_item 
			(icon_view, layout_submenu, NULL, 
			 ID_MANUAL_LAYOUT, manual_item_label));
		g_free (manual_item_label);
		if (!is_auto_layout) {
			gtk_check_menu_item_set_active 
				(GTK_CHECK_MENU_ITEM (manual_item), TRUE);
		}

		nautilus_gtk_menu_append_separator (layout_submenu);

		for (i = 0; i < NAUTILUS_N_ELEMENTS (sort_criteria); i++) {
			toggle_item = append_one_context_menu_layout_item 
				(icon_view, layout_submenu, manual_item, 
			 	sort_criteria[i].id,
			 	sort_criteria[i].menu_label);		
			if (is_auto_layout && strcmp (icon_view->details->sort->id, 
						      sort_criteria[i].id) == 0) {
				gtk_check_menu_item_set_active 
					(GTK_CHECK_MENU_ITEM (toggle_item), TRUE);
			}
		}
	 
		nautilus_gtk_menu_append_separator (layout_submenu);

		/* No callback here, since that is handled by the bonobo "id" field */
	     	toggle_item = append_one_context_menu_item
			(icon_view, layout_submenu, NULL, 
			 MENU_PATH_TIGHTER_LAYOUT,
			 NULL);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (toggle_item), 
					   	fm_icon_view_using_tighter_layout (icon_view)); 
		

	     	toggle_item = append_one_context_menu_item
			(icon_view, layout_submenu, NULL, 
			 MENU_PATH_SORT_REVERSED,
			 NULL);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (toggle_item), 
						icon_view->details->sort_reversed);
		
		gtk_menu_item_set_submenu (lay_out_item, GTK_WIDGET (layout_submenu));

		icon_view->details->updating_toggle_menu_item = FALSE;
	}

     	insert_one_context_menu_item
		(icon_view, menu, NULL, 
		 MENU_PATH_CLEAN_UP, position++,
		 GTK_SIGNAL_FUNC (clean_up_callback));
}

static void
unref_cover (NautilusIconData *data, gpointer callback_data)
{
	nautilus_file_unref (NAUTILUS_FILE (data));
}

static void
fm_icon_view_clear (FMDirectoryView *view)
{
	NautilusIconContainer *icon_container;
	
	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));

	/* Clear away the existing icons. */
	nautilus_icon_container_for_each (icon_container, unref_cover, NULL);
	nautilus_icon_container_clear (icon_container);
}

static void
fm_icon_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_icon_container_add (get_icon_container (FM_ICON_VIEW (view)),
					 NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
		nautilus_file_ref (file);
	}

}

static void
fm_icon_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	gboolean removed;

	/* This handles both changes to an existing file and the existing file going away. */
	if (!nautilus_directory_contains_file (fm_directory_view_get_model (view), file)) {
		removed = nautilus_icon_container_remove
			(get_icon_container (FM_ICON_VIEW (view)),
			 NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
		if (removed) {
			nautilus_file_unref (file);
		}
	} else {
		nautilus_icon_container_request_update
			(get_icon_container (FM_ICON_VIEW (view)),
			 NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
	}
}

static void
update_layout_menus (FMIconView *view)
{
	char *path;
	gboolean is_auto_layout;
	
	if (!view->details->menus_ready) {
		return;
	}

	is_auto_layout = fm_icon_view_using_auto_layout (view);

	if (fm_icon_view_supports_auto_layout (view)) {
		/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
		view->details->updating_toggle_menu_item = TRUE;
		
		/* Mark sort criterion. */
		path = g_strconcat (COMMAND_PREFIX,
				    is_auto_layout ? view->details->sort->id : ID_MANUAL_LAYOUT,
				    NULL);
		nautilus_bonobo_set_toggle_state (view->details->ui, path, TRUE);
		g_free (path);

		/* Set the checkmark for the "tighter layout" item */
		nautilus_bonobo_set_toggle_state 
			(view->details->ui, COMMAND_TIGHTER_LAYOUT, fm_icon_view_using_tighter_layout (view));

		/* Set the checkmark for the "reversed order" item */
		nautilus_bonobo_set_toggle_state 
			(view->details->ui, COMMAND_SORT_REVERSED, view->details->sort_reversed);

		/* Sort order isn't relevant for manual layout. */
		nautilus_bonobo_set_sensitive
			(view->details->ui, COMMAND_SORT_REVERSED, is_auto_layout);

		/* Tighter Layout is only relevant for auto layout */
		nautilus_bonobo_set_sensitive
			(view->details->ui, COMMAND_TIGHTER_LAYOUT, is_auto_layout);	
		 
		/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/GTK menu bug. */
		view->details->updating_toggle_menu_item = FALSE;
	}

	/* Clean Up is only relevant for manual layout */
	nautilus_bonobo_set_sensitive
		(view->details->ui, COMMAND_CLEAN_UP, !is_auto_layout);	
}


static char *
fm_icon_view_get_directory_sort_by (FMIconView *icon_view,
				    NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return g_strdup ("name");
	}

	return NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
				      get_directory_sort_by, (icon_view, file));
}

static char *
fm_icon_view_real_get_directory_sort_by (FMIconView *icon_view,
					 NautilusFile *file)
{
	return nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 sort_criteria[0].metadata_text);
}

static void
fm_icon_view_set_directory_sort_by (FMIconView *icon_view, 
				    NautilusFile *file, 
				    const char *sort_by)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
			       set_directory_sort_by, (icon_view, file, sort_by));
}

static void
fm_icon_view_real_set_directory_sort_by (FMIconView *icon_view,
					 NautilusFile *file,
					 const char *sort_by)
{
	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 sort_criteria[0].metadata_text,
		 sort_by);
}

static gboolean
fm_icon_view_get_directory_sort_reversed (FMIconView *icon_view,
					  NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	return NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
				      get_directory_sort_reversed, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_sort_reversed (FMIconView *icon_view,
					       NautilusFile *file)
{
	return  nautilus_file_get_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED, FALSE);
}

static void
fm_icon_view_set_directory_sort_reversed (FMIconView *icon_view,
					  NautilusFile *file,
					  gboolean sort_reversed)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
			       set_directory_sort_reversed,
			       (icon_view, file, sort_reversed));
}

static void
fm_icon_view_real_set_directory_sort_reversed (FMIconView *icon_view,
					       NautilusFile *file,
					       gboolean sort_reversed)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED, FALSE,
		 sort_reversed);
}

/* maintainence of auto layout boolean */

static gboolean
fm_icon_view_get_directory_auto_layout (FMIconView *icon_view,
					NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	return NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
				      get_directory_auto_layout, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_auto_layout (FMIconView *icon_view,
					     NautilusFile *file)
{
	return nautilus_file_get_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, TRUE);
}

static void
fm_icon_view_set_directory_auto_layout (FMIconView *icon_view,
					NautilusFile *file,
					gboolean auto_layout)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
			       set_directory_auto_layout, (icon_view, file, auto_layout));
}

static void
fm_icon_view_real_set_directory_auto_layout (FMIconView *icon_view,
					     NautilusFile *file,
					     gboolean auto_layout)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, TRUE,
		 auto_layout);
}
/* maintainence of tighter layout boolean */

static gboolean
fm_icon_view_get_directory_tighter_layout (FMIconView *icon_view,
					   NautilusFile *file)
{
	return NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
				      get_directory_tighter_layout, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_tighter_layout (FMIconView *icon_view,
						NautilusFile *file)
{
	return nautilus_file_get_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT, FALSE);
}

static void
fm_icon_view_set_directory_tighter_layout (FMIconView *icon_view,
					   NautilusFile *file,
					   gboolean tighter_layout)
{
	NAUTILUS_CALL_VIRTUAL (FM_ICON_VIEW_CLASS, icon_view,
			       set_directory_tighter_layout, (icon_view, file, tighter_layout));
}

static void
fm_icon_view_real_set_directory_tighter_layout (FMIconView *icon_view,
						NautilusFile *file,
						gboolean tighter_layout)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT, FALSE,
		 tighter_layout);
}

gboolean
fm_icon_view_supports_auto_layout (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return NAUTILUS_CALL_VIRTUAL
		(FM_ICON_VIEW_CLASS, view,
		 supports_auto_layout, (view));
}

static gboolean
real_supports_auto_layout (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return TRUE;
}

static gboolean
set_sort_reversed (FMIconView *icon_view, gboolean new_value)
{
	if (icon_view->details->sort_reversed == new_value) {
		return FALSE;
	}
	icon_view->details->sort_reversed = new_value;

	/* Store the new sort setting. */
	fm_icon_view_set_directory_sort_reversed (icon_view, fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view)), new_value);
	
	/* Update the layout menus to match the new sort-order setting. */
	update_layout_menus (icon_view);

	return TRUE;
}

static const SortCriterion *
get_sort_criterion_by_metadata_text (const char *metadata_text)
{
	int i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (sort_criteria); i++) {
		if (strcmp (sort_criteria[i].metadata_text, metadata_text) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_id (const char *id)
{
	int i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (sort_criteria); i++) {
		if (strcmp (sort_criteria[i].id, id) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static void
fm_icon_view_begin_loading (FMDirectoryView *view)
{
	FMIconView *icon_view;
	GtkWidget *icon_container;
	NautilusFile *file;
	int level;
	char *sort_name;
	
	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	file = fm_directory_view_get_directory_as_file (view);
	icon_container = GTK_WIDGET (get_icon_container (icon_view));

	/* kill any sound preview process that is ongoing */
	preview_sound (NULL, FALSE);
	
	/* FIXME: this should be done with a virtual function, not
           hardcoding knowledge about a subclass into the parent class. */

	if (FM_IS_DESKTOP_ICON_VIEW (view)) {
		nautilus_connect_desktop_background_to_file_metadata (icon_container, file);
	} else {
		nautilus_connect_background_to_file_metadata (icon_container, file);
	}
	
	/* Set up the zoom level from the metadata. */
	level = nautilus_file_get_integer_metadata
		(file, 
		 NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
		 icon_view->details->default_zoom_level);
	fm_icon_view_set_zoom_level (icon_view, level, TRUE);

	/* Set the sort mode.
	 * It's OK not to resort the icons because the
	 * container doesn't have any icons at this point.
	 */
	sort_name = fm_icon_view_get_directory_sort_by (icon_view, file);
	set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name));
	g_free (sort_name);

	/* Set the sort direction from the metadata. */
	set_sort_reversed (icon_view, fm_icon_view_get_directory_sort_reversed (icon_view, file));

	/* Set the layout modes.
	 * We must do this after getting the sort mode,
	 * because otherwise the layout_changed callback
	 * might overwrite the sort mode.
	 */
	nautilus_icon_container_set_auto_layout
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_auto_layout (icon_view, file));
	nautilus_icon_container_set_tighter_layout
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_tighter_layout (icon_view, file));

}

static NautilusZoomLevel
fm_icon_view_get_zoom_level (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);
	return nautilus_icon_container_get_zoom_level (get_icon_container (view));
}

static void
fm_icon_view_set_zoom_level (FMIconView *view,
			     NautilusZoomLevel new_level,
			     gboolean always_set_level)
{
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	icon_container = get_icon_container (view);
	if (nautilus_icon_container_get_zoom_level (icon_container) == new_level) {
		if (always_set_level) {
			fm_directory_view_set_zoom_level (&view->parent, new_level);
		}
		return;
	}

	nautilus_file_set_integer_metadata
		(fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view)), 
		 NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
		 view->details->default_zoom_level,
		 new_level);

	nautilus_icon_container_set_zoom_level (icon_container, new_level);
	fm_directory_view_set_zoom_level (&view->parent, new_level);

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
	fm_icon_view_set_zoom_level(icon_view, new_level, FALSE);
}

static void
fm_icon_view_zoom_to_level (FMDirectoryView *view, int zoom_level)
{
	FMIconView *icon_view;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	fm_icon_view_set_zoom_level(icon_view, zoom_level, FALSE);
}

static void
fm_icon_view_restore_default_zoom_level (FMDirectoryView *view)
{
	FMIconView *icon_view;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	fm_icon_view_set_zoom_level(icon_view, NAUTILUS_ZOOM_LEVEL_STANDARD, FALSE);
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

static GtkWidget * 
fm_icon_view_get_background_widget (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return GTK_WIDGET (get_icon_container (FM_ICON_VIEW (view)));
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
	char *all_names, *result, *c;
	int pieces_so_far, piece_count;
	const int pieces_by_level[] = {
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALLEST */
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALLER */
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALL */
		1,	/* NAUTILUS_ZOOM_LEVEL_STANDARD */
		2,	/* NAUTILUS_ZOOM_LEVEL_LARGE */
		2,	/* NAUTILUS_ZOOM_LEVEL_LARGER */
		3	/* NAUTILUS_ZOOM_LEVEL_LARGEST */
	};

	piece_count = pieces_by_level[fm_icon_view_get_zoom_level (view)];

	all_names = fm_get_text_attribute_names_preference_or_default ();
	pieces_so_far = 0;

	for (c = all_names; *c != '\0'; ++c) {
		if (pieces_so_far == piece_count) {
			break;
		}
		if (*c == '|') {
			++pieces_so_far;
		}
	}

	/* Return an initial substring of the full set */
	result = g_strndup (all_names, (c - all_names));

	g_free (all_names);
	
	return result;
}

static gboolean
fm_icon_view_is_empty (FMDirectoryView *view)
{
	g_assert (FM_IS_ICON_VIEW (view));

	return nautilus_icon_container_is_empty 
		(get_icon_container (FM_ICON_VIEW (view)));
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
set_sort_criterion_by_id (FMIconView *icon_view, const char *id)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (id != NULL);

	set_sort_criterion (icon_view, get_sort_criterion_by_id (id));
	nautilus_icon_container_sort (get_icon_container (icon_view));
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
sort_reversed_state_changed_callback (BonoboUIComponent *component,
				      const char        *path,
				      Bonobo_UIComponent_EventType type,
				      const char        *state,
				      gpointer          user_data)
{
	FMIconView *icon_view;

	g_assert (strcmp (path, ID_SORT_REVERSED) == 0);

	icon_view = FM_ICON_VIEW (user_data);

	/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo/Gtk menu bug. */
	if (icon_view->details->updating_toggle_menu_item) {
		return;
	}

	set_sort_reversed (icon_view, 
			   !fm_icon_view_get_directory_sort_reversed 
			   	(icon_view, 
			   	 fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view))));
	nautilus_icon_container_sort (get_icon_container (icon_view));
}

static void
switch_to_manual_layout (FMIconView *icon_view)
{
	icon_view->details->sort = &sort_criteria[0];
	nautilus_icon_container_set_auto_layout
		(get_icon_container (icon_view), FALSE);
}

static void
layout_changed_callback (NautilusIconContainer *container,
			 FMIconView *icon_view)
{
	NautilusFile *file;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));

	if (file != NULL) {
		fm_icon_view_set_directory_auto_layout
			(icon_view,
			 file,
			 fm_icon_view_using_auto_layout (icon_view));
		fm_icon_view_set_directory_tighter_layout
			(icon_view,
			 file,
			 fm_icon_view_using_tighter_layout (icon_view));
	}

	update_layout_menus (icon_view);
}

static void
fm_icon_view_start_renaming_item  (FMDirectoryView *view, const char *uri)
{
	/* call parent class to make sure the right icon is selected */
	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, start_renaming_item, (view, uri));
	/* start renaming */
	nautilus_icon_container_start_renaming_selected_item
		(get_icon_container (FM_ICON_VIEW (view)));
}

static void
handle_ui_event (BonoboUIComponent *ui,
		 const char *id,
		 Bonobo_UIComponent_EventType type,
		 const char *state,
		 FMIconView *view)
{
	if (type == Bonobo_UIComponent_STATE_CHANGED
	    && strcmp (state, "1") == 0) {
		handle_radio_item (view, id);
	}
}

static void
fm_icon_view_merge_menus (FMDirectoryView *view)
{
	FMIconView *icon_view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Rename", (BonoboUIVerbFn)rename_icon_callback),
		BONOBO_UI_VERB ("Icon Text", (BonoboUIVerbFn)customize_icon_text_callback),
		BONOBO_UI_VERB ("Stretch", (BonoboUIVerbFn)show_stretch_handles_callback),
		BONOBO_UI_VERB ("Unstretch", (BonoboUIVerbFn)unstretch_icons_callback),
		BONOBO_UI_VERB ("Clean Up", (BonoboUIVerbFn)clean_up_callback),
		BONOBO_UI_VERB_END
	};
	
        g_assert (FM_IS_ICON_VIEW (view));

	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

	icon_view = FM_ICON_VIEW (view);

	icon_view->details->ui = bonobo_ui_component_new ("Icon View");
	gtk_signal_connect (GTK_OBJECT (icon_view->details->ui),
			    "ui_event", handle_ui_event, icon_view);
	bonobo_ui_component_set_container (icon_view->details->ui,
					   fm_directory_view_get_bonobo_ui_container (view));
	bonobo_ui_util_set_ui (icon_view->details->ui,
			       DATADIR,
			       "nautilus-icon-view-ui.xml",
			       "nautilus");

	bonobo_ui_component_add_verb_list_with_data (icon_view->details->ui, verbs, view);
	
	bonobo_ui_component_add_listener (icon_view->details->ui, ID_TIGHTER_LAYOUT, tighter_layout_state_changed_callback, view);
	bonobo_ui_component_add_listener (icon_view->details->ui, ID_SORT_REVERSED, sort_reversed_state_changed_callback, view);
	icon_view->details->menus_ready = TRUE;

	update_layout_menus (icon_view);
}

static void
update_one_menu_item (FMIconView *view, 
		      GList *selection,
		      const char *menu_path,
		      const char *verb_path)
{
	char *label;
	gboolean sensitive;
	
	compute_menu_item_info (view, selection, menu_path, TRUE, &label, &sensitive, NULL);

	nautilus_bonobo_set_sensitive (view->details->ui, verb_path, sensitive);
	nautilus_bonobo_set_label (view->details->ui, menu_path, label);
	g_free (label);
}

static void
fm_icon_view_update_menus (FMDirectoryView *view)
{
        GList *selection;

	/* don't update if the menus aren't ready */
	if (!FM_ICON_VIEW (view)->details->menus_ready) {
		return;
	}
	
	NAUTILUS_CALL_PARENT_CLASS (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));

        selection = fm_directory_view_get_selection (view);

	update_one_menu_item (FM_ICON_VIEW (view), selection, 
			      MENU_PATH_STRETCH_ICON,
			      COMMAND_STRETCH_ICON);
        update_one_menu_item (FM_ICON_VIEW (view), selection, 
			      MENU_PATH_UNSTRETCH_ICONS,
			      COMMAND_UNSTRETCH_ICONS);
        update_one_menu_item (FM_ICON_VIEW (view), selection, 
			      MENU_PATH_RENAME,
			      COMMAND_RENAME);
	
	nautilus_file_list_free (selection);
}

static void
fm_icon_view_select_all (FMDirectoryView *view)
{
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));
        nautilus_icon_container_select_all (icon_container);
}

static void
fm_icon_view_reveal_selection (FMDirectoryView *view)
{
	GList *selection;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

        selection = fm_directory_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		nautilus_icon_container_reveal 
			(get_icon_container (FM_ICON_VIEW (view)), 
			 selection->data);
	}

        nautilus_file_list_free (selection);
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
				  GList *file_list,
				  FMIconView *icon_view)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	fm_directory_view_activate_files (FM_DIRECTORY_VIEW (icon_view), file_list);
}

static void
band_select_started_callback (NautilusIconContainer *container,
			      FMIconView *icon_view)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	fm_directory_view_start_batching_selection_changes (FM_DIRECTORY_VIEW (icon_view));
}

static void
band_select_ended_callback (NautilusIconContainer *container,
			    FMIconView *icon_view)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	fm_directory_view_stop_batching_selection_changes (FM_DIRECTORY_VIEW (icon_view));
}

/* handle the preview signal by inspecting the mime type.  For now, we only preview sound files. */

/* here's the timer task that actually plays the file using mpg123. */
/* FIXME bugzilla.eazel.com 1258: we should get the application from our mime-type stuff */

static int
play_file (gpointer callback_data)
{
	NautilusFile *file;
	char *file_uri;
	char *file_path, *mime_type;
	gboolean is_mp3;
	pid_t mp3_pid;
	
	file = NAUTILUS_FILE (callback_data);
	file_uri = nautilus_file_get_uri (file);
	file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	mime_type = nautilus_file_get_mime_type (file);
	is_mp3 = nautilus_strcasecmp (mime_type, "audio/x-mp3") == 0;
			
	if (file_path != NULL) {
		mp3_pid = fork ();
		if (mp3_pid == (pid_t) 0) {
			/* Set the group (session) id to this process for future killing. */
			setsid();
			if (is_mp3) {
				execlp ("mpg123", "mpg123", "-y", "-q", file_path, NULL);
			} else {
				execlp ("play", "play", file_path, NULL);
			}
			
			_exit (0);
		} else {
			nautilus_sound_register_sound (mp3_pid);
		}
	}

	g_free (file_path);
	g_free (file_uri);
	g_free (mime_type);
	
	timeout = -1;
	
	return 0;
}

/* FIXME bugzilla.eazel.com 2530: Hardcoding this here sucks. We should be using components
 * for open ended things like this.
 */

/* this routine is invoked from the preview signal handler to preview a sound file.  We
   want to wait a suitable delay until we actually do it, so set up a timer task to actually
   start playing.  If we move out before the task files, we remove it. */
   
static void
preview_sound (NautilusFile *file, gboolean start_flag)
{		
	nautilus_sound_kill_sound ();
	
	if (timeout >= 0) {
		gtk_timeout_remove (timeout);
		timeout = -1;
	}
	if (start_flag) {
		timeout = gtk_timeout_add (1000, play_file, file);
	}
}

static int
icon_container_preview_callback (NautilusIconContainer *container,
				 NautilusFile *file,
				 gboolean start_flag,
				 FMIconView *icon_view)
{
	int result;
	char *mime_type, *file_name, *message;
		
	result = 0;
	
	/* preview files based on the mime_type. */
	/* at first, we just handle sounds */
	mime_type = nautilus_file_get_mime_type (file);
	if (nautilus_istr_has_prefix (mime_type, "audio/")) {   	
		if (nautilus_sound_can_play_sound ()) {
			result = 1;
			preview_sound (file, start_flag);
		}
	}	
	g_free (mime_type);
	
	/* display file name in status area at low zoom levels, since the name is not displayed or hard to read */
	if (fm_icon_view_get_zoom_level (icon_view) <= NAUTILUS_ZOOM_LEVEL_SMALLER) {
		if (start_flag) {
			file_name = nautilus_file_get_name (file);
			message = g_strdup_printf (_("pointing at \"%s\""), file_name);
			g_free (file_name);
			nautilus_view_report_status
				(fm_directory_view_get_nautilus_view (FM_DIRECTORY_VIEW (icon_view)),
				 message);
			g_free (message);
		} else {
			fm_directory_view_display_selection_info (FM_DIRECTORY_VIEW(icon_view));
		}
	}
	
	return result;
}

static int
icon_container_compare_icons_callback (NautilusIconContainer *container,
				       NautilusFile *file_a,
				       NautilusFile *file_b,
				       FMIconView *icon_view)
{
	int result;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (NAUTILUS_IS_FILE (file_a));
	g_assert (NAUTILUS_IS_FILE (file_b));

	result = nautilus_file_compare_for_sort
		(file_a, file_b, icon_view->details->sort->sort_type);

	if (icon_view->details->sort_reversed) {
		result = -1 * result;
	}

	return result;
}

static void
selection_changed_callback (NautilusIconContainer *container,
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
icon_position_changed_callback (NautilusIconContainer *container,
				NautilusFile *file,
				const NautilusIconPosition *position,
				FMIconView *icon_view)
{
	char *position_string;
	char *scale_string, *scale_string_x, *scale_string_y;
	char *locale;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (NAUTILUS_IS_FILE (file));

	/* Doing formatting in the "C" locale instead of the one set
	 * by the user ensures that data in the metafile is not in
	 * a locale-specific format. It's only necessary for floating
	 * point values since there aren't locale-specific formats for
	 * integers in C stdio.
	 */
	locale = setlocale (LC_NUMERIC, "C");

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

	/* Store the new position of the icon in the metadata. */
	if (!fm_icon_view_using_auto_layout (icon_view)) {
		position_string = g_strdup_printf
			("%d,%d", position->x, position->y);
		nautilus_file_set_metadata
			(file, NAUTILUS_METADATA_KEY_ICON_POSITION, 
			 NULL, position_string);
		g_free (position_string);
	}

	/* FIXME bugzilla.eazel.com 662: 
	 * %.2f is not a good format for the scale factor. We'd like it to
	 * say "2" or "2x" instead of "2.00".
	 */
	scale_string_x = g_strdup_printf ("%.2f", position->scale_x);
	scale_string_y = g_strdup_printf ("%.2f", position->scale_y);
	if (strcmp (scale_string_x, scale_string_y) == 0) {
		scale_string = scale_string_x;
		g_free (scale_string_y);
	} else {
		scale_string = g_strconcat (scale_string_x, ",", scale_string_y, NULL);
		g_free (scale_string_x);
		g_free (scale_string_y);
	}
	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_SCALE,
		 "1.00", scale_string);
	g_free (scale_string);

	setlocale (LC_NUMERIC, locale);
}

/* Attempt to change the filename to the new text.  Notify user if operation fails. */
static void
fm_icon_view_icon_text_changed_callback (NautilusIconContainer *container,
					 NautilusFile *file,				    
					 char *new_name,
					 FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (new_name != NULL);

	/* Don't allow a rename with an empty string. Revert to original 
	 * without notifying the user.
	 */
	if (new_name[0] == '\0') {
		return;
	}
	fm_rename_file (file, new_name);
}

static NautilusScalableIcon *
get_icon_images_callback (NautilusIconContainer *container,
			  NautilusFile *file,
			  const char *modifier,
			  GList **emblem_icons,
			  FMIconView *icon_view)
{
	gboolean smooth_graphics;
	NautilusStringList *emblems_to_ignore;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	smooth_graphics = nautilus_icon_container_get_anti_aliased_mode (container);
	if (emblem_icons != NULL) {
		emblems_to_ignore = fm_directory_view_get_emblem_names_to_exclude 
			(FM_DIRECTORY_VIEW (icon_view));
		*emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file, smooth_graphics, emblems_to_ignore);
		nautilus_string_list_free (emblems_to_ignore);
	}
	return nautilus_icon_factory_get_icon_for_file (file, modifier, smooth_graphics);
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
get_icon_drop_target_uri_callback (NautilusIconContainer *container,
		       		   NautilusFile *file,
		       		   FMIconView *icon_view)
{
	char *uri, *path, *target_uri;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	uri = nautilus_file_get_uri (file);

	/* Check for Nautilus link */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.eazel.com 3020: This does sync. I/O and works only locally. */
		path = gnome_vfs_get_local_path_from_uri (uri);
		if (path != NULL) {
			target_uri = nautilus_link_local_get_link_uri (path);
			if (target_uri != NULL) {
				g_free (uri);
				uri = target_uri;
			}
			g_free (path);
		}
	}

	return uri;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
get_icon_text_callback (NautilusIconContainer *container,
			NautilusFile *file,
			char **editable_text,
			char **additional_text,
			FMIconView *icon_view)
{
	char *actual_uri, *path;
	char *attribute_names;
	char **text_array;
	int i , slot_index;
	char *attribute_string;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (editable_text != NULL);
	g_assert (additional_text != NULL);
	g_assert (FM_IS_ICON_VIEW (icon_view));

	/* In the smallest zoom mode, no text is drawn. */
	if (fm_icon_view_get_zoom_level (icon_view) == NAUTILUS_ZOOM_LEVEL_SMALLEST) {
		*editable_text = NULL;
	} else {
		/* Strip the suffix for nautilus object xml files. */
		*editable_text = nautilus_file_get_name (file);
	}
	
	/* Handle link files specially. */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.eazel.com 2531: Does sync. I/O and works only locally. */
		actual_uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (actual_uri);
		g_free (actual_uri);
		if (path != NULL) {
			*additional_text = nautilus_link_local_get_additional_text (path);
			g_free (path);
			return;
		}
	}
	
	/* Find out what attributes go below each icon. */
	attribute_names = fm_icon_view_get_icon_text_attribute_names (icon_view);
	text_array = g_strsplit (attribute_names, "|", 0);
	g_free (attribute_names);

	/* Get the attributes. */
	for (i = 0; text_array[i] != NULL; i++)	{
		/* if the attribute is "none", delete the array slot */
		while (nautilus_strcmp (text_array[i], "none") == 0) {
			g_free (text_array[i]);
			text_array[i] = NULL;
			slot_index = i + 1;			
			while (text_array[slot_index] != NULL) {
				text_array[slot_index - 1] = text_array[slot_index];
				text_array[slot_index++] = NULL;
			}
			if (text_array[i] == NULL)
				break;
		} 
		
		if (text_array[i] == NULL)
			break;
			
		attribute_string = nautilus_file_get_string_attribute_with_default
			(file, text_array[i]);
				
		/* Replace each attribute name in the array with its string value */
		g_free (text_array[i]);
		text_array[i] = attribute_string;
	}

	/* Return them. */
	*additional_text = g_strjoinv ("\n", text_array);

	g_strfreev (text_array);
}

/* Preferences changed callbacks */
static void
fm_icon_view_text_attribute_names_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	nautilus_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));	
}

static void
fm_icon_view_embedded_text_policy_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	nautilus_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));	
}

static void
fm_icon_view_image_display_policy_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	nautilus_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));	
}

static void
fm_icon_view_font_family_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	fm_icon_view_update_icon_container_fonts (FM_ICON_VIEW (directory_view));
}

static void
fm_icon_view_click_policy_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	fm_icon_view_update_click_mode (FM_ICON_VIEW (directory_view));
}

static void
fm_icon_view_smooth_graphics_mode_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	fm_icon_view_update_smooth_graphics_mode (FM_ICON_VIEW (directory_view));
}

/* GtkObject methods. */

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
	fm_directory_view_class->zoom_to_level = fm_icon_view_zoom_to_level;
	fm_directory_view_class->restore_default_zoom_level = fm_icon_view_restore_default_zoom_level;
	fm_directory_view_class->can_zoom_in = fm_icon_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_icon_view_can_zoom_out;
	fm_directory_view_class->get_background_widget = fm_icon_view_get_background_widget;
	fm_directory_view_class->clear = fm_icon_view_clear;
	fm_directory_view_class->file_changed = fm_icon_view_file_changed;
	fm_directory_view_class->is_empty = fm_icon_view_is_empty;
	fm_directory_view_class->get_selection = fm_icon_view_get_selection;
	fm_directory_view_class->select_all = fm_icon_view_select_all;
	fm_directory_view_class->set_selection = fm_icon_view_set_selection;
	fm_directory_view_class->reveal_selection = fm_icon_view_reveal_selection;
        fm_directory_view_class->create_background_context_menu_items =
		fm_icon_view_create_background_context_menu_items;
        fm_directory_view_class->create_selection_context_menu_items =
		fm_icon_view_create_selection_context_menu_items;
        fm_directory_view_class->merge_menus = fm_icon_view_merge_menus;
        fm_directory_view_class->update_menus = fm_icon_view_update_menus;
        fm_directory_view_class->start_renaming_item = fm_icon_view_start_renaming_item;
        fm_directory_view_class->text_attribute_names_changed = fm_icon_view_text_attribute_names_changed;
        fm_directory_view_class->embedded_text_policy_changed = fm_icon_view_embedded_text_policy_changed;
        fm_directory_view_class->image_display_policy_changed = fm_icon_view_image_display_policy_changed;
        fm_directory_view_class->font_family_changed = fm_icon_view_font_family_changed;
        fm_directory_view_class->click_policy_changed = fm_icon_view_click_policy_changed;
        fm_directory_view_class->smooth_graphics_mode_changed = fm_icon_view_smooth_graphics_mode_changed;


	klass->clean_up			   = fm_icon_view_real_clean_up;
        klass->get_directory_sort_by       = fm_icon_view_real_get_directory_sort_by;
        klass->set_directory_sort_by       = fm_icon_view_real_set_directory_sort_by;
        klass->get_directory_sort_reversed = fm_icon_view_real_get_directory_sort_reversed;
        klass->set_directory_sort_reversed = fm_icon_view_real_set_directory_sort_reversed;
        klass->get_directory_auto_layout   = fm_icon_view_real_get_directory_auto_layout;
        klass->set_directory_auto_layout   = fm_icon_view_real_set_directory_auto_layout;
        klass->get_directory_tighter_layout = fm_icon_view_real_get_directory_tighter_layout;
        klass->set_directory_tighter_layout = fm_icon_view_real_set_directory_tighter_layout;
	klass->supports_auto_layout = real_supports_auto_layout;
}

static void
fm_icon_view_initialize (FMIconView *icon_view)
{
        g_return_if_fail (GTK_BIN (icon_view)->child == NULL);

	icon_view->details = g_new0 (FMIconViewDetails, 1);
	icon_view->details->default_zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
	icon_view->details->sort = &sort_criteria[0];

	create_icon_container (icon_view);
}

static gboolean
icon_view_can_accept_item (NautilusIconContainer *container,
			   NautilusFile *target_item,
			   const char *item_uri,
			   FMDirectoryView *view)
{
	return fm_directory_view_can_accept_item (target_item, item_uri, view);
}

static char *
icon_view_get_container_uri (NautilusIconContainer *container,
			     FMDirectoryView *view)
{
	return fm_directory_view_get_uri (view);
}

static void
icon_view_move_copy_items (NautilusIconContainer *container,
			   const GList *item_uris,
			   GdkPoint *relative_item_points,
			   const char *target_dir,
			   int copy_action,
			   int x, int y,
			   FMDirectoryView *view)
{
	fm_directory_view_move_copy_items (item_uris, relative_item_points, target_dir,
		copy_action, x, y, view);
}

static void
fm_icon_view_update_icon_container_fonts (FMIconView *icon_view)
{
 	/* font size table - this isn't exactly proportional, but it looks better than computed */
	static guint font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1] = {
		8, 8, 10, 12, 14, 18, 18 };
	NautilusIconContainer *icon_container;
	GdkFont *font;
	guint i;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	for (i = 0; i <= NAUTILUS_ZOOM_LEVEL_LARGEST; i++) {

		font = nautilus_font_factory_get_font_from_preferences (font_size_table[i]);
		g_assert (font != NULL);
		nautilus_icon_container_set_label_font_for_zoom_level (icon_container, i, font);
		gdk_font_unref (font);
	}

	nautilus_icon_container_request_update_all (icon_container);
}

static void
fm_icon_view_update_click_mode (FMIconView *icon_view)
{
	NautilusIconContainer	*icon_container;
	int			click_mode;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	click_mode = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY,
						    NAUTILUS_CLICK_POLICY_DOUBLE);


	nautilus_icon_container_set_single_click_mode (icon_container,
						       click_mode == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
fm_icon_view_update_smooth_graphics_mode (FMIconView *icon_view)
{
	NautilusIconContainer	*icon_container;
	gboolean		smooth_graphics_mode;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	smooth_graphics_mode = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
								 TRUE);

	nautilus_icon_container_set_anti_aliased_mode (icon_container, smooth_graphics_mode);
}

static void
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
			    "band_select_started",
			    GTK_SIGNAL_FUNC (band_select_started_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "band_select_ended",
			    GTK_SIGNAL_FUNC (band_select_ended_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "compare_icons",
			    GTK_SIGNAL_FUNC (icon_container_compare_icons_callback),
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
			    "icon_position_changed",
			    GTK_SIGNAL_FUNC (icon_position_changed_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "icon_text_changed",
			    GTK_SIGNAL_FUNC (fm_icon_view_icon_text_changed_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed_callback),
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
			    "get_icon_drop_target_uri",
			    GTK_SIGNAL_FUNC (get_icon_drop_target_uri_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_icon_text",
			    GTK_SIGNAL_FUNC (get_icon_text_callback),
			    icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "move_copy_items",
			    GTK_SIGNAL_FUNC (icon_view_move_copy_items),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_container_uri",
			    GTK_SIGNAL_FUNC (icon_view_get_container_uri),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "can_accept_item",
			    GTK_SIGNAL_FUNC (icon_view_can_accept_item),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "get_stored_icon_position",
			    GTK_SIGNAL_FUNC (get_stored_icon_position_callback),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "layout_changed",
			    GTK_SIGNAL_FUNC (layout_changed_callback),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "preview",
			    GTK_SIGNAL_FUNC (icon_container_preview_callback),
			    icon_view);

	gtk_container_add (GTK_CONTAINER (icon_view),
			   GTK_WIDGET (icon_container));

	fm_icon_view_update_icon_container_fonts (icon_view);
	fm_icon_view_update_click_mode (icon_view);
	fm_icon_view_update_smooth_graphics_mode (icon_view);

	gtk_widget_show (GTK_WIDGET (icon_container));
}
