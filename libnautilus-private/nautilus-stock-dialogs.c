/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-stock-dialogs.c: Various standard dialogs for Nautilus.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-stock-dialogs.h"

#include "nautilus-gnome-extensions.h"
#include "nautilus-string.h"
#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

#define TIMED_WAIT_DURATION 5000

typedef struct {
	NautilusCancelCallback cancel_callback;
	gpointer callback_data;

	/* Parameters for creation of the window. */
	char *window_title;
	char *wait_message;
	GtkWindow *parent_window;

	/* Timer to determine when we need to create the window. */
	guint timeout_handler_id;

	/* Window, once it's created. */
	GnomeDialog *dialog;
} TimedWait;

static GHashTable *timed_wait_hash_table;

static void find_message_label_callback (GtkWidget *widget,
					 gpointer   callback_data);

static guint
timed_wait_hash (gconstpointer value)
{
	const TimedWait *wait;

	wait = value;

	return GPOINTER_TO_UINT (wait->cancel_callback)
		^ GPOINTER_TO_UINT (wait->callback_data);
}

static gboolean
timed_wait_hash_equal (gconstpointer value1, gconstpointer value2)
{
	const TimedWait *wait1, *wait2;

	wait1 = value1;
	wait2 = value2;

	return wait1->cancel_callback == wait2->cancel_callback
		&& wait1->callback_data == wait2->callback_data;
}

static void
add_label_to_dialog (GnomeDialog *dialog, const char *message)
{
	GtkLabel *message_widget;

	if (message == NULL) {
		return;
	}
	
	message_widget = GTK_LABEL (gtk_label_new (message));
	gtk_label_set_line_wrap (message_widget, TRUE);
	gtk_label_set_justify (message_widget,
			       GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    GTK_WIDGET (message_widget),
			    TRUE, TRUE, GNOME_PAD);
}

static void
timed_wait_free (TimedWait *wait)
{
	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) != NULL);

	g_hash_table_remove (timed_wait_hash_table, wait);

	g_free (wait->window_title);
	g_free (wait->wait_message);
	if (wait->parent_window != NULL) {
		gtk_widget_unref (GTK_WIDGET (wait->parent_window));
	}
	if (wait->timeout_handler_id != 0) {
		gtk_timeout_remove (wait->timeout_handler_id);
	}
	if (wait->dialog != NULL) {
		gtk_object_destroy (GTK_OBJECT (wait->dialog));
		gtk_object_unref (GTK_OBJECT (wait->dialog));
	}
	
	/* And the wait object itself. */
	g_free (wait);
}

static void
timed_wait_cancel_callback (GtkObject *object, gpointer callback_data)
{
	TimedWait *wait;

	wait = callback_data;

	g_assert (GNOME_DIALOG (object) == wait->dialog);

	(* wait->cancel_callback) (wait->callback_data);
	timed_wait_free (wait);
}

static gboolean
timed_wait_callback (gpointer callback_data)
{
	TimedWait *wait;
	GnomeDialog *dialog;

	wait = callback_data;

	/* Put up the timed wait window. */
	dialog = GNOME_DIALOG (gnome_dialog_new (wait->window_title,
						 GNOME_STOCK_BUTTON_CANCEL,
						 NULL));
	add_label_to_dialog (dialog, wait->wait_message);
	gnome_dialog_set_close (dialog, TRUE);
	gtk_widget_show_all (GTK_WIDGET (dialog));

	/* FIXME: Could parent here, but it's complicated because we
	 * don't want this window to go away just because the parent
	 * would go away first.
	 */

	/* Make the dialog cancel the timed wait when it goes away.
	 * Connect to "destroy" instead of "clicked" since we want
	 * to be called no matter how the dialog goes away.
	 */
	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    timed_wait_cancel_callback, wait);

	wait->timeout_handler_id = 0;
	return FALSE;
}

void
nautilus_timed_wait_start (NautilusCancelCallback cancel_callback,
			   gpointer callback_data,
			   const char *window_title,
			   const char *wait_message,
			   GtkWindow *parent_window)
{
	TimedWait *wait;
	
	g_return_if_fail (cancel_callback != NULL);
	g_return_if_fail (callback_data != NULL);
	g_return_if_fail (window_title != NULL);
	g_return_if_fail (wait_message != NULL);
	g_return_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window));

	/* Create the timed wait record. */
	wait = g_new0 (TimedWait, 1);
	wait->window_title = g_strdup (window_title);
	wait->wait_message = g_strdup (wait_message);
	wait->cancel_callback = cancel_callback;
	wait->callback_data = callback_data;
	wait->parent_window = parent_window;
	
	if (parent_window != NULL) {
		gtk_widget_ref (GTK_WIDGET (parent_window));
	}

	/* Start the timer. */
	wait->timeout_handler_id = gtk_timeout_add
		(TIMED_WAIT_DURATION,
		 timed_wait_callback, wait);

	/* Put in the hash table so we can find it later. */
	if (timed_wait_hash_table == NULL) {
		timed_wait_hash_table = g_hash_table_new
			(timed_wait_hash, timed_wait_hash_equal);
	}
	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == NULL);
	g_hash_table_insert (timed_wait_hash_table, wait, wait);
	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == wait);
}

void
nautilus_timed_wait_stop (NautilusCancelCallback cancel_callback,
			  gpointer callback_data)
{
	TimedWait key;
	TimedWait *wait;

	g_return_if_fail (cancel_callback != NULL);
	g_return_if_fail (callback_data != NULL);
	
	key.cancel_callback = cancel_callback;
	key.callback_data = callback_data;
	wait = g_hash_table_lookup (timed_wait_hash_table, &key);

	g_return_if_fail (wait != NULL);

	timed_wait_free (wait);
}

static const char **
convert_varargs_to_name_array (va_list args)
{
	GPtrArray *resizeable_array;
	const char *name;
	const char **plain_ole_array;
	
	resizeable_array = g_ptr_array_new ();

	do {
		name = va_arg (args, const char *);
		g_ptr_array_add (resizeable_array, (gpointer) name);
	} while (name != NULL);

	plain_ole_array = (const char **) resizeable_array->pdata;
	
	g_ptr_array_free (resizeable_array, FALSE);

	return plain_ole_array;
}

int
nautilus_simple_dialog (GtkWidget *parent, const char *text, const char *title, ...)
{
	va_list button_title_args;
	const char **button_titles;
        GtkWidget *dialog;
        GtkWidget *top_widget;
	
	/* Create the dialog. */
	va_start (button_title_args, title);
	button_titles = convert_varargs_to_name_array (button_title_args);
	va_end (button_title_args);
        dialog = gnome_dialog_newv (title, button_titles);
	g_free (button_titles);
	
	/* Allow close. */
        gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	
	/* Parent it if asked to. */
        if (parent != NULL) {
		top_widget = gtk_widget_get_toplevel (parent);
		if (GTK_IS_WINDOW (top_widget)) {
			gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (top_widget));
		}
	}
	
	/* Title it if asked to. */
	add_label_to_dialog (GNOME_DIALOG (dialog), text);
	
	/* Run it. */
        gtk_widget_show_all (dialog);
        return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
find_message_label (GtkWidget *widget, const char *message)
{
	char *text;

	/* Turn on the flag if we find a label with the message
	 * in it.
	 */
	if (GTK_IS_LABEL (widget)) {
		gtk_label_get (GTK_LABEL (widget), &text);
		if (nautilus_strcmp (text, message) == 0) {
			gtk_object_set_data (GTK_OBJECT (gtk_widget_get_toplevel (widget)),
					     "message label", widget);
		}
	}

	/* Recurse for children. */
	if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget),
				       find_message_label_callback,
				       (char *) message);
	}
}

static void
find_message_label_callback (GtkWidget *widget, gpointer callback_data)
{
	find_message_label (widget, callback_data);
}

static GnomeDialog *
show_message_box (const char *message,
		  const char *type,
		  const char *button_one,
		  const char *button_two,
		  GtkWindow *parent)
{  
	GtkWidget *box;
	GtkLabel *message_label;

	box = gnome_message_box_new (message, type, button_one, button_two, NULL);
	
	/* A bit of a hack. We want to use gnome_message_box_new,
	 * but we want the message to be wrapped. So, we search
	 * for the label with this message so we can mark it.
	 */
	find_message_label (box, message);
	message_label = GTK_LABEL (gtk_object_get_data (GTK_OBJECT (box), "message label"));
	gtk_label_set_line_wrap (message_label, TRUE);

	if (parent != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG (box), parent);
	}
	gtk_widget_show (box);
	return GNOME_DIALOG (box);
}

static GnomeDialog *
show_ok_box (const char *message,
	     const char *type,
	     GtkWindow *parent)
{  
	return show_message_box	(message, type, GNOME_STOCK_BUTTON_OK, NULL, parent);
}

GnomeDialog *
nautilus_info_dialog (const char *info,
		      GtkWindow *parent)
{
	return show_ok_box (info, GNOME_MESSAGE_BOX_INFO, parent);
}

GnomeDialog *
nautilus_warning_dialog (const char *warning,
			 GtkWindow *parent)
{
	return show_ok_box (warning, GNOME_MESSAGE_BOX_WARNING, parent);
}

GnomeDialog *
nautilus_error_dialog (const char *error,
		       GtkWindow *parent)
{
	return show_ok_box (error, GNOME_MESSAGE_BOX_ERROR, parent);
}

static void
clicked_callback (GnomeDialog *dialog,
		  int button_number,
		  const char *detailed_error_message)
{
	GtkLabel *label;

	switch (button_number) {
	case 0: /* Details */
		label = GTK_LABEL (gtk_object_get_data (GTK_OBJECT (dialog), "message label"));
		gtk_label_set_text (label, detailed_error_message);
		gtk_widget_hide (GTK_WIDGET (nautilus_gnome_dialog_get_button_by_index (dialog, 0)));
		break;
	case 1: /* OK */
		gnome_dialog_close (dialog);
		break;
	}
}

GnomeDialog *
nautilus_error_dialog_with_details (const char *error_message,
				    const char *detailed_error_message,
				    GtkWindow *parent)
{
	GnomeDialog *dialog;

	g_return_val_if_fail (error_message != NULL, NULL);
	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

	if (detailed_error_message == NULL
	    || strcmp (error_message, detailed_error_message) == 0) {
		return nautilus_error_dialog (error_message, parent);
	}

	dialog = show_message_box (error_message, GNOME_MESSAGE_BOX_ERROR,
				   _("Details"), GNOME_STOCK_BUTTON_OK, parent);

	/* Show the details when you click on the details button. */
	gnome_dialog_set_close (dialog, FALSE);
	gtk_signal_connect_full (GTK_OBJECT (dialog), "clicked",
				 clicked_callback, NULL, g_strdup (detailed_error_message),
				 g_free, FALSE, FALSE);

	return dialog;
}

/**
 * nautilus_yes_no_dialog:
 * 
 * Create a dialog asking a question with two choices.
 * The caller needs to set up any necessary callbacks 
 * for the buttons.
 * @question: The text of the question.
 * @yes_label: The label of the "yes" button.
 * @no_label: The label of the "no" button.
 * @parent: The parent window for this dialog.
 */
GnomeDialog *
nautilus_yes_no_dialog (const char *question, 
			const char *yes_label,
			const char *no_label,
			GtkWindow *parent)
{
	return show_message_box (question, 
				 GNOME_MESSAGE_BOX_QUESTION,
				 yes_label,
				 no_label,
				 parent);
}
