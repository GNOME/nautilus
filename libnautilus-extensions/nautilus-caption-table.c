/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-caption-table.c - An easy way to do tables of aligned captions.

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
#include "nautilus-caption-table.h"

#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include "nautilus-gtk-macros.h"

struct _NautilusCaptionTableDetail
{
	GtkWidget	**labels;
	GtkWidget	**entries;
	guint		num_rows;
	guint		size;
};

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

/* NautilusCaptionTableClass methods */
static void       nautilus_caption_table_initialize_class (NautilusCaptionTableClass *klass);
static void       nautilus_caption_table_initialize       (NautilusCaptionTable      *caption_table);

/* GtkObjectClass methods */
static void       caption_table_destroy                   (GtkObject                 *object);

/* Private methods */
static GtkWidget* caption_table_find_next_sensitive_entry (NautilusCaptionTable      *caption_table,
							   guint                      index);
static int        caption_table_index_of_entry            (NautilusCaptionTable      *caption_table,
							   GtkWidget                 *entry);

/* Entry callbacks */
static void       entry_activate                          (GtkWidget                 *widget,
							   gpointer                   data);

/* Boilerplate stuff */
NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusCaptionTable,
				   nautilus_caption_table,
				   GTK_TYPE_TABLE)

static int caption_table_signals[LAST_SIGNAL] = { 0 };

static void
nautilus_caption_table_initialize_class (NautilusCaptionTableClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	caption_table_signals[ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusCaptionTableClass, activate),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, caption_table_signals, LAST_SIGNAL);
	
	/* GtkObjectClass */
	object_class->destroy = caption_table_destroy;
}

#define CAPTION_TABLE_DEFAULT_ROWS 1

static void
nautilus_caption_table_initialize (NautilusCaptionTable *caption_table)
{
	GtkTable *table = GTK_TABLE (caption_table);

	caption_table->detail = g_new (NautilusCaptionTableDetail, 1);

	caption_table->detail->num_rows = 0;

	caption_table->detail->size = 0;

	caption_table->detail->labels = NULL;
	caption_table->detail->entries = NULL;

	table->homogeneous = FALSE;
}

/* GtkObjectClass methods */
static void
caption_table_destroy (GtkObject *object)
{
	NautilusCaptionTable *caption_table;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (object));
	
	caption_table = NAUTILUS_CAPTION_TABLE (object);

	if (caption_table->detail->labels)
		g_free(caption_table->detail->labels);

	if (caption_table->detail->entries)
		g_free(caption_table->detail->entries);

	g_free (caption_table->detail);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

void
nautilus_caption_table_resize (NautilusCaptionTable	*caption_table,
			       guint			num_rows)
{
	GtkTable* table = NULL;

	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));

	/* Make sure the num_rows have changed */
	if (caption_table->detail->num_rows == num_rows)
		return;

	caption_table->detail->num_rows = num_rows;

	/* Resize the GtkTable */
	table = GTK_TABLE (caption_table);
	gtk_table_resize(table, caption_table->detail->num_rows, 2);

	/* Create more label/entry pairs if needed */
	if (caption_table->detail->num_rows > caption_table->detail->size)
	{
		guint i;
		guint old_size = caption_table->detail->size;
		guint new_size = caption_table->detail->num_rows;
		guint realloc_size = sizeof(GtkWidget *) * new_size;
		
		/* FIXME bugzilla.eazel.com 680: Use a GList for this */
		caption_table->detail->labels = (GtkWidget**) g_realloc (caption_table->detail->labels,
									 realloc_size);

		caption_table->detail->entries = (GtkWidget**) g_realloc (caption_table->detail->entries,
									  realloc_size);
		
		for (i = old_size; i < new_size; i++)
		{
			caption_table->detail->labels[i] = gtk_label_new("");
			caption_table->detail->entries[i] = gtk_entry_new();
			
			gtk_signal_connect(GTK_OBJECT (caption_table->detail->entries[i]),
					   "activate",
					   GTK_SIGNAL_FUNC (entry_activate),
					   (gpointer) caption_table);

			gtk_misc_set_alignment (GTK_MISC (caption_table->detail->labels[i]), 1.0, 0.5);

			/* Column 1 */
			gtk_table_attach (table,
					  caption_table->detail->labels[i],	/* child */
					  0,					/* left_attatch */
					  1,					/* right_attatch */
					  i,					/* top_attatch */
					  i + 1,				/* bottom_attatch */
					  GTK_FILL,				/* xoptions */
					  (GTK_FILL|GTK_EXPAND),		/* yoptions */
					  0,					/* xpadding */
					  0);					/* ypadding */
			
			/* Column 2 */
			gtk_table_attach (table, 
					  caption_table->detail->entries[i],	/* child */
					  1,					/* left_attatch */
					  2,					/* right_attatch */
					  i,					/* top_attatch */
					  i + 1,				/* bottom_attatch */
					  (GTK_FILL|GTK_EXPAND),		/* xoptions */
					  (GTK_FILL|GTK_EXPAND),		/* yoptions */
					  0,					/* xpadding */
					  0);					/* ypadding */
		}

		caption_table->detail->size = new_size;
	}

	/* Show only the needed caption widgets */
	if (caption_table->detail->size > 0)
	{
		guint i;

		for(i = 0; i < caption_table->detail->size; i++)
		{
			if (i < caption_table->detail->num_rows)
			{
				gtk_widget_show (caption_table->detail->labels[i]);
				gtk_widget_show (caption_table->detail->entries[i]);
			}
			else
			{
				gtk_widget_hide (caption_table->detail->labels[i]);
				gtk_widget_hide (caption_table->detail->entries[i]);
			}
		}
	}

	/* Set inter row spacing */
	if (caption_table->detail->num_rows > 1)
	{
		guint i;

		for(i = 0; i < (caption_table->detail->num_rows - 1); i++)
			gtk_table_set_row_spacing (GTK_TABLE (table), i, 10);
	}
}

static int
caption_table_index_of_entry (NautilusCaptionTable *caption_table,
			      GtkWidget* entry)
{
	guint i;

	g_return_val_if_fail (caption_table != NULL, -1);
	g_return_val_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table), -1);

	for(i = 0; i < caption_table->detail->num_rows; i++)
		if (caption_table->detail->entries[i] == entry)
			return i;

	return -1;
}

static GtkWidget*
caption_table_find_next_sensitive_entry (NautilusCaptionTable	*caption_table,
					 guint			index)
{
	guint i;

	g_return_val_if_fail (caption_table != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table), NULL);

	for(i = index; i < caption_table->detail->num_rows; i++)
		if (GTK_WIDGET_SENSITIVE (caption_table->detail->entries[i]))
			return caption_table->detail->entries[i];

	return NULL;
}

static void
entry_activate (GtkWidget *widget, gpointer data)
{
	NautilusCaptionTable *caption_table = NAUTILUS_CAPTION_TABLE (data);
	int index;
	
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	
	index = caption_table_index_of_entry (caption_table, widget);
	
	/* Check for an invalid index */
	if (index == -1)
		return;
	
	/* Check for the last index */
	if (index < caption_table->detail->num_rows)
	{
		/* Look for the next sensitive entry */
		GtkWidget* sensitive_entry = 
			caption_table_find_next_sensitive_entry (caption_table, index + 1);
		
		/* Make the next sensitive entry take focus */
		if (sensitive_entry)
			gtk_widget_grab_focus (sensitive_entry);
	}
	
	/* Emit the activate signal */
	gtk_signal_emit (GTK_OBJECT (caption_table), 
			 caption_table_signals[ACTIVATE], 
			 index);
}

/* Public methods */
GtkWidget*
nautilus_caption_table_new (guint num_rows)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (nautilus_caption_table_get_type()));

	if (num_rows == 0)
		num_rows = 1;

	nautilus_caption_table_resize (NAUTILUS_CAPTION_TABLE(widget), num_rows);

	gtk_table_set_col_spacing (GTK_TABLE (widget), 0, 10);

	return widget;
}

void
nautilus_caption_table_set_row_info (NautilusCaptionTable *caption_table,
				     guint row,
				     const char* label_text,
				     const char* entry_text,
				     gboolean entry_visibility,
				     gboolean entry_readonly)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	gtk_label_set_text (GTK_LABEL (caption_table->detail->labels[row]), label_text);

	gtk_entry_set_text (GTK_ENTRY (caption_table->detail->entries[row]), entry_text);
	gtk_entry_set_visibility (GTK_ENTRY (caption_table->detail->entries[row]), entry_visibility);
	gtk_widget_set_sensitive (caption_table->detail->entries[row], !entry_readonly);
}

void
nautilus_caption_table_set_entry_text (NautilusCaptionTable *caption_table,
				       guint row,
				       const char* entry_text)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	gtk_entry_set_text (GTK_ENTRY (caption_table->detail->entries[row]), entry_text);
}

void
nautilus_caption_table_set_entry_readonly (NautilusCaptionTable *caption_table,
					   guint row,
					   gboolean readonly)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);
	
	gtk_widget_set_sensitive (caption_table->detail->entries[row], !readonly);
}

void
nautilus_caption_table_entry_grab_focus (NautilusCaptionTable *caption_table, guint row)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	if (GTK_WIDGET_SENSITIVE (caption_table->detail->entries[row]))
		gtk_widget_grab_focus (caption_table->detail->entries[row]);
}

char*
nautilus_caption_table_get_entry_text (NautilusCaptionTable *caption_table, guint row)
{
	char *text;

	g_return_val_if_fail (caption_table != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table), NULL);
	g_return_val_if_fail (row < caption_table->detail->num_rows, NULL);

	text = gtk_entry_get_text (GTK_ENTRY (caption_table->detail->entries[row]));

	return g_strdup (text);
}

guint
nautilus_caption_table_get_num_rows (NautilusCaptionTable *caption_table)
{
	g_return_val_if_fail (caption_table != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_CAPTION_TABLE (caption_table), 0);

	return caption_table->detail->num_rows;
}
