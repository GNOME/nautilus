/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-box.h - Interface for preferences box component.

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

#ifndef NAUTILUS_PREFERENCES_BOX_H
#define NAUTILUS_PREFERENCES_BOX_H

#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkhbox.h>
#include <libnautilus-extensions/nautilus-preferences-pane.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCES_BOX            (nautilus_preferences_box_get_type ())
#define NAUTILUS_PREFERENCES_BOX(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCES_BOX, NautilusPreferencesBox))
#define NAUTILUS_PREFERENCES_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCES_BOX, NautilusPreferencesBoxClass))
#define NAUTILUS_IS_PREFERENCES_BOX(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCES_BOX))
#define NAUTILUS_IS_PREFERENCES_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCES_BOX))

typedef struct NautilusPreferencesBox		 NautilusPreferencesBox;
typedef struct NautilusPreferencesBoxClass	 NautilusPreferencesBoxClass;
typedef struct NautilusPreferencesBoxDetails	 NautilusPreferencesBoxDetails;

struct NautilusPreferencesBox
{
	/* Super Class */
	GtkHBox hbox;

	/* Private stuff */
	NautilusPreferencesBoxDetails *details;
};

struct NautilusPreferencesBoxClass
{
	GtkHBoxClass parent_class;
};

GtkType    nautilus_preferences_box_get_type      (void);
GtkWidget* nautilus_preferences_box_new           (void);
GtkWidget* nautilus_preferences_box_add_pane      (NautilusPreferencesBox       *preferences_box,
						   const char                   *pane_title);
void       nautilus_preferences_box_update        (NautilusPreferencesBox       *preferences_box);
GtkWidget* nautilus_preferences_box_find_pane     (const NautilusPreferencesBox *preferences_box,
						   const char                   *pane_name);

END_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_BOX_H */


