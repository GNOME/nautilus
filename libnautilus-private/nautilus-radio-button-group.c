/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-radio-button-group.c - A radio button group container.

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
#include "nautilus-radio-button-group.h"
#include "nautilus-image.h"

#include <gtk/gtkradiobutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>

#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"

static const gint RADIO_BUTTON_GROUP_INVALID = -1;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} RadioGroupSignals;

struct _NautilusRadioButtonGroupDetails
{
	GList		*rows;
	GSList		*group;
	guint		num_items;
	gboolean	horizontal;
};

typedef struct
{
	GtkWidget	*button;
	GtkWidget	*image;
	GtkWidget	*description;
} TableRow;

/* NautilusRadioButtonGroupClass methods */
static void nautilus_radio_button_group_initialize_class (NautilusRadioButtonGroupClass *klass);
static void nautilus_radio_button_group_initialize       (NautilusRadioButtonGroup      *button_group);




/* GtkObjectClass methods */
static void nautilus_radio_button_group_destroy          (GtkObject                     *object);


/* Private stuff */
static void radio_button_group_free_button_group         (NautilusRadioButtonGroup      *button_group);
static void radio_button_group_emit_changed_signal       (NautilusRadioButtonGroup      *button_group,
							  GtkWidget                     *button);

/* Radio button callbacks */
static void button_toggled                               (GtkWidget                     *button,
							  gpointer                       user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusRadioButtonGroup,
				   nautilus_radio_button_group,
				   GTK_TYPE_TABLE)

static guint radio_group_signals[LAST_SIGNAL] = { 0 };

/*
 * NautilusRadioButtonGroupClass methods
 */
static void
nautilus_radio_button_group_initialize_class (NautilusRadioButtonGroupClass *radio_button_group_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (radio_button_group_class);
	widget_class = GTK_WIDGET_CLASS (radio_button_group_class);

 	parent_class = gtk_type_class (gtk_table_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_radio_button_group_destroy;

	/* Signals */
	radio_group_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__OBJECT,
				GTK_TYPE_NONE, 
				1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, radio_group_signals, LAST_SIGNAL);
}

static void
nautilus_radio_button_group_initialize (NautilusRadioButtonGroup *button_group)
{
	button_group->details = g_new (NautilusRadioButtonGroupDetails, 1);

	button_group->details->rows = NULL;
	button_group->details->group = NULL;
	button_group->details->num_items = 0;
	button_group->details->horizontal = FALSE;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_radio_button_group_destroy (GtkObject *object)
{
	NautilusRadioButtonGroup * button_group;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (object));
	
	button_group = NAUTILUS_RADIO_BUTTON_GROUP (object);

	radio_button_group_free_button_group (button_group);

	g_free (button_group->details);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * Private stuff
 */
static void
radio_button_group_emit_changed_signal (NautilusRadioButtonGroup *button_group,
				       GtkWidget *button)
{
	g_assert (button_group != NULL);
	g_assert (button_group->details != NULL);
	g_assert (button != NULL);

	gtk_signal_emit (GTK_OBJECT (button_group), 
			 radio_group_signals[CHANGED],
			 button);
}

static void
radio_button_group_free_button_group (NautilusRadioButtonGroup *button_group)
{
	g_assert (button_group != NULL);
	g_assert (button_group->details != NULL);

	if (button_group->details->rows) {
		nautilus_g_list_free_deep (button_group->details->rows);
		button_group->details->rows = NULL;
	}

	button_group->details->group = NULL;
}

/*
 * Radio button callbacks
 */
static void
button_toggled (GtkWidget *button, gpointer user_data)
{
	NautilusRadioButtonGroup *button_group = (NautilusRadioButtonGroup *) user_data;
	
	g_assert (button_group != NULL);
	g_assert (button_group->details != NULL);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		radio_button_group_emit_changed_signal (button_group, button);
	}
}

/*
 * NautilusRadioButtonGroup public methods
 */
GtkWidget*
nautilus_radio_button_group_new (gboolean is_horizontal)
{
	NautilusRadioButtonGroup *button_group;

	button_group = gtk_type_new (nautilus_radio_button_group_get_type ());
	button_group->details->horizontal = is_horizontal;
	
	return GTK_WIDGET (button_group);
}
 
/**
 * nautilus_radio_button_group_insert:
 * @button_group: The button group
 * @label: Label to use for the new button
 *
 * Create and insert a new radio button to the collection.
 *
 * Returns: The index of the new button.
 */
guint
nautilus_radio_button_group_insert (NautilusRadioButtonGroup	*button_group,
				    const gchar			*label)
{
	GtkTable	*table;
	TableRow	*row;

	g_return_val_if_fail (button_group != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group), 0);
	g_return_val_if_fail (label != NULL, 0);

	table = GTK_TABLE (button_group);

	row = g_new (TableRow, 1);

	row->button = gtk_radio_button_new_with_label (button_group->details->group, label);
	row->image = NULL;
	row->description = NULL;

	/*
	 * For some crazy reason I dont grok, the group has to be fetched each
	 * time from the previous button
	 */
	button_group->details->group = gtk_radio_button_group (GTK_RADIO_BUTTON (row->button));

	gtk_signal_connect (GTK_OBJECT (row->button),
			    "toggled",
			    GTK_SIGNAL_FUNC (button_toggled),
			    (gpointer) button_group);

	button_group->details->num_items++;

	if (button_group->details->horizontal) {
		/* Resize the table to fit all items in one row. */
		gtk_table_resize (table, 1, button_group->details->num_items);
		/* Place the radio button in the last (so far) column of the only row */
		gtk_table_attach (table, 
				  row->button,				/* child */
				  button_group->details->num_items - 1, /* left_attach */
				  button_group->details->num_items,	/* right_attach */
				  0,					/* top_attach */
				  1,					/* bottom_attach */
				  (GTK_FILL|GTK_EXPAND),		/* xoptions */
				  (GTK_FILL|GTK_EXPAND),		/* yoptions */
				  0,					/* xpadding */
				  0);					/* ypadding */
	} else {
		/* Resize the table to put each item on separate row. */
		gtk_table_resize (table, button_group->details->num_items, 3);
		/* Place the radio button in column 2 of the last (so far) row */
		gtk_table_attach (table, 
				  row->button,				/* child */
				  1,					/* left_attach */
				  2,					/* right_attach */
				  button_group->details->num_items - 1,	/* top_attach */
				  button_group->details->num_items,	/* bottom_attach */
				  (GTK_FILL|GTK_EXPAND),		/* xoptions */
				  (GTK_FILL|GTK_EXPAND),		/* yoptions */
				  0,					/* xpadding */
				  0);					/* ypadding */
	}


	gtk_widget_show (row->button);
	
	button_group->details->rows = g_list_append (button_group->details->rows, row);
	
	return g_list_length (button_group->details->rows) - 1;
}

/**
 * nautilus_radio_button_group_get_active_index:
 * @button_group: The button group
 *
 * Returns: The index of the active button.  There is always one active by law.
 */
guint
nautilus_radio_button_group_get_active_index (NautilusRadioButtonGroup *button_group)
{
	GList	*button_iterator;
	gint	i = 0;

 	g_return_val_if_fail (button_group != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group), 0);
	
	g_assert (button_group != NULL);

	button_iterator = button_group->details->rows;

	while (button_iterator) {
		TableRow *row;

		row = button_iterator->data;
		g_assert (row != NULL);
		g_assert (row->button != NULL);
		g_assert (GTK_TOGGLE_BUTTON (row->button));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (row->button))) {
			return i;
		}
		
		button_iterator = button_iterator->next;

		i++;
	}

	g_assert_not_reached ();

	return 0;
}

void
nautilus_radio_button_group_set_active_index (NautilusRadioButtonGroup *button_group,
					      guint active_index)
{
	TableRow *row;

 	g_return_if_fail (button_group != NULL);
	g_return_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group));

	row = g_list_nth_data (button_group->details->rows, active_index);
	g_assert (row != NULL);
	g_assert (row->button != NULL);
	g_assert (GTK_TOGGLE_BUTTON (row->button));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (row->button), TRUE);
}

/* Set an item's pixbuf. */
void
nautilus_radio_button_group_set_entry_pixbuf (NautilusRadioButtonGroup *button_group,
					      guint                     entry_index,
					      GdkPixbuf                *pixbuf)
{
	GtkTable	*table;
	TableRow	*row;

 	g_return_if_fail (button_group != NULL);
	g_return_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group));
	g_return_if_fail (entry_index < g_list_length (button_group->details->rows));
	g_return_if_fail (button_group->details->horizontal == FALSE);

	table = GTK_TABLE (button_group);

	row = g_list_nth_data (button_group->details->rows, entry_index);
	g_assert (row != NULL);

	if (row->image == NULL) {
		row->image = nautilus_image_new ();
		
		gtk_table_attach (table,
				  row->image,			/* child */
				  0,				/* left_attach */
				  1,				/* right_attach */
				  entry_index,			/* top_attach */
				  entry_index + 1,		/* bottom_attach */
				  GTK_FILL,			/* xoptions */
				  (GTK_FILL|GTK_EXPAND),	/* yoptions */
				  0,				/* xpadding */
				  0);				/* ypadding */
		
		gtk_widget_show (row->image);
	}

	g_assert (row->image != NULL);

	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (row->image), pixbuf);
}

/* Set an item's description. */
void
nautilus_radio_button_group_set_entry_description_text (NautilusRadioButtonGroup *button_group,
							guint                     entry_index,
							const char               *description_text)
{
	GtkTable	*table;
	TableRow	*row;

 	g_return_if_fail (button_group != NULL);
	g_return_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group));
	g_return_if_fail (entry_index < g_list_length (button_group->details->rows));
	g_return_if_fail (button_group->details->horizontal == FALSE);

	table = GTK_TABLE (button_group);

	row = g_list_nth_data (button_group->details->rows, entry_index);
	g_assert (row != NULL);
	
	if (row->description == NULL) {
		row->description = gtk_label_new (description_text);

		gtk_misc_set_alignment (GTK_MISC (row->description), 0, 0.5);
		
		gtk_table_attach (table,
				  row->description,		/* child */
				  2,				/* left_attach */
				  3,				/* right_attach */
				  entry_index,			/* top_attach */
				  entry_index + 1,		/* bottom_attach */
				  (GTK_FILL|GTK_EXPAND),	/* xoptions */
				  (GTK_FILL|GTK_EXPAND),	/* yoptions */
				  0,				/* xpadding */
				  0);				/* ypadding */
		
		gtk_widget_show (row->description);
	}
	else {
		gtk_label_set_text (GTK_LABEL (row->description), description_text);
	}
}

