/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-password-dialog.h - A use password prompting dialog widget.

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

#ifndef NAUTILUS_PASSWORD_DIALOG_H
#define NAUTILUS_PASSWORD_DIALOG_H

#include <libgnomeui/gnome-dialog.h>

BEGIN_GNOME_DECLS

#define GTK_TYPE_AUTH_DIALOG            (nautilus_password_dialog_get_type ())
#define NAUTILUS_PASSWORD_DIALOG(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_AUTH_DIALOG, NautilusPasswordDialog))
#define NAUTILUS_PASSWORD_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_AUTH_DIALOG, NautilusPasswordDialogClass))
#define NAUTILUS_IS_PASSWORD_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_AUTH_DIALOG))
#define NAUTILUS_IS_PASSWORD_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_AUTH_DIALOG))

typedef struct _NautilusPasswordDialog       NautilusPasswordDialog;
typedef struct _NautilusPasswordDialogClass  NautilusPasswordDialogClass;
typedef struct _NautilusPasswordDialogDetails NautilusPasswordDialogDetails;

struct _NautilusPasswordDialog
{
	GnomeDialog gnome_dialog;

	NautilusPasswordDialogDetails *details;
};

struct _NautilusPasswordDialogClass
{
	GnomeDialogClass parent_class;
};


GtkType    nautilus_password_dialog_get_type                (void);
GtkWidget* nautilus_password_dialog_new                     (const char             *dialog_title,
							     const char		    *message,
							     const char             *username,
							     const char             *password,
							     gboolean                readonly_username);
gboolean   nautilus_password_dialog_run_and_block           (NautilusPasswordDialog *password_dialog);

/* Attribute mutators */
void     nautilus_password_dialog_set_username            (NautilusPasswordDialog *password_dialog,
							   const char             *username);
void     nautilus_password_dialog_set_password            (NautilusPasswordDialog *password_dialog,
							   const char             *password);
void     nautilus_password_dialog_set_readonly_username   (NautilusPasswordDialog *password_dialog,
							   gboolean                readonly);
void     nautilus_password_dialog_set_remember            (NautilusPasswordDialog *password_dialog,
							   gboolean                remember);
void     nautilus_password_dialog_set_remember_label_text (NautilusPasswordDialog *password_dialog,
							   const char             *remember_label_text);

/* Attribute accessors */
char *   nautilus_password_dialog_get_username            (NautilusPasswordDialog *password_dialog);
char *   nautilus_password_dialog_get_password            (NautilusPasswordDialog *password_dialog);
gboolean nautilus_password_dialog_get_remember            (NautilusPasswordDialog *password_dialog);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PASSWORD_DIALOG_H */
