/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-location-bar.h - Location bar for Nautilus

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NAUTILUS_LOCATION_BAR_H
#define NAUTILUS_LOCATION_BAR_H

#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>

#define NAUTILUS_LOCATION_BAR(obj) \
	GTK_CHECK_CAST (obj, nautilus_location_bar_get_type (), NautilusLocationBar)
#define NAUTILUS_LOCATION_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, nautilus_location_bar_get_type (), NautilusLocationBarClass)
#define NAUTILUS_IS_LOCATION_BAR(obj) \
	GTK_CHECK_TYPE (obj, nautilus_location_bar_get_type ())

typedef struct NautilusLocationBar {
	GtkHBox hbox;

	GtkLabel *label;
	GtkEntry *entry;

	gchar *undo_text;
	gboolean undo_registered;
} NautilusLocationBar;

typedef struct {
	GtkHBoxClass parent_class;

	void         (*location_changed) (NautilusLocationBar *location_bar,
					  const char *location);
} NautilusLocationBarClass;

GtkType    nautilus_location_bar_get_type     	(void);
GtkWidget* nautilus_location_bar_new          	(void);
void       nautilus_location_bar_set_location 	(NautilusLocationBar *bar,
					       	 const char          *location);


#endif /* NAUTILUS_LOCATION_BAR_H */
