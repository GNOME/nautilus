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

#include <config.h>
#include "nautilus-preferences-item.h"

#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"
#include <eel/eel-art-gtk-extensions.h>
#include <eel/eel-enumeration.h>
#include <eel/eel-font-picker.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-radio-button-group.h>
#include <eel/eel-string-picker.h>
#include <eel/eel-string.h>
#include <eel/eel-text-caption.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

static const guint PREFERENCES_ITEM_TITLE_SPACING = 4;
static const guint PREFERENCES_ITEM_FRAME_BORDER_WIDTH = 6;
static const NautilusPreferencesItemType PREFERENCES_ITEM_UNDEFINED_ITEM = -1U;
static gboolean text_idle_handler = FALSE;
static gboolean integer_idle_handler = FALSE;

typedef struct
{
	GtkWidget *widget;
	guint signal_id;
} PreferencesItemConnection;

struct NautilusPreferencesItemDetails
{
	char *preference_name;
	NautilusPreferencesItemType item_type;
	GtkWidget *child;
	GSList *change_signal_connections;
	char *control_preference_name;
	NautilusPreferencesItemControlAction control_action;
};

/* GtkObjectClass methods */
static void nautilus_preferences_item_initialize_class                (NautilusPreferencesItemClass *preferences_item_class);
static void nautilus_preferences_item_initialize                      (NautilusPreferencesItem      *preferences_item);
static void preferences_item_destroy                                  (GtkObject                    *object);

/* Private stuff */
static void preferences_item_create_boolean                           (NautilusPreferencesItem      *item);
static void preferences_item_create_editable_integer                  (NautilusPreferencesItem      *item);
static void preferences_item_create_editable_string                   (NautilusPreferencesItem      *item);
static void preferences_item_create_enumeration_menu                  (NautilusPreferencesItem      *item);
static void preferences_item_create_enumeration_radio                 (NautilusPreferencesItem      *item,
								       gboolean                      horizontal);
static void       preferences_item_create_enumeration_list                  (NautilusPreferencesItem       *item,
									     gboolean                       horizontal);
static void       preferences_item_create_font                              (NautilusPreferencesItem       *item);
static void preferences_item_create_font                              (NautilusPreferencesItem      *item);
static void preferences_item_create_padding                           (NautilusPreferencesItem      *item);
static void preferences_item_create_smooth_font                       (NautilusPreferencesItem      *item);
static void preferences_item_update_displayed_value                   (NautilusPreferencesItem      *preferences_item);
static void preferences_item_update_editable_integer_settings_at_idle (NautilusPreferencesItem      *preferences_item);
static void preferences_item_update_text_settings_at_idle             (NautilusPreferencesItem      *preferences_item);

/* User triggered item changed callbacks */
static void enumeration_radio_changed_callback                        (GtkWidget                    *button_group,
								       GtkWidget                    *button,
								       gpointer                      user_data);
static void boolean_button_toggled_callback                           (GtkWidget                    *button_group,
								       gpointer                      user_data);
static void editable_string_changed_callback                          (GtkWidget                    *caption,
								       gpointer                      user_data);
static void editable_integer_changed_callback                         (GtkWidget                    *caption,
								       gpointer                      user_data);
static void enumeration_menu_changed_callback                         (EelStringPicker              *string_picker,
								       NautilusPreferencesItem      *item);
static void font_changed_callback                                     (GtkWidget                    *caption,
								       gpointer                      user_data);
static void       enumeration_list_changed_callback                         (EelStringPicker               *string_picker,
									     NautilusPreferencesItem       *item);
static void smooth_font_changed_callback                              (EelFontPicker                *font_picker,
								       gpointer                      callback_data);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesItem, nautilus_preferences_item, GTK_TYPE_VBOX)

/* NautilusPreferencesItemClass methods */
static void
nautilus_preferences_item_initialize_class (NautilusPreferencesItemClass *preferences_item_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_item_class);
	widget_class = GTK_WIDGET_CLASS (preferences_item_class);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkObjectClass */
	object_class->destroy = preferences_item_destroy;
}

static void
nautilus_preferences_item_initialize (NautilusPreferencesItem *item)
{
	item->details = g_new0 (NautilusPreferencesItemDetails, 1);
	item->details->item_type = PREFERENCES_ITEM_UNDEFINED_ITEM;
}

/* GtkObjectClass methods */
static void
preferences_item_destroy (GtkObject *object)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

	g_free (item->details->preference_name);
	g_free (item->details->control_preference_name);
	eel_g_slist_free_deep (item->details->change_signal_connections);
	g_free (item->details);

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * Private stuff
 */
static void
preferences_item_update_enumeration_radio (NautilusPreferencesItem *item)
{
	int value;
	char *enumeration_id;
	guint i;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
			  || item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO);

 	value = nautilus_preferences_get_integer (item->details->preference_name);

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);
	
	/* Set the active button */
	for (i = 0; i < eel_enumeration_id_get_length (enumeration_id); i++) {
		if (value == eel_enumeration_id_get_nth_value (enumeration_id, i)) {
			eel_radio_button_group_set_active_index (EEL_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}

 	g_free (enumeration_id);
}

static void
preferences_item_update_enumeration_list (NautilusPreferencesItem *item)
{
	char *enumeration_id;
	const GSList *node;
	EelStringList *value;
	char *nth_value_name;
	char *nth_value_description;
	guint i;
	int position;
	const PreferencesItemConnection *connection;

	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL
			  || item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_HORIZONTAL);
	
	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);

 	value = nautilus_preferences_get_string_list (item->details->preference_name);

	g_return_if_fail (eel_string_list_get_length (value)
			  == g_slist_length (item->details->change_signal_connections));

	for (node = item->details->change_signal_connections, i = 0; node != NULL; node = node->next, i++) {
		g_assert (node->data != NULL);

		connection = node->data;
		g_assert (EEL_IS_STRING_PICKER (connection->widget));
		
		nth_value_name = eel_string_list_nth (value, i);
		
		position = eel_enumeration_id_get_name_position (enumeration_id,
								 nth_value_name);
		
		nth_value_description = eel_enumeration_id_get_nth_description (enumeration_id,
										position);
		
		eel_string_picker_set_selected_string (EEL_STRING_PICKER (connection->widget),
						       nth_value_description);
		
		g_free (nth_value_name);
		g_free (nth_value_description);
	}

	eel_string_list_free (value);
 	g_free (enumeration_id);
}

/* This callback is called whenever the preference value changes, so that we can
 * update the item widgets accordingly.
 */
static void
preferences_item_value_changed_callback (gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (callback_data));
	
	preferences_item_update_displayed_value (NAUTILUS_PREFERENCES_ITEM (callback_data));
}

static void
preferences_item_set_main_child (NautilusPreferencesItem *item,
				 GtkWidget *child)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (item->details->child == NULL);

	if (item->details->item_type != NAUTILUS_PREFERENCE_ITEM_PADDING) {
		nautilus_preferences_add_callback_while_alive (item->details->preference_name,
							       preferences_item_value_changed_callback,
							       item,
							       GTK_OBJECT (item));
	}

	gtk_box_pack_start (GTK_BOX (item),
			    child,
			    FALSE,
			    FALSE,
			    0);
	
	gtk_widget_show (child);

	item->details->child = child;
}

static void
preferences_item_add_connection_child (NautilusPreferencesItem *item,
				       GtkWidget *child,
				       const char *signal_name,
				       GtkSignalFunc signal)
{
	PreferencesItemConnection *connection;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (eel_strlen (signal_name) > 0);
	g_return_if_fail (signal != NULL);

	connection = g_new0 (PreferencesItemConnection, 1);
	connection->widget = child;
	connection->signal_id = gtk_signal_connect (GTK_OBJECT (child),
						    signal_name,
						    signal,
						    item);
	
	item->details->change_signal_connections = g_slist_append (
		item->details->change_signal_connections, connection);
}

static void
preferences_item_create_enumeration_radio (NautilusPreferencesItem *item,
					   gboolean horizontal)
{
 	guint i;
	char *enumeration_id;
	char *description;
	GtkWidget *child;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);
	
	child = eel_radio_button_group_new (horizontal);

	/* Populate the radio group */
	for (i = 0; i < eel_enumeration_id_get_length (enumeration_id); i++) {
		description = eel_enumeration_id_get_nth_description (enumeration_id, i);
		g_assert (description != NULL);
		
		eel_radio_button_group_insert (EEL_RADIO_BUTTON_GROUP (child),
					       description);
		g_free (description);
	}
 	g_free (enumeration_id);
	
	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (enumeration_radio_changed_callback));

	preferences_item_set_main_child (item, child);
}

static void
preferences_item_create_enumeration_list (NautilusPreferencesItem *item,
					  gboolean horizontal)
{
 	guint i;
 	guint j;
	char *enumeration_id;
	char *description;
	char *enum_description;
	EelStringList *default_value;
	guint num_pickers;
	GtkWidget *string_picker;
	GtkWidget *picker_box;
	GtkWidget *title;
	GtkWidget *child;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	g_return_if_fail (eel_strlen (description) > 0);

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);

	/* FIXME: Hard coded user level */
	default_value = nautilus_preferences_default_get_string_list (item->details->preference_name, 0);

	num_pickers = eel_string_list_get_length (default_value);
	g_return_if_fail (num_pickers > 0);
	
	child = gtk_vbox_new (FALSE, 4);
	picker_box = horizontal ? gtk_hbox_new (FALSE, 4): gtk_vbox_new (FALSE, 4);

	title = gtk_label_new (description);
	gtk_misc_set_alignment (GTK_MISC (title), 0.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (child),
			    title,
			    FALSE,
			    FALSE,
			    0);

	gtk_box_pack_start (GTK_BOX (child),
			    picker_box,
			    TRUE,
			    TRUE,
			    0);

	gtk_widget_show (title);
	gtk_widget_show (picker_box);
	
	/* Populate the radio group */
	for (j = 0; j < num_pickers; j++) {
		string_picker = eel_string_picker_new ();
		eel_caption_set_show_title (EEL_CAPTION (string_picker), FALSE);
		
		for (i = 0; i < eel_enumeration_id_get_length (enumeration_id); i++) {
			enum_description = eel_enumeration_id_get_nth_description (enumeration_id, i);
			g_assert (enum_description != NULL);
			
			eel_string_picker_insert_string (EEL_STRING_PICKER (string_picker), enum_description);
			g_free (enum_description);
		}

		gtk_box_pack_start (GTK_BOX (picker_box),
				    string_picker,
				    FALSE,
				    FALSE,
				    0);
		
		gtk_widget_show (string_picker);

		preferences_item_add_connection_child (item,
						       string_picker,
						       "changed",
						       GTK_SIGNAL_FUNC (enumeration_list_changed_callback));

	}
 	g_free (enumeration_id);

	preferences_item_set_main_child (item, child);
}

static void
preferences_item_update_boolean (NautilusPreferencesItem *item)
{
	gboolean value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	value = nautilus_preferences_get_boolean (item->details->preference_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->details->child), value);
}

static void
preferences_item_create_boolean (NautilusPreferencesItem *item)
{
	char *description;
	GtkWidget *child;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	g_return_if_fail (eel_strlen (description) > 0);

	child = gtk_check_button_new_with_label (description);
	gtk_label_set_justify (GTK_LABEL (GTK_BIN (child)->child), GTK_JUSTIFY_LEFT);
	g_free (description);
	
	preferences_item_add_connection_child (item,
					       child,
					       "toggled",
					       GTK_SIGNAL_FUNC (boolean_button_toggled_callback));
				      
	preferences_item_set_main_child (item, child);
}

static void
preferences_item_update_editable_string (NautilusPreferencesItem *item)
{
	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);

	current_value = nautilus_preferences_get (item->details->preference_name);

	g_assert (current_value != NULL);
	eel_text_caption_set_text (EEL_TEXT_CAPTION (item->details->child), current_value);
	g_free (current_value);
}

static void
preferences_item_create_editable_string (NautilusPreferencesItem *item)
{
	char *description;
	GtkWidget *child;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	g_assert (description != NULL);
	
	child = eel_text_caption_new ();

	/* FIXME This is a special case for the home uri preference,
	   in the future this should be generalized. */
	if (g_strcasecmp (item->details->preference_name, NAUTILUS_PREFERENCES_HOME_URI) == 0)
	{
		eel_text_caption_set_expand_tilde (EEL_TEXT_CAPTION (child), TRUE);
	}

	eel_caption_set_title_label (EEL_CAPTION (child), description);

	g_free (description);
	
	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (editable_string_changed_callback));

	preferences_item_set_main_child (item, child);
}

static void
preferences_item_update_editable_integer (NautilusPreferencesItem *item)
{
	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER);

	current_value = g_strdup_printf ("%d", nautilus_preferences_get_integer (item->details->preference_name));

	g_assert (current_value != NULL);
	eel_text_caption_set_text (EEL_TEXT_CAPTION (item->details->child), current_value);
	g_free (current_value);
}

static void
preferences_item_create_editable_integer (NautilusPreferencesItem *item)
{
	char *description;
	GtkWidget *child;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	
	g_assert (description != NULL);
	
	child = eel_text_caption_new ();

	eel_caption_set_title_label (EEL_CAPTION (child), description);

	g_free (description);

	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (editable_integer_changed_callback));

	preferences_item_set_main_child (item, child);
}

static void
preferences_item_update_enumeration_menu (NautilusPreferencesItem *item)
{
	char *current_label;
	int current_value;
	int position;
	char *enumeration_id;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU);

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);

	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);

	current_value = nautilus_preferences_get_integer (item->details->preference_name);

	position = eel_enumeration_id_get_value_position (enumeration_id,
							  current_value);
	g_return_if_fail (position != EEL_STRING_LIST_NOT_FOUND);
	
	current_label = eel_enumeration_id_get_nth_description (enumeration_id,
								position);
	
	if (eel_string_picker_contains (EEL_STRING_PICKER (item->details->child), current_label)) {
		eel_string_picker_set_selected_string (EEL_STRING_PICKER (item->details->child),
						       current_label);
	} else {
		g_warning ("Value string for %s is %s, which isn't in the expected set of values",
			   item->details->preference_name,
			   current_label);
	}
	
 	g_free (enumeration_id);
	g_free (current_label);
}

static void
preferences_item_create_enumeration_menu (NautilusPreferencesItem *item)
{
 	guint i;
	char *enumeration_id;
	char *description;
	GtkWidget *child;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);
	
	description = nautilus_preferences_get_description (item->details->preference_name);
	g_return_if_fail (description != NULL);
	
	child = eel_string_picker_new ();
	eel_caption_set_title_label (EEL_CAPTION (child), description);
	g_free (description);

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);
	
	/* Populate the string picker */
	for (i = 0; i < eel_enumeration_id_get_length (enumeration_id); i++) {
		description = eel_enumeration_id_get_nth_description (enumeration_id, i);
		g_assert (description != NULL);
		
		eel_string_picker_insert_string (EEL_STRING_PICKER (child), description);
		
		g_free (description);
	}
 	g_free (enumeration_id);

	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (enumeration_menu_changed_callback));

	preferences_item_set_main_child (item, child);
}

static void
preferences_item_update_font (NautilusPreferencesItem *item)
{
	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_FONT);

	current_value = nautilus_preferences_get (item->details->preference_name);
	g_assert (current_value != NULL);

	/* The value of the gconf preference can be anything.  In theory garbage could
	 * be used for the preference using a third party tool.  So we make sure that
	 * it is one of the choice before trying to select it, otherwise we would get
	 * assertions.
	 */
	if (eel_string_picker_contains (EEL_STRING_PICKER (item->details->child), current_value)) {
		eel_string_picker_set_selected_string (EEL_STRING_PICKER (item->details->child),
							    current_value);
	}

	g_free (current_value);
}

static void
preferences_item_create_font (NautilusPreferencesItem *item)
{
	char *description;
	EelStringList *font_list;
	GtkWidget *child;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	g_return_if_fail (description != NULL);
	
	child = eel_string_picker_new ();
	eel_caption_set_title_label (EEL_CAPTION (child), description);
	
	g_free (description);

	/* FIXME bugzilla.eazel.com 1274: Need to query system for available fonts */
	font_list = eel_string_list_new (TRUE);

	/* Once upon a time we had a bug in Nautilus that caused crashes with the "fixed" 
	 * font.  That bug (2256) was fixed by removing the "fixed" choice from this menu
	 * below.  Subsequently we fixed many font bugs in nautilus (such hard coded font sizes)
	 * that would cause both crashes and ugliness. Bug 2256 seems to have been fixed by
	 * these changes as well.
	 *
	 * Anyhow, the "fixed" font choice is not very interesting because the other fonts 
	 * look much better.  However, in multi byte locales, the fixed font is usually the
	 * only one that is available at the right encoding.
	 */

	/* FIXME bugzilla.eazel.com 7907: 
	 * The "GTK System Font" string is hard coded in many places.
	 */
	eel_string_list_insert (font_list, "GTK System Font");
	eel_string_list_insert (font_list, "fixed");
	eel_string_list_insert (font_list, "helvetica");
	eel_string_list_insert (font_list, "times");
	eel_string_list_insert (font_list, "courier");
	eel_string_list_insert (font_list, "lucida");

	eel_string_picker_set_string_list (EEL_STRING_PICKER (child), font_list);
	eel_string_list_free (font_list);
	
	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (font_changed_callback));
	
	preferences_item_set_main_child (item, child);
}

static void
preferences_item_create_padding (NautilusPreferencesItem *item)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	preferences_item_set_main_child (item, gtk_label_new (""));
}

static void
preferences_item_update_smooth_font (NautilusPreferencesItem *item)
{
 	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT);

 	current_value = nautilus_preferences_get (item->details->preference_name);
 	g_assert (current_value != NULL);

 	eel_font_picker_set_selected_font (EEL_FONT_PICKER (item->details->child),
						current_value);
 	g_free (current_value);
}

static void
smooth_font_changed_callback (EelFontPicker *font_picker,
			      gpointer callback_data)
{
	NautilusPreferencesItem *item;
 	char *new_value;

	g_return_if_fail (EEL_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (callback_data));

	item = NAUTILUS_PREFERENCES_ITEM (callback_data);
 	new_value = eel_font_picker_get_selected_font (EEL_FONT_PICKER (item->details->child));
 	g_assert (new_value != NULL);
	nautilus_preferences_set (item->details->preference_name, new_value);
 	g_free (new_value);
}

static void
preferences_item_create_smooth_font (NautilusPreferencesItem *item)
{
	char *description = NULL;
	GtkWidget *child;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (eel_strlen (item->details->preference_name) > 0);

	description = nautilus_preferences_get_description (item->details->preference_name);
	g_return_if_fail (description != NULL);
	
	child = eel_font_picker_new ();
	eel_caption_set_title_label (EEL_CAPTION (child), description);
	g_free (description);

	preferences_item_add_connection_child (item,
					       child,
					       "changed",
					       GTK_SIGNAL_FUNC (smooth_font_changed_callback));

	preferences_item_set_main_child (item, child);
}

/* NautilusPreferencesItem public methods */
GtkWidget *
nautilus_preferences_item_new (const char *preference_name,
			       NautilusPreferencesItemType item_type)
{
	NautilusPreferencesItem *item;

	g_return_val_if_fail (eel_strlen (preference_name) > 0, NULL);
	g_return_val_if_fail (item_type >= NAUTILUS_PREFERENCE_ITEM_BOOLEAN, FALSE);
	g_return_val_if_fail (item_type <= NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT, FALSE);

	item = NAUTILUS_PREFERENCES_ITEM
		(gtk_widget_new (nautilus_preferences_item_get_type (), NULL));

	item->details->preference_name = g_strdup (preference_name);
	item->details->item_type = item_type;

	/* Create the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_create_boolean (item);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO:
		preferences_item_create_enumeration_radio (item, FALSE);
		break;

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO:
		preferences_item_create_enumeration_radio (item, TRUE);
		break;

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL:
		preferences_item_create_enumeration_list (item, FALSE);
		break;

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_HORIZONTAL:
		preferences_item_create_enumeration_list (item, TRUE);
		break;

	case NAUTILUS_PREFERENCE_ITEM_FONT:
		preferences_item_create_font (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT:
		preferences_item_create_smooth_font (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING:
		preferences_item_create_editable_string (item);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER:
		preferences_item_create_editable_integer (item);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU:
		preferences_item_create_enumeration_menu (item);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_PADDING:
		preferences_item_create_padding (item);
		break;	
	}

	g_return_val_if_fail (GTK_IS_WIDGET (item->details->child), NULL);

	preferences_item_update_displayed_value (item);

	return GTK_WIDGET (item);
}

static void
enumeration_radio_changed_callback (GtkWidget *buttons, GtkWidget * button, gpointer user_data)
{
	NautilusPreferencesItem *item;
	int i;
	char *enumeration_id;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->preference_name != NULL);

	i = eel_radio_button_group_get_active_index (EEL_RADIO_BUTTON_GROUP (buttons));

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail ((guint)i < eel_enumeration_id_get_length (enumeration_id));
	
	nautilus_preferences_set_integer (item->details->preference_name,
					  eel_enumeration_id_get_nth_value (enumeration_id, i));
	g_free (enumeration_id);
}

static void
boolean_button_toggled_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem *item;
	gboolean		active_state;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	active_state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	nautilus_preferences_set_boolean (item->details->preference_name, active_state);
}

static void
font_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	NautilusPreferencesItem *item;
	char *selected_string;

	g_return_if_fail (EEL_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_return_if_fail (item->details->preference_name != NULL);

	selected_string = eel_string_picker_get_selected_string (EEL_STRING_PICKER (string_picker));
	g_return_if_fail (selected_string != NULL);

	nautilus_preferences_set (item->details->preference_name, selected_string);

	g_free (selected_string);
}

static void
editable_string_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem *item;
	
	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (EEL_IS_TEXT_CAPTION (item->details->child));

	preferences_item_update_text_settings_at_idle (item);
}

static void
editable_integer_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem *item;
	
	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (EEL_IS_TEXT_CAPTION (item->details->child));

	preferences_item_update_editable_integer_settings_at_idle (item);
}

static void
enumeration_menu_changed_callback (EelStringPicker *string_picker,
				   NautilusPreferencesItem *item)
{
 	char *selected_label;
	int position;
	int new_value;
	char *enumeration_id;

	g_return_if_fail (EEL_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);

 	selected_label = eel_string_picker_get_selected_string (string_picker);
	g_return_if_fail (selected_label != NULL);

	position = eel_enumeration_id_get_description_position (enumeration_id,
								selected_label);
	g_free (selected_label);
	g_return_if_fail (position != EEL_STRING_LIST_NOT_FOUND);
	
	new_value = eel_enumeration_id_get_nth_value (enumeration_id,
						      position);
	
	nautilus_preferences_set_integer (item->details->preference_name, new_value);

 	g_free (enumeration_id);
}

static void
enumeration_list_changed_callback (EelStringPicker *string_picker,
				   NautilusPreferencesItem *item)
{
	const GSList *node;
	const PreferencesItemConnection *connection;
	char *selected_label;
	char *enumeration_id;
	int position;
	char *new_value_nth_name;
	EelStringList *new_value;

	g_return_if_fail (EEL_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	enumeration_id = nautilus_preferences_get_enumeration_id (item->details->preference_name);
	g_return_if_fail (eel_strlen (enumeration_id) > 0);
	g_return_if_fail (eel_enumeration_id_get_length (enumeration_id) > 0);

	new_value = eel_string_list_new (TRUE);

	for (node = item->details->change_signal_connections; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		connection = node->data;
		g_assert (EEL_IS_STRING_PICKER (connection->widget));

		selected_label = eel_string_picker_get_selected_string (EEL_STRING_PICKER (connection->widget));
		g_return_if_fail (selected_label != NULL);

		position = eel_enumeration_id_get_description_position (enumeration_id,
									selected_label);
		g_free (selected_label);
		g_return_if_fail (position != EEL_STRING_LIST_NOT_FOUND);
		
		new_value_nth_name = eel_enumeration_id_get_nth_name (enumeration_id,
								      position);

		eel_string_list_insert (new_value, new_value_nth_name);

		g_free (new_value_nth_name);
	}

	g_return_if_fail (eel_string_list_get_length (new_value)
			  == g_slist_length (item->details->change_signal_connections));

	nautilus_preferences_set_string_list (item->details->preference_name, new_value);

	eel_string_list_free (new_value);

 	g_free (enumeration_id);

// 	{
// 		EelStringList *foo;
// 		foo = nautilus_preferences_get_string_list (item->details->preference_name);
// 		g_print ("new_value = %s\n", eel_string_list_as_string (foo, ",", EEL_STRING_LIST_ALL_STRINGS));
// 	}
}

char *
nautilus_preferences_item_get_name (const NautilusPreferencesItem *preferences_item)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item), NULL);

	return g_strdup (preferences_item->details->preference_name);
}

static void
preferences_item_update_displayed_value (NautilusPreferencesItem *item)
{
	NautilusPreferencesItemType item_type;
	const GSList *node;
	const PreferencesItemConnection *connection;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	item_type = item->details->item_type;

	g_return_if_fail (item->details->item_type != PREFERENCES_ITEM_UNDEFINED_ITEM);

	/* Block the change signals while we update the widget to match the preference */
	for (node = item->details->change_signal_connections; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		connection = node->data;
		g_assert (GTK_IS_WIDGET (connection->widget));
		
		gtk_signal_handler_block (GTK_OBJECT (connection->widget),
					  connection->signal_id);
	}

	/* Update the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_update_boolean (item);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO:
	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO:
		preferences_item_update_enumeration_radio (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL:
	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_HORIZONTAL:
		preferences_item_update_enumeration_list (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_FONT:
		preferences_item_update_font (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT:
		preferences_item_update_smooth_font (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING:
		preferences_item_update_editable_string (item);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER:
		preferences_item_update_editable_integer (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU:
		preferences_item_update_enumeration_menu (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_PADDING:
		break;
	default:
		g_assert_not_reached ();
	}

	for (node = item->details->change_signal_connections; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		connection = node->data;
		g_assert (GTK_IS_WIDGET (connection->widget));
		
		gtk_signal_handler_unblock (GTK_OBJECT (connection->widget),
					    connection->signal_id);
	}
}

static gboolean
update_text_settings_at_idle (NautilusPreferencesItem *preferences_item)
{
	char *text;

	text = eel_text_caption_get_text (EEL_TEXT_CAPTION (preferences_item->details->child));

	if (text != NULL) {
		nautilus_preferences_set (preferences_item->details->preference_name, text);
		g_free (text);
	}
	
	text_idle_handler = FALSE;

	return FALSE;
}

static void
preferences_item_update_text_settings_at_idle (NautilusPreferencesItem *preferences_item)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item));

	if (text_idle_handler == FALSE) {
		gtk_idle_add ((GtkFunction) update_text_settings_at_idle, preferences_item);
		text_idle_handler = TRUE;
	}
}

static gboolean
update_integer_settings_at_idle (NautilusPreferencesItem *preferences_item)
{
	int value = 0;
	char *text;

	text = eel_text_caption_get_text (EEL_TEXT_CAPTION (preferences_item->details->child));

	if (text != NULL) {
		eel_eat_str_to_int (text, &value);
	}
	
	nautilus_preferences_set_integer (preferences_item->details->preference_name, value);

	integer_idle_handler = FALSE;

	return FALSE;
}

static void
preferences_item_update_editable_integer_settings_at_idle (NautilusPreferencesItem *preferences_item)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item));

	if (integer_idle_handler == FALSE) {
		gtk_idle_add ((GtkFunction) update_integer_settings_at_idle, preferences_item);
		integer_idle_handler = TRUE;
	}
}

void
nautilus_preferences_item_set_control_preference (NautilusPreferencesItem *preferences_item,
						  const char *control_preference_name)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item));

	if (eel_str_is_equal (preferences_item->details->control_preference_name,
				   control_preference_name)) {
		return;
	}

	g_free (preferences_item->details->control_preference_name);
	preferences_item->details->control_preference_name = g_strdup (control_preference_name);
}

void
nautilus_preferences_item_set_control_action (NautilusPreferencesItem *preferences_item,
					      NautilusPreferencesItemControlAction control_action)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item));
	g_return_if_fail (control_action >= NAUTILUS_PREFERENCE_ITEM_SHOW);
	g_return_if_fail (control_action <= NAUTILUS_PREFERENCE_ITEM_HIDE);

	if (preferences_item->details->control_action == control_action) {
		return;
	}

	preferences_item->details->control_action = control_action;
}

static gboolean
preferences_item_get_control_showing (const NautilusPreferencesItem *preferences_item)
{
	gboolean value;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item), FALSE);

	if (preferences_item->details->control_preference_name == NULL) {
		return TRUE;
	}

	value = nautilus_preferences_get_boolean (preferences_item->details->control_preference_name);

	if (preferences_item->details->control_action == NAUTILUS_PREFERENCE_ITEM_SHOW) {
		return value;
	}

	if (preferences_item->details->control_action == NAUTILUS_PREFERENCE_ITEM_HIDE) {
		return !value;
	}

	return !value;
}

gboolean
nautilus_preferences_item_child_is_caption (const NautilusPreferencesItem *item)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item), FALSE);

	return EEL_IS_CAPTION (item->details->child);
}

int
nautilus_preferences_item_get_child_width (const NautilusPreferencesItem *item)
{
	EelDimensions child_dimensions;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item), 0);

	if (item->details->child == NULL) {
		return 0;
	}

	child_dimensions = eel_gtk_widget_get_preferred_dimensions (item->details->child);
	
	return child_dimensions.width;
}

void
nautilus_preferences_item_set_caption_extra_spacing (NautilusPreferencesItem *item,
					             int extra_spacing)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (extra_spacing >= 0);

	if (!nautilus_preferences_item_child_is_caption (item)) {
		return;
	}

	eel_caption_set_extra_spacing (EEL_CAPTION (item->details->child), 
					    extra_spacing);
}

gboolean
nautilus_preferences_item_is_showing (const NautilusPreferencesItem *item)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item), FALSE);
	
	if (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_PADDING) {
		return TRUE;
	} else if (nautilus_preferences_is_visible (item->details->preference_name)) {
		return preferences_item_get_control_showing (item);
	}
	
	return FALSE;
}

void
nautilus_preferences_item_update_showing (NautilusPreferencesItem *item)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	eel_gtk_widget_set_shown (GTK_WIDGET (item),
				  nautilus_preferences_item_is_showing (item));
}
