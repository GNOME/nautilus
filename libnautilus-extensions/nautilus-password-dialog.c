/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-password-dialog.c - A use password prompting dialog widget.

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

#include <config.h>
#include "nautilus-password-dialog.h"
#include "nautilus-caption-table.h"

#include <libgnomeui/gnome-stock.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

struct _NautilusPasswordDialogDetail
{
	char		*username;
	char		*password;
	gint		last_button_clicked;
	gboolean	readonly_username;
	GtkWidget	*table;
	GtkWidget	*blurb;
};

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const char * stock_buttons[] =
{
	GNOME_STOCK_BUTTON_OK,
	GNOME_STOCK_BUTTON_CANCEL,
	NULL
};

static const gint UNKNOWN_BUTTON = -1;
static const gint OK_BUTTON = 0;
static const gint DEFAULT_BUTTON = 0;
static const guint DEFAULT_BORDER_WIDTH = 0;

enum 
{
	USERNAME_ROW = 0,
	PASSWORD_ROW
};

/* NautilusPasswordDialogClass methods */
static void nautilus_password_dialog_initialize_class (NautilusPasswordDialogClass *klass);
static void nautilus_password_dialog_initialize       (NautilusPasswordDialog      *password_dialog);


/* GtkObjectClass methods */
static void bnautilus_password_dialog_destroy         (GtkObject                   *object);

/* GtkDialog callbacks */
static void dialog_clicked_callback                   (GtkWidget                   *widget,
						       gint                         n,
						       gpointer                     data);
static void dialog_show_callback                      (GtkWidget                   *widget,
						       gpointer                     data);
static void dialog_destroy_callback                   (GtkWidget                   *widget,
						       gpointer                     data);

/* NautilusCaptionTable callbacks */
static void caption_table_activate_callback           (GtkWidget                   *widget,
						       gint                         entry,
						       gpointer                     data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPasswordDialog,
				   nautilus_password_dialog,
				   gnome_dialog_get_type ());


static void
nautilus_password_dialog_initialize_class (NautilusPasswordDialogClass * klass)
{
	GtkObjectClass * object_class;
	GtkWidgetClass * widget_class;
	
	object_class = GTK_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* GtkObjectClass */
	object_class->destroy = bnautilus_password_dialog_destroy;
}

static void
nautilus_password_dialog_initialize (NautilusPasswordDialog *password_dialog)
{
	password_dialog->detail = g_new (NautilusPasswordDialogDetail, 1);

	password_dialog->detail->username = NULL;
	password_dialog->detail->password = NULL;

	password_dialog->detail->last_button_clicked = UNKNOWN_BUTTON;
	password_dialog->detail->readonly_username = FALSE;

	password_dialog->detail->table = NULL;
}

/* GtkDialog callbacks */
static void
dialog_clicked_callback (GtkWidget *widget, gint n, gpointer data)
{
	NautilusPasswordDialog *password_dialog = (NautilusPasswordDialog *) data;

	g_assert (password_dialog != NULL);

	password_dialog->detail->last_button_clicked = n;

	gtk_grab_remove(GTK_WIDGET (password_dialog));

	gtk_widget_hide(GTK_WIDGET (password_dialog));
}

static void
dialog_show_callback (GtkWidget *widget, gpointer data)
{
	NautilusPasswordDialog *password_dialog = (NautilusPasswordDialog *) data;

	g_assert (password_dialog != NULL);

	nautilus_caption_table_entry_grab_focus (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table), 
						 PASSWORD_ROW);
}

static void
dialog_destroy_callback (GtkWidget *widget, gpointer data)
{
	NautilusPasswordDialog *password_dialog = (NautilusPasswordDialog *) data;

	g_assert (password_dialog != NULL);

	g_free (password_dialog->detail);

	gtk_grab_remove(GTK_WIDGET (password_dialog));
}


/* NautilusCaptionTable callbacks */
static void
caption_table_activate_callback (GtkWidget *widget, gint entry, gpointer data)
{
	NautilusPasswordDialog *password_dialog = (NautilusPasswordDialog *) data;

	g_assert (password_dialog != NULL);

	if (entry == (nautilus_caption_table_get_num_rows (NAUTILUS_CAPTION_TABLE (widget)) - 1))
		dialog_clicked_callback(GTK_WIDGET (password_dialog), OK_BUTTON, (gpointer) password_dialog);
}

/* Public NautilusPasswordDialog methods */
GtkWidget*
nautilus_password_dialog_new (const char	*dialog_title,
			      const char	*username,
			      const char	*password,
			      gboolean		readonly_username)
{
	NautilusPasswordDialog *password_dialog;

	password_dialog = gtk_type_new (nautilus_password_dialog_get_type ());

	gnome_dialog_constructv (GNOME_DIALOG (password_dialog), dialog_title, stock_buttons);

	/* Setup the dialog */
	gtk_window_set_policy (GTK_WINDOW (password_dialog), 
			      FALSE,	/* allow_shrink */
			      TRUE,	/* allow_grow */
			      FALSE);	/* auto_shrink */

	gtk_widget_realize (GTK_WIDGET (password_dialog));
	
 	gdk_window_set_functions (GTK_WIDGET (password_dialog)->window, 
				  GDK_FUNC_ALL | GDK_FUNC_RESIZE | GDK_FUNC_MINIMIZE);

 	gtk_window_set_position (GTK_WINDOW (password_dialog), GTK_WIN_POS_CENTER);

 	gtk_container_set_border_width (GTK_CONTAINER (password_dialog), 
				       DEFAULT_BORDER_WIDTH);

	gnome_dialog_set_default (GNOME_DIALOG (password_dialog), 
				 DEFAULT_BUTTON);

	gtk_signal_connect(GTK_OBJECT (password_dialog),
			   "clicked",
			   GTK_SIGNAL_FUNC (dialog_clicked_callback),
			   (gpointer) password_dialog);

	gtk_signal_connect(GTK_OBJECT (password_dialog),
			   "show",
			   GTK_SIGNAL_FUNC (dialog_show_callback),
			   (gpointer) password_dialog);

	gtk_signal_connect(GTK_OBJECT (password_dialog),
			   "destroy",
			   GTK_SIGNAL_FUNC (dialog_destroy_callback),
			   (gpointer) password_dialog);
	
	/* The table that holds the captions */
	password_dialog->detail->table = nautilus_caption_table_new (2);

	gtk_signal_connect (GTK_OBJECT (password_dialog->detail->table),
			   "activate",
			   GTK_SIGNAL_FUNC (caption_table_activate_callback),
			   (gpointer) password_dialog);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
					     USERNAME_ROW,
					     "Username:",
					     "",
					     TRUE,
					     TRUE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
					     PASSWORD_ROW,
					     "Password:",
					     "",
					     FALSE,
					     FALSE);
	
	/* Configure the GNOME_DIALOG's vbox */
 	g_assert (GNOME_DIALOG (password_dialog)->vbox != NULL);

	gtk_box_set_spacing (GTK_BOX (GNOME_DIALOG (password_dialog)->vbox), 10);
	
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (password_dialog)->vbox),
			    password_dialog->detail->table,
			    TRUE,	/* expand */
			    TRUE,	/* fill */
			    0);		/* padding */

	/* Configure the table */
 	gtk_container_set_border_width (GTK_CONTAINER(password_dialog->detail->table), 
					DEFAULT_BORDER_WIDTH);

	gtk_widget_show_all (GNOME_DIALOG (password_dialog)->vbox);
	
	nautilus_password_dialog_set_username (password_dialog, username);
	nautilus_password_dialog_set_password (password_dialog, password);
	nautilus_password_dialog_set_readonly_username (password_dialog, readonly_username);
	
	return GTK_WIDGET (password_dialog);
}

/* GtkObjectClass methods */
static void
bnautilus_password_dialog_destroy (GtkObject* object)
{
	NautilusPasswordDialog *password_dialog;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (object));
	
	password_dialog = NAUTILUS_PASSWORD_DIALOG (object);

	if (password_dialog->detail->username)
		g_free (password_dialog->detail->username);

	if (password_dialog->detail->password)
		g_free (password_dialog->detail->password);
}

gboolean
nautilus_password_dialog_run_and_block(NautilusPasswordDialog *password_dialog)
{
	g_return_val_if_fail (password_dialog != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog), FALSE);

	password_dialog->detail->last_button_clicked = UNKNOWN_BUTTON;

	gtk_widget_show_all (GTK_WIDGET (password_dialog));
	
	gtk_grab_add (GTK_WIDGET (password_dialog));

	while (password_dialog->detail->last_button_clicked == UNKNOWN_BUTTON)
		gtk_main_iteration ();
	
	if (password_dialog->detail->last_button_clicked == OK_BUTTON)
		return TRUE;
	
	return FALSE;
}

void
nautilus_password_dialog_set_username (NautilusPasswordDialog *password_dialog,
				       const char * username)
{
	g_return_if_fail (password_dialog != NULL);
	g_return_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog));

	nautilus_caption_table_set_entry_text (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
					       USERNAME_ROW,
					       username);
}

void
nautilus_password_dialog_set_password (NautilusPasswordDialog *password_dialog,
				       const char * password)
{
	g_return_if_fail (password_dialog != NULL);
	g_return_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog));
	
	nautilus_caption_table_set_entry_text (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
					       PASSWORD_ROW,
					       password);
}

void
nautilus_password_dialog_set_readonly_username (NautilusPasswordDialog *password_dialog,
						gboolean readonly)
{
	g_return_if_fail (password_dialog != NULL);
	g_return_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog));
	
	nautilus_caption_table_set_entry_readonly (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
						   USERNAME_ROW,
						   readonly);
}

char *
nautilus_password_dialog_get_username (NautilusPasswordDialog *password_dialog)
{
	g_return_val_if_fail (password_dialog != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog), NULL);

	return nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
						      USERNAME_ROW);
}

char *
nautilus_password_dialog_get_password (NautilusPasswordDialog *password_dialog)
{
	g_return_val_if_fail (password_dialog != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PASSWORD_DIALOG (password_dialog), NULL);

	return nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (password_dialog->detail->table),
						      PASSWORD_ROW);
}
