/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-dialog.h - Interface for preferences dialog.

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

#ifndef NAUTILUS_PREFERENCES_DIALOG_H
#define NAUTILUS_PREFERENCES_DIALOG_H

#include <libgnomeui/gnome-dialog.h>
#include <libnautilus-extensions/nautilus-preferences-box.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS_DIALOG            (nautilus_preferences_dialog_get_type ())
#define NAUTILUS_PREFERENCES_DIALOG(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS_DIALOG, NautilusPreferencesDialog))
#define NAUTILUS_PREFERENCES_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS_DIALOG, NautilusPreferencesDialogClass))
#define NAUTILUS_IS_PREFS_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS_DIALOG))
#define NAUTILUS_IS_PREFS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS_DIALOG))


typedef struct _NautilusPreferencesDialog	      NautilusPreferencesDialog;
typedef struct _NautilusPreferencesDialogClass      NautilusPreferencesDialogClass;
typedef struct _NautilusPreferencesDialogDetails    NautilusPreferencesDialogDetails;

struct _NautilusPreferencesDialog
{
	/* Super Class */
	GnomeDialog				gnome_dialog;

	/* Private stuff */
	NautilusPreferencesDialogDetails	*details;
};

struct _NautilusPreferencesDialogClass
{
	GnomeDialogClass parent_class;

	void (*activate) (GtkWidget * prefs_dialog, gint entry_number);
};

GtkType    nautilus_preferences_dialog_get_type      (void);
GtkWidget* nautilus_preferences_dialog_new           (const gchar         *dialog_title);
GtkWidget* nautilus_preferences_dialog_get_prefs_box (NautilusPreferencesDialog *prefs_dialog);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_DIALOG_H */


