/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-alert-dialog.h: An HIG compliant alert dialog.

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

*/

#ifndef EEL_ALERT_DIALOG_H
#define EEL_ALERT_DIALOG_H

#include <gtk/gtk.h>

#define EEL_TYPE_ALERT_DIALOG        (eel_alert_dialog_get_type ())
#define EEL_ALERT_DIALOG(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_ALERT_DIALOG, EelAlertDialog))

typedef struct _EelAlertDialog        EelAlertDialog;
typedef struct _EelAlertDialogClass   EelAlertDialogClass;
typedef struct _EelAlertDialogDetails EelAlertDialogDetails;

struct _EelAlertDialog
{
	GtkDialog parent_instance;
	EelAlertDialogDetails *details;
};

struct _EelAlertDialogClass
{
	GtkDialogClass parent_class;
};

GType      eel_alert_dialog_get_type (void);

GtkWidget* eel_alert_dialog_new                 (GtkWindow      *parent,
                                                 GtkDialogFlags  flags,
                                                 GtkMessageType  type,
                                                 GtkButtonsType  buttons,
                                                 const gchar    *primary_message,
                                                 const gchar    *secondary_message);
void       eel_alert_dialog_set_primary_label   (EelAlertDialog *dialog,
		                                 const gchar    *message);
void       eel_alert_dialog_set_secondary_label (EelAlertDialog *dialog,
		                                 const gchar    *message);
void       eel_alert_dialog_set_details_label   (EelAlertDialog *dialog,
						 const gchar    *message);

#endif /* EEL_ALERT_DIALOG_H */
