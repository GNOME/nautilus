/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.c - implementation for window that lets user choose 
                                a program from a list

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-program-chooser.h"

#include "nautilus-gtk-extensions.h"

#include <gtk/gtkradiobutton.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

#define PROGRAM_LIST_NAME_COLUMN	 0
#define PROGRAM_LIST_COLUMN_COUNT	 1

/* gtk_window_set_default_width (and some other functions) use a
 * magic undocumented number of -2 to mean "ignore this parameter".
 */
#define NO_DEFAULT_MAGIC_NUMBER		-2

/* Scrolling list has no idea how tall to make itself. Its
 * "natural height" is just enough to draw the scroll bar controls.
 * Hardwire an initial window size here, but let user resize
 * bigger or smaller.
 */
#define PROGRAM_CHOOSER_DEFAULT_HEIGHT	 303

static void
populate_program_list (NautilusProgramChooserType type,
		       NautilusFile *file,
		       GtkCList *clist)
{
	int i;
	char **text;


	if (type == NAUTILUS_PROGRAM_CHOOSER_COMPONENTS) {
		for (i = 0; i < 10; ++i) {
			/* One extra slot so it's NULL-terminated */
			text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup_printf ("View as Viewer %d", i+1); 
			gtk_clist_append (clist, text);
			
			g_strfreev (text);
		}
	} else {
		g_assert (type == NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS);
		for (i = 0; i < 10; ++i) {
			/* One extra slot so it's NULL-terminated */
			text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup_printf ("Application %d", i+1); 
			gtk_clist_append (clist, text);
			
			g_strfreev (text);
		}
	}

}

static NautilusFile *
nautilus_program_chooser_get_file (GnomeDialog *chooser)
{
	return NAUTILUS_FILE (gtk_object_get_data (GTK_OBJECT (chooser), "file"));
}

static GtkCList *
nautilus_program_chooser_get_clist (GnomeDialog *chooser)
{
	return GTK_CLIST (gtk_object_get_data (GTK_OBJECT (chooser), "clist"));
}

static GtkFrame *
nautilus_program_chooser_get_frame (GnomeDialog *chooser)
{
	return GTK_FRAME (gtk_object_get_data (GTK_OBJECT (chooser), "frame"));
}

static GtkLabel *
nautilus_program_chooser_get_status_label (GnomeDialog *chooser)
{
	return GTK_LABEL (gtk_object_get_data (GTK_OBJECT (chooser), "status_label"));
}

static void
nautilus_program_chooser_set_file (GnomeDialog *chooser, NautilusFile *file)
{
	nautilus_file_ref (file);
	gtk_object_set_data_full (GTK_OBJECT (chooser), 
				  "file", 
				  file, 
				  (GtkDestroyNotify)nautilus_file_unref);
}

static void
nautilus_program_chooser_set_clist (GnomeDialog *chooser, GtkCList *clist)
{
	gtk_object_set_data (GTK_OBJECT (chooser), "clist", clist);
}

static void
nautilus_program_chooser_set_frame (GnomeDialog *chooser, GtkFrame *frame)
{
	gtk_object_set_data (GTK_OBJECT (chooser), "frame", frame);
}

static void
nautilus_program_chooser_set_status_label (GnomeDialog *chooser, GtkLabel *status_label)
{
	gtk_object_set_data (GTK_OBJECT (chooser), "status_label", status_label);
}

static gboolean
is_in_short_list (NautilusFile *file, const char *program_name) {
	/* FIXME: This needs to use the real API when there is a
	 * real API. Passing the program name won't be good enough.
	 * For now, just return hardwired values.
	 */

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (program_name != NULL);

	return FALSE;
}

static gboolean
is_in_metadata_list (NautilusFile *file, const char *program_name) {
	/* FIXME: This needs to use the real API when there is a
	 * real API. Passing the program name won't be good enough.
	 * For now, just return hardwired values.
	 */

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (program_name != NULL);

	return TRUE;
}

static void
update_selected_item_details (GnomeDialog *dialog)
{
	NautilusFile *file;
	GtkCList *clist;
	GtkFrame *frame;
	GtkLabel *status_label;
	int selected_row;
	char *row_text;
	char *frame_label_text, *status_label_text;
	char *file_type, *file_name;

	file = nautilus_program_chooser_get_file (dialog);
	clist = nautilus_program_chooser_get_clist (dialog);
	frame = nautilus_program_chooser_get_frame (dialog);
	status_label = nautilus_program_chooser_get_status_label (dialog);

	selected_row = nautilus_gtk_clist_get_first_selected_row (clist);

	if (selected_row >= 0 && gtk_clist_get_text (clist, 
						     selected_row, 
						     PROGRAM_LIST_NAME_COLUMN, 
						     &row_text)) {
		/* row_text is now a pointer to the text in the list. */
		frame_label_text = g_strdup (row_text);

		if (is_in_short_list (file, frame_label_text)) {
			file_type = nautilus_file_get_string_attribute (file, "type");
			status_label_text = g_strdup_printf (_("Is in the menu for all \"%s\" items."), 
					      		     file_type);	
			g_free (file_type);
		} else if (is_in_metadata_list (file, frame_label_text)) {
	  		file_name = nautilus_file_get_name (file);	
			status_label_text = g_strdup_printf (_("Is in the menu for \"%s\"."), 
					      		     file_name);				
			g_free (file_name);
		} else {
	  		file_name = nautilus_file_get_name (file);	
			status_label_text = g_strdup_printf (_("Is not in the menu for \"%s\"."), 
					      		     file_name);
			g_free (file_name);
		}				     
	} else {
		frame_label_text = NULL;
		status_label_text = NULL;
	}

	gtk_frame_set_label (frame, frame_label_text);
	gtk_label_set_text (status_label, status_label_text);

	g_free (frame_label_text);
	g_free (status_label_text);
}		       

static void
program_list_selection_changed_callback (GtkCList *clist, 
					 gint row, 
					 gint column, 
					 GdkEventButton *event, 
					 gpointer user_data)
{
	g_assert (GTK_IS_CLIST (clist));
	g_assert (GNOME_IS_DIALOG (user_data));

	update_selected_item_details (GNOME_DIALOG (user_data));
}

static GtkRadioButton *
pack_radio_button (GtkBox *box, const char *label_text, GtkRadioButton *group)
{
	GtkWidget *radio_button;

	radio_button = gtk_radio_button_new_with_label_from_widget (group, label_text);
	gtk_widget_show (radio_button);
	gtk_box_pack_start_defaults (box, radio_button);

	return GTK_RADIO_BUTTON (radio_button);
}

static void
run_program_configurator_callback (GtkWidget *button, gpointer callback_data)
{
	GnomeDialog *program_chooser;
	NautilusFile *file;
	GtkCList *clist;
	GtkWidget *dialog;
	GtkRadioButton *type_radio_button, *item_radio_button, *none_radio_button;
	GtkRadioButton *old_active_button;
	char *radio_button_text;
	char *file_type, *file_name;
	char *row_text;
	char *title;
	int selected_row;

	g_assert (GNOME_IS_DIALOG (callback_data));

	program_chooser = GNOME_DIALOG (callback_data);
	
	file = nautilus_program_chooser_get_file (program_chooser);
	clist = nautilus_program_chooser_get_clist (program_chooser);

	selected_row = nautilus_gtk_clist_get_first_selected_row (clist);
	if (selected_row < 0 || !gtk_clist_get_text (clist, 
						     selected_row, 
						     PROGRAM_LIST_NAME_COLUMN, 
						     &row_text)) {
		/* No valid selected item, don't do anything. Probably the UI
		 * should prevent this.
		 */
		return;
	}

	title = g_strdup_printf (_("Change %s"), row_text);		     
	dialog = gnome_dialog_new (title,
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	g_free (title);

	/* Radio button for adding to short list for file type. */
	file_type = nautilus_file_get_string_attribute (file, "type");
	radio_button_text = g_strdup_printf ("Include %s in the menu for all \"%s\" items", 
					     row_text, file_type);
	type_radio_button = pack_radio_button (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
					       radio_button_text,
					       NULL);
	g_free (file_type);
	g_free (radio_button_text);
	

	/* Radio button for adding to short list for specific file. */
	file_name = nautilus_file_get_name (file);
	radio_button_text = g_strdup_printf ("Include %s in the menu just for \"%s\"", 
					   row_text, file_name);
	item_radio_button = pack_radio_button (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
					       radio_button_text,
					       type_radio_button);
	g_free (radio_button_text);

	/* Radio button for not including program in short list for type or file. */
	radio_button_text = g_strdup_printf ("Don't include %s in the menu for \"%s\"", 
					   row_text, file_name);
	none_radio_button = pack_radio_button (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
					       radio_button_text,
					       item_radio_button);
	g_free (file_name);
	g_free (radio_button_text);

	/* Activate the correct radio button. */
	if (is_in_short_list (file, row_text)) {
		old_active_button = type_radio_button;
	} else if (is_in_metadata_list (file, row_text)) {
		old_active_button = item_radio_button;	
	} else {
		old_active_button = none_radio_button;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (old_active_button), TRUE);

	/* Buttons close this dialog. */
  	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);

  	/* Make OK button the default. */
  	gnome_dialog_set_default (GNOME_DIALOG (dialog), GNOME_OK);

  	gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (program_chooser));
	
	/* Don't destroy on close because callers will need 
	 * to extract some information from the dialog after 
	 * it closes.
	 */
	gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

	if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_OK) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (old_active_button))) {
			g_message ("no change");
		} else {
			g_message ("user chose different radio button, will update when API is available");
			update_selected_item_details (program_chooser);
		}
	}

	gtk_widget_destroy (dialog);
}


GnomeDialog *
nautilus_program_chooser_new (NautilusProgramChooserType type,
			      NautilusFile *file)
{
	GtkWidget *window;
	GtkWidget *dialog_vbox;
	GtkWidget *prompt_label;
	GtkWidget *list_scroller, *clist;
	GtkWidget *frame;
	GtkWidget *framed_hbox;
	GtkWidget *status_label;
	GtkWidget *change_button_holder;
	GtkWidget *change_button;
	char *file_name, *prompt;
	const char *title;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	file_name = nautilus_file_get_name (file);

	switch (type) {
	case NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS:
		title = _("Nautilus: Open with Other");
		prompt = g_strdup_printf (_("Choose an application with which to open \"%s\"."), file_name);
		break;
	case NAUTILUS_PROGRAM_CHOOSER_COMPONENTS:
	default:
		title = _("Nautilus: View as Other");
		prompt = g_strdup_printf (_("Choose a view for \"%s\"."), file_name);
		break;
	}

	g_free (file_name);
	
	window = gnome_dialog_new (title, 
				   _("Choose"), 
				   GNOME_STOCK_BUTTON_CANCEL, 
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     NO_DEFAULT_MAGIC_NUMBER,
				     PROGRAM_CHOOSER_DEFAULT_HEIGHT);

	dialog_vbox = GNOME_DIALOG (window)->vbox;

	/* Prompt at top of dialog. */
	prompt_label = gtk_label_new (prompt);
	gtk_widget_show (prompt_label);
  	g_free (prompt);

  	gtk_box_pack_start (GTK_BOX (dialog_vbox), prompt_label, FALSE, FALSE, 0);

	/* Scrolling list to hold choices. */
	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start_defaults (GTK_BOX (GNOME_DIALOG (window)->vbox), list_scroller);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), 
					GTK_POLICY_NEVER, 
					GTK_POLICY_AUTOMATIC);	  
	  
	clist = gtk_clist_new (PROGRAM_LIST_COLUMN_COUNT);
	gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
	populate_program_list (type, file, GTK_CLIST (clist));
	gtk_widget_show (clist);
	gtk_container_add (GTK_CONTAINER (list_scroller), clist);
	gtk_clist_column_titles_hide (GTK_CLIST (clist));

	/* Framed area with selection-specific details */
	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
  	gtk_box_pack_start (GTK_BOX (dialog_vbox), frame, FALSE, FALSE, 0);

  	framed_hbox = gtk_hbox_new (FALSE, GNOME_PAD);
  	gtk_widget_show (framed_hbox);
  	gtk_container_add (GTK_CONTAINER (frame), framed_hbox);
  	gtk_container_set_border_width (GTK_CONTAINER (framed_hbox), GNOME_PAD);

  	status_label = gtk_label_new (NULL);
  	gtk_label_set_justify (GTK_LABEL (status_label), GTK_JUSTIFY_LEFT);
  	gtk_widget_show (status_label);
  	gtk_box_pack_start (GTK_BOX (framed_hbox), status_label, FALSE, FALSE, 0);

	change_button_holder = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (change_button_holder);
  	gtk_box_pack_end (GTK_BOX (framed_hbox), change_button_holder, FALSE, FALSE, 0);

  	change_button = gtk_button_new_with_label(_("Change..."));
  	gtk_widget_show (change_button);
  	gtk_box_pack_end (GTK_BOX (change_button_holder), change_button, TRUE, FALSE, 0);

  	gtk_signal_connect (GTK_OBJECT (change_button),
  			    "clicked",
  			    run_program_configurator_callback,
  			    window);

	/* Buttons close this dialog. */
  	gnome_dialog_set_close (GNOME_DIALOG (window), TRUE);

  	/* Make confirmation button the default. */
  	gnome_dialog_set_default (GNOME_DIALOG (window), GNOME_OK);


	/* Load up the dialog object with info other functions will need. */
	nautilus_program_chooser_set_file (GNOME_DIALOG (window), file);
	nautilus_program_chooser_set_clist (GNOME_DIALOG (window), GTK_CLIST (clist));
	nautilus_program_chooser_set_frame (GNOME_DIALOG (window), GTK_FRAME (frame));
	nautilus_program_chooser_set_status_label (GNOME_DIALOG (window), GTK_LABEL (status_label));
	
	/* Fill in initial info about the selected item. */
  	update_selected_item_details (GNOME_DIALOG (window));

  	/* Update selected item info whenever selection changes. */
  	gtk_signal_connect (GTK_OBJECT (clist),
  			    "select_row",
  			    program_list_selection_changed_callback,
  			    window);

  	return GNOME_DIALOG (window);
}
