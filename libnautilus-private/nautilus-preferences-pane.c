/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-pane.h - Interface for a prefs pane superclass.

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
#include "nautilus-preferences-pane.h"
#include "nautilus-gtk-macros.h"

#include <gtk/gtkhbox.h>

static const guint GROUPS_BOX_TOP_OFFSET = 0;
static const guint IN_BETWEEN_OFFSET = 4;

struct _NautilusPreferencesPaneDetails
{
	GtkWidget *groups_box;
	GList *groups;
};

/* NautilusPreferencesPaneClass methods */
static void nautilus_preferences_pane_initialize_class (NautilusPreferencesPaneClass *preferences_pane_class);
static void nautilus_preferences_pane_initialize       (NautilusPreferencesPane      *preferences_pane);

/* GtkObjectClass methods */
static void nautilus_preferences_pane_destroy          (GtkObject                    *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesPane, nautilus_preferences_pane, GTK_TYPE_VBOX)

/*
 * NautilusPreferencesPaneClass methods
 */
static void
nautilus_preferences_pane_initialize_class (NautilusPreferencesPaneClass *preferences_pane_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_pane_class);
	
	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_pane_destroy;
}

static void
nautilus_preferences_pane_initialize (NautilusPreferencesPane *preferences_pane)
{
	preferences_pane->details = g_new0 (NautilusPreferencesPaneDetails, 1);
}

/*
 *  GtkObjectClass methods
 */
static void
nautilus_preferences_pane_destroy (GtkObject* object)
{
	NautilusPreferencesPane *preferences_pane;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (object));
	
	preferences_pane = NAUTILUS_PREFERENCES_PANE (object);

	g_list_free (preferences_pane->details->groups);
	g_free (preferences_pane->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * NautilusPreferencesPane public methods
 */
GtkWidget *
nautilus_preferences_pane_new (void)
{
	NautilusPreferencesPane *preferences_pane;

	preferences_pane = NAUTILUS_PREFERENCES_PANE
		(gtk_widget_new (nautilus_preferences_pane_get_type (), NULL));

	/* Groups box */
	preferences_pane->details->groups_box = gtk_vbox_new (FALSE, 0);

	/* Add groups box to ourselves */
	gtk_box_pack_start (GTK_BOX (preferences_pane),
			    preferences_pane->details->groups_box,
			    FALSE,
			    FALSE,
			    GROUPS_BOX_TOP_OFFSET);

	gtk_widget_show (preferences_pane->details->groups_box);
	gtk_widget_show (GTK_WIDGET (preferences_pane));

	return GTK_WIDGET (preferences_pane);
}

GtkWidget *
nautilus_preferences_pane_add_group (NautilusPreferencesPane	*preferences_pane,
				     const char			*group_title)
{
	GtkWidget *group;

	g_return_val_if_fail (preferences_pane != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane), NULL);
	g_return_val_if_fail (group_title != NULL, NULL);

	group = nautilus_preferences_group_new (group_title);

	preferences_pane->details->groups = g_list_append (preferences_pane->details->groups,
						     (gpointer) group);

	gtk_box_pack_start (GTK_BOX (preferences_pane->details->groups_box),
			    group,
			    TRUE,
			    TRUE,
			    IN_BETWEEN_OFFSET);

	gtk_widget_show (group);

	return group;
}

GtkWidget *
nautilus_preferences_pane_add_item_to_nth_group (NautilusPreferencesPane	*preferences_pane,
						 guint				n,
						 const char			*preference_name,
						 NautilusPreferencesItemType	item_type)
{
	GtkWidget *item;
	GtkWidget *group;

	g_return_val_if_fail (preferences_pane != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane), NULL);

	g_return_val_if_fail (preference_name != NULL, NULL);

	if (!preferences_pane->details->groups) {
		g_warning ("nautilus_preferences_pane_add_item_to_nth_group () There are no groups!");
		return NULL;
	}

	if (n >= g_list_length (preferences_pane->details->groups)) {
		g_warning ("nautilus_preferences_pane_add_item_to_nth_group (n = %d) n is out of bounds!", n);
		return NULL;
	}

	g_return_val_if_fail (g_list_nth_data (preferences_pane->details->groups, n) != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (g_list_nth_data (preferences_pane->details->groups, n)), NULL);

	group = GTK_WIDGET (g_list_nth_data (preferences_pane->details->groups, n));

	item = nautilus_preferences_group_add_item (NAUTILUS_PREFERENCES_GROUP (group),
						    preference_name,
						    item_type);

	return item;
}

void
nautilus_preferences_pane_update (NautilusPreferencesPane *preferences_pane)
{
	GList *iterator;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane));

	for (iterator = preferences_pane->details->groups; iterator != NULL; iterator = iterator->next) {
		NautilusPreferencesGroup *group = NAUTILUS_PREFERENCES_GROUP (iterator->data);

		nautilus_preferences_group_update (group);
		
		if (nautilus_preferences_group_get_num_visible_items (group) == 0) {
			gtk_widget_hide (GTK_WIDGET (group));
		} else {
			gtk_widget_show (GTK_WIDGET (group));
		}
	}
}

guint
nautilus_preferences_pane_get_num_visible_groups (const NautilusPreferencesPane *pane)
{
	guint n = 0;
	GList *iterator;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (pane), 0);

	for (iterator = pane->details->groups; iterator != NULL; iterator = iterator->next) {
		NautilusPreferencesGroup *group = NAUTILUS_PREFERENCES_GROUP (iterator->data);

		if (GTK_WIDGET_VISIBLE (group)) {
			n++;
		}
	}

	return n;
}
