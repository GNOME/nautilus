/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-group.c - Interface for a prefs group superclass.

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

#include "nautilus-prefs-group.h"
#include <libnautilus/nautilus-gtk-macros.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>

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

static const guint PREFS_GROUP_TITLE_SPACING = 4;

struct _NautilusPrefsGroupPrivate
{
	gchar		*title_label_string;
	GtkWidget	*content_box;
	gboolean	is_constructed;
};

typedef void (*GnomeBoxSignal1) (GtkObject* object,
				    gint arg1,
				    gpointer data);

/* NautilusPrefsGroupClass methods */
static void nautilus_prefs_group_initialize_class (NautilusPrefsGroupClass *klass);
static void nautilus_prefs_group_initialize       (NautilusPrefsGroup      *prefs_group);

/* GtkObjectClass methods */
static void nautilus_prefs_group_destroy    (GtkObject            *object);

/* Misc private stuff */
static void prefs_group_construct (NautilusPrefsGroup *prefs_group,
				   const gchar *group_title);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPrefsGroup, nautilus_prefs_group, GTK_TYPE_FRAME)

/*
 * NautilusPrefsGroupClass methods
 */
static void
nautilus_prefs_group_initialize_class (NautilusPrefsGroupClass *prefs_group_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_group_class);
	widget_class = GTK_WIDGET_CLASS (prefs_group_class);

 	parent_class = gtk_type_class (gtk_frame_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_prefs_group_destroy;

	/* NautilusPrefsGroupClass */
	prefs_group_class->construct = prefs_group_construct;
	prefs_group_class->changed = NULL;
}

static void
nautilus_prefs_group_initialize (NautilusPrefsGroup *prefs_group)
{
	prefs_group->priv = g_new (NautilusPrefsGroupPrivate, 1);

	prefs_group->priv->title_label_string = NULL;
	prefs_group->priv->content_box = NULL;
	prefs_group->priv->is_constructed = FALSE;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_prefs_group_destroy (GtkObject *object)
{
	NautilusPrefsGroup * prefs_group;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP (object));
	
	prefs_group = NAUTILUS_PREFS_GROUP (object);

	g_free (prefs_group->priv);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * Misc private stuff
 */
static void
prefs_group_construct (NautilusPrefsGroup *prefs_group,
		       const gchar *group_title)
{
	g_assert (prefs_group != NULL);
	g_assert (prefs_group->priv != NULL);

	g_assert (group_title != NULL);

	g_assert (prefs_group->priv->content_box == NULL);

	/* Ourselves */
	gtk_frame_set_shadow_type (GTK_FRAME (prefs_group),
				   GTK_SHADOW_ETCHED_IN);

	nautilus_prefs_group_set_title (prefs_group, group_title);

	/* Main box */
	prefs_group->priv->content_box = 
		gtk_vbox_new (FALSE, PREFS_GROUP_TITLE_SPACING);

	gtk_container_add (GTK_CONTAINER (prefs_group),
			   prefs_group->priv->content_box);

	gtk_widget_show (prefs_group->priv->content_box);

#if 0
	NAUTILUS_PREFS_GROUP_ASSERT_METHOD (prefs_group, construct);

 	NAUTILUS_PREFS_GROUP_INVOKE_METHOD (prefs_group, construct) (prefs_group,
 							  prefs_group->priv->content_box);
#endif
}

/*
 * NautilusPrefsGroup public methods
 */
// GtkWidget*
// nautilus_prefs_group_new (const gchar *group_title)
// {
// 	NautilusPrefsGroup *prefs_group;

// 	g_return_val_if_fail (group_title != NULL, NULL);

// 	prefs_group = gtk_type_new (nautilus_prefs_group_get_type ());

// 	prefs_group_construct (prefs_group, group_title);

// 	return GTK_WIDGET (prefs_group);
// }

void
nautilus_prefs_group_set_title (NautilusPrefsGroup *prefs_group,
				const gchar *group_title)
{
	g_return_if_fail (prefs_group != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_GROUP (prefs_group));
	g_return_if_fail (GTK_IS_FRAME (prefs_group));
	g_return_if_fail (group_title != NULL);

	gtk_frame_set_label (GTK_FRAME (prefs_group), group_title);
}

GtkWidget*
nautilus_prefs_group_get_content_box (NautilusPrefsGroup *prefs_group)
{
	g_return_val_if_fail (prefs_group != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_GROUP (prefs_group), NULL);

	return prefs_group->priv->content_box;
}

