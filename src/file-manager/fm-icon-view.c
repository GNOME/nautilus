/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.c - implementation of icon view of directory.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

#include "fm-icon-container.h"
#include "fm-desktop-icon-view.h"
#include "fm-error-reporting.h"
#include <stdlib.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-background.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libnautilus-private/nautilus-audio-player.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-container.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-sound.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-scroll-positionable.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define USE_OLD_AUDIO_PREVIEW 1
#define READ_CHUNK_SIZE 16384

/* Paths to use when creating & referring to Bonobo menu items */
#define MENU_PATH_STRETCH_ICON 			"/menu/Edit/Edit Items Placeholder/Stretch"
#define MENU_PATH_UNSTRETCH_ICONS 		"/menu/Edit/Edit Items Placeholder/Unstretch"
#define MENU_PATH_LAY_OUT			"/menu/View/View Items Placeholder/Arrange Items"
#define MENU_PATH_MANUAL_LAYOUT 		"/menu/View/View Items Placeholder/Arrange Items/Manual Layout"
#define MENU_PATH_TIGHTER_LAYOUT 		"/menu/View/View Items Placeholder/Arrange Items/Tighter Layout"
#define MENU_PATH_SORT_REVERSED			"/menu/View/View Items Placeholder/Arrange Items/Reversed Order"
#define MENU_PATH_CLEAN_UP			"/menu/View/View Items Placeholder/Clean Up"

#define POPUP_PATH_LAY_OUT			"/popups/background/Before Zoom Items/View Items/Arrange Items"

#define POPUP_PATH_ICON_APPEARANCE		"/popups/selection/Icon Appearance Items"
#define POPUP_PATH_STRETCH_ICON		        "/popups/selection/Icon Appearance Items/Stretch"
#define POPUP_PATH_UNSTRETCH_ICON	        "/popups/selection/Icon Appearance Items/Unstretch"

#define COMMAND_PREFIX                          "/commands/"
#define COMMAND_STRETCH_ICON 			"/commands/Stretch"
#define COMMAND_UNSTRETCH_ICONS 		"/commands/Unstretch"
#define COMMAND_TIGHTER_LAYOUT 			"/commands/Tighter Layout"
#define COMMAND_SORT_REVERSED			"/commands/Reversed Order"
#define COMMAND_CLEAN_UP			"/commands/Clean Up"
#define COMMAND_KEEP_ALIGNED 			"/commands/Keep Aligned"

#define ID_MANUAL_LAYOUT                        "Manual Layout"
#define ID_TIGHTER_LAYOUT                       "Tighter Layout"
#define ID_SORT_REVERSED                        "Reversed Order"
#define ID_KEEP_ALIGNED 			"Keep Aligned"

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

struct FMIconViewDetails
{
	GList *icons_not_positioned;

	guint react_to_icon_change_idle_id;
	gboolean menus_ready;

	gboolean loading;

	const SortCriterion *sort;
	gboolean sort_reversed;

	NautilusScrollPositionable *positionable;
	
	BonoboUIComponent *ui;
	
	NautilusAudioPlayerData *audio_player_data;
	int audio_preview_timeout;
	NautilusFile *audio_preview_file;

	gboolean filter_by_screen;
	int num_screens;
};


/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] = {
	{
		NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
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

static gboolean default_sort_in_reverse_order = FALSE;
static int preview_sound_auto_value;
static gboolean gnome_esd_enabled_auto_value;

static void                 fm_icon_view_set_directory_sort_by                 (FMIconView           *icon_view,
										NautilusFile         *file,
										const char           *sort_by);
static void                 fm_icon_view_set_zoom_level                        (FMIconView           *view,
										NautilusZoomLevel     new_level,
										gboolean              always_set_level);
static void                 fm_icon_view_update_click_mode                     (FMIconView           *icon_view);
static void                 fm_icon_view_set_directory_tighter_layout          (FMIconView           *icon_view,
										NautilusFile         *file,
										gboolean              tighter_layout);
static const SortCriterion *get_sort_criterion_by_id                           (const char           *id);
static const SortCriterion *get_sort_criterion_by_sort_type                    (NautilusFileSortType  sort_type);
static void                 set_sort_criterion_by_id                           (FMIconView           *icon_view,
										const char           *id);
static gboolean             set_sort_reversed                                  (FMIconView           *icon_view,
										gboolean              new_value);
static void                 switch_to_manual_layout                            (FMIconView           *view);
static void                 preview_audio                                      (FMIconView           *icon_view,
										NautilusFile         *file,
										gboolean              start_flag);
static void                 update_layout_menus                                (FMIconView           *view);

GNOME_CLASS_BOILERPLATE (FMIconView, fm_icon_view,
			 FMDirectoryView, FM_TYPE_DIRECTORY_VIEW)

static void
fm_icon_view_finalize (GObject *object)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (object);

	/* don't try to update menus during the destroy process */
	icon_view->details->menus_ready = FALSE;

	if (icon_view->details->ui != NULL) {
		bonobo_ui_component_unset_container (icon_view->details->ui, NULL);
		bonobo_object_unref (icon_view->details->ui);
	}

        if (icon_view->details->react_to_icon_change_idle_id != 0) {
                g_source_remove (icon_view->details->react_to_icon_change_idle_id);
        }

	/* kill any sound preview process that is ongoing */
	preview_audio (icon_view, NULL, FALSE);

	nautilus_file_list_free (icon_view->details->icons_not_positioned);

	g_free (icon_view->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static NautilusIconContainer *
get_icon_container (FMIconView *icon_view)
{
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
	char c;

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
	locale = g_strdup (setlocale (LC_NUMERIC, NULL));	
	setlocale (LC_NUMERIC, "C");

	/* Get the current position of this icon from the metadata. */
	position_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_POSITION, "");
	position_good = sscanf
		(position_string, " %d , %d %c",
		 &position->x, &position->y, &c) == 2;
	g_free (position_string);

	/* If it is the desktop directory, maybe the gnome-libs metadata has information about it */

	/* Get the scale of the icon from the metadata. */
	scale_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_SCALE, "1");
	scale_good = sscanf
		(scale_string, " %lf %c",
		 &position->scale_x, &c) == 1;
	if (scale_good) {
		position->scale_y = position->scale_x;
	} else {
		scale_good = sscanf
			(scale_string, " %lf %lf %c",
			 &position->scale_x,
			 &position->scale_y, &c) == 2;
		if (!scale_good) {
			position->scale_x = 1.0;
			position->scale_y = 1.0;
		}
	}
	g_free (scale_string);

	setlocale (LC_NUMERIC, locale);
	g_free (locale);
	
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

static void
show_stretch_handles_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_ICON_VIEW (callback_data));

	nautilus_icon_container_show_stretch_handles
		(get_icon_container (FM_ICON_VIEW (callback_data)));
}

static void
unstretch_icons_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_ICON_VIEW (callback_data));

	nautilus_icon_container_unstretch
		(get_icon_container (FM_ICON_VIEW (callback_data)));
}

static void
fm_icon_view_clean_up (FMIconView *icon_view)
{
	EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view, clean_up, (icon_view));
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

static void
clean_up_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	fm_icon_view_clean_up (FM_ICON_VIEW (callback_data));
}

static void
set_tighter_layout (FMIconView *icon_view, gboolean new_value)
{
	fm_icon_view_set_directory_tighter_layout (icon_view,  
						   fm_directory_view_get_directory_as_file 
						   	(FM_DIRECTORY_VIEW (icon_view)), 
						   new_value);
	nautilus_icon_container_set_tighter_layout (get_icon_container (icon_view), 
						    new_value);	
}

static void
tighter_layout_state_changed_callback (BonoboUIComponent   *component,
				       const char          *path,
				       Bonobo_UIComponent_EventType type,
				       const char          *state,
				       gpointer            user_data)
{
	g_assert (strcmp (path, ID_TIGHTER_LAYOUT) == 0);
	g_assert (FM_IS_ICON_VIEW (user_data));

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	set_tighter_layout (FM_ICON_VIEW (user_data), strcmp (state, "1") == 0);
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
handle_radio_item (FMIconView *view,
		   const char *id)
{
	/* Note that id might be a toggle item.
	 * Ignore non-sort ids so that they don't cause sorting.
	 */
	if (strcmp (id, ID_MANUAL_LAYOUT) == 0) {
		switch_to_manual_layout (view);
	} else if (get_sort_criterion_by_id (id) != NULL) {
		set_sort_criterion_by_id (view, id);
	}
}

static void
list_covers (NautilusIconData *data, gpointer callback_data)
{
	GSList **file_list;

	file_list = callback_data;

	*file_list = g_slist_prepend (*file_list, data);
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
	GSList *file_list;
	
	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_container = get_icon_container (FM_ICON_VIEW (view));
	if (!icon_container)
		return;

	/* Clear away the existing icons. */
	file_list = NULL;
	nautilus_icon_container_for_each (icon_container, list_covers, &file_list);
	nautilus_icon_container_clear (icon_container);
	g_slist_foreach (file_list, (GFunc)unref_cover, NULL);
	g_slist_free (file_list);
}


static gboolean
should_show_file_on_screen (FMDirectoryView *view, NautilusFile *file)
{
	char *screen_string;
	int screen_num;
	FMIconView *icon_view;
	GdkScreen *screen;

	icon_view = FM_ICON_VIEW (view);

	/* Get the screen for this icon from the metadata. */
	screen_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_SCREEN, "0");
	screen_num = atoi (screen_string);
	g_free (screen_string);
	screen = gtk_widget_get_screen (GTK_WIDGET (view));

	if (screen_num != gdk_screen_get_number (screen) &&
	    (screen_num < icon_view->details->num_screens ||
	     gdk_screen_get_number (screen) > 0)) {
		return FALSE;
	}

	return TRUE;
}

static void
fm_icon_view_remove_file (FMDirectoryView *view, NautilusFile *file)
{
	if (nautilus_icon_container_remove (get_icon_container (FM_ICON_VIEW (view)),
					    NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
		nautilus_file_unref (file);
	}
}

static void
fm_icon_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	FMIconView *icon_view;
	NautilusIconContainer *icon_container;
	
	icon_view = FM_ICON_VIEW (view);
	icon_container = get_icon_container (icon_view);

	if (icon_view->details->filter_by_screen &&
	    !should_show_file_on_screen (view, file)) {
			return;
	}

	/* Reset scroll region for the first icon added when loading a directory. */
	if (icon_view->details->loading && nautilus_icon_container_is_empty (icon_container)) {
		nautilus_icon_container_reset_scroll_region (icon_container);
	}
	
	if (nautilus_icon_container_add (icon_container,
					 NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
		nautilus_file_ref (file);
	}
}

static void
fm_icon_view_flush_added_files (FMDirectoryView *view)
{
	nautilus_icon_container_layout_now (get_icon_container (FM_ICON_VIEW (view)));
}

static void
fm_icon_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	FMIconView *icon_view;

	g_return_if_fail (view != NULL);
	icon_view = FM_ICON_VIEW (view);

	if (!icon_view->details->filter_by_screen) {
		nautilus_icon_container_request_update
			(get_icon_container (icon_view),
			 NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
		return;
	}
	
	if (!should_show_file_on_screen (view, file)) {
		fm_icon_view_remove_file (view, file);
	} else {

		nautilus_icon_container_request_update
			(get_icon_container (icon_view),
			 NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
	}
}

static gboolean
fm_icon_view_supports_auto_layout (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, view,
		 supports_auto_layout, (view));
}

static gboolean
fm_icon_view_supports_keep_aligned (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, view,
		 supports_keep_aligned, (view));
}

static gboolean
fm_icon_view_supports_labels_beside_icons (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, view,
		 supports_labels_beside_icons, (view));
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

	bonobo_ui_component_freeze (view->details->ui, NULL);

	if (fm_icon_view_supports_auto_layout (view)) {
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
	}

	/* Clean Up is only relevant for manual layout */
	nautilus_bonobo_set_sensitive
		(view->details->ui, COMMAND_CLEAN_UP, !is_auto_layout);	


	nautilus_bonobo_set_hidden (view->details->ui,
				    COMMAND_KEEP_ALIGNED, 
				    !fm_icon_view_supports_keep_aligned (view));
	
	nautilus_bonobo_set_toggle_state 
		(view->details->ui, COMMAND_KEEP_ALIGNED, 
		 nautilus_icon_container_is_keep_aligned (get_icon_container (view)));
	
	nautilus_bonobo_set_sensitive
		(view->details->ui, COMMAND_KEEP_ALIGNED, !is_auto_layout);

	bonobo_ui_component_thaw (view->details->ui, NULL);
}


static char *
fm_icon_view_get_directory_sort_by (FMIconView *icon_view,
				    NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return g_strdup ("name");
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, icon_view,
		 get_directory_sort_by, (icon_view, file));
}

static NautilusFileSortType default_sort_order = NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;

static NautilusFileSortType
get_default_sort_order (void)
{
	static gboolean auto_storaged_added = FALSE;

	if (auto_storaged_added == FALSE) {
		auto_storaged_added = TRUE;
		eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
					       (int *) &default_sort_order);
	}

	return CLAMP (default_sort_order, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, NAUTILUS_FILE_SORT_BY_EMBLEMS);
}

static char *
fm_icon_view_real_get_directory_sort_by (FMIconView *icon_view,
					 NautilusFile *file)
{
	const SortCriterion *default_sort_criterion;
	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order ());
	g_return_val_if_fail (default_sort_criterion != NULL, NULL);

	return nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text);
}

static void
fm_icon_view_set_directory_sort_by (FMIconView *icon_view, 
				    NautilusFile *file, 
				    const char *sort_by)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
			 set_directory_sort_by, (icon_view, file, sort_by));
}

static void
fm_icon_view_real_set_directory_sort_by (FMIconView *icon_view,
					 NautilusFile *file,
					 const char *sort_by)
{
	const SortCriterion *default_sort_criterion;
	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order ());
	g_return_if_fail (default_sort_criterion != NULL);

	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text,
		 sort_by);
}

static gboolean
fm_icon_view_get_directory_sort_reversed (FMIconView *icon_view,
					  NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, icon_view,
		 get_directory_sort_reversed, (icon_view, file));
}

static gboolean
get_default_sort_in_reverse_order (void)
{
	static gboolean auto_storaged_added = FALSE;
	
	if (auto_storaged_added == FALSE) {
		auto_storaged_added = TRUE;
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
						  &default_sort_in_reverse_order);
	}

	return default_sort_in_reverse_order;
}

static gboolean
fm_icon_view_real_get_directory_sort_reversed (FMIconView *icon_view,
					       NautilusFile *file)
{
	return  nautilus_file_get_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 get_default_sort_in_reverse_order ());
}

static void
fm_icon_view_set_directory_sort_reversed (FMIconView *icon_view,
					  NautilusFile *file,
					  gboolean sort_reversed)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
			 set_directory_sort_reversed,
			 (icon_view, file, sort_reversed));
}

static void
fm_icon_view_real_set_directory_sort_reversed (FMIconView *icon_view,
					       NautilusFile *file,
					       gboolean sort_reversed)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 get_default_sort_in_reverse_order (),
		 sort_reversed);
}

static gboolean
get_default_directory_keep_aligned (void)
{
	return TRUE;
}

static gboolean
fm_icon_view_get_directory_keep_aligned (FMIconView *icon_view,
					 NautilusFile *file)
{
	if (!fm_icon_view_supports_keep_aligned (icon_view)) {
		return FALSE;
	}
	
	return  nautilus_file_get_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned ());
}

static void
fm_icon_view_set_directory_keep_aligned (FMIconView *icon_view,
					 NautilusFile *file,
					 gboolean keep_aligned)
{
	if (!fm_icon_view_supports_keep_aligned (icon_view)) {
		return;
	}

	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned (),
		 keep_aligned);
}

/* maintainence of auto layout boolean */
static gboolean default_directory_manual_layout = FALSE;

static gboolean
get_default_directory_manual_layout (void)
{
	static gboolean auto_storaged_added = FALSE;
	
	if (auto_storaged_added == FALSE) {
		auto_storaged_added = TRUE;
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
						       &default_directory_manual_layout);
	}

	return default_directory_manual_layout;
}

static gboolean
fm_icon_view_get_directory_auto_layout (FMIconView *icon_view,
					NautilusFile *file)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, icon_view,
		 get_directory_auto_layout, (icon_view, file));
}

static gboolean
fm_icon_view_real_get_directory_auto_layout (FMIconView *icon_view,
					     NautilusFile *file)
{
	return nautilus_file_get_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, !get_default_directory_manual_layout ());
}

static void
fm_icon_view_set_directory_auto_layout (FMIconView *icon_view,
					NautilusFile *file,
					gboolean auto_layout)
{
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
			       set_directory_auto_layout, (icon_view, file, auto_layout));
}

static void
fm_icon_view_real_set_directory_auto_layout (FMIconView *icon_view,
					     NautilusFile *file,
					     gboolean auto_layout)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
		 !get_default_directory_manual_layout (),
		 auto_layout);
}
/* maintainence of tighter layout boolean */

static gboolean
fm_icon_view_get_directory_tighter_layout (FMIconView *icon_view,
					   NautilusFile *file)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_ICON_VIEW_CLASS, icon_view,
		 get_directory_tighter_layout, (icon_view, file));
}

static gboolean default_directory_tighter_layout = FALSE;

static gboolean
get_default_directory_tighter_layout (void)
{
	static gboolean auto_storaged_added = FALSE;
	
	if (auto_storaged_added == FALSE) {
		auto_storaged_added = TRUE;
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
						       &default_directory_tighter_layout);
	}

	return default_directory_tighter_layout;
}

static gboolean
fm_icon_view_real_get_directory_tighter_layout (FMIconView *icon_view,
						NautilusFile *file)
{
	return nautilus_file_get_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
		 get_default_directory_tighter_layout ());
}

static void
fm_icon_view_set_directory_tighter_layout (FMIconView *icon_view,
					   NautilusFile *file,
					   gboolean tighter_layout)
{
	EEL_CALL_METHOD (FM_ICON_VIEW_CLASS, icon_view,
			 set_directory_tighter_layout, (icon_view, file, tighter_layout));
}

static void
fm_icon_view_real_set_directory_tighter_layout (FMIconView *icon_view,
						NautilusFile *file,
						gboolean tighter_layout)
{
	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
		 get_default_directory_tighter_layout (),
		 tighter_layout);
}

static gboolean
real_supports_auto_layout (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return TRUE;
}

static gboolean
real_supports_keep_aligned (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return FALSE;
}

static gboolean
real_supports_labels_beside_icons (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), TRUE);

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
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (strcmp (sort_criteria[i].metadata_text, metadata_text) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_id (const char *id)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (strcmp (sort_criteria[i].id, id) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_sort_type (NautilusFileSortType sort_type)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (sort_type == sort_criteria[i].sort_type) {
			return &sort_criteria[i];
		}
	}

	return NULL;
}

static NautilusZoomLevel default_zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;

static NautilusZoomLevel
get_default_zoom_level (void)
{
	static gboolean auto_storage_added = FALSE;

	if (!auto_storage_added) {
		auto_storage_added = TRUE;
		eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
					       (int *) &default_zoom_level);
	}

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
set_labels_beside_icons (FMIconView *icon_view)
{
	gboolean labels_beside;

	if (fm_icon_view_supports_labels_beside_icons (icon_view)) {
		labels_beside = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS);
		
		if (labels_beside) {
			nautilus_icon_container_set_label_position
				(get_icon_container (icon_view), 
				 NAUTILUS_ICON_LABEL_POSITION_BESIDE);
		} else {
			nautilus_icon_container_set_label_position
				(get_icon_container (icon_view), 
				 NAUTILUS_ICON_LABEL_POSITION_UNDER);
		}
	}
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

	icon_view->details->loading = TRUE;

	/* kill any sound preview process that is ongoing */
	preview_audio (icon_view, NULL, FALSE);

	/* FIXME bugzilla.gnome.org 45060: Should use methods instead
	 * of hardcoding desktop knowledge in here.
	 */
	if (FM_IS_DESKTOP_ICON_VIEW (view)) {
		nautilus_connect_desktop_background_to_file_metadata (NAUTILUS_ICON_CONTAINER (icon_container), file);
	} else {
		nautilus_connect_background_to_file_metadata (icon_container, file);
	}

	
	/* Set up the zoom level from the metadata. */
	if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (icon_view))) {
		level = nautilus_file_get_integer_metadata
			(file, 
			 NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
			 get_default_zoom_level ());
		fm_icon_view_set_zoom_level (icon_view, level, TRUE);
	}

	/* Set the sort mode.
	 * It's OK not to resort the icons because the
	 * container doesn't have any icons at this point.
	 */
	sort_name = fm_icon_view_get_directory_sort_by (icon_view, file);
	set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name));
	g_free (sort_name);

	/* Set the sort direction from the metadata. */
	set_sort_reversed (icon_view, fm_icon_view_get_directory_sort_reversed (icon_view, file));

	nautilus_icon_container_set_keep_aligned
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_keep_aligned (icon_view, file));
	nautilus_icon_container_set_tighter_layout
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_tighter_layout (icon_view, file));

	set_labels_beside_icons (icon_view);

	/* We must set auto-layout last, because it invokes the layout_changed 
	 * callback, which works incorrectly if the other layout criteria are
	 * not already set up properly (see bug 6500, e.g.)
	 */
	nautilus_icon_container_set_auto_layout
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_auto_layout (icon_view, file));
}

static void
fm_icon_view_end_loading (FMDirectoryView *view)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (view);

	icon_view->details->loading = FALSE;
}

static void
fm_icon_view_update_font_size_table (FMIconView *view)
{
	NautilusIconContainer *container;
	int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];

	container = get_icon_container (view);
	g_assert (container != NULL);

	switch (get_default_zoom_level ())
	{
	case NAUTILUS_ZOOM_LEVEL_LARGEST:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -5 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    = -4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  0 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_LARGER:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  2 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_LARGE:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  4 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_STANDARD:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  4 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_SMALL:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  5 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_SMALLER:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  5 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  5 * PANGO_SCALE;
		break;
	case NAUTILUS_ZOOM_LEVEL_SMALLEST:
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  5 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  5 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  6 * PANGO_SCALE;
		break;
	default:
		g_warning ("invalid default list-view zoom level");
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER]  = -3 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL]    = -2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] =  0 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE]    =  2 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER]   =  4 * PANGO_SCALE;
		font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST]  =  4 * PANGO_SCALE;
		break;
	}

	nautilus_icon_container_set_font_size_table (container, font_size_table);
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
		 get_default_zoom_level (),
		 new_level);

	nautilus_icon_container_set_zoom_level (icon_container, new_level);
	fm_directory_view_set_zoom_level (&view->parent, new_level);

	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
fm_icon_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
	FMIconView *icon_view;
	NautilusZoomLevel new_level;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	new_level = fm_icon_view_get_zoom_level (icon_view) + zoom_increment;

	if (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST) {
		fm_icon_view_set_zoom_level(icon_view, new_level, FALSE);
	}
}

static void
fm_icon_view_zoom_to_level (FMDirectoryView *view, int zoom_level)
{
	FMIconView *icon_view;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	fm_icon_view_set_zoom_level (icon_view, zoom_level, FALSE);
}

static void
fm_icon_view_restore_default_zoom_level (FMDirectoryView *view)
{
	FMIconView *icon_view;

	g_return_if_fail (FM_IS_ICON_VIEW (view));

	icon_view = FM_ICON_VIEW (view);
	fm_icon_view_set_zoom_level
		(icon_view, get_default_zoom_level (), FALSE);
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
	const SortCriterion *sort;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (id != NULL);

	sort = get_sort_criterion_by_id (id);
	g_return_if_fail (sort != NULL);
	
	if (sort == icon_view->details->sort
	    && fm_icon_view_using_auto_layout (icon_view)) {
		return;
	}

	set_sort_criterion (icon_view, sort);
	nautilus_icon_container_sort (get_icon_container (icon_view));
}

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

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	if (set_sort_reversed (icon_view, strcmp (state, "1") == 0)) {
		nautilus_icon_container_sort (get_icon_container (icon_view));
	}
}

static void
keep_aligned_state_changed_callback (BonoboUIComponent *component,
				     const char        *path,
				     Bonobo_UIComponent_EventType type,
				     const char        *state,
				     gpointer          user_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	gboolean keep_aligned;

	g_assert (strcmp (path, ID_KEEP_ALIGNED) == 0);

	icon_view = FM_ICON_VIEW (user_data);

	if (strcmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	keep_aligned = strcmp (state, "1") == 0 ? TRUE : FALSE;

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
	fm_icon_view_set_directory_keep_aligned (icon_view,
						 file,
						 keep_aligned);
						      
	nautilus_icon_container_set_keep_aligned (get_icon_container (icon_view),
						  keep_aligned);
}

static void
switch_to_manual_layout (FMIconView *icon_view)
{
	if (!fm_icon_view_using_auto_layout (icon_view)) {
		return;
	}

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

static gboolean
fm_icon_view_can_rename_file (FMDirectoryView *view, NautilusFile *file)
{
	if (!(fm_icon_view_get_zoom_level (FM_ICON_VIEW (view)) > NAUTILUS_ZOOM_LEVEL_SMALLEST)) {
		return FALSE;
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE (
		FM_DIRECTORY_VIEW_CLASS, can_rename_file, (view, file));
}

static void
fm_icon_view_start_renaming_file (FMDirectoryView *view, NautilusFile *file)
{
	/* call parent class to make sure the right icon is selected */
	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, start_renaming_file, (view, file));
	
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
	Bonobo_UIContainer ui_container;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Stretch", show_stretch_handles_callback),
		BONOBO_UI_VERB ("Unstretch", unstretch_icons_callback),
		BONOBO_UI_VERB ("Clean Up", clean_up_callback),
		BONOBO_UI_VERB_END
	};
	
        g_assert (FM_IS_ICON_VIEW (view));

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

	icon_view = FM_ICON_VIEW (view);

	icon_view->details->ui = bonobo_ui_component_new ("Icon View");
	g_signal_connect_object (icon_view->details->ui,
				 "ui_event", G_CALLBACK (handle_ui_event), icon_view, 0);
	ui_container = fm_directory_view_get_bonobo_ui_container (view);
	bonobo_ui_component_set_container (icon_view->details->ui,
					   ui_container, NULL);
	bonobo_object_release_unref (ui_container, NULL);
	bonobo_ui_util_set_ui (icon_view->details->ui,
			       DATADIR,
			       "nautilus-icon-view-ui.xml",
			       "nautilus", NULL);

	bonobo_ui_component_add_verb_list_with_data (icon_view->details->ui, verbs, view);
	
	bonobo_ui_component_add_listener (icon_view->details->ui, ID_TIGHTER_LAYOUT, tighter_layout_state_changed_callback, view);
	bonobo_ui_component_add_listener (icon_view->details->ui, ID_SORT_REVERSED, sort_reversed_state_changed_callback, view);
	bonobo_ui_component_add_listener (icon_view->details->ui, ID_KEEP_ALIGNED, keep_aligned_state_changed_callback, view);
	icon_view->details->menus_ready = TRUE;

	bonobo_ui_component_freeze (icon_view->details->ui, NULL);
	
	/* Do one-time state-setting here; context-dependent state-setting
	 * is done in update_menus.
	 */
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		nautilus_bonobo_set_hidden 
			(icon_view->details->ui, POPUP_PATH_LAY_OUT, TRUE);
	}

	nautilus_bonobo_set_hidden
		(icon_view->details->ui, POPUP_PATH_ICON_APPEARANCE, TRUE);

	nautilus_bonobo_set_hidden
		(icon_view->details->ui, POPUP_PATH_ICON_APPEARANCE, 
		 !FM_IS_DESKTOP_ICON_VIEW (view));

	update_layout_menus (icon_view);

	bonobo_ui_component_thaw (icon_view->details->ui, NULL);
}

static void
fm_icon_view_update_menus (FMDirectoryView *view)
{
	FMIconView *icon_view;
        GList *selection;
        int selection_count;
        NautilusIconContainer *icon_container;

        icon_view = FM_ICON_VIEW (view);

	/* don't update if the menus aren't ready */
	if (!icon_view->details->menus_ready) {
		return;
	}

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));

	/* don't update if we have no remote BonoboUIContainer */
	if (bonobo_ui_component_get_container (icon_view->details->ui)
	    == CORBA_OBJECT_NIL) {
		return;
	}

        selection = fm_directory_view_get_selection (view);
        selection_count = g_list_length (selection);
        icon_container = get_icon_container (icon_view);

	bonobo_ui_component_freeze (icon_view->details->ui, NULL);

	nautilus_bonobo_set_sensitive (icon_view->details->ui, 
				       COMMAND_STRETCH_ICON,
				       selection_count == 1
				       && icon_container != NULL
			    	       && !nautilus_icon_container_has_stretch_handles (icon_container));

	nautilus_bonobo_set_label
		(icon_view->details->ui,
		 COMMAND_UNSTRETCH_ICONS,
		 eel_g_list_more_than_one_item (selection)
		 	? _("Restore Icons' Original Si_zes")
		 	: _("Restore Icon's Original Si_ze"));
	nautilus_bonobo_set_sensitive (icon_view->details->ui, 
				       COMMAND_UNSTRETCH_ICONS,
				       icon_container != NULL
			    	       && nautilus_icon_container_is_stretched (icon_container));

	bonobo_ui_component_thaw (icon_view->details->ui, NULL);
	
	nautilus_file_list_free (selection);
}

static void
fm_icon_view_reset_to_defaults (FMDirectoryView *view)
{
	NautilusIconContainer *icon_container;
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (view);
	icon_container = get_icon_container (icon_view);

	set_sort_criterion (icon_view, get_sort_criterion_by_sort_type (get_default_sort_order ()));
	set_sort_reversed (icon_view, get_default_sort_in_reverse_order ());
	nautilus_icon_container_set_keep_aligned 
		(icon_container, get_default_directory_keep_aligned ());
	nautilus_icon_container_set_tighter_layout
		(icon_container, get_default_directory_tighter_layout ());

	nautilus_icon_container_sort (icon_container);

	/* Switch to manual layout of the default calls for it.
	 * This needs to happen last for the sort order menus
	 * to be in sync.
	 */
 	if (get_default_directory_manual_layout ()) {
 		switch_to_manual_layout (icon_view);
 	}

	update_layout_menus (icon_view);

	fm_icon_view_restore_default_zoom_level (view);
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

static GArray *
fm_icon_view_get_selected_icon_locations (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

	return nautilus_icon_container_get_selected_icon_locations
		(get_icon_container (FM_ICON_VIEW (view)));
}


static void
fm_icon_view_set_selection (FMDirectoryView *view, GList *selection)
{
	g_return_if_fail (FM_IS_ICON_VIEW (view));

	nautilus_icon_container_set_selection
		(get_icon_container (FM_ICON_VIEW (view)), selection);
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

/* handle the preview signal by inspecting the mime type.  For now, we only preview local sound files. */

/* here's the timer task that actually plays the file using mpg123, ogg123 or play. */
/* FIXME bugzilla.gnome.org 41258: we should get the application from our mime-type stuff */
static gboolean
play_file (gpointer callback_data)
{
#if USE_OLD_AUDIO_PREVIEW	
	NautilusFile *file;
	FMIconView *icon_view;
	FILE *sound_process;
	char *file_uri;
	char *suffix;
	char *mime_type;
	const char *command_str;
	gboolean is_mp3;
	gboolean is_ogg;
	pid_t mp3_pid;
	
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *buffer;
	GnomeVFSFileSize bytes_read;

	icon_view = FM_ICON_VIEW (callback_data);
	
	file = icon_view->details->audio_preview_file;
	file_uri = nautilus_file_get_uri (file);
	mime_type = nautilus_file_get_mime_type (file);
	is_mp3 = eel_strcasecmp (mime_type, "audio/mpeg") == 0;
	is_ogg = eel_strcasecmp (mime_type, "application/x-ogg") == 0;

	mp3_pid = fork ();
	if (mp3_pid == (pid_t) 0) {
		/* Set the group (session) id to this process for future killing. */
		setsid();
		if (is_mp3) {
			command_str = "mpg123 -y -q -";
		} else if (is_ogg) {
			command_str = "ogg123 -q -";
		} else {
			suffix = strrchr(file_uri, '.');
			if (suffix == NULL) {
				suffix = "wav";
			} else {
				suffix += 1; /* skip the period */
			}
			command_str = g_strdup_printf("play -t %s -", suffix);
		}

		/* read the file with gnome-vfs, feeding it to the sound player's standard input */
		/* First, open the file. */
		result = gnome_vfs_open (&handle, file_uri, GNOME_VFS_OPEN_READ);
		if (result != GNOME_VFS_OK) {
			_exit (0);
		}
			
		/* since the uri could be local or remote, we launch the sound player with popen and feed it
		 * the data by fetching it with gnome_vfs
		 */
		sound_process = popen(command_str, "w");
		if (sound_process == 0) {
			/* Close the file. */
			result = gnome_vfs_close (handle);			
			_exit (0);
		}
			
		/* allocate a buffer. */
		buffer = g_malloc(READ_CHUNK_SIZE);
			
		/* read and write a chunk at a time, until we're done */
		do {
			result = gnome_vfs_read (handle,
					buffer,
					READ_CHUNK_SIZE,
					&bytes_read);
			if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
				g_free (buffer);
				gnome_vfs_close (handle);
				break;
			}

			/* pass the data the buffer to the sound process by writing to it */
			fwrite(buffer, 1, bytes_read, sound_process);

		} while (result == GNOME_VFS_OK);

		/* Close the file. */
		result = gnome_vfs_close (handle);			
		g_free(buffer);
		pclose(sound_process);
		_exit (0);
	} else if (mp3_pid > (pid_t) 0) {
		nautilus_sound_register_sound (mp3_pid);
	}
		
	g_free (file_uri);
	g_free (mime_type);

	icon_view->details->audio_preview_timeout = 0;
#else
	char *file_path, *file_uri, *mime_type;
	gboolean is_mp3;
	FMIconView *icon_view;
	
	icon_view = FM_ICON_VIEW (callback_data);
		
	file_uri = nautilus_file_get_uri (icon_view->details->audio_preview_file);
	file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	mime_type = nautilus_file_get_mime_type (icon_view->details->audio_preview_file);

	is_mp3 = eel_strcasecmp (mime_type, "audio/mpeg") == 0;

	if (file_path != NULL && !is_mp3) {
		icon_view->details->audio_player_data = nautilus_audio_player_play (file_path);
	}
	
	g_free (file_uri);
	g_free (file_path);	
	g_free (mime_type);

	icon_view->details->audio_preview_timeout = 0;
	icon_view->details->audio_preview_file = NULL;
#endif
	return FALSE;
}

/* FIXME bugzilla.gnome.org 42530: Hardcoding this here sucks. We should be using components
 * for open ended things like this.
 */

/* this routine is invoked from the preview signal handler to preview a sound file.  We
   want to wait a suitable delay until we actually do it, so set up a timer task to actually
   start playing.  If we move out before the task files, we remove it. */

static void
preview_audio (FMIconView *icon_view, NautilusFile *file, gboolean start_flag)
{		
	/* Stop current audio playback */
#if USE_OLD_AUDIO_PREVIEW
	nautilus_sound_kill_sound ();
#else
	if (icon_view->details->audio_player_data != NULL) {
		nautilus_audio_player_stop (icon_view->details->audio_player_data);
		g_free (icon_view->details->audio_player_data);
		icon_view->details->audio_player_data = NULL;
	}
#endif
	if (icon_view->details->audio_preview_timeout != 0) {
		g_source_remove (icon_view->details->audio_preview_timeout);
		icon_view->details->audio_preview_timeout = 0;
	}
			
	if (start_flag) {
		icon_view->details->audio_preview_file = file;
#if USE_OLD_AUDIO_PREVIEW			
		icon_view->details->audio_preview_timeout = g_timeout_add (1000, play_file, icon_view);
#else
		/* FIXME: Need to kill the existing timeout if there is one? */
		icon_view->details->audio_preview_timeout = g_timeout_add (1000, play_file, icon_view);
#endif
	}
}

static gboolean
should_preview_sound (NautilusFile *file)
{
	/* Check gnome config sound preference */
	if (!gnome_esd_enabled_auto_value) {
		return FALSE;
	}

	/* Check user performance preference */	
	if (preview_sound_auto_value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}
		
	if (preview_sound_auto_value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	return nautilus_file_is_local (file);
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
	if (should_preview_sound (file)) {
		mime_type = nautilus_file_get_mime_type (file);

		if ((eel_istr_has_prefix (mime_type, "audio/")
		     || eel_istr_has_prefix (mime_type, "application/x-ogg"))
		    && eel_strcasecmp (mime_type, "audio/x-pn-realaudio") != 0
		    && eel_strcasecmp (mime_type, "audio/x-mpegurl") != 0
		    && nautilus_sound_can_play_sound ()) {
			result = 1;
			preview_audio (icon_view, file, start_flag);
		}	
		g_free (mime_type);
	}
	
	/* Display file name in status area at low zoom levels, since
	 * the name is not displayed or hard to read in the icon view.
	 */
	if (fm_icon_view_get_zoom_level (icon_view) <= NAUTILUS_ZOOM_LEVEL_SMALLER) {
		if (start_flag) {
			file_name = nautilus_file_get_display_name (file);
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

static void
renaming_icon_callback (NautilusIconContainer *container,
			GtkWidget *widget,
			gpointer callback_data)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (callback_data);
	nautilus_clipboard_set_up_editable_in_control
		(GTK_EDITABLE (widget),
		 fm_directory_view_get_bonobo_control (directory_view),
		 FALSE);
}

int
fm_icon_view_compare_files (FMIconView   *icon_view,
			    NautilusFile *a,
			    NautilusFile *b)
{
	return nautilus_file_compare_for_sort
		(a, b, icon_view->details->sort->sort_type,
		 /* Use type-unsafe cast for performance */
		 fm_directory_view_should_sort_directories_first ((FMDirectoryView *)icon_view),
		 icon_view->details->sort_reversed);
}

void
fm_icon_view_filter_by_screen (FMIconView *icon_view,
			       gboolean filter)
{
	icon_view->details->filter_by_screen = filter;
	icon_view->details->num_screens = gdk_display_get_n_screens (gtk_widget_get_display (GTK_WIDGET (icon_view)));
}

static void
fm_icon_view_screen_changed (GtkWidget *widget,
			     GdkScreen *previous_screen)
{
	FMDirectoryView *view;
	GList *files, *l;
	NautilusFile *file;
	NautilusIconContainer *icon_container;

	if (GTK_WIDGET_CLASS (parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (parent_class)->screen_changed (widget, previous_screen);
	}

	view = FM_DIRECTORY_VIEW (widget);
	if (FM_ICON_VIEW (view)->details->filter_by_screen) {
		icon_container = get_icon_container (FM_ICON_VIEW (view));
		
		files = nautilus_directory_get_file_list (fm_directory_view_get_model (view));

		for (l = files; l != NULL; l = l->next) {
			file = l->data;
			
			if (!should_show_file_on_screen (view, file)) {
				fm_icon_view_remove_file (view, file);
			} else {
				if (nautilus_icon_container_add (icon_container,
								 NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
					nautilus_file_ref (file);
				}
			}
		}
		
		nautilus_file_list_unref (files);
		g_list_free (files);
	}
}


static int
compare_files_cover (gconstpointer a, gconstpointer b, gpointer callback_data)
{
	return fm_icon_view_compare_files (callback_data,
					   NAUTILUS_FILE (a),
					   NAUTILUS_FILE (b));
}

static void
fm_icon_view_sort_files (FMDirectoryView *view, GList **files)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (view);
	if (!fm_icon_view_using_auto_layout (icon_view)) {
		return;
	}
	*files = g_list_sort_with_data (*files, compare_files_cover, icon_view);
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
						 GdkEventButton *event,
						 FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	fm_directory_view_pop_up_selection_context_menu 
		(FM_DIRECTORY_VIEW (icon_view), event);
}

static void
icon_container_context_click_background_callback (NautilusIconContainer *container,
						  GdkEventButton *event,
						  FMIconView *icon_view)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	fm_directory_view_pop_up_background_context_menu 
		(FM_DIRECTORY_VIEW (icon_view), event);
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
	locale = g_strdup (setlocale (LC_NUMERIC, NULL));	
	setlocale (LC_NUMERIC, "C");

	/* Schedule updating menus for the next idle. Doing it directly here
	 * noticeably slows down icon stretching.  The other work here to
	 * store the icon position and scale does not seem to noticeably
	 * slow down icon stretching. It would be trickier to move to an
	 * idle call, because we'd have to keep track of potentially multiple
	 * sets of file/geometry info.
	 */
	if (icon_view->details->react_to_icon_change_idle_id == 0) {
                icon_view->details->react_to_icon_change_idle_id
                        = g_idle_add (fm_icon_view_react_to_icon_change_idle_callback,
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

	/* FIXME bugzilla.gnome.org 40662: 
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
	g_free (locale);
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
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), NULL);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	g_return_val_if_fail (FM_IS_ICON_VIEW (icon_view), NULL);

	return nautilus_file_get_drop_target_uri (file);
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
fm_icon_view_click_policy_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));

	fm_icon_view_update_click_mode (FM_ICON_VIEW (directory_view));
}

static void
fm_icon_view_emblems_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_ICON_VIEW (directory_view));
	
	nautilus_icon_container_request_update_all (get_icon_container (FM_ICON_VIEW (directory_view)));	
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	char *sort_name;
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
	sort_name = fm_icon_view_get_directory_sort_by (icon_view, file);
	set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name));
	g_free (sort_name);

	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (icon_container));

	nautilus_icon_container_request_update_all (icon_container);
}

static void
default_sort_in_reverse_order_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
	set_sort_reversed (icon_view, fm_icon_view_get_directory_sort_reversed (icon_view, file));
	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (icon_container));

	nautilus_icon_container_request_update_all (icon_container);
}

static void
default_use_tighter_layout_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (icon_container));

	nautilus_icon_container_set_tighter_layout (
		icon_container,
		fm_icon_view_get_directory_tighter_layout (icon_view, file));

	nautilus_icon_container_request_update_all (icon_container);
}

static void
default_use_manual_layout_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	NautilusIconContainer *icon_container;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (icon_container));

	nautilus_icon_container_set_auto_layout (
		icon_container,
		fm_icon_view_get_directory_auto_layout (icon_view, file));

	nautilus_icon_container_request_update_all (icon_container);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;
	NautilusFile *file;
	int level;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (icon_view))) {
		file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (icon_view));
		
		level = nautilus_file_get_integer_metadata (file, 
							    NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
							    get_default_zoom_level ());
		fm_icon_view_update_font_size_table (icon_view);
		fm_icon_view_set_zoom_level (icon_view, level, TRUE);
	}
}

static void
labels_beside_icons_changed_callback (gpointer callback_data)
{
	FMIconView *icon_view;

	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	icon_view = FM_ICON_VIEW (callback_data);

	set_labels_beside_icons (icon_view);
}

static void
fm_icon_view_sort_directories_first_changed (FMDirectoryView *directory_view)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (directory_view);

	if (fm_icon_view_using_auto_layout (icon_view)) {
		nautilus_icon_container_sort 
			(get_icon_container (icon_view));
	}
}

/* GtkObject methods. */

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
			   GArray *relative_item_points,
			   const char *target_dir,
			   int copy_action,
			   int x, int y,
			   FMDirectoryView *view)
{
	fm_directory_view_move_copy_items (item_uris, relative_item_points, target_dir,
		copy_action, x, y, view);
}

static void
fm_icon_view_update_click_mode (FMIconView *icon_view)
{
	NautilusIconContainer	*icon_container;
	int			click_mode;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	click_mode = eel_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY);

	nautilus_icon_container_set_single_click_mode (icon_container,
						       click_mode == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
create_icon_container (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;

	icon_container = fm_icon_container_new (icon_view);

	GTK_WIDGET_SET_FLAGS (icon_container, GTK_CAN_FOCUS);
	
	g_signal_connect_object (icon_container, "activate",	
			 G_CALLBACK (icon_container_activate_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "band_select_started",
				 G_CALLBACK (band_select_started_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "band_select_ended",
				 G_CALLBACK (band_select_ended_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "context_click_selection",
				 G_CALLBACK (icon_container_context_click_selection_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "context_click_background",
				 G_CALLBACK (icon_container_context_click_background_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_position_changed",
				 G_CALLBACK (icon_position_changed_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_text_changed",
				 G_CALLBACK (fm_icon_view_icon_text_changed_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "selection_changed",
				 G_CALLBACK (selection_changed_callback), icon_view, 0);
	/* FIXME: many of these should move into fm-icon-container as virtual methods */
	g_signal_connect_object (icon_container, "get_icon_uri",
				 G_CALLBACK (get_icon_uri_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "get_icon_drop_target_uri",
				 G_CALLBACK (get_icon_drop_target_uri_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "move_copy_items",
				 G_CALLBACK (icon_view_move_copy_items), icon_view, 0);
	g_signal_connect_object (icon_container, "get_container_uri",
				 G_CALLBACK (icon_view_get_container_uri), icon_view, 0);
	g_signal_connect_object (icon_container, "can_accept_item",
				 G_CALLBACK (icon_view_can_accept_item), icon_view, 0);
	g_signal_connect_object (icon_container, "get_stored_icon_position",
				 G_CALLBACK (get_stored_icon_position_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "layout_changed",
				 G_CALLBACK (layout_changed_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "preview",
				 G_CALLBACK (icon_container_preview_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "renaming_icon",
				 G_CALLBACK (renaming_icon_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_stretch_started",
				 G_CALLBACK (fm_directory_view_update_menus), icon_view,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (icon_container, "icon_stretch_ended",
				 G_CALLBACK (fm_directory_view_update_menus), icon_view,
				 G_CONNECT_SWAPPED);

	gtk_container_add (GTK_CONTAINER (icon_view),
			   GTK_WIDGET (icon_container));

	fm_icon_view_update_click_mode (icon_view);
	fm_icon_view_update_font_size_table (icon_view);

	gtk_widget_show (GTK_WIDGET (icon_container));
}

static void
icon_view_handle_uri_list (NautilusIconContainer *container, const char *item_uris,
			   GdkDragAction action, int x, int y, FMIconView *view)
{

	GList *uri_list;
	GList *node, *real_uri_list = NULL;
	GnomeDesktopItem *entry;
	GdkPoint point;
	char *uri;
	char *path;
	char *stripped_uri;
	char *container_uri;
	char *mime_type;
	const char *last_slash, *link_name;
	int n_uris;
	gboolean all_local;
	GArray *points;
	GdkScreen *screen;
	int screen_num;

	if (item_uris == NULL) {
		return;
	}

	container_uri = fm_directory_view_get_backing_uri (FM_DIRECTORY_VIEW (view));
	g_return_if_fail (container_uri != NULL);

	if (eel_vfs_has_capability (container_uri,
				    EEL_VFS_CAPABILITY_IS_REMOTE_AND_SLOW)) {
		eel_show_warning_dialog (_("Drag and drop is only supported to local file systems."),
					 _("Drag and Drop error"),
					 fm_directory_view_get_containing_window (FM_DIRECTORY_VIEW (view)));
		g_free (container_uri);
		return;
	}

	if (action == GDK_ACTION_ASK) {
		action = nautilus_drag_drop_action_ask 
			(GTK_WIDGET (container),
			 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	}
	
	/* We don't support GDK_ACTION_ASK or GDK_ACTION_PRIVATE
	 * and we don't support combinations either. */
	if ((action != GDK_ACTION_DEFAULT) &&
	    (action != GDK_ACTION_COPY) &&
	    (action != GDK_ACTION_MOVE) &&
	    (action != GDK_ACTION_LINK)) {
		eel_show_warning_dialog (_("An invalid drag type was used."),
					 _("Drag and Drop error"),
					 fm_directory_view_get_containing_window (FM_DIRECTORY_VIEW (view)));
		g_free (container_uri);
		return;
	}

	point.x = x;
	point.y = y;

	screen = gtk_widget_get_screen (GTK_WIDGET (view));
	screen_num = gdk_screen_get_number (screen);
		
	/* Most of what comes in here is not really URIs, but rather paths that
	 * have a file: prefix in them.  We try to sanitize the uri list as a
	 * result.  Additionally, if they are all local files, then we can copy
	 * them.  Otherwise, we just make links.
	 */
	all_local = TRUE;
	n_uris = 0;
	uri_list = nautilus_icon_dnd_uri_list_extract_uris (item_uris);
	for (node = uri_list; node != NULL; node = node->next) {
		gchar *sanitized_uri;

		sanitized_uri = eel_make_uri_from_half_baked_uri (node->data);
		if (sanitized_uri == NULL)
			continue;
		real_uri_list = g_list_append (real_uri_list, sanitized_uri);
		if (eel_vfs_has_capability (sanitized_uri,
					    EEL_VFS_CAPABILITY_IS_REMOTE_AND_SLOW)) {
			all_local = FALSE;
		}
		n_uris++;
	}
	nautilus_icon_dnd_uri_list_free_strings (uri_list);

	if (all_local == TRUE &&
	    (action == GDK_ACTION_COPY ||
	     action == GDK_ACTION_MOVE)) {
		/* Copying files */
		if (n_uris == 1) {
			GdkPoint tmp_point = { 0, 0 };

			/* pass in a 1-item array of icon positions, relative to x, y */
			points = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
			g_array_append_val (points, tmp_point);
		} else {
			points = NULL;
		}
		fm_directory_view_move_copy_items (real_uri_list, points,
						   container_uri,
						   action, x, y, FM_DIRECTORY_VIEW (view));
		
		if (points)
			g_array_free (points, TRUE);
	} else {
		for (node = real_uri_list; node != NULL; node = node->next) {
			/* Make a link using the desktop file contents? */
			uri = node->data;
			path = gnome_vfs_get_local_path_from_uri (uri);

			if (path != NULL) {
				mime_type = gnome_vfs_get_mime_type (uri);

				if (mime_type != NULL &&
				    strcmp (mime_type, "application/x-gnome-app-info") == 0) {
					entry = gnome_desktop_item_new_from_file (path,
							GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
							NULL);
				} else {
					entry = NULL;
				}

				g_free (mime_type);
				g_free (path);
			} else {
				entry = NULL;
			}

			if (entry != NULL) {
				/* FIXME: Handle name conflicts? */
				nautilus_link_local_create_from_gnome_entry (entry, container_uri, &point, screen_num);

				gnome_desktop_item_unref (entry);
				continue;
			}

			/* Make a link from the URI alone. Generate the file
			 * name by extracting the basename of the URI.
			 */
			/* FIXME: This should be using eel_uri_get_basename
			 * instead of a "roll our own" solution.
			 */
			stripped_uri = eel_str_strip_trailing_chr (uri, '/');
			last_slash = strrchr (stripped_uri, '/');
			link_name = last_slash == NULL ? NULL : last_slash + 1;
			
			if (!eel_str_is_empty (link_name)) {
				/* FIXME: Handle name conflicts? */
				nautilus_link_local_create (container_uri, link_name,
							    NULL, uri,
							    &point, screen_num,
							    NAUTILUS_LINK_GENERIC);
			}
			
			g_free (stripped_uri);
			break;
		}
	}
	
	nautilus_icon_dnd_uri_list_free_strings (real_uri_list);

	g_free (container_uri);
	
}

static char *
icon_view_get_first_visible_file_callback (NautilusScrollPositionable *positionable,
					   FMIconView *icon_view)
{
	NautilusFile *file;

	file = NAUTILUS_FILE (nautilus_icon_container_get_first_visible_icon (get_icon_container (icon_view)));

	if (file) {
		return nautilus_file_get_uri (file);
	}
	
	return NULL;
}

static void
icon_view_scroll_to_file_callback (NautilusScrollPositionable *positionable,
				   const char *uri,
				   FMIconView *icon_view)
{
	NautilusFile *file;

	if (uri != NULL) {
		file = nautilus_file_get (uri);
		if (file != NULL) {
			nautilus_icon_container_scroll_to_icon (get_icon_container (icon_view),
								NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
			nautilus_file_unref (file);
		}
	}
}

static void
fm_icon_view_class_init (FMIconViewClass *klass)
{
	FMDirectoryViewClass *fm_directory_view_class;

	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = fm_icon_view_finalize;
	
	GTK_WIDGET_CLASS (klass)->screen_changed = fm_icon_view_screen_changed;
	
	fm_directory_view_class->add_file = fm_icon_view_add_file;
	fm_directory_view_class->flush_added_files = fm_icon_view_flush_added_files;
	fm_directory_view_class->begin_loading = fm_icon_view_begin_loading;
	fm_directory_view_class->bump_zoom_level = fm_icon_view_bump_zoom_level;
	fm_directory_view_class->can_rename_file = fm_icon_view_can_rename_file;
	fm_directory_view_class->can_zoom_in = fm_icon_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_icon_view_can_zoom_out;
	fm_directory_view_class->clear = fm_icon_view_clear;
	fm_directory_view_class->end_loading = fm_icon_view_end_loading;
	fm_directory_view_class->file_changed = fm_icon_view_file_changed;
	fm_directory_view_class->get_background_widget = fm_icon_view_get_background_widget;
	fm_directory_view_class->get_selected_icon_locations = fm_icon_view_get_selected_icon_locations;
	fm_directory_view_class->get_selection = fm_icon_view_get_selection;
	fm_directory_view_class->is_empty = fm_icon_view_is_empty;
	fm_directory_view_class->remove_file = fm_icon_view_remove_file;
	fm_directory_view_class->reset_to_defaults = fm_icon_view_reset_to_defaults;
	fm_directory_view_class->restore_default_zoom_level = fm_icon_view_restore_default_zoom_level;
	fm_directory_view_class->reveal_selection = fm_icon_view_reveal_selection;
	fm_directory_view_class->select_all = fm_icon_view_select_all;
	fm_directory_view_class->set_selection = fm_icon_view_set_selection;
	fm_directory_view_class->sort_files = fm_icon_view_sort_files;
	fm_directory_view_class->zoom_to_level = fm_icon_view_zoom_to_level;
        fm_directory_view_class->click_policy_changed = fm_icon_view_click_policy_changed;
        fm_directory_view_class->embedded_text_policy_changed = fm_icon_view_embedded_text_policy_changed;
        fm_directory_view_class->emblems_changed = fm_icon_view_emblems_changed;
        fm_directory_view_class->image_display_policy_changed = fm_icon_view_image_display_policy_changed;
        fm_directory_view_class->merge_menus = fm_icon_view_merge_menus;
        fm_directory_view_class->sort_directories_first_changed = fm_icon_view_sort_directories_first_changed;
        fm_directory_view_class->start_renaming_file = fm_icon_view_start_renaming_file;
        fm_directory_view_class->text_attribute_names_changed = fm_icon_view_text_attribute_names_changed;
        fm_directory_view_class->update_menus = fm_icon_view_update_menus;

	klass->clean_up = fm_icon_view_real_clean_up;
	klass->supports_auto_layout = real_supports_auto_layout;
	klass->supports_keep_aligned = real_supports_keep_aligned;
	klass->supports_labels_beside_icons = real_supports_labels_beside_icons;
        klass->get_directory_auto_layout = fm_icon_view_real_get_directory_auto_layout;
        klass->get_directory_sort_by = fm_icon_view_real_get_directory_sort_by;
        klass->get_directory_sort_reversed = fm_icon_view_real_get_directory_sort_reversed;
        klass->get_directory_tighter_layout = fm_icon_view_real_get_directory_tighter_layout;
        klass->set_directory_auto_layout = fm_icon_view_real_set_directory_auto_layout;
        klass->set_directory_sort_by = fm_icon_view_real_set_directory_sort_by;
        klass->set_directory_sort_reversed = fm_icon_view_real_set_directory_sort_reversed;
        klass->set_directory_tighter_layout = fm_icon_view_real_set_directory_tighter_layout;
}

static void
fm_icon_view_instance_init (FMIconView *icon_view)
{
	static gboolean setup_sound_preview = FALSE;
	NautilusView *nautilus_view;

        g_return_if_fail (GTK_BIN (icon_view)->child == NULL);

	icon_view->details = g_new0 (FMIconViewDetails, 1);
	icon_view->details->sort = &sort_criteria[0];
	icon_view->details->filter_by_screen = FALSE;

	create_icon_container (icon_view);

	icon_view->details->positionable = nautilus_scroll_positionable_new ();
	nautilus_view = fm_directory_view_get_nautilus_view (FM_DIRECTORY_VIEW (icon_view));
	bonobo_object_add_interface (BONOBO_OBJECT (nautilus_view),
				     BONOBO_OBJECT (icon_view->details->positionable));
	
	if (!setup_sound_preview) {
		eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_PREVIEW_SOUND,
					       &preview_sound_auto_value);

		eel_preferences_monitor_directory ("/desktop/gnome/sound");
		eel_preferences_add_auto_boolean ("/desktop/gnome/sound/enable_esd",
						  &gnome_esd_enabled_auto_value);
		
		setup_sound_preview = TRUE;
	}

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
						  default_sort_order_changed_callback,
						  icon_view, G_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
						  default_sort_in_reverse_order_changed_callback,
						  icon_view, G_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
						  default_use_tighter_layout_changed_callback,
						  icon_view, G_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
						  default_use_manual_layout_changed_callback,
						  icon_view, G_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
						  default_zoom_level_changed_callback,
						  icon_view, G_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS,
						  labels_beside_icons_changed_callback,
						  icon_view, G_OBJECT (icon_view));

	g_signal_connect_object (get_icon_container (icon_view), "handle_uri_list",
				 G_CALLBACK (icon_view_handle_uri_list), icon_view, 0);
	g_signal_connect_object (icon_view->details->positionable, "get_first_visible_file",
				 G_CALLBACK (icon_view_get_first_visible_file_callback), icon_view, 0);
	g_signal_connect_object (icon_view->details->positionable, "scroll_to_file",
				 G_CALLBACK (icon_view_scroll_to_file_callback), icon_view, 0);
}
