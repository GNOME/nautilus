/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-group-radio.c - Radio button prefs group implementation.

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

#include "nautilus-preferences-group.h"
//#include "nautilus-prefs.h"

#include <gnome.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-gtk-macros.h>

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} RadioGroupSignals;

typedef struct
{
	GtkWidget	*radio_button;
} ButtonInfo;

struct _NautilusPreferencesGroupPrivate
{
	GtkWidget	*main_box;
	GtkWidget	*content_box;
	GtkWidget	*description_label;
	gboolean	show_description;
};

static const gint PREFERENCES_GROUP_NOT_FOUND = -1;

/* NautilusPreferencesGroupClass methods */
static void        nautilus_preferences_group_initialize_class (NautilusPreferencesGroupClass *klass);
static void        nautilus_preferences_group_initialize       (NautilusPreferencesGroup      *preferences_group);


/* GtkObjectClass methods */
static void        nautilus_preferences_group_destroy          (GtkObject                    *object);


/* NautilusPrefsGroupClass methods */
static void        preferences_group_construct                 (NautilusPreferencesGroup *prefs_group,
								const gchar * title);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesGroup,
				   nautilus_preferences_group,
				   GTK_TYPE_FRAME);

/*
 * NautilusPreferencesGroupClass methods
 */
static void
nautilus_preferences_group_initialize_class (NautilusPreferencesGroupClass *preferences_group_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_group_class);
	widget_class = GTK_WIDGET_CLASS (preferences_group_class);

 	parent_class = gtk_type_class (gtk_frame_get_type ());
	
	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_group_destroy;
}

static void
nautilus_preferences_group_initialize (NautilusPreferencesGroup *group)
{
	group->priv = g_new (NautilusPreferencesGroupPrivate, 1);

	group->priv->main_box = NULL;
	group->priv->content_box = NULL;
	group->priv->description_label = NULL;
	group->priv->show_description = FALSE;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_preferences_group_destroy(GtkObject* object)
{
	NautilusPreferencesGroup *group;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (object));
	
	group = NAUTILUS_PREFERENCES_GROUP (object);

	g_free (group->priv);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
preferences_group_construct (NautilusPreferencesGroup *group,
			     const gchar * title)
{
	g_assert (group != NULL);
	g_assert (title != NULL);

	g_assert (group->priv->content_box == NULL);
	g_assert (group->priv->main_box == NULL);
	g_assert (group->priv->description_label == NULL);

//	printf ("preferences_group_construct\n");

// 	pref_info = nautilus_prefs_get_pref_info (NAUTILUS_PREFS (prefs), 
// 						  pref_name);

// 	g_assert (pref_info != NULL);

// 	g_assert (pref_info->pref_description != NULL);

	/* Ourselves */
	gtk_frame_set_shadow_type (GTK_FRAME (group),
				   GTK_SHADOW_ETCHED_IN);


// 	gtk_object_set (GTK_OBJECT (group),
// 			"title_string", group_title,
// 			NULL);

	gtk_frame_set_label (GTK_FRAME (group), title);

	/* Main box */
	group->priv->main_box = gtk_vbox_new (FALSE, 0);

	gtk_container_add (GTK_CONTAINER (group),
			   group->priv->main_box);

// 	gtk_container_set_border_width (GTK_CONTAINER (group->priv->content_box),
// 					GROUP_FRAME_BORDER_WIDTH);

	/* Description label */
	group->priv->description_label = gtk_label_new ("Blurb");

	gtk_label_set_justify (GTK_LABEL (group->priv->description_label),
			       GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (group->priv->main_box),
			    group->priv->description_label,
			    FALSE,
			    FALSE,
			    0);

	if (group->priv->show_description)
	{
		gtk_widget_show (group->priv->description_label);
	}

	/* Content box */
	group->priv->content_box = 
		gtk_vbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (group->priv->main_box),
			    group->priv->content_box,
			    FALSE,
			    FALSE,
			    0);

	gtk_container_set_border_width (GTK_CONTAINER (group->priv->content_box),
					4);

	gtk_widget_show (group->priv->content_box);
	gtk_widget_show (group->priv->main_box);
}

/*
 * NautilusPreferencesGroup public methods
 */
GtkWidget*
nautilus_preferences_group_new (const gchar *title)
{
	NautilusPreferencesGroup *group;

	g_return_val_if_fail (title != NULL, NULL);

/* 	printf("nautilus_preferences_group_new()\n"); */

	group = gtk_type_new (nautilus_preferences_group_get_type ());

	preferences_group_construct (group, title);
	
	return GTK_WIDGET (group);
}

// void
// nautilus_preferences_group_clear (NautilusPreferencesGroup *preferences_group)
// {
// }

void
nautilus_preferences_group_add (NautilusPreferencesGroup *group,
				GtkWidget		 *item)
{
	g_return_if_fail (group != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_GROUP (group));
	g_return_if_fail (item != NULL);

/* 	printf("nautilus_preferences_group_add()\n"); */
	
	gtk_box_pack_start (GTK_BOX (group->priv->content_box),
			    item,
			    TRUE,
			    TRUE,
			    0);

//	gtk_widget_show (item);
}

