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
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-string-list.h>

#include <gtk/gtkhbox.h>

static const guint GROUPS_BOX_TOP_OFFSET = 0;
static const guint IN_BETWEEN_OFFSET = 4;

struct NautilusPreferencesPaneDetails
{
	GtkWidget *groups_box;
	GList *groups;
	EelStringList *control_preference_list;
};

/* NautilusPreferencesPaneClass methods */
static void nautilus_preferences_pane_initialize_class (NautilusPreferencesPaneClass *preferences_pane_class);
static void nautilus_preferences_pane_initialize       (NautilusPreferencesPane      *preferences_pane);

/* GtkObjectClass methods */
static void nautilus_preferences_pane_destroy          (GtkObject                    *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesPane, nautilus_preferences_pane, GTK_TYPE_VBOX)

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
preferences_pane_update_and_resize_callback (gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (callback_data));

	nautilus_preferences_pane_update (NAUTILUS_PREFERENCES_PANE (callback_data));

	gtk_widget_queue_resize (GTK_WIDGET (callback_data));
}

static void
nautilus_preferences_pane_initialize (NautilusPreferencesPane *preferences_pane)
{
	preferences_pane->details = g_new0 (NautilusPreferencesPaneDetails, 1);
	
 	nautilus_preferences_add_callback_while_alive ("user_level",
						       preferences_pane_update_and_resize_callback,
						       preferences_pane,
						       GTK_OBJECT (preferences_pane));
}

/* GtkObjectClass methods */
static void
nautilus_preferences_pane_destroy (GtkObject* object)
{
	NautilusPreferencesPane *preferences_pane;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (object));
	
	preferences_pane = NAUTILUS_PREFERENCES_PANE (object);

	g_list_free (preferences_pane->details->groups);
	eel_string_list_free (preferences_pane->details->control_preference_list);
	g_free (preferences_pane->details);

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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
nautilus_preferences_pane_add_group (NautilusPreferencesPane *preferences_pane,
				     const char *group_title)
{
	GtkWidget *group;
	
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane), NULL);
	g_return_val_if_fail (group_title != NULL, NULL);

	group = nautilus_preferences_group_new (group_title);

	preferences_pane->details->groups = g_list_append (preferences_pane->details->groups,
							   group);

	gtk_box_pack_start (GTK_BOX (preferences_pane->details->groups_box),
			    group,
			    TRUE,
			    TRUE,
			    IN_BETWEEN_OFFSET);

	gtk_widget_show (group);

	return group;
}

static int
preferences_pane_get_max_caption_width (const NautilusPreferencesPane *preferences_pane,
					int column)
{
	NautilusPreferencesGroup *group;
	GList *node;
	int max_caption_width = 0;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane), 0);
	g_return_val_if_fail (column >= 0, 0);
	g_return_val_if_fail (column <= 1, 0);

	for (node = preferences_pane->details->groups; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_GROUP (node->data));
		group = NAUTILUS_PREFERENCES_GROUP (node->data);

		if (GTK_WIDGET_VISIBLE (group)) {
			max_caption_width = MAX (max_caption_width,
						 nautilus_preferences_group_get_max_caption_width (group, column));
		}
	}

	return max_caption_width;
}

void
nautilus_preferences_pane_update (NautilusPreferencesPane *preferences_pane)
{
	GList *node;
	int max_caption_widths[2];
	NautilusPreferencesGroup *group;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (preferences_pane));

	for (node = preferences_pane->details->groups; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_GROUP (node->data));
		group = NAUTILUS_PREFERENCES_GROUP (node->data);
		nautilus_preferences_group_update (group);
		eel_gtk_widget_set_shown (GTK_WIDGET (group),
					  nautilus_preferences_group_get_num_visible_items (group) > 0);
	}

	max_caption_widths[0] = preferences_pane_get_max_caption_width (preferences_pane, 0);
	max_caption_widths[1] = preferences_pane_get_max_caption_width (preferences_pane, 1);

	for (node = preferences_pane->details->groups; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_GROUP (node->data));
		group = NAUTILUS_PREFERENCES_GROUP (node->data);

		if (GTK_WIDGET_VISIBLE (group)) {
			if (max_caption_widths[0] > 0) {
				nautilus_preferences_group_align_captions (group,
									   max_caption_widths[0],
									   0);
			}

			if (max_caption_widths[1] > 0) {
				nautilus_preferences_group_align_captions (group,
									   max_caption_widths[1],
									   1);
			}
		}
	}
}

guint
nautilus_preferences_pane_get_num_visible_groups (const NautilusPreferencesPane *pane)
{
	guint n = 0;
	GList *node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (pane), 0);

	for (node = pane->details->groups; node != NULL; node = node->next) {
		NautilusPreferencesGroup *group = NAUTILUS_PREFERENCES_GROUP (node->data);

		if (GTK_WIDGET_VISIBLE (group)) {
			n++;
		}
	}

	return n;
}

guint
nautilus_preferences_pane_get_num_groups (const NautilusPreferencesPane *pane)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (pane), 0);

	return g_list_length (pane->details->groups);
}

GtkWidget *
nautilus_preferences_pane_find_group (const NautilusPreferencesPane *pane,
				      const char *group_title)
{
	GList *node;
	char *title;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_PANE (pane), 0);

	for (node = pane->details->groups; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_GROUP (node->data));

		title = nautilus_preferences_group_get_title_label (NAUTILUS_PREFERENCES_GROUP (node->data));
		if (eel_str_is_equal (title, group_title)) {
			g_free (title);
			return node->data;
		}

		g_free (title);
	}
	
	return NULL;
}

void
nautilus_preferences_pane_add_control_preference (NautilusPreferencesPane *pane,
						  const char *control_preference_name)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (pane));
	g_return_if_fail (control_preference_name != NULL);

	if (eel_string_list_contains (pane->details->control_preference_list,
					   control_preference_name)) {
		return;
	}

	if (pane->details->control_preference_list == NULL) {
		pane->details->control_preference_list = eel_string_list_new (TRUE);
	}

	eel_string_list_insert (pane->details->control_preference_list,
				     control_preference_name);

 	nautilus_preferences_add_callback_while_alive (control_preference_name,
						       preferences_pane_update_and_resize_callback,
						       pane,
						       GTK_OBJECT (pane));
}
