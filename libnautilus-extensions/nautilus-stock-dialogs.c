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

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include "nautilus-string.h"

struct NautilusTimedWait {
	char *window_title;
	char *wait_message;
	NautilusCancelCallback cancel_callback;
	gpointer callback_data;
	GDestroyNotify destroy_notify;
	GtkWindow *parent_window;
};

static void turn_on_line_wrap_flag_callback (GtkWidget *widget, gpointer callback_data);

NautilusTimedWait *
nautilus_timed_wait_start (const char *window_title,
			   const char *wait_message,
			   NautilusCancelCallback cancel_callback,
			   gpointer callback_data,
			   GDestroyNotify destroy_notify,
			   GtkWindow *parent_window)
{
	NautilusTimedWait *timed_wait;

	g_return_val_if_fail (window_title != NULL, NULL);
	g_return_val_if_fail (wait_message != NULL, NULL);
	g_return_val_if_fail (cancel_callback != NULL, NULL);
	g_return_val_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window), NULL);

	/* Create the timed wait record. */
	timed_wait = g_new (NautilusTimedWait, 1);
	timed_wait->window_title = g_strdup (window_title);
	timed_wait->wait_message = g_strdup (wait_message);
	timed_wait->cancel_callback = cancel_callback;
	timed_wait->callback_data = callback_data;
	timed_wait->destroy_notify = destroy_notify;
	timed_wait->parent_window = parent_window;
	if (parent_window != NULL) {
		gtk_widget_ref (GTK_WIDGET (parent_window));
	}

	return timed_wait;
}

static void
nautilus_timed_wait_free (NautilusTimedWait *timed_wait)
{
	/* Let the caller destroy the callback data. */
	if (timed_wait->destroy_notify != NULL) {
		(* timed_wait->destroy_notify) (timed_wait->callback_data);
	}

	/* Now free the other stuff we were holding onto. */
	g_free (timed_wait->window_title);
	g_free (timed_wait->wait_message);
	if (timed_wait->parent_window != NULL) {
		gtk_widget_unref (GTK_WIDGET (timed_wait->parent_window));
	}

	/* And the wait object itself. */
	g_free (timed_wait);
}

void
nautilus_timed_wait_stop (NautilusTimedWait *timed_wait)
{
	g_return_if_fail (timed_wait != NULL);

	nautilus_timed_wait_free (timed_wait);
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
        GtkWidget *prompt_widget;
	
	/* Create the dialog. */
	va_start (button_title_args, title);
	button_titles = convert_varargs_to_name_array (button_title_args);
	va_end (button_title_args);
        dialog = gnome_dialog_newv (title, button_titles);
	g_free (button_titles);
	
	/* Allow close. */
        gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);
	
	/* Parent it if asked to. */
        if (parent != NULL) {
		top_widget = gtk_widget_get_toplevel (parent);
		if (GTK_IS_WINDOW (top_widget)) {
			gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (top_widget));
		}
	}
	
	/* Title it if asked to. */
	if (text != NULL) {
		prompt_widget = gtk_label_new (text);
		gtk_label_set_line_wrap (GTK_LABEL (prompt_widget), TRUE);
		gtk_label_set_justify (GTK_LABEL (prompt_widget),
				       GTK_JUSTIFY_LEFT);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
				    prompt_widget,
				    TRUE, TRUE, GNOME_PAD);
	}
	
	/* Run it. */
        gtk_widget_show_all (dialog);
        return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
turn_on_line_wrap_flag (GtkWidget *widget, const char *message)
{
	char *text;

	/* Turn on the flag if we find a label with the message
	 * in it.
	 */
	if (GTK_IS_LABEL (widget)) {
		gtk_label_get (GTK_LABEL (widget), &text);
		if (nautilus_strcmp (text, message) == 0) {
			gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		}
	}

	/* Recurse for children. */
	if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget),
				       turn_on_line_wrap_flag_callback,
				       (char *) message);
	}
}

static void
turn_on_line_wrap_flag_callback (GtkWidget *widget, gpointer callback_data)
{
	turn_on_line_wrap_flag (widget, callback_data);
}

/* Shamelessly stolen from gnome-dialog-util.c: */
static GtkWidget *
show_ok_box (const char *message,
	     const char *type,
	     GtkWindow *parent)
{  
	GtkWidget *box;

	box = gnome_message_box_new
		(message, type, GNOME_STOCK_BUTTON_OK, NULL);
	
	/* A bit of a hack. We want to use gnome_message_box_new,
	 * but we want the message to be wrapped. So, we search
	 * for the label with this message so we can mark it.
	 */
	turn_on_line_wrap_flag (box, message);

	if (parent != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG(box), parent);
	}
	gtk_widget_show (box);
	return box;
}

static GtkWidget *
show_yes_no_box (const char *message,
	     	 const char *type,
	     	 const char *yes_label,
	     	 const char *no_label,
	     	 GtkWindow *parent)
{  
	GtkWidget *box;

	box = gnome_message_box_new
		(message, type, yes_label, no_label, NULL);
	
	/* A bit of a hack. We want to use gnome_message_box_new,
	 * but we want the message to be wrapped. So, we search
	 * for the label with this message so we can mark it.
	 */
	turn_on_line_wrap_flag (box, message);

	if (parent != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG(box), parent);
	}
	gtk_widget_show (box);
	return box;
}

GtkWidget *
nautilus_info_dialog (const char *info)
{
	return show_ok_box (info, GNOME_MESSAGE_BOX_INFO, NULL);
}

GtkWidget *
nautilus_info_dialog_parented (const char *info,
			       GtkWindow *parent)
{
	return show_ok_box (info, GNOME_MESSAGE_BOX_INFO, parent);
}

GtkWidget *
nautilus_warning_dialog (const char *warning)
{
	return show_ok_box (warning, GNOME_MESSAGE_BOX_WARNING, NULL);
}

GtkWidget *
nautilus_warning_dialog_parented (const char *warning,
				  GtkWindow *parent)
{
	return show_ok_box (warning, GNOME_MESSAGE_BOX_WARNING, parent);
}

GtkWidget *
nautilus_error_dialog (const char *error)
{
	return show_ok_box (error, GNOME_MESSAGE_BOX_ERROR, NULL);
}

GtkWidget *
nautilus_error_dialog_parented (const char *error,
				GtkWindow *parent)
{
	return show_ok_box (error, GNOME_MESSAGE_BOX_ERROR, parent);
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
 */
GtkWidget *
nautilus_yes_no_dialog (const char *question, 
			const char *yes_label,
			const char *no_label)
{
	return show_yes_no_box (question, 
			        GNOME_MESSAGE_BOX_QUESTION,
			        yes_label,
			        no_label,
			        NULL);
}

/**
 * nautilus_yes_no_dialog_parented:
 * 
 * Create a parented dialog asking a question with two choices.
 * The caller needs to set up any necessary callbacks 
 * for the buttons.
 * @question: The text of the question.
 * @yes_label: The label of the "yes" button.
 * @no_label: The label of the "no" button.
 * @parent: The parent window for this dialog.
 */
GtkWidget *
nautilus_yes_no_dialog_parented (const char *question, 
			     	 const char *yes_label,
			    	 const char *no_label,
			    	 GtkWindow *parent)
{
	return show_yes_no_box (question, 
			        GNOME_MESSAGE_BOX_QUESTION,
			        yes_label,
			        no_label,
			        parent);
}
