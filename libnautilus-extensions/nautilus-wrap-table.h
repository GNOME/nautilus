/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-wrap-table.h - A table that can wrap its contents as needed.

   Copyright (C) 2000 Eazel, Inc.

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

#ifndef NAUTILUS_WRAP_TABLE_H
#define NAUTILUS_WRAP_TABLE_H

#include <gtk/gtkcontainer.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_WRAP_TABLE            (nautilus_wrap_table_get_type ())
#define NAUTILUS_WRAP_TABLE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WRAP_TABLE, NautilusWrapTable))
#define NAUTILUS_WRAP_TABLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WRAP_TABLE, NautilusWrapTableClass))
#define NAUTILUS_IS_WRAP_TABLE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WRAP_TABLE))
#define NAUTILUS_IS_WRAP_TABLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WRAP_TABLE))

typedef struct NautilusWrapTable	  NautilusWrapTable;
typedef struct NautilusWrapTableClass     NautilusWrapTableClass;
typedef struct NautilusWrapTableDetails   NautilusWrapTableDetails;

struct NautilusWrapTable
{
	/* Superclass */
	GtkContainer container;

	/* Private things */
	NautilusWrapTableDetails *details;
};

struct NautilusWrapTableClass
{
	GtkContainerClass parent_class;
};

typedef enum
{
	NAUTILUS_JUSTIFICATION_BEGINNING,
	NAUTILUS_JUSTIFICATION_MIDDLE,
	NAUTILUS_JUSTIFICATION_END
} NautilusJustification;

/* Public GtkWrapTable methods */
GtkType               nautilus_wrap_table_get_type                  (void);
GtkWidget *           nautilus_wrap_table_new                       (gboolean                 homogeneous);
void                  nautilus_wrap_table_set_x_spacing             (NautilusWrapTable       *wrap_table,
								     guint                    x_spacing);
guint                 nautilus_wrap_table_get_x_spacing             (const NautilusWrapTable *wrap_table);
void                  nautilus_wrap_table_set_y_spacing             (NautilusWrapTable       *wrap_table,
								     guint                    y_spacing);
guint                 nautilus_wrap_table_get_y_spacing             (const NautilusWrapTable *wrap_table);
GtkWidget *           nautilus_wrap_table_find_child_at_event_point (const NautilusWrapTable *wrap_table,
								     int                      x,
								     int                      y);
void                  nautilus_wrap_table_set_x_justification       (NautilusWrapTable       *wrap_table,
								     GtkJustification         justification);
NautilusJustification nautilus_wrap_table_get_x_justification       (const NautilusWrapTable *wrap_table);
void                  nautilus_wrap_table_set_y_justification       (NautilusWrapTable       *wrap_table,
								     NautilusJustification    justification);
NautilusJustification nautilus_wrap_table_get_y_justification       (const NautilusWrapTable *wrap_table);
void                  nautilus_wrap_table_set_homogeneous           (NautilusWrapTable       *wrap_table,
								     gboolean                 homogeneous);
gboolean              nautilus_wrap_table_get_homogeneous           (const NautilusWrapTable *wrap_table);

END_GNOME_DECLS

#endif /* NAUTILUS_WRAP_TABLE_H */


