/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-error-dialog.c - A very simple program used to post an
 * error dialog or a question dialog.
 *
 * To post a error dialog:
 *
 * nautilus-error-dialog --message "My message" --title "My Title"
 *
 * Posts a error dialog (error icon) with an OK button.
 *
 *
 * To post a warning dialog:
 *
 * nautilus-error-dialog --button-one "No" --button-two "Yes" \
 *                       --message "My Question" --title "My Question"
 *
 * Posts a question dialog (question icon) with two buttons ordered
 * left to right such that "No" is left and "Yes" is right.  "Yes"
 * is the default button.
 *
 * The result value of the script is the number of the clicked button
 * starting at 0.  In the question example above, "Yes" would return
 * "1" and "No" would return "0".
 *
 */

#include <config.h>

#include <gnome.h>
#include <popt.h>

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

/*
 * The following dialog widgetry code cut-n-pasted from libnautilus-extension.
 * The reason why we dont use libnautilus-private is that we dont want to
 * incur its dependencies, especially libnautilus and thus libbonobo.
 */
static void find_message_label_callback (GtkWidget *widget,
					 gpointer   callback_data);

static void
find_message_label (GtkWidget *widget, const char *message)
{
	char *text;

	/* Turn on the flag if we find a label with the message
	 * in it.
	 */
	if (GTK_IS_LABEL (widget)) {
		gtk_label_get (GTK_LABEL (widget), &text);
		if (strcmp (text, message) == 0) {
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
		  const char *dialog_title,
		  const char *type,
		  const char *button_one,
		  const char *button_two,
		  GtkWindow *parent)
{  
	GtkWidget *box;
	GtkLabel *message_label;

	g_assert (dialog_title != NULL);

	box = gnome_message_box_new (message, type, button_one, button_two, NULL);
	gtk_window_set_title (GTK_WINDOW (box), dialog_title);
	gtk_window_set_wmclass (GTK_WINDOW (box), "error_dialog", "Nautilus");
	
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
	     const char *dialog_title,
	     const char *type,
	     GtkWindow *parent)
{  
	return show_message_box	(message, dialog_title, type, GNOME_STOCK_BUTTON_OK, NULL, parent);
}

static GnomeDialog *
nautilus_error_dialog (const char *error,
	 	       const char *dialog_title,
		       GtkWindow *parent)
{
	return show_ok_box (error,
			    dialog_title == NULL ? _("Error") : dialog_title, 
			    GNOME_MESSAGE_BOX_ERROR, parent);
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
static GnomeDialog *
nautilus_yes_no_dialog (const char *question, 
		    	const char *dialog_title,
			const char *yes_label,
			const char *no_label,
			GtkWindow *parent)
{
	return show_message_box (question,
				 dialog_title == NULL ? _("Question") : dialog_title, 
				 GNOME_MESSAGE_BOX_QUESTION,
				 yes_label,
				 no_label,
				 parent);
}

int main (int argc, char *argv[])
{
	GnomeDialog *dialog;
	poptContext popt_context;
	char *title = NULL;
	char *message = NULL;
	char *button_one_label = NULL;
	char *button_two_label = NULL;

	struct poptOption options[] = {
		{ "message", '\0', POPT_ARG_STRING, &message, 0, N_("Message."), NULL },
		{ "title", '\0', POPT_ARG_STRING, &title, 0, N_("Title."), NULL },
		{ "button-one", '\0', POPT_ARG_STRING, &button_one_label, 0, N_("Button one label."), NULL },
		{ "button-two", '\0', POPT_ARG_STRING, &button_two_label, 0, N_("Button two label."), NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	
        gnome_init_with_popt_table ("nautilus-error-dialog",
				    VERSION,
				    argc,
				    argv,
				    options,
				    0,
				    &popt_context);

	if (button_two_label) {
		dialog = nautilus_yes_no_dialog (message ? message : _("Question"),
						 title ? title : _("Question"),
						 button_one_label ? button_one_label : _("Ok"),
						 button_two_label,
						 NULL);
	}
	else {
		dialog = nautilus_error_dialog (message ? message : _("Question"), 
						title ? title : _("Question"),
						NULL);
	}

	return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}
