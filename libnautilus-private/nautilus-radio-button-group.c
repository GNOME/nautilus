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

#include <nautilus-widgets/nautilus-radio-button-group.h>

#include <gtk/gtkradiobutton.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-gtk-macros.h>

static const gint RADIO_BUTTON_GROUP_INVALID = -1;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} RadioGroupSignals;

struct _NautilusRadioButtonGroupDetails
{
	GList		*buttons;
	GSList		*group;
};

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
				   GTK_TYPE_VBOX)

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

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

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

	button_group->details->buttons = NULL;
	button_group->details->group = NULL;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_radio_button_group_destroy(GtkObject* object)
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

	if (button_group->details->buttons)
	{
		g_list_free (button_group->details->buttons);
	}

	button_group->details->buttons = NULL;
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

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
		radio_button_group_emit_changed_signal (button_group, button);
	}
}

/*
 * NautilusRadioButtonGroup public methods
 */
GtkWidget*
nautilus_radio_button_group_new (void)
{
	NautilusRadioButtonGroup *button_group;

	button_group = gtk_type_new (nautilus_radio_button_group_get_type ());
	
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
nautilus_radio_button_group_insert (NautilusRadioButtonGroup *button_group,
				   const gchar             *label)
{
	GtkWidget	*button;

	g_return_val_if_fail (button_group != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group), 0);
	g_return_val_if_fail (label != NULL, 0);
	
	button = gtk_radio_button_new_with_label (button_group->details->group, label);

	/*
	 * For some crazy reason I dont grok, the group has to be fetched each
	 * time from the previous button
	 */
	button_group->details->group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));

	gtk_signal_connect (GTK_OBJECT (button),
			    "toggled",
			    GTK_SIGNAL_FUNC (button_toggled),
			    (gpointer) button_group);

	gtk_box_pack_start (GTK_BOX (button_group),
			    button,
			    TRUE,
			    TRUE,
			    0);

	gtk_widget_show (button);
	
	button_group->details->buttons = g_list_append (button_group->details->buttons, 
						    (gpointer) button);


	return g_list_length (button_group->details->buttons) - 1;
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

	button_iterator = button_group->details->buttons;

	while (button_iterator)
	{
		g_assert (GTK_TOGGLE_BUTTON (button_iterator->data));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_iterator->data)))
		{
			return i;
		}
		
		button_iterator = button_iterator->next;

		i++;
	}

	g_assert (0);

	return 0;
}

void
nautilus_radio_button_group_set_active_index (NautilusRadioButtonGroup *button_group,
					      guint active_index)
{
	GList	*node;

 	g_return_if_fail (button_group != NULL);
	g_return_if_fail (NAUTILUS_IS_RADIO_BUTTON_GROUP (button_group));

	node = g_list_nth (button_group->details->buttons, active_index);

	g_assert (node != NULL);

	g_assert (GTK_TOGGLE_BUTTON (node->data));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (node->data), TRUE);
}

