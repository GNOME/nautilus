/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-group.c - A group of preferences items bounded by a frame.

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
#include "nautilus-preferences-group.h"

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gnome.h>

struct NautilusPreferencesGroupDetails
{
	GtkWidget *main_box;
	GtkWidget *columns[2];
	GList *items[2];
};

/* NautilusPreferencesGroupClass methods */
static void nautilus_preferences_group_initialize_class (NautilusPreferencesGroupClass *klass);
static void nautilus_preferences_group_initialize       (NautilusPreferencesGroup      *preferences_group);

/* GtkObjectClass methods */
static void nautilus_preferences_group_destroy          (GtkObject                     *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesGroup,
			      nautilus_preferences_group,
			      GTK_TYPE_FRAME);

/*
 * NautilusPreferencesGroupClass methods
 */
static void
nautilus_preferences_group_initialize_class (NautilusPreferencesGroupClass *preferences_group_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_group_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_group_destroy;
}

static void
nautilus_preferences_group_initialize (NautilusPreferencesGroup *group)
{
	group->details = g_new0 (NautilusPreferencesGroupDetails, 1);
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_preferences_group_destroy (GtkObject *object)
{
	NautilusPreferencesGroup *group;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (object));
	
	group = NAUTILUS_PREFERENCES_GROUP (object);

	g_list_free (group->details->items[0]);
	g_list_free (group->details->items[1]);
	g_free (group->details);

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * NautilusPreferencesGroup public methods
 */
GtkWidget *
nautilus_preferences_group_new (const gchar *title)
{
	NautilusPreferencesGroup *group;
	
	g_return_val_if_fail (title != NULL, NULL);

	group = NAUTILUS_PREFERENCES_GROUP
		(gtk_widget_new (nautilus_preferences_group_get_type (), NULL));

	/* Ourselves */
	gtk_frame_set_shadow_type (GTK_FRAME (group), GTK_SHADOW_ETCHED_IN);

	gtk_frame_set_label (GTK_FRAME (group), title);

	/* Main box */
	group->details->main_box = gtk_hbox_new (FALSE, 20);
	gtk_container_add (GTK_CONTAINER (group), group->details->main_box);

	/* Column 1 */
	group->details->columns[0] = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (group->details->main_box),
			    group->details->columns[0],
			    TRUE,
			    TRUE,
			    0);

	/* Column 2 */
	group->details->columns[1] = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (group->details->main_box),
			    group->details->columns[1],
			    TRUE,
			    TRUE,
			    0);

	gtk_container_set_border_width (GTK_CONTAINER (group->details->columns[0]), 6);

	gtk_widget_show (group->details->columns[0]);
	gtk_widget_show (group->details->columns[1]);
	gtk_widget_show (group->details->main_box);
	
	return GTK_WIDGET (group);
}

GtkWidget *
nautilus_preferences_group_add_item (NautilusPreferencesGroup *group,
				     const char *preference_name,
				     NautilusPreferencesItemType item_type,
				     int column)
{
	GtkWidget *item;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group), NULL);
	g_return_val_if_fail (preference_name != NULL, NULL);
	g_return_val_if_fail (column >= 0, NULL);
	g_return_val_if_fail (column <= 1, NULL);

	item = nautilus_preferences_item_new (preference_name, item_type);

	group->details->items[column] = g_list_append (group->details->items[column],
						       item);

	gtk_box_pack_start (GTK_BOX (group->details->columns[column]),
			    item,
 			    FALSE,
 			    FALSE,
			    0);

	gtk_widget_show (item);

	return item;
}

void
nautilus_preferences_group_update (NautilusPreferencesGroup *group)
{
	GList *node;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group));

	for (node = group->details->items[0]; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_ITEM (node->data));
		nautilus_preferences_item_update_showing (NAUTILUS_PREFERENCES_ITEM (node->data));
	}

	for (node = group->details->items[1]; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_ITEM (node->data));
		nautilus_preferences_item_update_showing (NAUTILUS_PREFERENCES_ITEM (node->data));
	}
}

guint
nautilus_preferences_group_get_num_visible_items (const NautilusPreferencesGroup *group)
{
	guint n = 0;
	GList *node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group), 0);

	for (node = group->details->items[0]; node != NULL; node = node->next) {
		if (nautilus_preferences_item_is_showing (NAUTILUS_PREFERENCES_ITEM (node->data))) {
			n++;
		}
	}

	for (node = group->details->items[1]; node != NULL; node = node->next) {
		if (nautilus_preferences_item_is_showing (NAUTILUS_PREFERENCES_ITEM (node->data))) {
			n++;
		}
	}
	
	return n;
}

char *
nautilus_preferences_group_get_title_label (const NautilusPreferencesGroup *group)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group), NULL);

	return g_strdup (GTK_FRAME (group)->label);
}

int
nautilus_preferences_group_get_max_caption_width (const NautilusPreferencesGroup *group,
						  int column)
{
	GList *node;
	NautilusPreferencesItem *item;
	int max_caption_width = 0;
	
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group), 0);
	g_return_val_if_fail (column >= 0, 0);
	g_return_val_if_fail (column <= 1, 0);

	for (node = group->details->items[column]; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_ITEM (node->data));
		item = NAUTILUS_PREFERENCES_ITEM (node->data);
		
		if (nautilus_preferences_item_is_showing (item)
		    && nautilus_preferences_item_child_is_caption (item)) {
			max_caption_width = MAX (max_caption_width,
						 nautilus_preferences_item_get_child_width (item));
		}
	}

	return max_caption_width;
}

void
nautilus_preferences_group_align_captions (NautilusPreferencesGroup *group,
					   int max_caption_width,
					   int column)
{
	GList *node;
	NautilusPreferencesItem *item;
	int width;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group));
	g_return_if_fail (max_caption_width > 0);
	g_return_if_fail (column >= 0);
	g_return_if_fail (column <= 1);

	/* Set the spacing on all the captions */
	for (node = group->details->items[column]; node != NULL; node = node->next) {
		g_assert (NAUTILUS_IS_PREFERENCES_ITEM (node->data));
		item = NAUTILUS_PREFERENCES_ITEM (node->data);

		if (nautilus_preferences_item_is_showing (item)		
		    && nautilus_preferences_item_child_is_caption (item)) {
			width = nautilus_preferences_item_get_child_width (item);
			g_assert (width <= max_caption_width);
			nautilus_preferences_item_set_caption_extra_spacing (item, max_caption_width - width);
		}
	}
}

