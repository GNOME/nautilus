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
#include "nautilus-preferences.h"
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"
#include <libgnomevfs/gnome-vfs.h>

#include <libgnome/gnome-i18n.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>

#include "nautilus-radio-button-group.h"
#include "nautilus-string-picker.h"
#include "nautilus-font-picker.h"
#include "nautilus-text-caption.h"

#include "nautilus-global-preferences.h"

static const guint PREFERENCES_ITEM_TITLE_SPACING = 4;
static const guint PREFERENCES_ITEM_FRAME_BORDER_WIDTH = 6;
static const NautilusPreferencesItemType PREFERENCES_ITEM_UNDEFINED_ITEM = -1U;
static gboolean text_idle_handler = FALSE;
static gboolean integer_idle_handler = FALSE;

struct _NautilusPreferencesItemDetails
{
	char *preference_name;
	NautilusPreferencesItemType item_type;
	GtkWidget *child;
	guint change_signal_ID;
	char *control_preference_name;
	NautilusPreferencesItemControlAction control_action;
};

/* GtkObjectClass methods */
static void nautilus_preferences_item_initialize_class       (NautilusPreferencesItemClass *preferences_item_class);
static void nautilus_preferences_item_initialize             (NautilusPreferencesItem      *preferences_item);
static void preferences_item_destroy                         (GtkObject                    *object);

/* Private stuff */
static void preferences_item_construct                       (NautilusPreferencesItem      *item,
							      const char                   *preference_name,
							      NautilusPreferencesItemType   item_type);
static void preferences_item_create_enum                     (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_short_enum               (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_boolean                  (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_editable_string          (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_integer                  (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_font_family              (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_create_smooth_font              (NautilusPreferencesItem      *item,
							      const char                   *preference_name);
static void preferences_item_update_text_settings_at_idle    (NautilusPreferencesItem      *preferences_item);
static void preferences_item_update_integer_settings_at_idle (NautilusPreferencesItem      *preferences_item);
static void enum_radio_group_changed_callback                (GtkWidget                    *button_group,
							      GtkWidget                    *button,
							      gpointer                      user_data);
static void boolean_button_toggled_callback                  (GtkWidget                    *button_group,
							      gpointer                      user_data);
static void text_item_changed_callback                       (GtkWidget                    *string_picker,
							      gpointer                      user_data);
static void editable_string_changed_callback                 (GtkWidget                    *caption,
							      gpointer                      user_data);
static void integer_changed_callback                         (GtkWidget                    *caption,
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
	g_free (item->details);
	
	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * Private stuff
 */
static void
preferences_item_construct (NautilusPreferencesItem	*item,
			    const char			*preference_name,
			    NautilusPreferencesItemType	item_type)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	g_return_if_fail (nautilus_strlen (preference_name) > 0);
	g_assert (item_type != PREFERENCES_ITEM_UNDEFINED_ITEM);

	g_assert (item->details->child == NULL);

	g_assert (item->details->preference_name == NULL);
	
	item->details->preference_name = g_strdup (preference_name);
	item->details->item_type = item_type;

	/* Create the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_create_boolean (item, preference_name);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUM:
		preferences_item_create_enum (item, preference_name);
		break;

	case NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM:
		preferences_item_create_short_enum (item, preference_name);
		break;

	case NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY:
		preferences_item_create_font_family (item, preference_name);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT:
		preferences_item_create_smooth_font (item, preference_name);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING:
		preferences_item_create_editable_string (item, preference_name);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_INTEGER:
		preferences_item_create_integer (item, preference_name);
		break;	
	}

	g_assert (item->details->child != NULL);
	g_assert (item->details->change_signal_ID != 0);

	nautilus_preferences_item_update_displayed_value (item);

	gtk_box_pack_start (GTK_BOX (item),
			    item->details->child,
			    FALSE,
			    FALSE,
			    0);
	
	gtk_widget_show (item->details->child);
}

static void
preferences_item_update_enum (const NautilusPreferencesItem *item)
{
	int value;
	char *preference_name;
	guint i;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_ENUM);

	preference_name = item->details->preference_name;
 	value = nautilus_preferences_get_integer (preference_name);

	for (i = 0; i < nautilus_preferences_enumeration_get_num_entries (preference_name); i++) {
		if (value == nautilus_preferences_enumeration_get_nth_value (preference_name, i)) {
			nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}
}

static void
preferences_item_create_enum (NautilusPreferencesItem	*item,
			      const char 	*preference_name)
{
	guint	i;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	g_assert (item->details->preference_name != NULL);

	item->details->child = nautilus_radio_button_group_new (FALSE);
		
	for (i = 0; i < nautilus_preferences_enumeration_get_num_entries (preference_name); i++) {
		char *description;
		
		description = nautilus_preferences_enumeration_get_nth_description (preference_name, i);
		
		g_assert (description != NULL);
		
		nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child),
						    description);

		g_free (description);
	}
	
	item->details->change_signal_ID = 
		gtk_signal_connect (GTK_OBJECT (item->details->child),
			    	    "changed",
			    	    GTK_SIGNAL_FUNC (enum_radio_group_changed_callback),
			    	    (gpointer) item);
}

static void
preferences_item_update_short_enum (const NautilusPreferencesItem *item)
{
	int value;
	char *preference_name;
	guint i;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM);

	preference_name = item->details->preference_name;
 	value = nautilus_preferences_get_integer (preference_name);

	for (i = 0; i < nautilus_preferences_enumeration_get_num_entries (preference_name); i++) {
		if (value == nautilus_preferences_enumeration_get_nth_value (preference_name, i)) {
			nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}
}

/* This is just like preferences_item_create_enum except the choices
 * are laid out horizontally instead of vertically (hence it works decently
 * only with short text for the choices).
 */
static void
preferences_item_create_short_enum (NautilusPreferencesItem	*item,
			      	    const char 	*preference_name)
{
	guint	i;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	g_assert (item->details->preference_name != NULL);

	item->details->child = nautilus_radio_button_group_new (TRUE);
		
	for (i = 0; i < nautilus_preferences_enumeration_get_num_entries (preference_name); i++) {
		char *description;
		
		description = nautilus_preferences_enumeration_get_nth_description (preference_name, i);

		g_assert (description != NULL);

		nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child),
						    description);

		g_free (description);
	}
	
	item->details->change_signal_ID = 
		gtk_signal_connect (GTK_OBJECT (item->details->child),
			    	    "changed",
			    	    GTK_SIGNAL_FUNC (enum_radio_group_changed_callback),
			    	    (gpointer) item);
}

static void
preferences_item_update_boolean (const NautilusPreferencesItem *item)
{
	gboolean value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	value = nautilus_preferences_get_boolean (item->details->preference_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->details->child), value);
}

static void
preferences_item_create_boolean (NautilusPreferencesItem	*item,
				 const char 	*preference_name)
{
	char		*description;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preferences_get_description (preference_name);

	g_assert (description != NULL);

	item->details->child = gtk_check_button_new_with_label (description);
	gtk_label_set_justify (GTK_LABEL (GTK_BIN (item->details->child)->child), GTK_JUSTIFY_LEFT);

	g_free (description);
				      
	item->details->change_signal_ID = 
		gtk_signal_connect (GTK_OBJECT (item->details->child),
			    	    "toggled",
			    	    GTK_SIGNAL_FUNC (boolean_button_toggled_callback),
			    	    (gpointer) item);
}

static void
preferences_item_update_editable_string (const NautilusPreferencesItem *item)
{
	char	*current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);

	current_value = nautilus_preferences_get (item->details->preference_name);

	g_assert (current_value != NULL);
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (item->details->child), current_value);
	g_free (current_value);
}

static void
preferences_item_create_editable_string (NautilusPreferencesItem	*item,
					 const char 	*preference_name)
{
	char	*description;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	description = nautilus_preferences_get_description (preference_name);
	g_assert (description != NULL);
	
	item->details->child = nautilus_text_caption_new ();

	/* FIXME This is a special case for the home uri preference,
	   in the future this should be generalized. */
	if (g_strcasecmp (preference_name, NAUTILUS_PREFERENCES_HOME_URI) == 0)
	{
		nautilus_text_caption_set_expand_tilde (NAUTILUS_TEXT_CAPTION (item->details->child), TRUE);
	}

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);

	g_free (description);
	
	item->details->change_signal_ID = 
		gtk_signal_connect (GTK_OBJECT (item->details->child),
	 			    "changed",
	 			    GTK_SIGNAL_FUNC (editable_string_changed_callback),
			    	    (gpointer) item);
}

static void
preferences_item_update_integer (const NautilusPreferencesItem *item)
{
	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_INTEGER);

	current_value = g_strdup_printf ("%d", nautilus_preferences_get_integer (item->details->preference_name));

	g_assert (current_value != NULL);
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (item->details->child), current_value);
	g_free (current_value);
}

static void
preferences_item_create_integer (NautilusPreferencesItem	*item,
				 const char 	*preference_name)
{
	char	*description;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	description = nautilus_preferences_get_description (preference_name);
	
	g_assert (description != NULL);
	
	item->details->child = nautilus_text_caption_new ();

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);

	g_free (description);

	item->details->change_signal_ID = 
		gtk_signal_connect (GTK_OBJECT (item->details->child),
	 			    "changed",
	 			    GTK_SIGNAL_FUNC (integer_changed_callback),
			    	    (gpointer) item);
}

static void
preferences_item_update_font_family (const NautilusPreferencesItem *item)
{
	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY);

	current_value = nautilus_preferences_get (item->details->preference_name);
	g_assert (current_value != NULL);

	nautilus_string_picker_set_selected_string (NAUTILUS_STRING_PICKER (item->details->child), current_value);

	g_free (current_value);
}

static void
preferences_item_create_font_family (NautilusPreferencesItem *item,
				     const char *preference_name)
{
	char *description;
	NautilusStringList *font_list;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	description = nautilus_preferences_get_description (preference_name);
	g_return_if_fail (description != NULL);
	
	item->details->child = nautilus_string_picker_new ();
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);
	
	g_free (description);

	/* FIXME bugzilla.eazel.com 1274: Need to query system for available fonts */
	font_list = nautilus_string_list_new (TRUE);

	nautilus_string_list_insert (font_list, _("helvetica"));
	nautilus_string_list_insert (font_list, _("times"));
	nautilus_string_list_insert (font_list, _("courier"));
	nautilus_string_list_insert (font_list, _("lucida"));

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (item->details->child), font_list);
	nautilus_string_list_free (font_list);

	item->details->change_signal_ID = gtk_signal_connect (GTK_OBJECT (item->details->child),
							      "changed",
							      GTK_SIGNAL_FUNC (text_item_changed_callback),
							      (gpointer) item);
}


static void
preferences_item_update_smooth_font (const NautilusPreferencesItem *item)
{
 	char *current_value;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (item->details->item_type == NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT);

 	current_value = nautilus_preferences_get (item->details->preference_name);
 	g_assert (current_value != NULL);

 	nautilus_font_picker_set_selected_font (NAUTILUS_FONT_PICKER (item->details->child),
						current_value);
 	g_free (current_value);
}

static void
preferences_smooth_font_changed_callback (NautilusFontPicker *font_picker,
					  gpointer callback_data)
{
	NautilusPreferencesItem *item;
 	char *new_value;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (callback_data));

	item = NAUTILUS_PREFERENCES_ITEM (callback_data);
 	new_value = nautilus_font_picker_get_selected_font (NAUTILUS_FONT_PICKER (item->details->child));
 	g_assert (new_value != NULL);
	nautilus_preferences_set (item->details->preference_name, new_value);
 	g_free (new_value);
}

static void
preferences_item_create_smooth_font (NautilusPreferencesItem *item,
				     const char *preference_name)
{
	char *description = NULL;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));
	g_return_if_fail (nautilus_strlen (preference_name) > 0);

	description = nautilus_preferences_get_description (preference_name);
	g_return_if_fail (description != NULL);
	
	item->details->child = nautilus_font_picker_new ();
	nautilus_font_picker_set_title_label (NAUTILUS_FONT_PICKER (item->details->child),
					      description);
	
	g_free (description);

	item->details->change_signal_ID = gtk_signal_connect (GTK_OBJECT (item->details->child),
							      "changed",
							      GTK_SIGNAL_FUNC (preferences_smooth_font_changed_callback),
							      item);
}

/* NautilusPreferencesItem public methods */
GtkWidget *
nautilus_preferences_item_new (const char			*preference_name,
			       NautilusPreferencesItemType	item_type)
{
	NautilusPreferencesItem * item;

	g_return_val_if_fail (preference_name != NULL, NULL);

	item = NAUTILUS_PREFERENCES_ITEM
		(gtk_widget_new (nautilus_preferences_item_get_type (), NULL));

	/* Cast away the constness so that the preferences object can be
	 * refed in this object. */
	preferences_item_construct (item, preference_name, item_type);

	return GTK_WIDGET (item);
}

static void
enum_radio_group_changed_callback (GtkWidget *buttons, GtkWidget * button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	int i;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->preference_name != NULL);

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));
	
	nautilus_preferences_set_integer (item->details->preference_name,
					  nautilus_preferences_enumeration_get_nth_value (item->details->preference_name, i));
}

static void
boolean_button_toggled_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	gboolean		active_state;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	active_state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	nautilus_preferences_set_boolean (item->details->preference_name, active_state);
}

static void
text_item_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (NAUTILUS_IS_STRING_PICKER (item->details->child));

	preferences_item_update_text_settings_at_idle (item);
}

static void
editable_string_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	
	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (NAUTILUS_IS_TEXT_CAPTION (item->details->child));

	preferences_item_update_text_settings_at_idle (item);
}

static void
integer_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	
	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (NAUTILUS_IS_TEXT_CAPTION (item->details->child));

	preferences_item_update_integer_settings_at_idle (item);
}

char *
nautilus_preferences_item_get_name (const NautilusPreferencesItem *preferences_item)
{
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (preferences_item), NULL);

	return g_strdup (preferences_item->details->preference_name);
}

void
nautilus_preferences_item_update_displayed_value (const NautilusPreferencesItem *item)
{
	NautilusPreferencesItemType item_type;
	
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (item));

	item_type = item->details->item_type;

	g_return_if_fail (item->details->item_type != PREFERENCES_ITEM_UNDEFINED_ITEM);

	/* Block the change signal while we update the widget to match the preference */
	g_assert (item->details->change_signal_ID != 0);
	g_assert (GTK_IS_WIDGET (item->details->child));
	gtk_signal_handler_block (GTK_OBJECT (item->details->child),
				  item->details->change_signal_ID);

	/* Update the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_update_boolean (item);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUM:
		preferences_item_update_enum (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM:
		preferences_item_update_short_enum (item);
		break;

	case NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY:
		preferences_item_update_font_family (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT:
		preferences_item_update_smooth_font (item);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING:
		preferences_item_update_editable_string (item);
		break;	

	case NAUTILUS_PREFERENCE_ITEM_INTEGER:
		preferences_item_update_integer (item);
		break;
	default:
		g_assert_not_reached ();
	}

	gtk_signal_handler_unblock (GTK_OBJECT (item->details->child),
				    item->details->change_signal_ID);
}

static gboolean
update_text_settings_at_idle (NautilusPreferencesItem *preferences_item)
{
	char *text;

	text = nautilus_text_caption_get_text (NAUTILUS_TEXT_CAPTION (preferences_item->details->child));

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

	text = nautilus_text_caption_get_text (NAUTILUS_TEXT_CAPTION (preferences_item->details->child));

	if (text != NULL) {
		nautilus_eat_str_to_int (text, &value);
	}
	
	nautilus_preferences_set_integer (preferences_item->details->preference_name, value);

	integer_idle_handler = FALSE;

	return FALSE;
}

static void
preferences_item_update_integer_settings_at_idle (NautilusPreferencesItem *preferences_item)
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

	if (nautilus_str_is_equal (preferences_item->details->control_preference_name,
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

gboolean
nautilus_preferences_item_get_control_showing (const NautilusPreferencesItem *preferences_item)
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
