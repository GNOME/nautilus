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

#ifndef NAUTILUS_PREFERENCES_PANE_H
#define NAUTILUS_PREFERENCES_PANE_H

#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkvbox.h>
#include <libnautilus-private/nautilus-preferences-group.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCES_PANE            (nautilus_preferences_pane_get_type ())
#define NAUTILUS_PREFERENCES_PANE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCES_PANE, NautilusPreferencesPane))
#define NAUTILUS_PREFERENCES_PANE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCES_PANE, NautilusPreferencesPaneClass))
#define NAUTILUS_IS_PREFERENCES_PANE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCES_PANE))
#define NAUTILUS_IS_PREFERENCES_PANE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCES_PANE))

typedef struct NautilusPreferencesPane		  NautilusPreferencesPane;
typedef struct NautilusPreferencesPaneClass	  NautilusPreferencesPaneClass;
typedef struct NautilusPreferencesPaneDetails	  NautilusPreferencesPaneDetails;

struct NautilusPreferencesPane
{
	/* Super Class */
	GtkVBox vbox;

	/* Private stuff */
	NautilusPreferencesPaneDetails *details;
};

struct NautilusPreferencesPaneClass
{
	GtkVBoxClass parent_class;
};

GtkType    nautilus_preferences_pane_get_type               (void);
GtkWidget* nautilus_preferences_pane_new                    (void);
GtkWidget *nautilus_preferences_pane_add_group              (NautilusPreferencesPane       *preferences_pane,
							     const char                    *group_title);
void       nautilus_preferences_pane_update                 (NautilusPreferencesPane       *preferences_pane);
guint      nautilus_preferences_pane_get_num_groups         (const NautilusPreferencesPane *pane);
guint      nautilus_preferences_pane_get_num_visible_groups (const NautilusPreferencesPane *pane);
GtkWidget* nautilus_preferences_pane_find_group             (const NautilusPreferencesPane *preferences_pane,
							     const char                    *group_title);
void       nautilus_preferences_pane_add_control_preference (NautilusPreferencesPane       *preferences_pane,
							     const char                    *control_preference_name);

END_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_PANE_H */
