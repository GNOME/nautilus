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

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

typedef struct
{
	GtkWidget	*radio_button;
} ButtonInfo;

struct _NautilusPrefsGroupRadioPrivate
{
	GList		*button_list;
	GSList		*button_group;
};

/* NautilusPrefsGroupRadioClass methods */
static void nautilus_prefs_group_radio_initialize_class (NautilusPrefsGroupRadioClass *klass);
static void nautilus_prefs_group_radio_initialize       (NautilusPrefsGroupRadio      *prefs_group_radio);


/* GtkObjectClass methods */
static void nautilus_prefs_group_radio_destroy          (GtkObject                    *object);


/* Misc private stuff */
static void prefs_group_radio_construct                 (NautilusPrefsGroup           *prefs_group,
							 const gchar                  *group_title);
static void prefs_group_radio_free_button_list              (NautilusPrefsGroupRadio      *prefs_group_radio);

/* ButtonInfo functions */
static ButtonInfo *button_info_alloc                     (GtkWidget * radio_button);
static void                   button_info_free                      (ButtonInfo *info);
static void		      button_info_free_func			  (gpointer	data,
									   gpointer	user_data);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPrefsGroupRadio,
				   nautilus_prefs_group_radio,
				   NAUTILUS_TYPE_PREFS_GROUP)

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

	/* NautilusPrefsGroupClass */
	prefs_group_class->construct = prefs_group_radio_construct;
}

static void
nautilus_prefs_group_radio_initialize (NautilusPrefsGroupRadio *prefs_group_radio)
{
	prefs_group_radio->priv = g_new (NautilusPrefsGroupRadioPrivate, 1);

	prefs_group_radio->priv->button_list = NULL;
	prefs_group_radio->priv->button_group = NULL;
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
 * Misc private stuff
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

	if (!prefs_group_radio->priv->button_group)
	{
		radio_button = gtk_radio_button_new_with_label (NULL, label);

		prefs_group_radio->priv->button_group = 
			gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button));
	}
	else
	{
		radio_button = 
			gtk_radio_button_new_with_label (prefs_group_radio->priv->button_group, 
							 label);
	}
	
	gtk_object_set (GTK_OBJECT (radio_button),
			"active",
			(gpointer) (gint)active,
			NULL);

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
