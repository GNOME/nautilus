/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-dialog.h - Interface for preferences dialog.

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

#ifndef NAUTILUS_PREFS_DIALOG_H
#define NAUTILUS_PREFS_DIALOG_H

#include <libgnomeui/gnome-dialog.h>
#include "nautilus-prefs-box.h"

//#include <gnome.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS_DIALOG            (nautilus_prefs_dialog_get_type ())
#define NAUTILUS_PREFS_DIALOG(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS_DIALOG, NautilusPrefsDialog))
#define NAUTILUS_PREFS_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS_DIALOG, NautilusPrefsDialogClass))
#define NAUTILUS_IS_PREFS_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS_DIALOG))
#define NAUTILUS_IS_PREFS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS_DIALOG))


typedef struct _NautilusPrefsDialog	      NautilusPrefsDialog;
typedef struct _NautilusPrefsDialogClass      NautilusPrefsDialogClass;
typedef struct _NautilusPrefsDialogPrivate    NautilusPrefsDialogPrivate;

struct _NautilusPrefsDialog
{
	/* Super Class */
	GnomeDialog			gnome_dialog;

	/* Private stuff */
	NautilusPrefsDialogPrivate	*priv;
};

struct _NautilusPrefsDialogClass
{
	GnomeDialogClass parent_class;

	void (*activate) (GtkWidget * prefs_dialog, gint entry_number);
};

GtkType    nautilus_prefs_dialog_get_type              (void);
GtkWidget* nautilus_prefs_dialog_new                   (const gchar *dialog_title);
gboolean   nautilus_prefs_dialog_run_and_block         (NautilusPrefsDialog *prefs_dialog);
GtkWidget* nautilus_prefs_dialog_get_prefs_box         (NautilusPrefsDialog *prefs_dialog);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_DIALOG_H */


