/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-caption-table.h - An easy way to do tables of aligned captions.

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

#ifndef NAUTILUS_CAPTION_TABLE_H
#define NAUTILUS_CAPTION_TABLE_H

#include <gtk/gtktable.h>
#include <gnome.h>

/*
 * NautilusCaptionTable is a GtkTable sublass that allows you to painlessly
 * create tables of nicely aligned captions.
 */

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_CAPTION_TABLE			(nautilus_caption_table_get_type ())
#define NAUTILUS_CAPTION_TABLE(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CAPTION_TABLE, NautilusCaptionTable))
#define NAUTILUS_CAPTION_TABLE_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CAPTION_TABLE, NautilusCaptionTableClass))
#define NAUTILUS_IS_CAPTION_TABLE(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CAPTION_TABLE))
#define NAUTILUS_IS_CAPTION_TABLE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CAPTION_TABLE))


typedef struct _NautilusCaptionTable		NautilusCaptionTable;
typedef struct _NautilusCaptionTableClass	NautilusCaptionTableClass;
typedef struct _NautilusCaptionTableDetail	NautilusCaptionTableDetail;

struct _NautilusCaptionTable
{
	GtkTable table;

	NautilusCaptionTableDetail *detail;
};

struct _NautilusCaptionTableClass
{
	GtkTableClass parent_class;

	void (*activate) (GtkWidget *caption_table, int active_entry);
};

GtkType    nautilus_caption_table_get_type           (void);
GtkWidget* nautilus_caption_table_new                (guint                 num_rows);
void       nautilus_caption_table_set_row_info       (NautilusCaptionTable *caption_table,
						      guint                 row,
						      const char           *label_text,
						      const char           *entry_text,
						      gboolean              entry_visibility,
						      gboolean              entry_readonly);
void       nautilus_caption_table_set_entry_text     (NautilusCaptionTable *caption_table,
						      guint                 row,
						      const char           *entry_text);
void       nautilus_caption_table_set_entry_readonly (NautilusCaptionTable *caption_table,
						      guint                 row,
						      gboolean              readonly);
void       nautilus_caption_table_entry_grab_focus   (NautilusCaptionTable *caption_table,
						      guint                 row);
char*      nautilus_caption_table_get_entry_text     (NautilusCaptionTable *caption_table,
						      guint                 row);
guint      nautilus_caption_table_get_num_rows       (NautilusCaptionTable *caption_table);
void       nautilus_caption_table_resize             (NautilusCaptionTable *caption_table,
						      guint                 num_rows);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_CAPTION_TABLE_H */


