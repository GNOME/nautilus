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

#include "fm-desktop-icon-view.h"
#include "fm-error-reporting.h"
#include <bonobo/bonobo-ui-util.h>
#include <ctype.h>
#include <eel/eel-background.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-dnd.h>
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
#include <libgnome/gnome-mime.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libnautilus-private/nautilus-audio-player.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-container.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-sound.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-clipboard.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define USE_OLD_AUDIO_PREVIEW 1

/* Paths to use when creating & referring to Bonobo menu items */
#define MENU_PATH_RENAME 			"/menu/File/File Items Placeholder/Rename"
#define MENU_PATH_STRETCH_ICON 			"/menu/Edit/Edit Items Placeholder/Stretch"
#define MENU_PATH_UNSTRETCH_ICONS 		"/menu/Edit/Edit Items Placeholder/Unstretch"
#define MENU_PATH_LAY_OUT			"/menu/View/View Items Placeholder/Lay Out"
#define MENU_PATH_MANUAL_LAYOUT 		"/menu/View/View Items Placeholder/Lay Out/Manual Layout"
#define MENU_PATH_TIGHTER_LAYOUT 		"/menu/View/View Items Placeholder/Lay Out/Tighter Layout"
#define MENU_PATH_SORT_REVERSED			"/menu/View/View Items Placeholder/Lay Out/Reversed Order"
#define MENU_PATH_CLEAN_UP			"/menu/View/View Items Placeholder/Clean Up"

#define POPUP_PATH_LAY_OUT			"/popups/background/Before Zoom Items/View Items/Lay Out"

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

#define ICON_TEXT_ATTRIBUTES_NUM_ITEMS		3
#define ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS	"size,date_modified,type"

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

/* forward declarations */
static void                 create_icon_container                              (FMIconView            *icon_view);
static void                 fm_icon_view_initialize                            (FMIconView            *icon_view);
static void                 fm_icon_view_initialize_class                      (FMIconViewClass       *klass);
static void                 fm_icon_view_set_directory_sort_by                 (FMIconView            *icon_view,
										NautilusFile          *file,
										const char            *sort_by);
static void                 fm_icon_view_set_zoom_level                        (FMIconView            *view,
										NautilusZoomLevel      new_level,
										gboolean               always_set_level);
gboolean                    fm_icon_view_supports_auto_layout                  (FMIconView            *view);
static void                 fm_icon_view_update_icon_container_fonts           (FMIconView            *icon_view);
static void                 fm_icon_view_update_icon_container_smooth_font     (FMIconView            *icon_view);
static void                 fm_icon_view_update_icon_container_font_size_table (FMIconView            *icon_view);
static void                 fm_icon_view_update_click_mode                     (FMIconView            *icon_view);
static void                 fm_icon_view_update_smooth_graphics_mode           (FMIconView            *icon_view);
static gboolean             fm_icon_view_using_tighter_layout                  (FMIconView            *icon_view);
static gboolean             fm_icon_view_get_directory_tighter_layout          (FMIconView            *icon_view,
										NautilusFile          *file);
static void                 fm_icon_view_set_directory_tighter_layout          (FMIconView            *icon_view,
										NautilusFile          *file,
										gboolean               tighter_layout);
static gboolean             real_supports_auto_layout                          (FMIconView            *view);
static const SortCriterion *get_sort_criterion_by_id                           (const char            *id);
static const SortCriterion *get_sort_criterion_by_sort_type                    (NautilusFileSortType   sort_type);
static void                 set_sort_criterion_by_id                           (FMIconView            *icon_view,
										const char            *id);
static gboolean             set_sort_reversed                                  (FMIconView            *icon_view,
										gboolean               new_value);
static void                 switch_to_manual_layout                            (FMIconView            *view);
static void                 preview_audio                                      (FMIconView            *icon_view,
										NautilusFile          *file,
										gboolean               start_flag);
static void                 update_layout_menus                                (FMIconView            *view);
static void                 default_sort_in_reverse_order_changed_callback     (gpointer               callback_data);
static void                 default_sort_order_changed_callback                (gpointer               callback_data);
static void                 default_use_tighter_layout_changed_callback        (gpointer               callback_data);
static void                 default_use_manual_layout_changed_callback         (gpointer               callback_data);
static void                 default_zoom_level_changed_callback                (gpointer               callback_data);
static void                 default_zoom_level_font_size_changed_callback      (gpointer               callback_data);
static void                 font_changed_callback                              (gpointer               callback_data);
static void                 smooth_font_changed_callback                       (gpointer               callback_data);
static void                 icon_view_handle_uri_list                          (NautilusIconContainer *container,
										const char            *item_uris,
										GdkDragAction          action,
										int                    x,
										int                    y,
										FMIconView            *view);


static int preview_sound_auto_value;

EEL_DEFINE_CLASS_BOILERPLATE (FMIconView,
			      fm_icon_view,
			      FM_TYPE_DIRECTORY_VIEW)

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

struct FMIconViewDetails
{
	GList *icons_not_positioned;

	guint react_to_icon_change_idle_id;
	gboolean menus_ready;

	gboolean loading;

	const SortCriterion *sort;
	gboolean sort_reversed;

	BonoboUIComponent *ui;
	
	NautilusAudioPlayerData *audio_player_data;
	int audio_preview_timeout;
	NautilusFile *audio_preview_file;
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
	preview_audio (icon_view, NULL, FALSE);

	nautilus_file_list_free (icon_view->details->icons_not_positioned);
	g_free (icon_view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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
	locale = setlocale (LC_NUMERIC, "C");

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
rename_icon_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_ICON_VIEW (callback_data));
  		
	nautilus_icon_container_start_renaming_selected_item
		(get_icon_container (FM_ICON_VIEW (callback_data)));

	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (callback_data));
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
	FMIconView *icon_view;
	NautilusIconContainer *icon_container;

	icon_view = FM_ICON_VIEW (view);
	icon_container = get_icon_container (icon_view);
	
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
fm_icon_view_remove_file (FMDirectoryView *view, NautilusFile *file)
{
	if (nautilus_icon_container_remove (get_icon_container (FM_ICON_VIEW (view)),
					    NAUTILUS_ICON_CONTAINER_ICON_DATA (file))) {
		nautilus_file_unref (file);
	}
}

static void
fm_icon_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	nautilus_icon_container_request_update
		(get_icon_container (FM_ICON_VIEW (view)),
		 NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
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
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
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

static gboolean default_sort_in_reverse_order = FALSE;

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

gboolean
fm_icon_view_supports_auto_layout (FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
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
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < EEL_N_ELEMENTS (sort_criteria); i++) {
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
	for (i = 0; i < EEL_N_ELEMENTS (sort_criteria); i++) {
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
	for (i = 0; i < EEL_N_ELEMENTS (sort_criteria); i++) {
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
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
						       (int *) &default_zoom_level);
	}

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
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

	nautilus_icon_container_set_tighter_layout
		(get_icon_container (icon_view), 
		 fm_icon_view_get_directory_tighter_layout (icon_view, file));

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

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static EelStringList *
fm_icon_view_get_icon_text_attributes_from_preferences (void)
{
	static const EelStringList *attributes;

	if (attributes == NULL) {
		eel_preferences_add_auto_string_list (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
							   &attributes);
	}

	/* A simple check that the attributes list matches the expected length */
	g_return_val_if_fail (eel_string_list_get_length (attributes) == ICON_TEXT_ATTRIBUTES_NUM_ITEMS,
			      eel_string_list_new_from_tokens (ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS, ",", TRUE));


	/* We don't need to sanity check the attributes list even though it came
	 * from preferences.
	 *
	 * There are 2 ways that the values in the list could be bad.
	 *
	 * 1) The user picks "bad" values.  "bad" values are those that result in 
	 *    there being duplicate attributes in the list.
	 *
	 * 2) Value stored in GConf are tampered with.  Its possible physically do
	 *    this by pulling the rug underneath GConf and manually editing its
	 *    config files.  Its also possible to use a third party GConf key 
	 *    editor and store garbage for the keys in question.
	 *
	 * Thankfully, the Nautilus preferences machinery deals with both of 
	 * these cases.
	 *
	 * In the first case, the preferences dialog widgetry prevents
	 * duplicate attributes by making "bad" choices insensitive.
	 *
	 * In the second case, the preferences getter (and also the auto storage) for
	 * string_list values are always valid members of the enumeration associated
	 * with the preference.
	 *
	 * So, no more error checking on attributes is needed here and we can return
	 * a copy of the auto stored value.
	 */
	return eel_string_list_copy (attributes);
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
	EelStringList *attributes;
	char *result;
	int piece_count;

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
	
	attributes = fm_icon_view_get_icon_text_attributes_from_preferences ();
	g_return_val_if_fail ((guint)piece_count <= eel_string_list_get_length (attributes), NULL);
	
	result = eel_string_list_as_string (attributes, "|", piece_count);
	eel_string_list_free (attributes);

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

static void
fm_icon_view_start_renaming_item  (FMDirectoryView *view, const char *uri)
{
	/* call parent class to make sure the right icon is selected */
	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, start_renaming_item, (view, uri));
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
		BONOBO_UI_VERB ("Rename", rename_icon_callback),
		BONOBO_UI_VERB ("Stretch", show_stretch_handles_callback),
		BONOBO_UI_VERB ("Unstretch", unstretch_icons_callback),
		BONOBO_UI_VERB ("Clean Up", clean_up_callback),
		BONOBO_UI_VERB_END
	};
	
        g_assert (FM_IS_ICON_VIEW (view));

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

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

	bonobo_ui_component_freeze (icon_view->details->ui, NULL);
	
	/* Do one-time state-setting here; context-dependent state-setting
	 * is done in update_menus.
	 */
	if (!fm_icon_view_supports_auto_layout (icon_view)) {
		nautilus_bonobo_set_hidden 
			(icon_view->details->ui, POPUP_PATH_LAY_OUT, TRUE);
	}

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

        selection = fm_directory_view_get_selection (view);
        selection_count = g_list_length (selection);
        icon_container = get_icon_container (icon_view);

	bonobo_ui_component_freeze (icon_view->details->ui, NULL);

	nautilus_bonobo_set_sensitive (icon_view->details->ui, 
				       COMMAND_STRETCH_ICON,
				       selection_count == 1
				       && icon_container != NULL
			    	       && !nautilus_icon_container_has_stretch_handles (icon_container));

	nautilus_bonobo_set_label_for_menu_item_and_command 
		(icon_view->details->ui,
		 MENU_PATH_UNSTRETCH_ICONS,
		 COMMAND_UNSTRETCH_ICONS,
		 eel_g_list_more_than_one_item (selection)
		 	? _("_Restore Icons' Original Sizes")
		 	: _("_Restore Icon's Original Size"));
	nautilus_bonobo_set_sensitive (icon_view->details->ui, 
				       COMMAND_UNSTRETCH_ICONS,
				       icon_container != NULL
			    	       && nautilus_icon_container_is_stretched (icon_container));

	nautilus_bonobo_set_sensitive (icon_view->details->ui, 
				       COMMAND_RENAME,
				       selection_count == 1
				       && fm_icon_view_get_zoom_level (icon_view) > NAUTILUS_ZOOM_LEVEL_SMALLEST
				       && nautilus_file_can_rename (selection->data));
				       
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

/* here's the timer task that actually plays the file using mpg123. */
/* FIXME bugzilla.gnome.org 41258: we should get the application from our mime-type stuff */
static gboolean
play_file (gpointer callback_data)
{
#if USE_OLD_AUDIO_PREVIEW	
	NautilusFile *file;
	char *file_uri;
	char *file_path, *mime_type;
	gboolean is_mp3;
	gboolean is_ogg;
	pid_t mp3_pid;
	
	file = NAUTILUS_FILE (callback_data);
	file_uri = nautilus_file_get_uri (file);
	file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	mime_type = nautilus_file_get_mime_type (file);
	is_mp3 = eel_strcasecmp (mime_type, "audio/x-mp3") == 0;
	is_ogg = eel_strcasecmp (mime_type, "application/x-ogg") == 0;

	if (file_path != NULL) {
		mp3_pid = fork ();
		if (mp3_pid == (pid_t) 0) {
			/* Set the group (session) id to this process for future killing. */
			setsid();
			if (is_mp3) {
				execlp ("mpg123", "mpg123", "-y", "-q", file_path, NULL);
			} else if (is_ogg) {
				execlp ("ogg123", "ogg123", "-q", file_path, NULL);
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

	/* FIXME: Need to zero out the timeout. */
#else
	char *file_path, *file_uri, *mime_type;
	gboolean is_mp3;
	FMIconView *icon_view;
	
	icon_view = FM_ICON_VIEW (callback_data);
		
	file_uri = nautilus_file_get_uri (icon_view->details->audio_preview_file);
	file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	mime_type = nautilus_file_get_mime_type (icon_view->details->audio_preview_file);

	is_mp3 = eel_strcasecmp (mime_type, "audio/x-mp3") == 0;

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
		gtk_timeout_remove (icon_view->details->audio_preview_timeout);
		icon_view->details->audio_preview_timeout = 0;
	}
			
	if (start_flag) {
		icon_view->details->audio_preview_file = file;
#if USE_OLD_AUDIO_PREVIEW			
		icon_view->details->audio_preview_timeout = gtk_timeout_add (1000, play_file, file);
#else
		/* FIXME: Need to kill the existing timeout if there is one? */
		icon_view->details->audio_preview_timeout = gtk_timeout_add (1000, play_file, icon_view);
#endif
	}
}

static gboolean
should_preview_sound (NautilusFile *file) {

	/* Check gnome config sound preference */
	if (!gnome_config_get_bool ("/sound/system/settings/start_esd=true")) {
		return FALSE;
	}

	/* Check user performance preference */	
	if (preview_sound_auto_value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}
		
	/* the following is disabled until we can preview remote sounds, which we currently can't do */
	/*
	if (preview_mode == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	*/
	
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
			GtkEditable *editable,
			gpointer callback_data)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (callback_data);

	nautilus_clipboard_set_up_editable_in_control
		(editable,
		 fm_directory_view_get_bonobo_control (directory_view),
		 TRUE);
}

static int
compare_files (FMIconView *icon_view,
	       NautilusFile *a,
	       NautilusFile *b)
{
	g_assert (FM_IS_ICON_VIEW (icon_view));
	return nautilus_file_compare_for_sort
		(a, b, icon_view->details->sort->sort_type,
		 fm_directory_view_should_sort_directories_first (FM_DIRECTORY_VIEW (icon_view)),
		 icon_view->details->sort_reversed);
}

static int
compare_files_cover (gconstpointer a, gconstpointer b, gpointer callback_data)
{
	return compare_files (callback_data, NAUTILUS_FILE (a), NAUTILUS_FILE (b));
}

static void
fm_icon_view_sort_files (FMDirectoryView *view, GList **files)
{
	FMIconView *icon_view;

	icon_view = FM_ICON_VIEW (view);
	if (!fm_icon_view_using_auto_layout (icon_view)) {
		return;
	}
	*files = eel_g_list_sort_custom (*files, compare_files_cover, icon_view);
}

static int
icon_container_compare_icons_callback (NautilusIconContainer *container,
				       NautilusFile *a,
				       NautilusFile *b,
				       FMIconView *icon_view)
{
	return compare_files (icon_view, a, b);
}

/* This is used by type-to-select code. It deliberately ignores the
 * folders-first and reversed-sort settings.
 */
static int
icon_container_compare_icons_by_name_callback (NautilusIconContainer *container,
					       NautilusFile *file_a,
					       NautilusFile *file_b,
					       FMIconView *icon_view)
{
	int result;

	g_assert (FM_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (NAUTILUS_IS_FILE (file_a));
	g_assert (NAUTILUS_IS_FILE (file_b));

	result = nautilus_file_compare_for_sort (file_a, file_b, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
		 				 FALSE, FALSE);

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
	EelStringList *emblems_to_ignore;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_ICON_VIEW (icon_view));

	if (emblem_icons != NULL) {
		emblems_to_ignore = fm_directory_view_get_emblem_names_to_exclude 
			(FM_DIRECTORY_VIEW (icon_view));
		*emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file, emblems_to_ignore);
		eel_string_list_free (emblems_to_ignore);
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
		/* FIXME bugzilla.gnome.org 43020: This does sync. I/O and works only locally. */
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
	gchar *description;
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
		*editable_text = nautilus_file_get_display_name (file);
	}
	
	/* Handle link files specially. */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.gnome.org 42531: Does sync. I/O and works only locally. */
		actual_uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (actual_uri);
		g_free (actual_uri);
 		*additional_text = NULL;
		if (path != NULL) {
			description = nautilus_link_local_get_additional_text (path);
			if (description)
				*additional_text = g_strdup_printf (" \n%s\n ", description);
			g_free (description);
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
		while (eel_strcmp (text_array[i], "none") == 0) {
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
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	fm_icon_view_update_icon_container_fonts (FM_ICON_VIEW (callback_data));
}

static void
smooth_font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	fm_icon_view_update_icon_container_smooth_font (FM_ICON_VIEW (callback_data));
}

static void
default_zoom_level_font_size_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (FM_IS_ICON_VIEW (callback_data));

	fm_icon_view_update_icon_container_font_size_table (FM_ICON_VIEW (callback_data));
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
		fm_icon_view_set_zoom_level (icon_view, level, TRUE);
	}

	fm_icon_view_update_icon_container_font_size_table (icon_view);
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

static int default_zoom_level_font_size = 12;

static int
get_default_zoom_level_font_size (void)
{
	static gboolean auto_storaged_added = FALSE;

	if (auto_storaged_added == FALSE) {
		auto_storaged_added = TRUE;
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
						       &default_zoom_level_font_size);
	}

	return default_zoom_level_font_size;
}

static void
fm_icon_view_update_icon_container_font_size_table (FMIconView *icon_view)
{
	/* Deltas in points from the default zoom level.  The actual font size table
	 * is computed by taking the default_zoom level and assigning the default
	 * zoom level font size for that.  Then all values above the default zoom
	 * level are computed by using the positive deltas.  The values below the 
	 * default zoom level as respectively computed by using the negative deltas
	 */
	static int positive_font_size_delta_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1] = {
		0, +2, +4, +4, +6, +6, +8 };
	static int negative_font_size_delta_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1] = {
		0, -2, -3, -3, -4, -4, -5 };

	const int min_font_size = 5;
	const int max_font_size = 30;

	int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];
	NautilusIconContainer *icon_container;
	int default_zoom_level_font_size;
	int i;
 	int j;
	NautilusZoomLevel default_level;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	default_zoom_level_font_size = get_default_zoom_level_font_size ();
	default_level = get_default_zoom_level ();

	g_return_if_fail (default_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST);
	g_return_if_fail (default_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	for (i = default_level, j = 0; i <= NAUTILUS_ZOOM_LEVEL_LARGEST; i++,j++) {
		font_size_table[i] = CLAMP (default_zoom_level_font_size + positive_font_size_delta_table[j],
					    min_font_size,
					    max_font_size);
	}

	for (i = default_level - 1, j = 0; i >= NAUTILUS_ZOOM_LEVEL_SMALLEST; i--,j++) {
		font_size_table[i] = CLAMP (default_zoom_level_font_size + negative_font_size_delta_table[j],
					    min_font_size,
					    max_font_size);

	}

	for (i = 0; i <= NAUTILUS_ZOOM_LEVEL_LARGEST; i++) {
		g_assert (font_size_table[i] >= min_font_size);
		g_assert (font_size_table[i] <= max_font_size);
	}

	nautilus_icon_container_set_font_size_table (icon_container, font_size_table);
	nautilus_icon_container_request_update_all (icon_container);
}

static void
fm_icon_view_update_icon_container_smooth_font (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;
	EelScalableFont *scalable_font;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);
	
	scalable_font = nautilus_global_preferences_get_icon_view_smooth_font ();
	g_assert (EEL_IS_SCALABLE_FONT (scalable_font));

	nautilus_icon_container_set_smooth_label_font (icon_container,
						       scalable_font);
	gtk_object_unref (GTK_OBJECT (scalable_font));
	
	nautilus_icon_container_request_update_all (icon_container);
}

static void
fm_icon_view_update_click_mode (FMIconView *icon_view)
{
	NautilusIconContainer	*icon_container;
	int			click_mode;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	click_mode = eel_preferences_get_integer (NAUTILUS_PREFERENCES_CLICK_POLICY);


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

	smooth_graphics_mode = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);
	
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
			    "compare_icons_by_name",
			    GTK_SIGNAL_FUNC (icon_container_compare_icons_by_name_callback),
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
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "renaming_icon",
			    renaming_icon_callback,
			    directory_view);
	gtk_signal_connect_object (GTK_OBJECT (icon_container),
			           "icon_stretch_started",
			           fm_directory_view_update_menus,
			           GTK_OBJECT (directory_view));
	gtk_signal_connect_object (GTK_OBJECT (icon_container),
			           "icon_stretch_ended",
			           fm_directory_view_update_menus,
			           GTK_OBJECT (directory_view));

	gtk_container_add (GTK_CONTAINER (icon_view),
			   GTK_WIDGET (icon_container));

	fm_icon_view_update_icon_container_fonts (icon_view);
	fm_icon_view_update_icon_container_smooth_font (icon_view);
	fm_icon_view_update_icon_container_font_size_table (icon_view);
	fm_icon_view_update_click_mode (icon_view);
	fm_icon_view_update_smooth_graphics_mode (icon_view);

	gtk_widget_show (GTK_WIDGET (icon_container));
}

static void
icon_view_handle_uri_list (NautilusIconContainer *container, const char *item_uris,
			   GdkDragAction action, int x, int y, FMIconView *view)
{

	GList *uri_list, *node, *real_uri_list = NULL;
	GnomeVFSURI *container_uri;
	GnomeDesktopEntry *entry;
	GdkPoint point;
	char *local_path;
	char *stripped_uri;
	char *container_uri_string;
	char *container_path;
	const char *last_slash, *link_name;
	int n_uris;
	gboolean all_local;
	GArray *points;

	if (item_uris == NULL) {
		return;
	}

	container_uri_string = fm_directory_view_get_uri (FM_DIRECTORY_VIEW (view));
	container_uri = gnome_vfs_uri_new (container_uri_string);
	g_return_if_fail (container_uri != NULL);

	if (!gnome_vfs_uri_is_local (container_uri)) {
		eel_show_warning_dialog (_("Drag and drop is only supported to local file systems."),
					 _("Drag and Drop error"),
					 fm_directory_view_get_containing_window (FM_DIRECTORY_VIEW (view)));
		gnome_vfs_uri_unref (container_uri);
		g_free (container_uri_string);
		return;
	}

	if (action == GDK_ACTION_ASK) {
		action = eel_drag_drop_action_ask 
			(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
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
		gnome_vfs_uri_unref (container_uri);
		return;
	}

	point.x = x;
	point.y = y;
		
	/* Most of what comes in here is not really URIs, but rather paths that
	 * have a file: prefix in them.  We try to sanitize the uri list as a
	 * result.  Additionally, if they are all local files, then we can copy
	 * them.  Otherwise, we just make links.
	 */
	all_local = TRUE;
	n_uris = 0;
	uri_list = gnome_uri_list_extract_uris (item_uris);
	for (node = uri_list; node != NULL; node = node->next) {
		gchar *sanitized_uri;

		sanitized_uri = eel_make_uri_from_half_baked_uri (node->data);
		if (sanitized_uri == NULL)
			continue;
		real_uri_list = g_list_append (real_uri_list, sanitized_uri);
		if (strncmp (sanitized_uri, "file", 4) != 0)
			all_local = FALSE;
		n_uris++;
	}
	gnome_uri_list_free_strings (uri_list);

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
						   container_uri_string,
						   action, x, y, FM_DIRECTORY_VIEW (view));
		
		if (points)
			g_array_free (points, TRUE);
	} else {
		container_path = gnome_vfs_get_local_path_from_uri (container_uri_string);
		for (node = real_uri_list; node != NULL; node = node->next) {
			/* Make a link using the desktop file contents? */
			local_path = gnome_vfs_get_local_path_from_uri (node->data);
			if (local_path != NULL) {
				entry = gnome_desktop_entry_load (local_path);
				if (entry != NULL) {
					
					/* FIXME: Handle name conflicts? */
					nautilus_link_local_create_from_gnome_entry (entry, container_path, &point);
					gnome_desktop_entry_free (entry);
				}
				g_free (local_path);
				if (entry != NULL) {
					continue;
				}
			}

			/* Make a link from the URI alone. Generate the file
			 * name by extracting the basename of the URI.
			 */
			/* FIXME: This should be using eel_uri_get_basename
			 * instead of a "roll our own" solution.
			 */
			stripped_uri = eel_str_strip_trailing_chr ((char *)node->data, '/');
			g_print ("local_path:%s\nstripped_uri:%s\n", (char *)node->data, stripped_uri);
			last_slash = strrchr (stripped_uri, '/');
			link_name = last_slash == NULL ? NULL : last_slash + 1;
			
			if (!eel_str_is_empty (link_name)) {
				/* FIXME: Handle name conflicts? */
				nautilus_link_local_create (container_path, link_name,
							    "gnome-http-url", local_path,
							    &point, NAUTILUS_LINK_GENERIC);
			}
			
			g_free (stripped_uri);

			break;
		}
		g_free (container_path);
	}
	
	gnome_uri_list_free_strings (real_uri_list);
	gnome_vfs_uri_unref (container_uri);
	g_free (container_uri_string);
	
}

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
        fm_directory_view_class->image_display_policy_changed = fm_icon_view_image_display_policy_changed;
        fm_directory_view_class->merge_menus = fm_icon_view_merge_menus;
        fm_directory_view_class->smooth_graphics_mode_changed = fm_icon_view_smooth_graphics_mode_changed;
        fm_directory_view_class->emblems_changed = fm_icon_view_emblems_changed;
        fm_directory_view_class->sort_directories_first_changed = fm_icon_view_sort_directories_first_changed;
        fm_directory_view_class->start_renaming_item = fm_icon_view_start_renaming_item;
        fm_directory_view_class->text_attribute_names_changed = fm_icon_view_text_attribute_names_changed;
        fm_directory_view_class->update_menus = fm_icon_view_update_menus;

	klass->clean_up = fm_icon_view_real_clean_up;
	klass->supports_auto_layout = real_supports_auto_layout;
        klass->get_directory_auto_layout = fm_icon_view_real_get_directory_auto_layout;
        klass->get_directory_sort_by = fm_icon_view_real_get_directory_sort_by;
        klass->get_directory_sort_reversed = fm_icon_view_real_get_directory_sort_reversed;
        klass->get_directory_tighter_layout = fm_icon_view_real_get_directory_tighter_layout;
        klass->set_directory_auto_layout = fm_icon_view_real_set_directory_auto_layout;
        klass->set_directory_sort_by = fm_icon_view_real_set_directory_sort_by;
        klass->set_directory_sort_reversed = fm_icon_view_real_set_directory_sort_reversed;
        klass->set_directory_tighter_layout = fm_icon_view_real_set_directory_tighter_layout;

	eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_PREVIEW_SOUND,
					       &preview_sound_auto_value);
}

static void
fm_icon_view_initialize (FMIconView *icon_view)
{
	NautilusIconContainer *icon_container;

        g_return_if_fail (GTK_BIN (icon_view)->child == NULL);

	icon_view->details = g_new0 (FMIconViewDetails, 1);
	icon_view->details->sort = &sort_criteria[0];

	create_icon_container (icon_view);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
						  font_changed_callback, 
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
						  smooth_font_changed_callback, 
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
						  default_zoom_level_font_size_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
						  default_sort_order_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
						  default_sort_in_reverse_order_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
						  default_use_tighter_layout_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
						  default_use_manual_layout_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
						  default_zoom_level_changed_callback,
						  icon_view,
						  GTK_OBJECT (icon_view));

	icon_container = get_icon_container (icon_view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "handle_uri_list",
			    GTK_SIGNAL_FUNC (icon_view_handle_uri_list),
			    icon_view);

}
