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
#include <libnautilus-extensions/nautilus-preferences-group.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS_PANE            (nautilus_preferences_pane_get_type ())
#define NAUTILUS_PREFERENCES_PANE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS_PANE, NautilusPreferencesPane))
#define NAUTILUS_PREFERENCES_PANE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS_PANE, NautilusPreferencesPaneClass))
#define NAUTILUS_IS_PREFS_PANE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS_PANE))
#define NAUTILUS_IS_PREFS_PANE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS_PANE))

typedef struct _NautilusPreferencesPane	   NautilusPreferencesPane;
typedef struct _NautilusPreferencesPaneClass      NautilusPreferencesPaneClass;
typedef struct _NautilusPreferencesPaneDetails    NautilusPreferencesPaneDetails;

struct _NautilusPreferencesPane
{
	/* Super Class */
	GtkVBox				vbox;

	/* Private stuff */
	NautilusPreferencesPaneDetails	*details;
};

struct _NautilusPreferencesPaneClass
{
	GtkVBoxClass	parent_class;

	void (*construct) (NautilusPreferencesPane *prefs_pane, GtkWidget *box);
};

GtkType    nautilus_preferences_pane_get_type              (void);
GtkWidget* nautilus_preferences_pane_new                   (const gchar                 *pane_title,
							    const gchar                 *pane_description);
void       nautilus_preferences_pane_set_title             (NautilusPreferencesPane     *prefs_pane,
							    const gchar                 *pane_title);
void       nautilus_preferences_pane_set_description       (NautilusPreferencesPane     *prefs_pane,
							    const gchar                 *pane_description);
GtkWidget *nautilus_preferences_pane_add_group             (NautilusPreferencesPane     *prefs_pane,
							    const char                  *group_title);
GtkWidget *nautilus_preferences_pane_add_item_to_nth_group (NautilusPreferencesPane     *prefs_pane,
							    guint                        n,
							    const char                  *preference_name,
							    NautilusPreferencesItemType  item_type);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_PANE_H */
