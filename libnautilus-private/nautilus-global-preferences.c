/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-dialog.c - Implementation for preferences dialog.

   Copyright (C) 1999, 2000 Eazel, Inc.

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
#include "nautilus-global-preferences.h"

#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences-dialog.h>
#include <libnautilus/nautilus-glib-extensions.h>

/* 
 * Constants
 */
#define GLOBAL_PREFERENCES_DIALOG_TITLE _("Nautilus Preferences")

/* Private stuff */
static GtkWidget *global_preferences_create_dialog      (void);
static GtkWidget *global_preferences_create_enum_group  (GtkWidget           *pane,
							 const char          *group_title,
							 const char          *pref_name);
static GtkWidget *global_preferences_create_check_group (GtkWidget           *pane,
							 const char          *group_title,
							 const char * const   pref_names[],
							 guint                num_prefs);
static GtkWidget *global_preferences_get_dialog         (void);
static void       global_preferences_register_for_ui   (void);
static void       global_preferences_register_static    (NautilusPreferences *prefs);
static void       global_preferences_register_dynamic   (NautilusPreferences *prefs);

static const char * const global_preferences_window_option_pref_names[] =
{
	NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING
};

static const char * const global_preferences_meta_view_names[] =
{
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
	NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH
};

static const char * const global_preferences_user_level_names[] =
{
	"novice",
	"intermediate",
	"hacker"
};

static const char * const global_preferences_user_level_descriptions[] =
{
	"Novice",
	"Intermediate",
	"Hacker"
};

static const gint global_preferences_user_level_values[] =
{
	NAUTILUS_USER_LEVEL_NOVICE,
	NAUTILUS_USER_LEVEL_INTERMEDIATE,
	NAUTILUS_USER_LEVEL_HACKER
};

static const NautilusPreferencesEnumData global_preferences_user_level_data =
{
	global_preferences_user_level_names,
	global_preferences_user_level_descriptions,
	global_preferences_user_level_values,
	NAUTILUS_N_ELEMENTS (global_preferences_user_level_names)
};

static const NautilusPreferencesInfo global_preferences_static_info[] =
{
	{
		NAUTILUS_PREFERENCES_USER_LEVEL,
		"User Level",
		NAUTILUS_PREFERENCE_ENUM,
		(gconstpointer) NAUTILUS_USER_LEVEL_HACKER,
		(gpointer) &global_preferences_user_level_data
	},
	{
		NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
		"Create new window for each new page",
		NAUTILUS_PREFERENCE_BOOLEAN,
		FALSE,
		NULL
	},
	{
		NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING,
		"Do not open more than one window with the same page",
		NAUTILUS_PREFERENCE_BOOLEAN,
		FALSE,
		NULL
	},
};

/*
 * Private stuff
 */
static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget		*panes[3];
	GtkWidget		*prefs_dialog;
	NautilusPreferencesBox	*prefs_box;

	global_preferences_register_for_ui ();

	prefs_dialog = nautilus_preferences_dialog_new (GLOBAL_PREFERENCES_DIALOG_TITLE);
	
	prefs_box = NAUTILUS_PREFERENCES_BOX (nautilus_preferences_dialog_get_prefs_box (NAUTILUS_PREFERENCES_DIALOG (prefs_dialog)));

	panes[0] = nautilus_preferences_box_add_pane (prefs_box,
						      "User Level",
						      "User Level Something");

	global_preferences_create_enum_group (panes[0],
					     "User Level",
					     NAUTILUS_PREFERENCES_USER_LEVEL);

	panes[1] = nautilus_preferences_box_add_pane (prefs_box,
						      "Window Options",
						      "Window Options Something");
	
	global_preferences_create_check_group (panes[1],
					       "Basic window options",
					       global_preferences_window_option_pref_names,
					       NAUTILUS_N_ELEMENTS (global_preferences_window_option_pref_names));

	panes[2] = nautilus_preferences_box_add_pane (prefs_box,
						      "Meta Views",
						      "Meta Views Something");
	
	global_preferences_create_check_group (panes[2],
					       "Meta Views",
					       global_preferences_meta_view_names,
					       NAUTILUS_N_ELEMENTS (global_preferences_meta_view_names));

	return prefs_dialog;
}

static GtkWidget *
global_preferences_create_check_group (GtkWidget *pane,
				       const char *group_title,
				       const char * const pref_names[],
				       guint num_prefs)
{
	GtkWidget *group;
	guint i;

	group = nautilus_preferences_group_new (group_title);

	for (i = 0; i < num_prefs; i++)
	{
		GtkWidget *item;

		item = nautilus_preferences_item_new (GTK_OBJECT (nautilus_preferences_get_global_preferences ()),
						      pref_names[i],
						      NAUTILUS_PREFERENCE_BOOLEAN);

		nautilus_preferences_group_add (NAUTILUS_PREFERENCES_GROUP (group),
						item);

		gtk_widget_show (item);
	}

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (pane), group);
	
	gtk_widget_show (group);

	return group;
}

static GtkWidget *
global_preferences_create_enum_group (GtkWidget	*pane, 
				      const char *group_title,
				      const char *pref_name)
{
	GtkWidget *group;
	GtkWidget *item;
	
	group = nautilus_preferences_group_new (group_title);

	item = nautilus_preferences_item_new (GTK_OBJECT (nautilus_preferences_get_global_preferences ()),
					      pref_name,
					      NAUTILUS_PREFERENCE_ENUM);
	
	
	nautilus_preferences_group_add (NAUTILUS_PREFERENCES_GROUP (group), item);
	
	gtk_widget_show (item);

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (pane), group);
	
	gtk_widget_show (group);

	return group;
}

static void
global_preferences_register_static (NautilusPreferences *prefs)
{
	guint i;

	g_assert (prefs != NULL);

	/* Register the static prefs */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (global_preferences_static_info); i++) {
		nautilus_preferences_set_info (prefs,
					       global_preferences_static_info[i].name,
					       global_preferences_static_info[i].description,
					       global_preferences_static_info[i].type,
					       global_preferences_static_info[i].default_value,
					       global_preferences_static_info[i].data);
	}
}

/* 
 * Presummably, the following would be registered
 * only if the component was present.  Once we
 * have smarter activation, that will be case.
 * 
 * For now turn on all the ones we know about.
 */

const NautilusStringList *
nautilus_global_preferences_get_meta_view_iids (void)
{
	static NautilusStringList * meta_view_names = NULL;
	
	if (!meta_view_names)
	{
		meta_view_names = nautilus_string_list_new ();

		nautilus_string_list_insert (meta_view_names, "ntl_notes_view");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_tree");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_index");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_search");
		nautilus_string_list_insert (meta_view_names, "ntl_history_view");
		nautilus_string_list_insert (meta_view_names, "ntl_websearch_view");
	}

	return meta_view_names;
}

static void
global_preferences_register_dynamic (NautilusPreferences *prefs)
{
	g_assert (prefs != NULL);

	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
				       "History View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE,
				       NULL);
	
	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH,
				       "Web Search View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE,
				       NULL);

	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
				       "Annotations",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE,
				       NULL);

	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
				       "Help Contents",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE,
				       NULL);

	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
				       "Help Index",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE,
				       NULL);

	nautilus_preferences_set_info (prefs,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
				       "Help Search",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE,
				       NULL);
}

static GtkWidget *
global_preferences_get_dialog (void)
{
	static GtkWidget * global_prefs_dialog = NULL;

	NautilusPreferences * global_prefs = nautilus_preferences_get_global_preferences ();

	if (!global_prefs)
	{
		g_warning ("something went terribly wrong with implicit prefs initialization");

		return NULL;
	}

	if (!global_prefs_dialog)
	{
		global_prefs_dialog = global_preferences_create_dialog ();
	}

	return global_prefs_dialog;
}

static void
global_preferences_register_for_ui (void)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
		initialized = TRUE;

 		global_preferences_register_static (nautilus_preferences_get_global_preferences ());
 		global_preferences_register_dynamic (nautilus_preferences_get_global_preferences ());
	}
}

/*
 * Public functions
 */
void
nautilus_global_preferences_show_dialog (void)
{
	GtkWidget * global_prefs_dialog = global_preferences_get_dialog ();

	gtk_widget_show (global_prefs_dialog);
}

void
nautilus_global_preferences_shutdown (void)
{
	GtkWidget * global_prefs_dialog;
	GtkObject * global_prefs;

	return;

	global_prefs_dialog = global_preferences_get_dialog ();

	global_prefs = GTK_OBJECT (nautilus_preferences_get_global_preferences ());

	gtk_widget_unref (global_prefs_dialog);

	gtk_object_unref (global_prefs);
}

