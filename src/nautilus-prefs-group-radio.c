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

#include "nautilus-prefs-group-radio.h"

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

struct _NautilusPrefsGroupRadioPrivate
{
	GList		*button_list;
	GSList		*button_group;
	guint		active_button;
};

static const gint PREFS_GROUP_RADIO_NOT_FOUND = -1;

/* NautilusPrefsGroupRadioClass methods */
static void        nautilus_prefs_group_radio_initialize_class (NautilusPrefsGroupRadioClass *klass);
static void        nautilus_prefs_group_radio_initialize       (NautilusPrefsGroupRadio      *prefs_group_radio);


/* GtkObjectClass methods */
static void        nautilus_prefs_group_radio_destroy          (GtkObject                    *object);


/* NautilusPrefsGroupClass methods */
static void        prefs_group_radio_construct                 (NautilusPrefsGroup           *prefs_group,
								const gchar                  *group_title);

/* Private stuff */
static void        prefs_group_radio_construct                 (NautilusPrefsGroup           *prefs_group,
								const gchar                  *group_title);
static void        prefs_group_radio_free_button_list          (NautilusPrefsGroupRadio      *prefs_group_radio);
static void        prefs_group_radio_emit_changed_signal       (NautilusPrefsGroupRadio      *prefs_group_radio,
								GtkWidget *button);

static gint prefs_group_radio_index_of_button (NautilusPrefsGroupRadio *prefs_group_radio,
					       GtkWidget *button);


/* ButtonInfo functions */
static ButtonInfo *button_info_alloc                           (GtkWidget                    *radio_button);
static void        button_info_free                            (ButtonInfo                   *info);
static void        button_info_free_func                       (gpointer                      data,
								gpointer                      user_data);
static void        button_info_set_active_func                 (gpointer                      data,
								gpointer                      user_data);



/* Radio button callbacks */
static void        radio_button_toggled                        (GtkWidget                    *button,
								gpointer                      user_data);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPrefsGroupRadio,
				   nautilus_prefs_group_radio,
				   NAUTILUS_TYPE_PREFS_GROUP)

static guint radio_group_signals[LAST_SIGNAL] = { 0 };

/*
 * NautilusPrefsGroupRadioClass methods
 */
static void
nautilus_prefs_group_radio_initialize_class (NautilusPrefsGroupRadioClass *prefs_group_radio_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	NautilusPrefsGroupClass *prefs_group_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_group_radio_class);
	widget_class = GTK_WIDGET_CLASS (prefs_group_radio_class);

	prefs_group_class = NAUTILUS_PREFS_GROUP_CLASS (prefs_group_radio_class);

 	parent_class = gtk_type_class (nautilus_prefs_group_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_prefs_group_radio_destroy;

	/* Signals */
	radio_group_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				0,//GTK_SIGNAL_OFFSET (NautilusPrefGroupRadioClass, changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 
				1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, radio_group_signals, LAST_SIGNAL);

	/* NautilusPrefsGroupClass */
	prefs_group_class->construct = prefs_group_radio_construct;
}

static void
nautilus_prefs_group_radio_initialize (NautilusPrefsGroupRadio *prefs_group_radio)
{
	prefs_group_radio->priv = g_new (NautilusPrefsGroupRadioPrivate, 1);

	prefs_group_radio->priv->button_list = NULL;
	prefs_group_radio->priv->button_group = NULL;
	prefs_group_radio->priv->active_button = 0;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_prefs_group_radio_destroy(GtkObject* object)
{
	NautilusPrefsGroupRadio * prefs_group_radio;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP_RADIO (object));
	
	prefs_group_radio = NAUTILUS_PREFS_GROUP_RADIO (object);

	prefs_group_radio_free_button_list (prefs_group_radio);

	g_free (prefs_group_radio->priv);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * NautilusPrefsGroupClass methods
 */
static void
prefs_group_radio_construct (NautilusPrefsGroup *prefs_group,
			     const gchar *group_title)
{
	NautilusPrefsGroupRadio *prefs_group_radio;

	g_return_if_fail (prefs_group != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP_RADIO (prefs_group));

	prefs_group_radio = NAUTILUS_PREFS_GROUP_RADIO (prefs_group);

	/* Construct the superclass part */
	g_assert (NAUTILUS_PREFS_GROUP_CLASS (parent_class)->construct );

	(* NAUTILUS_PREFS_GROUP_CLASS (parent_class)->construct) (prefs_group,
								  group_title);
}

/*
 * ButtonInfo functions
 */
static ButtonInfo *
button_info_alloc (GtkWidget * radio_button)
{
	ButtonInfo * info;

	g_assert (radio_button != NULL);

	info = g_new (ButtonInfo, 1);

	info->radio_button = radio_button;

	return info;
}

static void
button_info_free (ButtonInfo *info)
{
	g_assert (info != NULL);

	g_free (info);
}

static void
button_info_free_func (gpointer	data,
		       gpointer	user_data)
{
	ButtonInfo *info = (ButtonInfo *) data;

	g_assert (info != NULL);

	g_assert (info->radio_button != NULL);

	gtk_widget_destroy (info->radio_button);

	button_info_free (info);
}

static void
button_info_set_active_func (gpointer	data,
			     gpointer	user_data)
{
	ButtonInfo	*info = (ButtonInfo *) data;
	GtkWidget	*active_button = (GtkWidget *) user_data;

	g_assert (info != NULL);
	g_assert (active_button != NULL);

	g_assert (info->radio_button != NULL);

	if (info->radio_button == active_button)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->radio_button), TRUE);
	}
}

/*
 * Private stuff
 */
static void
prefs_group_radio_emit_changed_signal (NautilusPrefsGroupRadio *prefs_group_radio,
				       GtkWidget *button)
{
	gint index;

	NautilusPrefsGroupRadioSignalData signal_data;

	g_assert (prefs_group_radio != NULL);
	g_assert (prefs_group_radio->priv != NULL);
	g_assert (button != NULL);

	index = prefs_group_radio_index_of_button (prefs_group_radio, button);

	g_assert (index != PREFS_GROUP_RADIO_NOT_FOUND);

	signal_data.active_button_index = (guint) index;
	
	gtk_signal_emit (GTK_OBJECT (prefs_group_radio), 
			 radio_group_signals[CHANGED],
			 (gpointer) &signal_data);
}

static gint
prefs_group_radio_index_of_button (NautilusPrefsGroupRadio *prefs_group_radio,
				   GtkWidget *button)
{
	GList	*button_iterator;
	gint	i = 0;

	g_assert (prefs_group_radio != NULL);
	g_assert (button != NULL);

	button_iterator = prefs_group_radio->priv->button_list;

	while (button_iterator)
	{
		ButtonInfo * info = button_iterator->data;
		
		g_assert (info != NULL);

		if (info->radio_button == button)
		{
			return i;
		}

		button_iterator = button_iterator->next;

		i++;
	}

	return PREFS_GROUP_RADIO_NOT_FOUND;
}

static void
prefs_group_radio_free_button_list (NautilusPrefsGroupRadio *prefs_group_radio)
{
	g_assert (prefs_group_radio != NULL);
	g_assert (prefs_group_radio->priv != NULL);

	if (prefs_group_radio->priv->button_list)
	{
		g_list_foreach (prefs_group_radio->priv->button_list,
				button_info_free_func,
				NULL);
		
		g_list_free (prefs_group_radio->priv->button_list);
	}

	prefs_group_radio->priv->button_list = NULL;
	prefs_group_radio->priv->button_group = NULL;
	prefs_group_radio->priv->active_button = 0;
}

/*
 * Radio button callbacks
 */
static void
radio_button_toggled (GtkWidget *button, gpointer user_data)
{
	NautilusPrefsGroupRadio *prefs_group_radio = (NautilusPrefsGroupRadio *) user_data;
	
	g_assert (prefs_group_radio != NULL);
	g_assert (prefs_group_radio->priv != NULL);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
		prefs_group_radio_emit_changed_signal (prefs_group_radio, button);
	}
}

/*
 * NautilusPrefsGroupRadio public methods
 */
GtkWidget*
nautilus_prefs_group_radio_new (const gchar *group_title)
{
// 	return nautilus_prefs_group_new (group_title);
	NautilusPrefsGroupRadio *prefs_group_radio;
	NautilusPrefsGroup *prefs_group;

	g_return_val_if_fail (group_title != NULL, NULL);


	prefs_group_radio = gtk_type_new (nautilus_prefs_group_radio_get_type ());
	
	prefs_group = NAUTILUS_PREFS_GROUP (prefs_group_radio);

//	prefs_group_radio_construct (prefs_group_radio, group_title);

	NAUTILUS_PREFS_GROUP_ASSERT_METHOD (prefs_group_radio, construct);

 	NAUTILUS_PREFS_GROUP_INVOKE_METHOD (prefs_group_radio, construct) (prefs_group,
									   group_title);
	return GTK_WIDGET (prefs_group_radio);
}

void
nautilus_prefs_group_radio_clear (NautilusPrefsGroupRadio *prefs_group_radio)
{
}

void
nautilus_prefs_group_radio_insert (NautilusPrefsGroupRadio *prefs_group_radio,
				   const gchar             *label,
				   gboolean                 active)
{
	ButtonInfo	*info;
	GtkWidget	*radio_button;
	GtkWidget	*content_box;

	g_return_if_fail (prefs_group_radio != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP_RADIO (prefs_group_radio));
	g_return_if_fail (label != NULL);
	
	content_box = 
		nautilus_prefs_group_get_content_box (NAUTILUS_PREFS_GROUP (prefs_group_radio));

	g_assert (content_box != NULL);

	printf("label = %s, group = %p\n",label,prefs_group_radio->priv->button_group);

	radio_button = gtk_radio_button_new_with_label (prefs_group_radio->priv->button_group, 
							label);

	/*
	 * For some crazy reason I dont grok, the group has to be fetched each
	 * time from the previous button
	 */
	prefs_group_radio->priv->button_group =
			gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button));

	gtk_signal_connect (GTK_OBJECT (radio_button),
			    "toggled",
			    GTK_SIGNAL_FUNC (radio_button_toggled),
			    (gpointer) prefs_group_radio);

	gtk_box_pack_start (GTK_BOX (content_box),
			    radio_button,
			    TRUE,
			    TRUE,
			    0);

	info = button_info_alloc (radio_button);
	
	gtk_widget_show (radio_button);
	
	prefs_group_radio->priv->button_list = 
		g_list_append (prefs_group_radio->priv->button_list, 
			       (gpointer) info);
}

void
nautilus_prefs_group_radio_set_active_button (NautilusPrefsGroupRadio *prefs_group_radio,
					      guint button_index)
{
	GList		*button_node;
	ButtonInfo	*button_info;

	g_return_if_fail (prefs_group_radio != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP_RADIO (prefs_group_radio));
	g_return_if_fail (button_index < g_list_length (prefs_group_radio->priv->button_list));

	prefs_group_radio->priv->active_button = button_index;

	button_node = g_list_nth (prefs_group_radio->priv->button_list, button_index);

	g_assert (button_node);

	button_info = (ButtonInfo *) button_node->data;

	g_assert (button_info);

	g_list_foreach (prefs_group_radio->priv->button_list,
			button_info_set_active_func,
			(gpointer) button_info->radio_button);
}

guint
nautilus_prefs_group_radio_get_active_button (NautilusPrefsGroupRadio *prefs_group_radio)
{
	g_return_val_if_fail (prefs_group_radio != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_GROUP_RADIO (prefs_group_radio), 0);

	return prefs_group_radio->priv->active_button;
}
