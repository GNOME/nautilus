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

#include <nautilus-widgets/nautilus-string-picker.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkcombo.h>

#include <gtk/gtksignal.h>

static const gint STRING_PICKER_INVALID = -1;
static const gint STRING_PICKER_SPACING = 10;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} RadioGroupSignals;

struct _NautilusStringPickerDetail
{
	GtkWidget		*title_label;
	GtkWidget		*combo_box;
	NautilusStringList	*string_list;
};

/* NautilusStringPickerClass methods */
static void      nautilus_string_picker_initialize_class (NautilusStringPickerClass *klass);
static void      nautilus_string_picker_initialize       (NautilusStringPicker      *string_picker);

/* GtkObjectClass methods */
static void      nautilus_string_picker_destroy          (GtkObject                 *object);

/* Private stuff */
static GtkEntry *string_picker_get_entry_widget          (NautilusStringPicker      *string_picker);

/* Editable (entry) callbacks */
static void      entry_changed_callback                  (GtkWidget                 *entry,
							  gpointer                   user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusStringPicker, nautilus_string_picker, GTK_TYPE_HBOX)

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

 	parent_class = gtk_type_class (gtk_hbox_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_string_picker_destroy;

	/* Signals */
	string_picker_signals[CHANGED] =
		gtk_signal_new ("changed",
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

	string_picker->detail->title_label = gtk_label_new ("Title Label:");
	string_picker->detail->combo_box = gtk_combo_new ();

	gtk_entry_set_editable (string_picker_get_entry_widget (string_picker), FALSE);

	gtk_box_pack_start (GTK_BOX (string_picker),
			    string_picker->detail->title_label,
			    FALSE,	/* expand */
			    TRUE,	/* fill */
			    0);		/* padding */
	
	gtk_box_pack_end (GTK_BOX (string_picker),
			  string_picker->detail->combo_box,
			  TRUE,		/* expand */
			  TRUE,		/* fill */
			  0);		/* padding */

	gtk_signal_connect (GTK_OBJECT (string_picker_get_entry_widget (string_picker)),
			    "changed",
			    GTK_SIGNAL_FUNC (entry_changed_callback),
			    (gpointer) string_picker);
	
	gtk_widget_show (string_picker->detail->title_label);
	gtk_widget_show (string_picker->detail->combo_box);
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_string_picker_destroy(GtkObject* object)
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

/*
 * Private stuff
 */
static GtkEntry *
string_picker_get_entry_widget (NautilusStringPicker *string_picker)
{
	g_assert (string_picker != NULL);
	g_assert (NAUTILUS_IS_STRING_PICKER (string_picker));

	return GTK_ENTRY (GTK_COMBO (string_picker->detail->combo_box)->entry);
}	

/*
 * Editable (entry) callbacks
 */
static void
entry_changed_callback (GtkWidget *entry, gpointer user_data)
{
	const char *entry_text;

	NautilusStringPicker *string_picker;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_STRING_PICKER (user_data));

	string_picker = NAUTILUS_STRING_PICKER (user_data);

	/* WATCHOUT: 
	 * As of gtk1.2, gtk_entry_get_text() returns a non const reference to
	 * the internal string.
	 */
	entry_text = (const char *) gtk_entry_get_text (string_picker_get_entry_widget (string_picker));

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
 *
 * Returns: The index of the active button.  There is always one active by law.
 */
void
nautilus_string_picker_set_string_list (NautilusStringPicker		*string_picker,
					const NautilusStringList	*string_list)
{
	GList *strings;

 	g_return_if_fail (string_picker != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	string_picker->detail->string_list = nautilus_string_list_new_from_string_list (string_list);

	strings = nautilus_string_list_as_g_list (string_picker->detail->string_list);

	gtk_combo_set_popdown_strings (GTK_COMBO (string_picker->detail->combo_box), strings);

	nautilus_g_list_free_deep (strings);
}

/**
 * nautilus_string_picker_set_title_label:
 * @string_picker: A NautilusStringPicker
 * @title_label: The title label
 *
 */
void
nautilus_string_picker_set_title_label (NautilusStringPicker		*string_picker,
					const char			*title_label)
{
 	g_return_if_fail (string_picker != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));
	g_return_if_fail (title_label != NULL);

	gtk_label_set_text (GTK_LABEL (string_picker->detail->title_label), title_label);
}

/**
 * nautilus_string_picker_get_text
 * @string_picker: A NautilusStringPicker
 *
 * Returns: A copy of the currently selected text.  Need to g_free() it.
 */
char *
nautilus_string_picker_get_text (NautilusStringPicker *string_picker)
{
	const char *entry_text;

 	g_return_val_if_fail (string_picker != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker), NULL);

	/* WATCHOUT: 
	 * As of gtk1.2, gtk_entry_get_text() returns a non const reference to
	 * the internal string.
	 */
	entry_text = (const char *) gtk_entry_get_text (string_picker_get_entry_widget (string_picker));

	return g_strdup (entry_text);
}

void
nautilus_string_picker_set_text (NautilusStringPicker	*string_picker,
				 const char		*text)
{
 	g_return_if_fail (string_picker != NULL);
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	g_return_if_fail (string_picker->detail->string_list != NULL);
	g_return_if_fail (nautilus_string_list_contains (string_picker->detail->string_list, text));

	gtk_entry_set_text (string_picker_get_entry_widget (string_picker), text);
}
