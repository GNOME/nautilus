/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-box.h - Implementation for preferences box component.

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
#include "nautilus-preferences-box.h"
#include "nautilus-gtk-macros.h"

#include <gtk/gtkclist.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const guint PREFS_BOX_NUM_CATEGORY_COLUMNS = 1;
static const guint PREFS_BOX_CATEGORY_COLUMN = 0;
static const guint PREFS_BOX_SPACING = 4;
static const guint PREFS_SELECTED_PANE_UNKNOWN = -1;
static const guint PREFS_BOX_PANE_LEFT_OFFSET = 10;

typedef struct
{
	gchar		*pane_name;
	GtkWidget	*pane_widget;
	gboolean	constructed;
} PaneInfo;

struct _NautilusPreferencesBoxDetails
{
	GtkWidget	*category_list;
	GtkWidget	*pane_container;

	GList		*panes;

	gint		selected_pane;
};

typedef void (*GnomeBoxSignal1) (GtkObject* object,
				    gint arg1,
				    gpointer data);

/* NautilusPreferencesBoxClass methods */
static void                   nautilus_preferences_box_initialize_class (NautilusPreferencesBoxClass *klass);
static void                   nautilus_preferences_box_initialize       (NautilusPreferencesBox      *prefs_box);



/* GtkObjectClass methods */
static void                   nautilus_preferences_box_destroy          (GtkObject             *object);



/* Misc private stuff */
static void                   prefs_box_construct                 (NautilusPreferencesBox      *prefs_box);
static void                   prefs_box_select_pane               (NautilusPreferencesBox      *prefs_box,
								   guint                  pane_row);




/* PaneInfo functions */
static PaneInfo *pane_info_alloc                     (const gchar           *pane_name);
static void                   pane_info_free                      (PaneInfo *info);




/* Category list callbacks */
static void                   category_list_select_row            (GtkCList              *clist,
								   gint                   row,
								   gint                   column,
								   GdkEventButton        *event,
								   gpointer               user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesBox, nautilus_preferences_box, GTK_TYPE_HBOX)

/*
 * NautilusPreferencesBoxClass methods
 */
static void
nautilus_preferences_box_initialize_class (NautilusPreferencesBoxClass *prefs_box_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_box_class);
	widget_class = GTK_WIDGET_CLASS (prefs_box_class);

 	parent_class = gtk_type_class (gtk_hbox_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_box_destroy;
}

static void
nautilus_preferences_box_initialize (NautilusPreferencesBox *prefs_box)
{
	prefs_box->details = g_new (NautilusPreferencesBoxDetails, 1);

	prefs_box->details->category_list = NULL;
	prefs_box->details->pane_container = NULL;
	prefs_box->details->panes = NULL;

	prefs_box->details->selected_pane = PREFS_SELECTED_PANE_UNKNOWN;
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_preferences_box_destroy (GtkObject *object)
{
	NautilusPreferencesBox * prefs_box;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_BOX (object));
	
	prefs_box = NAUTILUS_PREFERENCES_BOX (object);

	if (prefs_box->details->panes)
	{
		GList *panes;

		panes = prefs_box->details->panes;

		while (panes)
		{
			PaneInfo * info = panes->data;

			g_assert (info != NULL);

			pane_info_free (info);

			panes = panes->next;
		}
		
		g_list_free (prefs_box->details->panes);
	}

	g_free (prefs_box->details);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * Misc private stuff
 */
static void
prefs_box_construct (NautilusPreferencesBox *prefs_box)
{
	g_assert (prefs_box != NULL);
	g_assert (prefs_box->details != NULL);

	g_assert (prefs_box->details->category_list == NULL);
	g_assert (prefs_box->details->panes == NULL);

	/* Configure ourselves */
 	gtk_box_set_homogeneous (GTK_BOX (prefs_box), FALSE);

 	gtk_box_set_spacing (GTK_BOX (prefs_box), PREFS_BOX_SPACING);

	/* The category list */
	prefs_box->details->category_list = 
		gtk_clist_new (PREFS_BOX_NUM_CATEGORY_COLUMNS);

	gtk_signal_connect (GTK_OBJECT (prefs_box->details->category_list), 
			    "select_row",
			    GTK_SIGNAL_FUNC (category_list_select_row),
			    (gpointer) prefs_box);

	gtk_clist_set_selection_mode (GTK_CLIST (prefs_box->details->category_list), 
				      GTK_SELECTION_BROWSE);

	gtk_clist_set_column_auto_resize (GTK_CLIST (prefs_box->details->category_list),
					  PREFS_BOX_CATEGORY_COLUMN,
					  TRUE);
	
	gtk_box_pack_start (GTK_BOX (prefs_box),
			    prefs_box->details->category_list,
			    FALSE,
			    TRUE,
			    0);

	gtk_widget_show (prefs_box->details->category_list);
}

static void
prefs_box_select_pane (NautilusPreferencesBox	*prefs_box,
		       guint		pane_row)
{
	GList			*pane_node;
	PaneInfo	*pane_info;
	GList			*pane_iterator;

	g_assert (prefs_box != NULL);
	g_assert (NAUTILUS_IS_PREFS_BOX (prefs_box));
	g_assert (prefs_box->details != NULL);
	g_assert (prefs_box->details->panes != NULL);

	g_assert (pane_row < g_list_length (prefs_box->details->panes));

	pane_node = g_list_nth (prefs_box->details->panes, pane_row);

	g_assert (pane_node != NULL);

	pane_info = pane_node->data;

	/* Show only the corresponding pane widget */
	pane_iterator = prefs_box->details->panes;

	while (pane_iterator)
	{
		PaneInfo * info = pane_iterator->data;
		
		g_assert (info != NULL);

		if (pane_info == info)
		{
			/* Construct pane for first time if needed */
			if (!info->constructed)
			{
				
				info->constructed = TRUE;
			}

			gtk_widget_show (info->pane_widget);
		}
		else
		{
			gtk_widget_hide (info->pane_widget);
		}
		
		pane_iterator = pane_iterator->next;
	}
}

/*
 * PaneInfo functions
 */
static PaneInfo *
pane_info_alloc (const gchar *pane_name)
{
	PaneInfo * info;

	g_assert (pane_name != NULL);

	info = g_new (PaneInfo, 1);

	info->pane_name = g_strdup (pane_name);

	return info;
}

static void
pane_info_free (PaneInfo *info)
{
	g_assert (info != NULL);

	g_free (info->pane_name);

	g_free (info);
}

/*
 * Category list callbacks
 */
static void
category_list_select_row (GtkCList		*clist,
			  gint			row,
			  gint			column,
			  GdkEventButton	*event,
			  gpointer		user_data)
{
	NautilusPreferencesBox *prefs_box = (NautilusPreferencesBox *) user_data;

	g_assert (prefs_box != NULL);
	g_assert (NAUTILUS_IS_PREFS_BOX (prefs_box));

	prefs_box_select_pane (prefs_box, (guint) row);
}

/*
 * NautilusPreferencesBox public methods
 */
GtkWidget*
nautilus_preferences_box_new (const gchar *box_title)
{
	NautilusPreferencesBox *prefs_box;

	prefs_box = gtk_type_new (nautilus_preferences_box_get_type ());

	prefs_box_construct (prefs_box);

	return GTK_WIDGET (prefs_box);
}

GtkWidget *
nautilus_preferences_box_add_pane (NautilusPreferencesBox	*prefs_box,
			     const gchar *pane_title,
			     const gchar *pane_description)
{
	PaneInfo	*info;
	gint			new_row;
	gchar			*text[PREFS_BOX_NUM_CATEGORY_COLUMNS];

	g_return_val_if_fail (prefs_box != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_BOX (prefs_box), NULL);
	g_return_val_if_fail (pane_title != NULL, NULL);
	g_return_val_if_fail (pane_description != NULL, NULL);

	info = pane_info_alloc (pane_title);

	prefs_box->details->panes = g_list_append (prefs_box->details->panes, 
						(gpointer) info);

	info->pane_widget = nautilus_preferences_pane_new (pane_title,
						     pane_description);

	gtk_box_pack_start (GTK_BOX (prefs_box),
			    info->pane_widget,
			    TRUE,
			    TRUE,
			    PREFS_BOX_PANE_LEFT_OFFSET);

	text[PREFS_BOX_CATEGORY_COLUMN] = (gchar *) pane_title;

	new_row = gtk_clist_append (GTK_CLIST (prefs_box->details->category_list), 
				    text);

	return info->pane_widget;
}
