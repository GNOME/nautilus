/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-dialog.c - Nautilus shell specific preferences things.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-preferences-dialog.h"

#include "libnautilus-private/nautilus-global-preferences.h"
#include "libnautilus-private/nautilus-sidebar-functions.h"
#include "nautilus-theme-selector.h"
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-preferences-box.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

/* 
 * This file contains the description of the preferences dialog in
 * Nautilus.  If you would like to add an item to the Nautilus
 * preference dialog, you might want to follow this procedure:
 *
 * 1.  Info Coming Soon.
 *
 */

static void preferences_dialog_populate_sidebar_tabs_group (EelPreferencesGroup *group);
static void preferences_dialog_populate_themes_group       (EelPreferencesGroup *group);

static GtkWidget *preferences_dialog;

static EelPreferencesItemDescription appearance_items[] = {
	{ N_("Smoother Graphics"),
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  N_("Use smoother (but slower) graphics"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
	  N_("Default smooth font:"),
	  EEL_PREFERENCE_ITEM_SMOOTH_FONT,
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_FONT,
	  N_("Default non-smooth font:"),
	  EEL_PREFERENCE_ITEM_FONT,
	},
	{ N_("Nautilus Themes"),
	  NULL,
	  NULL,
	  0,
	  NULL,
	  0,
	  0,
	  preferences_dialog_populate_themes_group
	},
	{ NULL }
};

static EelPreferencesItemDescription windows_and_desktop_items[] = {
	{ N_("Desktop"),
	  NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  N_("Use Nautilus to draw the desktop"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
        { N_("Desktop"),
	  NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
	  N_("Use your home folder as the desktop"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  N_("Open each file or folder in a separate window"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  N_("Display toolbar in new windows"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  N_("Display location bar in new windows"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  N_("Display status bar in new windows"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  N_("Display sidebar in new windows"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  N_("Ask before emptying the Trash or deleting files"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  N_("Include a Delete command that bypasses Trash"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	/* FIXME: This group clearly doesn't belong in Windows &
	 * Desktop, but there's no obviously-better place for it and
	 * it probably doesn't deserve a pane of its own.
	 */
	{ N_("Keyboard Shortcuts"),
	  NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
	  N_("Use Emacs-style keyboard shortcuts in text fields"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static EelPreferencesItemDescription directory_views_items[] = {
	{ N_("Click Behavior"),
	  NAUTILUS_PREFERENCES_CLICK_POLICY,
	  N_("Click Behavior"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
	{ N_("Executable Text Files"),
	  NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  N_("Executable Text Files"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
	  N_("Show hidden files (file names start with \".\")"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  N_("Show backup files (file names end with \"~\")"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  N_("Show special flags in Properties window"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Sorting Order"),
	  NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  N_("Always list folders before files"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static EelPreferencesItemDescription icon_captions_items[] = {
	{ N_("Icon Captions"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
	  N_("Choose the order for information to appear beneath icon names.\n"
	     "More information appears as you zoom in closer"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL,
	  NULL,
	  0,
	  0,
	  NULL,
	  "none"
	},
	{ NULL }
};

static EelPreferencesItemDescription view_preferences_items[] = {
	{ N_("Default View"),
	  NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
	  N_("View new folders using:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU
	},

	/* Icon View Defaults */
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
	  N_("Lay Out Items:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  N_("Sort in reversed order"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
	  N_("Font:"),
	  EEL_PREFERENCE_ITEM_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  EEL_PREFERENCE_ITEM_HIDE
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
	  N_("Font:"),
	  EEL_PREFERENCE_ITEM_SMOOTH_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  EEL_PREFERENCE_ITEM_SHOW
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
	  N_("Default zoom level:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
	  N_("Use tighter layout"),
	  EEL_PREFERENCE_ITEM_BOOLEAN,
	  NULL, 0, 1
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  N_("Font size at default zoom level:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},

	/* List View Defaults */
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
	  N_("Lay Out Items:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  N_("Sort in reversed order"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
	  N_("Font:"),
	  EEL_PREFERENCE_ITEM_FONT
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
	  N_("Default zoom level:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ N_("List View Defaults"),
	  "dummy-string",
	  NULL,
	  EEL_PREFERENCE_ITEM_PADDING,
	  NULL, 0, 1
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  N_("Font size at default zoom level:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ NULL }
};

static EelPreferencesItemDescription search_items[] = {
#ifdef HAVE_MEDUSA
	{ N_("Search Complexity Options"),
	  NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  N_("search type to do by default"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
#endif
	{ N_("Search Engines"),
	  NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
	  N_("Search Engine Location"),
	  EEL_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ NULL }
};

static EelPreferencesItemDescription navigation_items[] = {
	{ N_("Home"),
	  NAUTILUS_PREFERENCES_HOME_URI,
	  N_("Location:"),
	  EEL_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  N_("Use HTTP Proxy"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_HOST,
	  N_("Location:"),
	  EEL_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
	  N_("Port:"),
	  EEL_PREFERENCE_ITEM_EDITABLE_INTEGER
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_USE_AUTH,
	  N_("Proxy requires a username and password:"),
	  EEL_PREFERENCE_ITEM_BOOLEAN,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  EEL_PREFERENCE_ITEM_SHOW
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_AUTH_USERNAME,
	  N_("Username:"),
	  EEL_PREFERENCE_ITEM_EDITABLE_STRING,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  EEL_PREFERENCE_ITEM_SHOW
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_USE_AUTH_PASSWORD,
	  N_("Password:"),
	  EEL_PREFERENCE_ITEM_EDITABLE_STRING,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  EEL_PREFERENCE_ITEM_SHOW
	},
	{ N_("Built-in Bookmarks"),
	  NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
	  N_("Don't include the built-in bookmarks in the Bookmarks menu"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static EelPreferencesItemDescription tradeoffs_items[] = {
	{ N_("Show Text in Icons"),
	  NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  NULL,
	  EEL_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Count of Items in Folders"),
	  NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  NULL,
	  EEL_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  NULL,
	  EEL_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  N_("Don't make thumbnails for files larger than:"),
	  EEL_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("Preview Sound Files"),
	  NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  NULL,
	  EEL_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},

	/* FIXME bugzilla.gnome.org 42560: This title phrase needs improvement. */
	{ N_("Make Folder Appearance Details Public"),
	  NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  NULL,
	  EEL_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ NULL }
};

static EelPreferencesItemDescription sidebar_items[] = {
	{ N_("Tabs"),
	  NULL,
	  NULL,
	  0,
	  NULL,
	  0,
	  0,
	  preferences_dialog_populate_sidebar_tabs_group,
	},
	{ N_("Tree"),
	  NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  N_("Show only folders (no files) in the tree"),
	  EEL_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static EelPreferencesItemDescription news_panel_items[] = {
	{ N_("Maximum items per site"),
	  NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS,
	  N_("Maximum number of items displayed per site"),
	  EEL_PREFERENCE_ITEM_EDITABLE_INTEGER
	},
	{ N_("Update Minutes"),
	  NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL,
	  N_("Update frequency in minutes"),
	  EEL_PREFERENCE_ITEM_EDITABLE_INTEGER

	},
	{ NULL }
};


static EelPreferencesPaneDescription panes[] = {
	{ N_("View Preferences"),	  view_preferences_items },
	{ N_("Appearance"),		  appearance_items },
	{ N_("Windows & Desktop"),	  windows_and_desktop_items },
	{ N_("Icon & List Views"),	  directory_views_items },
	{ N_("Icon Captions"),		  icon_captions_items },
	{ N_("Sidebar Panels"),		  sidebar_items },
	{ N_("Search"),			  search_items },
	{ N_("Navigation"),		  navigation_items },
	{ N_("Speed Tradeoffs"),	  tradeoffs_items },
	{ N_("News Panel"),		  news_panel_items },
	{ NULL }
};

static void
dialog_button_clicked_callback (GnomeDialog *dialog,
				gint n,
				gpointer callback_data)
{
	g_return_if_fail (GNOME_IS_DIALOG (dialog));
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static gboolean
dialog_close_callback (GtkWidget *dialog,
		       gpointer user_data)
{
	g_return_val_if_fail (GNOME_IS_DIALOG (dialog), TRUE);
	gtk_widget_hide (dialog);
	return TRUE;
}

static GtkWidget *
preferences_dialog_create (void)
{
	GtkWidget *dialog;

	dialog = eel_preferences_dialog_new (_("Preferences"), panes);

	gtk_window_set_wmclass (GTK_WINDOW (dialog), "nautilus_preferences", "Nautilus");

	gtk_signal_connect (GTK_OBJECT (dialog),
			    "clicked",
			    GTK_SIGNAL_FUNC (dialog_button_clicked_callback),
			    dialog);

	gtk_signal_connect (GTK_OBJECT (dialog),
			    "close",
			    GTK_SIGNAL_FUNC (dialog_close_callback),
			    NULL);
	return dialog;
}

static void
global_preferences_populate_sidebar_panels_callback (const char *name,
						     const char *iid,
						     const char *preference_key,
						     gpointer callback_data) 
{
	char *description;

	g_return_if_fail (name != NULL);
	g_return_if_fail (iid != NULL);
	g_return_if_fail (preference_key != NULL);
	g_return_if_fail (EEL_IS_PREFERENCES_GROUP (callback_data));
	
	description = g_strdup_printf (_("Display %s tab in sidebar"), name);
	
	eel_preferences_set_description (preference_key, description);

	eel_preferences_group_add_item (EEL_PREFERENCES_GROUP (callback_data),
					preference_key,
					EEL_PREFERENCE_ITEM_BOOLEAN,
					0);

	g_free (description);
}

static void
preferences_dialog_populate_sidebar_tabs_group (EelPreferencesGroup *group)
{
	g_return_if_fail (EEL_IS_PREFERENCES_GROUP (group));

	nautilus_sidebar_for_each_panel (global_preferences_populate_sidebar_panels_callback, group);
}

/* Callback for when the user chooses a new theme from the list */
static void
theme_changed_callback (NautilusThemeSelector *theme_selector,
			gpointer callback_data)
{
	char *selected_theme;

	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	selected_theme = nautilus_theme_selector_get_selected_theme (NAUTILUS_THEME_SELECTOR (theme_selector));
	g_return_if_fail (selected_theme != NULL);

	eel_preferences_set (NAUTILUS_PREFERENCES_THEME, selected_theme);

	g_free (selected_theme);
}

/* PreferenceItem callback for when its time to update the theme chooser
 * with the theme currently stored in preferences
*/
static void
update_theme_selector_displayed_value_callback (EelPreferencesItem *item,
						gpointer callback_data)
{
	char *current_theme_name;

	g_return_if_fail (EEL_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));

	current_theme_name = eel_preferences_get (NAUTILUS_PREFERENCES_THEME);

	nautilus_theme_selector_set_selected_theme (NAUTILUS_THEME_SELECTOR (callback_data),
						    current_theme_name);

	g_free (current_theme_name);
}

static void
preferences_dialog_populate_themes_group (EelPreferencesGroup *group)
{
	GtkWidget *item;
	GtkWidget *child;
	GtkWidget *parent_window;

	g_return_if_fail (EEL_IS_PREFERENCES_GROUP (group));

	child = nautilus_theme_selector_new ();

	parent_window = gtk_widget_get_ancestor (GTK_WIDGET (group), GTK_TYPE_WINDOW);

	if (GTK_IS_WINDOW (parent_window)) {
		nautilus_theme_selector_set_parent_window (NAUTILUS_THEME_SELECTOR (child),
							   GTK_WINDOW (parent_window));
	}
	
	item = eel_preferences_group_add_custom_item (group,
						      NAUTILUS_PREFERENCES_THEME,
						      child,
						      "theme_changed",
						      0);
	/* Keep track of theme chooser changes */
	gtk_signal_connect (GTK_OBJECT (child),
			    "theme_changed",
			    GTK_SIGNAL_FUNC (theme_changed_callback),
			    NULL);

	/* Have the custom preferences item tell us when its time to update the displayed
	 * with with the one stored in preferences
	 */
	gtk_signal_connect (GTK_OBJECT (item),
			    "custom_update_displayed_value",
			    GTK_SIGNAL_FUNC (update_theme_selector_displayed_value_callback),
			    child);
	update_theme_selector_displayed_value_callback (EEL_PREFERENCES_ITEM (item), child);
}

static void
preferences_dialog_destroy (void)
{
	if (preferences_dialog == NULL) {
		return;
	}
	
	/* Since it's a top-level window, it's OK to destroy rather than unref'ing. */
	gtk_widget_destroy (preferences_dialog);
	preferences_dialog = NULL;
}

static GtkWidget *
global_preferences_get_dialog (void)
{
	nautilus_global_preferences_initialize ();
	
	if (preferences_dialog == NULL) {
		preferences_dialog = preferences_dialog_create ();
		g_atexit (preferences_dialog_destroy);
	}

	g_return_val_if_fail (GNOME_IS_DIALOG (preferences_dialog), NULL);
	return preferences_dialog;
}

void
nautilus_preferences_dialog_show (void)
{
	GtkWidget *dialog;

	dialog = global_preferences_get_dialog ();
	g_return_if_fail (GNOME_IS_DIALOG (dialog));

	eel_gtk_window_present (GTK_WINDOW (dialog));
}
