/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* ammonite-login-dialog.h - A use password prompting dialog widget.

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

#ifndef AMMONITE_LOGIN_DIALOG_H
#define AMMONITE_LOGIN_DIALOG_H

#include <libgnomeui/gnome-dialog.h>

BEGIN_GNOME_DECLS

#define GTK_TYPE_AUTH_DIALOG            (ammonite_login_dialog_get_type ())
#define AMMONITE_LOGIN_DIALOG(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_AUTH_DIALOG, AmmoniteLoginDialog))
#define AMMONITE_LOGIN_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_AUTH_DIALOG, AmmoniteLoginDialogClass))
#define NAUTILUS_IS_PASSWORD_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_AUTH_DIALOG))
#define NAUTILUS_IS_PASSWORD_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_AUTH_DIALOG))

typedef struct _AmmoniteLoginDialog       AmmoniteLoginDialog;
typedef struct _AmmoniteLoginDialogClass  AmmoniteLoginDialogClass;
typedef struct _AmmoniteLoginDialogDetails AmmoniteLoginDialogDetails;

struct _AmmoniteLoginDialog
{
	GnomeDialog gnome_dialog;

	AmmoniteLoginDialogDetails *details;
};

struct _AmmoniteLoginDialogClass
{
	GnomeDialogClass parent_class;
};


GtkType    ammonite_login_dialog_get_type                (void);
GtkWidget* ammonite_login_dialog_new                     (const char             *dialog_title,
							     const char		    *message,
							     const char             *username,
							     const char             *password,
							     gboolean                readonly_username,
							     gboolean		     has_remember_box);
gboolean   ammonite_login_dialog_run_and_block           (AmmoniteLoginDialog *password_dialog);

/* Attribute mutators */
void     ammonite_login_dialog_set_username            (AmmoniteLoginDialog *password_dialog,
							   const char             *username);
void     ammonite_login_dialog_set_password            (AmmoniteLoginDialog *password_dialog,
							   const char             *password);
void     ammonite_login_dialog_set_readonly_username   (AmmoniteLoginDialog *password_dialog,
							   gboolean                readonly);
void     ammonite_login_dialog_set_remember            (AmmoniteLoginDialog *password_dialog,
							   gboolean                remember);
void     ammonite_login_dialog_set_remember_label_text (AmmoniteLoginDialog *password_dialog,
							   const char             *remember_label_text);

/* Attribute accessors */
char *   ammonite_login_dialog_get_username            (AmmoniteLoginDialog *password_dialog);
char *   ammonite_login_dialog_get_password            (AmmoniteLoginDialog *password_dialog);
gboolean ammonite_login_dialog_get_remember            (AmmoniteLoginDialog *password_dialog);

END_GNOME_DECLS

#endif /* AMMONITE_LOGIN_DIALOG_H */
