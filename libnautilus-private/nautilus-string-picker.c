/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-string-picker.c - A widget to pick a string from a list.

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

#include "nautilus-string-picker.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>

#include <gtk/gtksignal.h>

static const gint STRING_PICKER_SPACING = 10;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} NautilusStringPickerSignals;

struct _NautilusStringPickerDetail
{
	GtkWidget		*option_menu;
	GtkWidget		*menu;
	NautilusStringList	*string_list;
};

/* NautilusStringPickerClass methods */
static void nautilus_string_picker_initialize_class (NautilusStringPickerClass *klass);
static void nautilus_string_picker_initialize       (NautilusStringPicker      *string_picker);


/* GtkObjectClass methods */
static void nautilus_string_picker_destroy          (GtkObject                 *object);


/* Option menu item callbacks */
static void option_menu_activate_callback           (GtkWidget                 *menu_item,
						     gpointer                   callback_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusStringPicker, nautilus_string_picker, NAUTILUS_TYPE_CAPTION)

static guint string_picker_signals[LAST_SIGNAL] = { 0 };

/*
 * NautilusStringPickerClass methods
 */
static void
nautilus_string_picker_initialize_class (NautilusStringPickerClass *string_picker_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (string_picker_class);
	widget_class = GTK_WIDGET_CLASS (string_picker_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_string_picker_destroy;
	
	/* Signals */
	string_picker_signals[CHANGED] = gtk_signal_new ("changed",
							 GTK_RUN_LAST,
							 object_class->type,
							 0,
							 gtk_marshal_NONE__NONE,
							 GTK_TYPE_NONE, 
							 0);

	gtk_object_class_add_signals (object_class, string_picker_signals, LAST_SIGNAL);
}

static void
nautilus_string_picker_initialize (NautilusStringPicker *string_picker)
{
	string_picker->detail = g_new (NautilusStringPickerDetail, 1);

	gtk_box_set_homogeneous (GTK_BOX (string_picker), FALSE);
	gtk_box_set_spacing (GTK_BOX (string_picker), STRING_PICKER_SPACING);

	string_picker->detail->string_list = NULL;
	string_picker->detail->menu = NULL;

	string_picker->detail->option_menu = gtk_option_menu_new ();

	nautilus_caption_set_child (NAUTILUS_CAPTION (string_picker),
				    string_picker->detail->option_menu);

	gtk_widget_show (string_picker->detail->option_menu);
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_string_picker_destroy (GtkObject* object)
{
	NautilusStringPicker * string_picker;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (object));
	
	string_picker = NAUTILUS_STRING_PICKER (object);

	if (string_picker->detail->string_list != NULL) {
		nautilus_string_list_free (string_picker->detail->string_list);
	}

	g_free (string_picker->detail);

	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Option menu item callbacks */
static void
option_menu_activate_callback (GtkWidget *menu_item, gpointer callback_data)
{
	NautilusStringPicker *string_picker;

	g_return_if_fail (menu_item != NULL);
	g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
	
	g_return_if_fail (callback_data != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (callback_data));

	string_picker = NAUTILUS_STRING_PICKER (callback_data);

	gtk_signal_emit (GTK_OBJECT (string_picker), string_picker_signals[CHANGED]);
}

/*
 * NautilusStringPicker public methods
 */
GtkWidget*
nautilus_string_picker_new (void)
{
	NautilusStringPicker *string_picker;

	string_picker = gtk_type_new (nautilus_string_picker_get_type ());
	
	return GTK_WIDGET (string_picker);
}

/**
 * nautilus_string_picker_set_string_list:
 * @string_picker: A NautilusStringPicker
 * @string_list: A list of strings
 *
 * Returns: nope
 */
void
nautilus_string_picker_set_string_list (NautilusStringPicker		*string_picker,
					const NautilusStringList	*string_list)
{
	guint i;

 	g_return_if_fail (string_picker != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	string_picker->detail->string_list = nautilus_string_list_new_from_string_list (string_list);

	/* Kill the old menu if alive */
	if (string_picker->detail->menu != NULL) {
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (string_picker->detail->option_menu));
		gtk_widget_destroy (string_picker->detail->menu);
		string_picker->detail->menu = NULL;
	}

	/* Make a new menu */
	string_picker->detail->menu = gtk_menu_new ();

	for (i = 0; i < nautilus_string_list_get_length (string_picker->detail->string_list); i++) {
		GtkWidget *menu_item;
		char *item_label = nautilus_string_list_nth (string_picker->detail->string_list, i);
		g_assert (item_label != NULL);

		menu_item = gtk_menu_item_new_with_label (item_label);
		g_free (item_label);

		/* Save the index so we can later use it to retrieve the nth label from the list */
		gtk_object_set_data (GTK_OBJECT (menu_item), "index", GINT_TO_POINTER (i));

		gtk_signal_connect (GTK_OBJECT (menu_item),
				    "activate",
				    GTK_SIGNAL_FUNC (option_menu_activate_callback),
				    string_picker);
		
		gtk_widget_show (menu_item);
		
		gtk_menu_append (GTK_MENU (string_picker->detail->menu), menu_item);
		
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (string_picker->detail->option_menu), string_picker->detail->menu);
}

/**
 * nautilus_string_picker_get_string_list:
 * @string_picker: A NautilusStringPicker
 *
 * Returns: A copy of the list of strings for the string picker.  Need to free it.
 */
NautilusStringList*
nautilus_string_picker_get_string_list (const NautilusStringPicker *string_picker)
{

 	g_return_val_if_fail (string_picker != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker), NULL);

	return (string_picker->detail->string_list != NULL) ?
		nautilus_string_list_new_from_string_list (string_picker->detail->string_list) :
		NULL;
}

/* FIXME bugzilla.eazel.com 1556: 
 * Rename confusing string picker get/set functions
 */

/**
 * nautilus_string_picker_get_text
 * @string_picker: A NautilusStringPicker
 *
 * Returns: A copy of the currently selected text.  Need to g_free() it.
 */
char *
nautilus_string_picker_get_text (NautilusStringPicker *string_picker)
{
	gint		item_index;
	GtkWidget	*option_menu;
	GtkWidget	*menu_item;

 	g_return_val_if_fail (string_picker != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker), NULL);

	option_menu = string_picker->detail->option_menu;
	
	menu_item = GTK_OPTION_MENU (option_menu)->menu_item;

	item_index = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "index"));

	return (item_index != -1) ? nautilus_string_list_nth (string_picker->detail->string_list, item_index) : NULL;
}

/**
 * nautilus_string_picker_set_text
 * @string_picker: A NautilusStringPicker
 *
 * Set the active item corresponding to the given text.
 */
void
nautilus_string_picker_set_text (NautilusStringPicker	*string_picker,
				 const char		*text)
{
	gint item_index;

 	g_return_if_fail (string_picker != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	g_return_if_fail (string_picker->detail->string_list != NULL);
	g_return_if_fail (nautilus_string_list_contains (string_picker->detail->string_list, text));

	item_index = nautilus_string_list_get_index_for_string (string_picker->detail->string_list, text);
	g_assert (item_index != NAUTILUS_STRING_LIST_NOT_FOUND);

	gtk_option_menu_set_history (GTK_OPTION_MENU (string_picker->detail->option_menu), item_index);
}
