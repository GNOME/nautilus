/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-pane.h - Interface for a prefs pane superclass.

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


#include "nautilus-prefs-pane.h"
#include <libnautilus/nautilus-gtk-macros.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>

// #include <gtk/gtkmain.h>
#include <gnome.h>

#include <libgnomeui/gnome-stock.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const guint PREFS_PANE_TITLE_SPACING = 100;
static const guint PREFS_PANE_GROUPS_BOX_TOP_OFFSET = 10;

struct _NautilusPrefsPanePrivate
{
	GtkWidget	*title_box;
	GtkWidget	*title_frame;
	GtkWidget	*title_label;
	GtkWidget	*description_label;

	GtkWidget	*groups_box;

	GSList		*groups;
};

typedef void (*GnomeBoxSignal1) (GtkObject* object,
				    gint arg1,
				    gpointer data);

/* NautilusPrefsPaneClass methods */
static void nautilus_prefs_pane_initialize_class (NautilusPrefsPaneClass *klass);
static void nautilus_prefs_pane_initialize       (NautilusPrefsPane      *prefs_pane);

/* GtkObjectClass methods */
static void nautilus_prefs_pane_destroy    (GtkObject            *object);

/* Private stuff */
static void prefs_pane_construct (NautilusPrefsPane *prefs_pane,
				  const gchar *pane_title,
				  const gchar *pane_description);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPrefsPane, nautilus_prefs_pane, GTK_TYPE_VBOX)

/*
 * NautilusPrefsPaneClass methods
 */
static void
nautilus_prefs_pane_initialize_class (NautilusPrefsPaneClass *prefs_pane_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_pane_class);
	widget_class = GTK_WIDGET_CLASS (prefs_pane_class);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_prefs_pane_destroy;
}

static void
nautilus_prefs_pane_initialize (NautilusPrefsPane *prefs_pane)
{
	prefs_pane->priv = g_new (NautilusPrefsPanePrivate, 1);

	prefs_pane->priv->title_label = NULL;
	prefs_pane->priv->description_label = NULL;
	prefs_pane->priv->title_box = NULL;
	prefs_pane->priv->title_frame = NULL;
	prefs_pane->priv->groups_box = NULL;

	prefs_pane->priv->groups = NULL;
}

/*
 *  GtkObjectClass methods
 */
static void
nautilus_prefs_pane_destroy(GtkObject* object)
{
	NautilusPrefsPane * prefs_pane;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (object));
	
	prefs_pane = NAUTILUS_PREFS_PANE (object);

	if (prefs_pane->priv->groups)
	{
		g_slist_free (prefs_pane->priv->groups);
	}

	g_free (prefs_pane->priv);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * Private stuff
 */
static void
prefs_pane_construct (NautilusPrefsPane *prefs_pane,
		      const gchar *pane_title,
		      const gchar *pane_description)
{
	g_assert (prefs_pane != NULL);
	g_assert (prefs_pane->priv != NULL);

	g_assert (pane_title != NULL);
	g_assert (pane_description != NULL);

	g_assert (prefs_pane->priv->title_label == NULL);
	g_assert (prefs_pane->priv->description_label == NULL);
	g_assert (prefs_pane->priv->title_box == NULL);
	g_assert (prefs_pane->priv->groups_box == NULL);
	g_assert (prefs_pane->priv->title_frame == NULL);
	g_assert (prefs_pane->priv->groups == NULL);

	prefs_pane->priv->groups = g_slist_alloc ();

	/* Title frame */
	prefs_pane->priv->title_frame = gtk_frame_new (NULL);

	gtk_frame_set_shadow_type (GTK_FRAME (prefs_pane->priv->title_frame),
				   GTK_SHADOW_ETCHED_IN);

	/* Title box */
	prefs_pane->priv->title_box = 
		gtk_hbox_new (FALSE, PREFS_PANE_TITLE_SPACING);

	/* Title labels */
	prefs_pane->priv->title_label = gtk_label_new (pane_title);
	prefs_pane->priv->description_label = gtk_label_new (pane_description);

	gtk_box_pack_start (GTK_BOX (prefs_pane->priv->title_box),
			    prefs_pane->priv->title_label,
			    FALSE,
			    FALSE,
			    0);

	gtk_box_pack_end (GTK_BOX (prefs_pane->priv->title_box),
			  prefs_pane->priv->description_label,
			  FALSE,
			  FALSE,
			  0);

	gtk_widget_show (prefs_pane->priv->title_label);
	gtk_widget_show (prefs_pane->priv->description_label);

	/* Add title box to title frame */
	gtk_container_add (GTK_CONTAINER (prefs_pane->priv->title_frame),
			   prefs_pane->priv->title_box);

	gtk_widget_show (prefs_pane->priv->title_box);

	/* Add title frame to ourselves */
	gtk_box_pack_start (GTK_BOX (prefs_pane),
			    prefs_pane->priv->title_frame,
			    FALSE,
			    FALSE,
			    0);

	gtk_widget_show (prefs_pane->priv->title_frame);

	/* Groups box */
	prefs_pane->priv->groups_box = 
		gtk_vbox_new (TRUE, PREFS_PANE_TITLE_SPACING);

	/* Add groups box to ourselves */
	gtk_box_pack_start (GTK_BOX (prefs_pane),
			    prefs_pane->priv->groups_box,
			    FALSE,
			    FALSE,
			    PREFS_PANE_GROUPS_BOX_TOP_OFFSET);

	gtk_widget_show (prefs_pane->priv->groups_box);
}


/*
 * NautilusPrefsPane public methods
 */
GtkWidget*
nautilus_prefs_pane_new (const gchar *pane_title,
			 const gchar *pane_description)
{
	NautilusPrefsPane *prefs_pane;

	g_return_val_if_fail (pane_title != NULL, NULL);
	g_return_val_if_fail (pane_description != NULL, NULL);

	prefs_pane = gtk_type_new (nautilus_prefs_pane_get_type ());

	prefs_pane_construct (prefs_pane, pane_title, pane_description);

	return GTK_WIDGET (prefs_pane);
}

void
nautilus_prefs_pane_set_title (NautilusPrefsPane *prefs_pane,
			       const gchar	 *pane_title)
{
	g_return_if_fail (prefs_pane != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane));

	g_assert (prefs_pane->priv->title_label != NULL);
	
	gtk_label_set_text (GTK_LABEL (prefs_pane->priv->title_label),
			    pane_title);
}

void
nautilus_prefs_pane_set_description (NautilusPrefsPane *prefs_pane,
				     const gchar	 *pane_description)
{
	g_return_if_fail (prefs_pane != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane));

	g_assert (prefs_pane->priv->description_label != NULL);

	gtk_label_set_text (GTK_LABEL (prefs_pane->priv->description_label),
			    pane_description);
}

void
nautilus_prefs_pane_add_group (NautilusPrefsPane	*prefs_pane,
			       GtkWidget		*prefs_group)
{
	g_return_if_fail (prefs_pane != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_PANE (prefs_pane));
	g_return_if_fail (prefs_group != NULL);

// 	group = nautilus_prefs_group_new (group_title);

	gtk_box_pack_start (GTK_BOX (prefs_pane->priv->groups_box),
			    prefs_group,
			    TRUE,
			    TRUE,
			    0);

//	gtk_widget_show (prefs_group);
}

