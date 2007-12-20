/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-column-choose.h - A column chooser widget

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#ifndef NAUTILUS_COLUMN_CHOOSER_H
#define NAUTILUS_COLUMN_CHOOSER_H

#include <gtk/gtkhbox.h>

#define NAUTILUS_TYPE_COLUMN_CHOOSER                (nautilus_column_chooser_get_type ())
#define NAUTILUS_COLUMN_CHOOSER(obj)                (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_COLUMN_CHOOSER, NautilusColumnChooser))
#define NAUTILUS_COLUMN_CHOOSER_CLASS(klass)        (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_COLUMN_CHOOSER, NautilusColumnChooserClass))
#define NAUTILUS_IS_COLUMN_CHOOSER(obj)             (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_COLUMN_CHOOSER))
#define NAUTILUS_IS_COLUMN_CHOOSER_CLASS(klass)     (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_COLUMN_CHOOSER))
 
typedef struct _NautilusColumnChooserDetails NautilusColumnChooserDetails;

typedef struct {
	GtkHBox parent;
	
	NautilusColumnChooserDetails *details;
} NautilusColumnChooser;

typedef struct {
        GtkHBoxClass parent_slot;

	void (*changed) (NautilusColumnChooser *chooser);
	void (*use_default) (NautilusColumnChooser *chooser);
} NautilusColumnChooserClass;

GType      nautilus_column_chooser_get_type            (void);
GtkWidget *nautilus_column_chooser_new                 (void);
void       nautilus_column_chooser_set_settings    (NautilusColumnChooser   *chooser,
						    char                   **visible_columns, 
						    char                   **column_order);
void       nautilus_column_chooser_get_settings    (NautilusColumnChooser *chooser,
						    char                  ***visible_columns, 
						    char                  ***column_order);

#endif /* NAUTILUS_COLUMN_CHOOSER_H */
