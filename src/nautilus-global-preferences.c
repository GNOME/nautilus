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

#include "nautilus-global-preferences.h"
#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences-dialog.h>

#include <gnome.h>

BEGIN_GNOME_DECLS


#include <libnautilus/nautilus-glib-extensions.h>


/* 
 * Constants
 */
#define NAUTILUS_PREFS_DIALOG_TITLE _("Nautilus Preferences")

/* Private stuff */
static GtkWidget *prefs_global_create_dialog      (void);
static GtkWidget *prefs_global_create_enum_group (GtkWidget   *pane,
						   const gchar *group_title,
						  const gchar *pref_name);
static GtkWidget *prefs_global_create_check_group (GtkWidget   *pane,
						   const gchar *group_title,
						   const gchar *pref_names[],
						   guint        num_prefs);

static GtkWidget *prefs_global_get_prefs_dialog (void);

//static void user_level_changed_cb (GtkWidget *group, gpointer call_data, gpointer client_data);

static void prefs_global_register_static_prefs (NautilusPrefs *prefs);

static void prefs_global_register_dynamic_prefs (NautilusPrefs *prefs);


static GtkWidget *panes[3];

static const gchar * PREFS_GLOBAL_NAMESPACE = "Nautilus::Global";

static const gchar *prefs_global_window_option_pref_names[] =
{
	NAUTILUS_PREFS_WINDOW_ALWAYS_NEW,
	NAUTILUS_PREFS_WINDOW_SEARCH_EXISTING
};

#if 0
static const gchar *META_VIEWS[] =
{
	"Annotations",
	"Help Contents",
	"Help Index",
	"Help Search",
	"History",
	"Web Search"
};
#endif


static const gchar * prefs_global_user_level_names[] =
{
	"novice",
	"intermediate",
	"hacker",
	"ettore"
};

static const gchar * prefs_global_user_level_descriptions[] =
{
	"Novice",
	"Intermediate",
	"Hacker",
	"Ettore"
};

static const gint prefs_global_user_level_values[] =
{
	NAUTILUS_USER_LEVEL_NOVICE,
	NAUTILUS_USER_LEVEL_INTERMEDIATE,
	NAUTILUS_USER_LEVEL_HACKER,
	NAUTILUS_USER_LEVEL_ETTORE
};

static NautilusPrefEnumData prefs_global_user_level_data =
{
	prefs_global_user_level_names,
	prefs_global_user_level_descriptions,
	prefs_global_user_level_values,
	NAUTILUS_N_ELEMENTS (prefs_global_user_level_names)
};

static NautilusPrefInfo prefs_global_static_pref_info[] =
{
	{
		NAUTILUS_PREFS_USER_LEVEL,
		"User Level",
		GTK_TYPE_ENUM,
		FALSE,
		(gpointer) &prefs_global_user_level_data
	},
	{
		NAUTILUS_PREFS_WINDOW_ALWAYS_NEW,
		"Create new window for each new page",
		GTK_TYPE_BOOL,
		FALSE,
		NULL
	},
	{
		NAUTILUS_PREFS_WINDOW_SEARCH_EXISTING,
		"Do not open more than one window with the same page",
		GTK_TYPE_BOOL,
		FALSE,
		NULL
	},
};

/*
 * Private stuff
 */
static GtkWidget *
prefs_global_create_dialog (void)
{
	GtkWidget		*prefs_dialog;
	NautilusPrefsBox	*prefs_box;
	GtkWidget		*user_level_group;

	prefs_dialog = nautilus_prefs_dialog_new (NAUTILUS_PREFS_DIALOG_TITLE);

	prefs_box = NAUTILUS_PREFS_BOX (nautilus_prefs_dialog_get_prefs_box (NAUTILUS_PREFS_DIALOG (prefs_dialog)));

	panes[0] = nautilus_prefs_box_add_pane (prefs_box,
						"User Level",
						"User Level Something");

	user_level_group = prefs_global_create_enum_group (panes[0],
							   "User Level",
							   NAUTILUS_PREFS_USER_LEVEL);

	panes[1] = nautilus_prefs_box_add_pane (prefs_box,
						"Window Options",
						"Window Options Something");
	
	prefs_global_create_check_group (panes[1],
					 "Basic window options",
					 prefs_global_window_option_pref_names,
					 NAUTILUS_N_ELEMENTS (prefs_global_window_option_pref_names));

#if 0
	panes[2] = nautilus_prefs_box_add_pane (prefs_box,
						"Meta Views",
						"Meta Views Something");
	
	prefs_global_create_check_group (panes[2],
				  "Meta Views",
				  META_VIEWS,
				  NAUTILUS_N_ELEMENTS (META_VIEWS));
#endif

	return prefs_dialog;
}

static GtkWidget *
prefs_global_create_check_group (GtkWidget   *pane,
				 const gchar *group_title,
				 const gchar *pref_names[],
				 guint        num_prefs)
{
	GtkWidget *group;
	guint i;

	group = nautilus_preferences_group_new (group_title);

	for (i = 0; i < num_prefs; i++)
	{
		GtkWidget *item;

		item = nautilus_preferences_item_new (GTK_OBJECT (nautilus_prefs_global_get_prefs ()),
						      pref_names[i],
						      NAUTILUS_PREFERENCES_ITEM_BOOL);

		nautilus_preferences_group_add (NAUTILUS_PREFERENCES_GROUP (group),
						item);

		gtk_widget_show (item);
	}

	nautilus_prefs_pane_add_group (NAUTILUS_PREFS_PANE (pane), group);
	
	gtk_widget_show (group);

	return group;
}

static GtkWidget *
prefs_global_create_enum_group (GtkWidget	*pane, 
				const gchar *group_title,
				const gchar	*pref_name)
{
	GtkWidget *group;
	GtkWidget *item;
	
	group = nautilus_preferences_group_new (group_title);

	item = nautilus_preferences_item_new (GTK_OBJECT (nautilus_prefs_global_get_prefs ()),
					      pref_name,
					      NAUTILUS_PREFERENCES_ITEM_ENUM);
	
	
	nautilus_preferences_group_add (NAUTILUS_PREFERENCES_GROUP (group), item);
	
	gtk_widget_show (item);

	nautilus_prefs_pane_add_group (NAUTILUS_PREFS_PANE (pane), group);
	
	gtk_widget_show (group);

	return group;
}

#if 0
static void
user_level_changed_cb (GtkWidget *group, gpointer call_data, gpointer client_data)
{
	NautilusPrefsGroupRadioSignalData *data = (NautilusPrefsGroupRadioSignalData *) call_data;

	g_assert (data != NULL);

	// Do something
}
#endif

static void
prefs_global_register_static_prefs (NautilusPrefs *prefs)
{
	guint i;

	g_assert (prefs != NULL);

	/* Register the static prefs */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (prefs_global_static_pref_info); i++)
	{
		nautilus_prefs_register_from_info (prefs, &prefs_global_static_pref_info[i]);
	}
}

static void
prefs_global_register_dynamic_prefs (NautilusPrefs *prefs)
{
	g_assert (prefs != NULL);

	/* 
	 * Presummably, the following would be registered
	 * only if the component was present.  Once we
	 * have smarter activation, that will be case.
	 * 
	 * For now turn onall the ones we know off.
	 */

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::ntl_history_view",
					     "History View",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::ntl_websearch_view",
					     "Web Search View",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::ntl_notes_view",
					     "Annotations",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::hyperbola_navigation_tree",
					     "Help Contents",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::hyperbola_navigation_index",
					     "Help Index",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);

	nautilus_prefs_register_from_values (prefs,
					     "/nautilus/prefs/metaviews::hyperbola_navigation_search",
					     "Help Search",
					     GTK_TYPE_BOOL,
					     FALSE,
					     NULL);
}

static GtkWidget *
prefs_global_get_prefs_dialog (void)
{
	static GtkWidget * global_prefs_dialog = NULL;

	NautilusPrefs * global_prefs = nautilus_prefs_global_get_prefs ();

	if (!global_prefs)
	{
		g_warning ("something went terribly wrong wrt implicit prefs initialization\n");

		return NULL;
	}

	if (!global_prefs_dialog)
	{
		global_prefs_dialog = prefs_global_create_dialog ();
	}

	return global_prefs_dialog;
}

/*
 * Public functions
 */
NautilusPrefs *
nautilus_prefs_global_get_prefs	(void)
{
	static GtkObject * global_prefs = NULL;

	if (!global_prefs)
	{
		global_prefs = nautilus_prefs_new (PREFS_GLOBAL_NAMESPACE);

		prefs_global_register_static_prefs (NAUTILUS_PREFS (global_prefs));
		prefs_global_register_dynamic_prefs (NAUTILUS_PREFS (global_prefs));
	}

	return NAUTILUS_PREFS (global_prefs);
}

void
nautilus_prefs_global_show_dialog (void)
{
	GtkWidget * global_prefs_dialog = prefs_global_get_prefs_dialog ();

	nautilus_prefs_set_enum (nautilus_prefs_global_get_prefs (),
				 NAUTILUS_PREFS_USER_LEVEL,
				 NAUTILUS_USER_LEVEL_ETTORE);

	gtk_widget_show (global_prefs_dialog);
}

void
nautilus_prefs_global_shutdown (void)
{
	GtkWidget * global_prefs_dialog;
	GtkObject * global_prefs;

	global_prefs_dialog = prefs_global_get_prefs_dialog ();

	global_prefs = GTK_OBJECT (nautilus_prefs_global_get_prefs ());

	gtk_widget_unref (global_prefs_dialog);

	gtk_object_unref (global_prefs);
}
