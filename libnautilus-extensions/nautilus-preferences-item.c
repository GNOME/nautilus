/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-item.c - Implementation for an individual prefs item.

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

#include "nautilus-preferences-item.h"
#include "nautilus-preferences.h"
#include <libnautilus-extensions/nautilus-gtk-macros.h>

#include <gtk/gtkcheckbutton.h>
#include <nautilus-widgets/nautilus-radio-button-group.h>

/* Arguments */
enum
{
	ARG_0,
	ARG_SHOW_DESCRIPTION,
	ARG_DESCRIPTION_STRING,
	ARG_TITLE_STRING
};

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const guint PREFERENCES_ITEM_TITLE_SPACING = 4;
static const guint PREFERENCES_ITEM_FRAME_BORDER_WIDTH = 6;
static const gint PREFERENCES_ITEM_UNDEFINED_ITEM = -1;

struct _NautilusPreferencesItemDetails
{
	gchar				*preference_name;
	NautilusPreferencesItemType	item_type;
	GtkWidget			*child;
	NautilusPreferences		*preferences;
};

/* NautilusPreferencesItemClass methods */
static void nautilus_preferences_item_initialize_class (NautilusPreferencesItemClass *klass);
static void nautilus_preferences_item_initialize       (NautilusPreferencesItem      *preferences_item);


/* GtkObjectClass methods */
static void preferences_item_destroy                   (GtkObject                    *object);
static void preferences_item_set_arg                   (GtkObject                    *object,
							GtkArg                       *arg,
							guint                         arg_id);
static void preferences_item_get_arg                   (GtkObject                    *object,
							GtkArg                       *arg,
							guint                         arg_id);

/* Private stuff */
static void preferences_item_construct                 (NautilusPreferencesItem      *item,
							NautilusPreferences    *preferences,
							const gchar                  *preference_name,
							NautilusPreferencesItemType   item_type);
static void preferences_item_create_enum               (NautilusPreferencesItem      *item,
							const NautilusPreference     *prefrence);
static void preferences_item_create_boolean            (NautilusPreferencesItem      *item,
							const NautilusPreference     *prefrence);
static void enum_radio_group_changed_cb                (GtkWidget                    *button_group,
							GtkWidget                    *button,
							gpointer                      user_data);
static void boolean_button_toggled_cb                  (GtkWidget                    *button_group,
							gpointer                      user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesItem, nautilus_preferences_item, GTK_TYPE_VBOX)

/* NautilusPreferencesItemClass methods */
static void
nautilus_preferences_item_initialize_class (NautilusPreferencesItemClass *preferences_item_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_item_class);
	widget_class = GTK_WIDGET_CLASS (preferences_item_class);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* Arguments */
	gtk_object_add_arg_type ("NautilusPreferencesItem::show_description",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SHOW_DESCRIPTION);

	gtk_object_add_arg_type ("NautilusPreferencesItem::description_string",
				 GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE,
				 ARG_DESCRIPTION_STRING);

	gtk_object_add_arg_type ("NautilusPreferencesItem::title_string",
				 GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE,
				 ARG_TITLE_STRING);

	/* GtkObjectClass */
	object_class->destroy = preferences_item_destroy;
	object_class->set_arg = preferences_item_set_arg;
	object_class->get_arg = preferences_item_get_arg;
}

static void
nautilus_preferences_item_initialize (NautilusPreferencesItem *item)
{
	item->details = g_new (NautilusPreferencesItemDetails, 1);

	item->details->preferences = NULL;
	item->details->preference_name = NULL;
	item->details->item_type = PREFERENCES_ITEM_UNDEFINED_ITEM;
	item->details->child = NULL;
}

/* GtkObjectClass methods */
static void
preferences_item_destroy (GtkObject *object)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

	if (item->details->preference_name)
	{
		g_free (item->details->preference_name);
	}

	if (item->details->preferences)
		gtk_object_unref (GTK_OBJECT (item->details->preferences));

	g_free (item->details);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
preferences_item_set_arg (GtkObject	*object,
			  GtkArg	*arg,
			  guint		arg_id)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

#if 0	
	switch (arg_id)
	{
	case ARG_SHOW_DESCRIPTION:
		item->details->show_description = GTK_VALUE_BOOL (*arg);

		if (item->details->show_description)
		{
			gtk_widget_show (item->details->description_label);
		}
		else
		{
			gtk_widget_hide (item->details->description_label);
		}

		break;

	case ARG_DESCRIPTION_STRING:
		
		gtk_label_set_text (GTK_LABEL (item->details->description_label),
				    GTK_VALUE_STRING (*arg));
		break;

	case ARG_TITLE_STRING:

		gtk_frame_set_label (GTK_FRAME (object), GTK_VALUE_STRING (*arg));

		break;
	}
#endif

}

static void
preferences_item_get_arg (GtkObject	*object,
			  GtkArg	*arg,
			  guint		arg_id)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

#if 0	
	switch (arg_id)
	{
	case ARG_SHOW_DESCRIPTION:
 		GTK_VALUE_BOOL (*arg) = 
			GTK_WIDGET_VISIBLE (item->details->description_label);
		break;
	}
#endif
}

/*
 * Private stuff
 */
static void
preferences_item_construct (NautilusPreferencesItem	*item,
			    NautilusPreferences	*preferences,
			    const gchar			*preference_name,
			    NautilusPreferencesItemType	item_type)
{
	const NautilusPreference	*preference;

	g_assert (item != NULL);

	g_assert (preferences != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES (preferences));

	g_assert (preference_name != NULL);
	g_assert (item_type != PREFERENCES_ITEM_UNDEFINED_ITEM);

	g_assert (item->details->child == NULL);

	g_assert (item->details->preference_name == NULL);
	
	item->details->preference_name = g_strdup (preference_name);

	item->details->preferences = preferences;
	
	gtk_object_ref (GTK_OBJECT (item->details->preferences));
	
	preference = nautilus_preferences_get_preference (preferences,
							  item->details->preference_name);
	
	g_assert (preference != NULL);

	/* Create the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_create_boolean (item, preference);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUM:
		preferences_item_create_enum (item, preference);
		break;
	}

	gtk_object_unref (GTK_OBJECT (preference));

	g_assert (item->details->child != NULL);

	gtk_box_pack_start (GTK_BOX (item),
			    item->details->child,
			    FALSE,
			    FALSE,
			    0);
	
	gtk_widget_show (item->details->child);
}

static void
preferences_item_create_enum (NautilusPreferencesItem	*item,
			      const NautilusPreference	*preference)
{
	guint	i;
	gint	value;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);

	item->details->child = nautilus_radio_button_group_new ();
		
 	value = nautilus_preferences_get_enum (item->details->preferences, 
					       item->details->preference_name, 
					       0);
	
	for (i = 0; i < nautilus_preference_enum_get_num_entries (preference); i++) {
		char *description;
		
		description = nautilus_preference_enum_get_nth_entry_description (preference, i);

		g_assert (description != NULL);

		nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child),
						    description);

		g_free (description);
		
		if (value == nautilus_preference_enum_get_nth_entry_value (preference, i)) {
			
			nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}
	
	gtk_signal_connect (GTK_OBJECT (item->details->child),
			    "changed",
			    GTK_SIGNAL_FUNC (enum_radio_group_changed_cb),
			    (gpointer) item);
}

static void
preferences_item_create_boolean (NautilusPreferencesItem	*item,
				 const NautilusPreference	*preference)
{
	gboolean	value;
	char		*description;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preference_get_description (preference);

	g_assert (description != NULL);

	item->details->child = gtk_check_button_new_with_label (description);

	g_free (description);

	value = nautilus_preferences_get_boolean (item->details->preferences,
						  item->details->preference_name,
						  FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->details->child), value);
				      
	gtk_signal_connect (GTK_OBJECT (item->details->child),
			    "toggled",
			    GTK_SIGNAL_FUNC (boolean_button_toggled_cb),
			    (gpointer) item);
}

/* NautilusPreferencesItem public methods */
GtkWidget *
nautilus_preferences_item_new (const NautilusPreferences	*preferences,
			       const gchar			*preference_name,
			       NautilusPreferencesItemType	item_type)
{
	NautilusPreferencesItem * item;

	g_return_val_if_fail (preferences != NULL, NULL);
	g_return_val_if_fail (preference_name != NULL, NULL);

	item = gtk_type_new (nautilus_preferences_item_get_type ());

	/* Cast away the constness so that the preferences object can be
	 * refed in this object. */
	preferences_item_construct (item,
				    (NautilusPreferences *) preferences, 
				    preference_name, 
				    item_type);

	return GTK_WIDGET (item);
}

static void
enum_radio_group_changed_cb (GtkWidget *buttons, GtkWidget * button, gpointer user_data)
{
	NautilusPreferencesItem		*item = (NautilusPreferencesItem *) user_data;
	const NautilusPreference	*preference;
	gint				i;

	g_assert (item != NULL);
	g_assert (item->details->preference_name != NULL);
	g_assert (item->details->preferences != NULL);

	preference = nautilus_preferences_get_preference (item->details->preferences,
							  item->details->preference_name);

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	nautilus_preferences_set_enum (NAUTILUS_PREFERENCES (item->details->preferences),
				       item->details->preference_name,
				       nautilus_preference_enum_get_nth_entry_value (preference, i));
}

static void
boolean_button_toggled_cb (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item = (NautilusPreferencesItem *) user_data;
	gboolean		active_state;

	g_assert (item != NULL);

	active_state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES (item->details->preferences),
					  item->details->preference_name,
					  active_state);
}
