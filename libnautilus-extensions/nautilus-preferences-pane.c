/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-pane.h - Interface for a prefs pane superclass.

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
#include "nautilus-preferences-pane.h"
#include "nautilus-gtk-macros.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>

#include <gnome.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const guint PREFS_PANE_GROUPS_BOX_TOP_OFFSET = 0;
static const guint PREFS_PANE_IN_BETWEEN_OFFSET = 4;

struct _NautilusPreferencesPaneDetails
{
	GtkWidget	*title_box;
	GtkWidget	*title_frame;
	GtkWidget	*title_label;
	GtkWidget	*description_label;

	GtkWidget	*groups_box;

	gboolean	show_title;

	GList		*groups;
};

typedef void (*GnomeBoxSignal1) (GtkObject* object,
				    gint arg1,
				    gpointer data);

/* NautilusPreferencesPaneClass methods */
static void nautilus_preferences_pane_initialize_class (NautilusPreferencesPaneClass *klass);
static void nautilus_preferences_pane_initialize       (NautilusPreferencesPane      *prefs_pane);

/* GtkObjectClass methods */
static void nautilus_preferences_pane_destroy    (GtkObject            *object);

/* Private stuff */
static void prefs_pane_construct (NautilusPreferencesPane *prefs_pane,
				  const gchar *pane_title,
				  const gchar *pane_description);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesPane, nautilus_preferences_pane, GTK_TYPE_VBOX)

/*
 * NautilusPreferencesPaneClass methods
 */
static void
nautilus_preferences_pane_initialize_class (NautilusPreferencesPaneClass *prefs_pane_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_pane_class);
	widget_class = GTK_WIDGET_CLASS (prefs_pane_class);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_pane_destroy;
}

static void
nautilus_preferences_pane_initialize (NautilusPreferencesPane *prefs_pane)
{
	prefs_pane->details = g_new (NautilusPreferencesPaneDetails, 1);

	prefs_pane->details->title_label = NULL;
	prefs_pane->details->description_label = NULL;
	prefs_pane->details->title_box = NULL;
	prefs_pane->details->title_frame = NULL;
	prefs_pane->details->groups_box = NULL;
	prefs_pane->details->groups = NULL;
	prefs_pane->details->show_title = FALSE;
}

/*
 *  GtkObjectClass methods
 */
static void
nautilus_preferences_pane_destroy(GtkObject* object)
{
	NautilusPreferencesPane * prefs_pane;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (object));
	
	prefs_pane = NAUTILUS_PREFERENCES_PANE (object);

	if (prefs_pane->details->groups)
	{
		g_list_free (prefs_pane->details->groups);
	}

	g_free (prefs_pane->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * Private stuff
 */
static void
prefs_pane_construct (NautilusPreferencesPane	*prefs_pane,
		      const gchar		*pane_title,
		      const gchar		*pane_description)
{
	g_assert (prefs_pane != NULL);
	g_assert (prefs_pane->details != NULL);

	g_assert (pane_title != NULL);
	g_assert (pane_description != NULL);

	g_assert (prefs_pane->details->title_label == NULL);
	g_assert (prefs_pane->details->description_label == NULL);
	g_assert (prefs_pane->details->title_box == NULL);
	g_assert (prefs_pane->details->groups_box == NULL);
	g_assert (prefs_pane->details->title_frame == NULL);
	g_assert (prefs_pane->details->groups == NULL);

	if (prefs_pane->details->show_title)
	{
		/* Title frame */
		prefs_pane->details->title_frame = gtk_frame_new (NULL);
		
		gtk_frame_set_shadow_type (GTK_FRAME (prefs_pane->details->title_frame),
					   GTK_SHADOW_ETCHED_IN);
		
		/* Title box */
		prefs_pane->details->title_box = gtk_hbox_new (FALSE, 0);
		
		/* Title labels */
		prefs_pane->details->title_label = gtk_label_new (pane_title);
		prefs_pane->details->description_label = gtk_label_new (pane_description);
		
		gtk_box_pack_start (GTK_BOX (prefs_pane->details->title_box),
				    prefs_pane->details->title_label,
				    FALSE,
				    FALSE,
				    0);
		
		gtk_box_pack_end (GTK_BOX (prefs_pane->details->title_box),
				  prefs_pane->details->description_label,
				  FALSE,
				  FALSE,
				  0);
		
		gtk_widget_show (prefs_pane->details->title_label);
		gtk_widget_show (prefs_pane->details->description_label);

		/* Add title box to title frame */
		gtk_container_add (GTK_CONTAINER (prefs_pane->details->title_frame),
				   prefs_pane->details->title_box);

		gtk_widget_show (prefs_pane->details->title_box);
		
		/* Add title frame to ourselves */
		gtk_box_pack_start (GTK_BOX (prefs_pane),
				    prefs_pane->details->title_frame,
				    FALSE,
				    FALSE,
				    0);
		
		gtk_widget_show (prefs_pane->details->title_frame);
	}

	/* Groups box */
	prefs_pane->details->groups_box = gtk_vbox_new (FALSE, 0);

	/* Add groups box to ourselves */
	gtk_box_pack_start (GTK_BOX (prefs_pane),
			    prefs_pane->details->groups_box,
			    FALSE,
			    FALSE,
			    PREFS_PANE_GROUPS_BOX_TOP_OFFSET);

	gtk_widget_show (prefs_pane->details->groups_box);
}


/*
 * NautilusPreferencesPane public methods
 */
GtkWidget*
nautilus_preferences_pane_new (const gchar *pane_title,
			       const gchar *pane_description)
{
	NautilusPreferencesPane *prefs_pane;

	g_return_val_if_fail (pane_title != NULL, NULL);
	g_return_val_if_fail (pane_description != NULL, NULL);

	prefs_pane = gtk_type_new (nautilus_preferences_pane_get_type ());

	prefs_pane_construct (prefs_pane, pane_title, pane_description);

	return GTK_WIDGET (prefs_pane);
}

void
nautilus_preferences_pane_set_title (NautilusPreferencesPane *prefs_pane,
				     const gchar	 *pane_title)
{
	g_return_if_fail (prefs_pane != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane));

	g_assert (prefs_pane->details->title_label != NULL);
	
	gtk_label_set_text (GTK_LABEL (prefs_pane->details->title_label),
			    pane_title);
}

void
nautilus_preferences_pane_set_description (NautilusPreferencesPane *prefs_pane,
					   const gchar	 *pane_description)
{
	g_return_if_fail (prefs_pane != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane));

	g_assert (prefs_pane->details->description_label != NULL);

	gtk_label_set_text (GTK_LABEL (prefs_pane->details->description_label),
			    pane_description);
}

GtkWidget *
nautilus_preferences_pane_add_group (NautilusPreferencesPane	*prefs_pane,
				     const char			*group_title)
{
	GtkWidget *group;

	g_return_val_if_fail (prefs_pane != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane), NULL);
	g_return_val_if_fail (group_title != NULL, NULL);

	group = nautilus_preferences_group_new (group_title);

	prefs_pane->details->groups = g_list_append (prefs_pane->details->groups,
						     (gpointer) group);
	
	gtk_box_pack_start (GTK_BOX (prefs_pane->details->groups_box),
			    group,
			    TRUE,
			    TRUE,
			    PREFS_PANE_IN_BETWEEN_OFFSET);

	gtk_widget_show (group);

	return group;
}

GtkWidget *
nautilus_preferences_pane_add_item_to_nth_group (NautilusPreferencesPane	*prefs_pane,
						 guint				n,
						 const char			*preference_name,
						 NautilusPreferencesItemType	item_type)
{
	GtkWidget *item;
	GtkWidget *group;

	g_return_val_if_fail (prefs_pane != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane), NULL);

	g_return_val_if_fail (preference_name != NULL, NULL);

	if (!prefs_pane->details->groups) {
		g_warning ("nautilus_preferences_pane_add_item_to_nth_group () There are no groups!");

		return NULL;
	}

	if (n >= g_list_length (prefs_pane->details->groups)) {
		g_warning ("nautilus_preferences_pane_add_item_to_nth_group (n = %d) n is out of bounds!", n);

		return NULL;
	}

	g_assert (g_list_nth_data (prefs_pane->details->groups, n) != NULL);
	g_assert (GTK_IS_WIDGET (g_list_nth_data (prefs_pane->details->groups, n)));

	group = GTK_WIDGET (g_list_nth_data (prefs_pane->details->groups, n));

	item = nautilus_preferences_group_add_item (NAUTILUS_PREFERENCES_GROUP (group),
						    preference_name,
						    item_type);

	g_assert (item != NULL);

	return item;
}


