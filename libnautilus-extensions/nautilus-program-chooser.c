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
#include "nautilus-view-identifier.h"
#include "nautilus-mime-actions.h"

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

static GnomeVFSMimeActionType
nautilus_program_chooser_get_type (GnomeDialog *program_chooser)
{
	GnomeVFSMimeActionType type;

	type = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (program_chooser), "type"));

	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT ||
		  type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

	return type;
}

static void
populate_program_list (GnomeVFSMimeActionType type,
		       NautilusFile *file,
		       GtkCList *clist)
{
	char **text;
	char *uri;
	GList *programs, *program;
	NautilusViewIdentifier *view_identifier;
	GnomeVFSMimeApplication *application;
	int new_row;

	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		  || type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

	uri = nautilus_file_get_uri (file);
	programs = type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		? nautilus_mime_get_all_components_for_uri (uri)
		: nautilus_mime_get_all_applications_for_uri (uri);
	g_free (uri);
		

	for (program = programs; program != NULL; program = program->next) {
		/* One extra slot so it's NULL-terminated */
		text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);

		if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
			view_identifier = nautilus_view_identifier_new_from_content_view 
				((OAF_ServerInfo *)program->data);
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup_printf 
				("View as %s", view_identifier->name);
		} else {
			application = (GnomeVFSMimeApplication *)program->data;
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup (application->name);			
		}		

		new_row = gtk_clist_append (clist, text);

		if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
			gtk_clist_set_row_data_full 
				(clist, new_row, 
				 view_identifier, 
				 (GtkDestroyNotify)nautilus_view_identifier_free);
		} else {
			gtk_clist_set_row_data_full 
				(clist, new_row, 
				 gnome_vfs_mime_application_copy (application), 
				 (GtkDestroyNotify)gnome_vfs_mime_application_free);
		}
		
		g_strfreev (text);
	}

	if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		gnome_vfs_mime_component_list_free (programs);
	} else {
		gnome_vfs_mime_application_list_free (programs);
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

static gint
compare_mime_applications (GnomeVFSMimeApplication *app_1, GnomeVFSMimeApplication *app_2)
{
	return strcmp (app_1->id, app_2->id);
}

static gint
compare_component_with_view (OAF_ServerInfo *info, NautilusViewIdentifier *identifier)
{
	return strcmp (info->iid, identifier->iid);
}

static gboolean
is_component_in_short_list (NautilusViewIdentifier *identifier, const char *mime_type)
{
	GList *list;
	gboolean result;

	list = gnome_vfs_mime_get_short_list_components (mime_type);
	result = g_list_find_custom (list, 
				     identifier, 
				     (GCompareFunc)compare_component_with_view) 
		 != NULL;
	gnome_vfs_mime_component_list_free (list);

	return result;
}

static gboolean
is_component_in_short_list_for_uri (NautilusViewIdentifier *identifier, const char *uri)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_short_list_components_for_uri (uri);
	result = g_list_find_custom (list, 
				     identifier, 
				     (GCompareFunc)compare_component_with_view) 
		 != NULL;
	gnome_vfs_mime_component_list_free (list);

	return result;
}

static gboolean
is_application_in_short_list (GnomeVFSMimeApplication *application, const char *mime_type)
{
	GList *list;
	gboolean result;

	list = gnome_vfs_mime_get_short_list_applications (mime_type);
	result = g_list_find_custom (list, 
				     application, 
				     (GCompareFunc)compare_mime_applications) 
		 != NULL;
	gnome_vfs_mime_application_list_free (list);

	return result;
}

static gboolean
is_application_in_short_list_for_uri (GnomeVFSMimeApplication *application, const char *uri)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_short_list_applications_for_uri (uri);
	result = g_list_find_custom (list, 
				     application, 
				     (GCompareFunc)compare_mime_applications) 
		 != NULL;
	gnome_vfs_mime_application_list_free (list);

	return result;
}

static gboolean
is_in_short_list_for_file_type (GnomeDialog *program_chooser, NautilusFile *file, gpointer data) 
{
	char *mime_type;
	gboolean result;
	
	g_assert (GNOME_IS_DIALOG (program_chooser));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (data != NULL);

	mime_type = nautilus_file_get_mime_type (file);

	if (nautilus_program_chooser_get_type (program_chooser)
	    == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_in_short_list ((NautilusViewIdentifier *)data, mime_type);
	} else {
		result = is_application_in_short_list ((GnomeVFSMimeApplication *)data, mime_type);
	}

	g_free (mime_type);

	return result;
}

static gboolean
is_in_short_list_for_file (GnomeDialog *program_chooser, NautilusFile *file, gpointer data) 
{
	char *uri;
	gboolean result;

	g_assert (GNOME_IS_DIALOG (program_chooser));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (data != NULL);

	uri = nautilus_file_get_uri (file);

	if (nautilus_program_chooser_get_type (program_chooser)
	    == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_in_short_list_for_uri ((NautilusViewIdentifier *)data, uri);
	} else {
		result = is_application_in_short_list_for_uri ((GnomeVFSMimeApplication *)data, uri);
	}

	g_free (uri);

	return result;
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
	gpointer selected_row_data;

	file = nautilus_program_chooser_get_file (dialog);
	clist = nautilus_program_chooser_get_clist (dialog);
	frame = nautilus_program_chooser_get_frame (dialog);
	status_label = nautilus_program_chooser_get_status_label (dialog);

	selected_row = nautilus_gtk_clist_get_first_selected_row (clist);

	if (selected_row >= 0 && gtk_clist_get_text (clist, 
						     selected_row, 
						     PROGRAM_LIST_NAME_COLUMN, 
						     &row_text)) {
		selected_row_data = gtk_clist_get_row_data (clist, selected_row);

		/* row_text is now a pointer to the text in the list. */
		frame_label_text = g_strdup (row_text);

		if (is_in_short_list_for_file_type (dialog, file, selected_row_data)) {
			file_type = nautilus_file_get_string_attribute (file, "type");
			status_label_text = g_strdup_printf (_("Is in the menu for all \"%s\" items."), 
					      		     file_type);	
			g_free (file_type);
		} else if (is_in_short_list_for_file (dialog, file, selected_row_data)) {
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
add_to_short_list_for_file (GnomeDialog *program_chooser, NautilusFile *file, gpointer data)
{
	char *uri;

	uri = nautilus_file_get_uri (file);
	
	if (nautilus_program_chooser_get_type (program_chooser) 
	    == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		nautilus_mime_add_application_to_short_list_for_uri (uri, ((GnomeVFSMimeApplication *)data)->id);
	} else {
		nautilus_mime_add_component_to_short_list_for_uri (uri, ((NautilusViewIdentifier *)data)->iid);
	}

	g_free (uri);
}

static void
remove_from_short_list_for_file (GnomeDialog *program_chooser, NautilusFile *file, gpointer data)
{
	char *uri;

	uri = nautilus_file_get_uri (file);
	
	if (nautilus_program_chooser_get_type (program_chooser) 
	    == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		nautilus_mime_remove_application_from_short_list_for_uri (uri, ((GnomeVFSMimeApplication *)data)->id);
	} else {
		nautilus_mime_remove_component_from_short_list_for_uri (uri, ((NautilusViewIdentifier *)data)->iid);
	}

	g_free (uri);
}

static void
add_to_short_list_for_type (GnomeDialog *program_chooser, NautilusFile *file, gpointer data)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (file);
	
	if (nautilus_program_chooser_get_type (program_chooser) 
	    == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		gnome_vfs_mime_add_application_to_short_list (mime_type, ((GnomeVFSMimeApplication *)data)->id);
	} else {
		gnome_vfs_mime_add_component_to_short_list (mime_type, ((NautilusViewIdentifier *)data)->iid);
	}

	g_free (mime_type);
}

static void
remove_from_short_list_for_type (GnomeDialog *program_chooser, NautilusFile *file, gpointer data)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (file);
	
	if (nautilus_program_chooser_get_type (program_chooser) 
	    == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		gnome_vfs_mime_remove_application_from_short_list (mime_type, ((GnomeVFSMimeApplication *)data)->id);
	} else {
		gnome_vfs_mime_remove_component_from_short_list (mime_type, ((NautilusViewIdentifier *)data)->iid);
	}

	g_free (mime_type);
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
	gpointer selected_row_data;

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

	title = g_strdup_printf (_("Modify \"%s\""), row_text);		     
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
	selected_row_data = gtk_clist_get_row_data (clist, selected_row);
	if (is_in_short_list_for_file_type (program_chooser, file, selected_row_data)) {
		old_active_button = type_radio_button;
	} else if (is_in_short_list_for_file (program_chooser, file, selected_row_data)) {
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
		if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (old_active_button))) {
			/* Selected button has changed, update stuff as necessary. */

			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item_radio_button))) {
				if (old_active_button == type_radio_button) {
					remove_from_short_list_for_type (program_chooser, file, selected_row_data);
				} else {
					g_assert (old_active_button == none_radio_button);
				}
				add_to_short_list_for_file (program_chooser, file, selected_row_data);
			} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_radio_button))) {
				if (old_active_button == item_radio_button) {
					remove_from_short_list_for_file (program_chooser, file, selected_row_data);
				} else {
					g_assert (old_active_button == none_radio_button);
				}
				add_to_short_list_for_type (program_chooser, file, selected_row_data);
			} else {
				g_assert (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (none_radio_button)));
				if (old_active_button == item_radio_button) {
					remove_from_short_list_for_file (program_chooser, file, selected_row_data);
				} else {
					g_assert (old_active_button == type_radio_button);
				}
				remove_from_short_list_for_type (program_chooser, file, selected_row_data);
			}

			update_selected_item_details (program_chooser);
		}
	}

	gtk_widget_destroy (dialog);
}


GnomeDialog *
nautilus_program_chooser_new (GnomeVFSMimeActionType type,
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
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		title = _("Nautilus: Open with Other");
		prompt = g_strdup_printf (_("Choose an application with which to open \"%s\"."), file_name);
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
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

	gtk_object_set_data (GTK_OBJECT (window), "type", GINT_TO_POINTER (type));

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

	gtk_object_set_data (GTK_OBJECT (window), "list", clist);

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

  	change_button = gtk_button_new_with_label(_("Modify..."));
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

/**
 * nautilus_program_chooser_get_application:
 * 
 * Get the currently-chosen application in the program-choosing dialog.
 * Usually used after the dialog has been closed (but not yet destroyed)
 * to get the user's final choice. The returned value is the actual one
 * stored in the dialog, and thus cannot be accessed after the dialog
 * has been destroyed.
 * 
 * @program_chooser: The result of calling nautilus_program_chooser_new
 * with type GNOME_VFS_MIME_ACTION_TYPE_APPLICATION.
 * 
 * Return value: a GnomeVFSMimeApplication specifying a component. The caller
 * should make a copy if they want to use it after the dialog has been
 * destroyed.
 */
GnomeVFSMimeApplication *
nautilus_program_chooser_get_application (GnomeDialog *program_chooser)
{
	GtkCList *clist;

	g_return_val_if_fail (GNOME_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (nautilus_program_chooser_get_type (program_chooser)
			== GNOME_VFS_MIME_ACTION_TYPE_APPLICATION,
		 NULL);

	clist = GTK_CLIST (gtk_object_get_data (GTK_OBJECT (program_chooser), "list"));
	return (GnomeVFSMimeApplication *)gtk_clist_get_row_data 
		(clist, nautilus_gtk_clist_get_first_selected_row (clist));
}

/**
 * nautilus_program_chooser_get_component:
 * 
 * Get the currently-chosen component in the program-choosing dialog.
 * Usually used after the dialog has been closed (but not yet destroyed)
 * to get the user's final choice. The returned value is the actual one
 * stored in the dialog, and thus cannot be accessed after the dialog
 * has been destroyed.
 * 
 * @program_chooser: The result of calling nautilus_program_chooser_new
 * with type GNOME_VFS_MIME_ACTION_TYPE_COMPONENT.
 * 
 * Return value: a NautilusViewIdentifier specifying a component. The caller
 * should make a copy if they want to use it after the dialog has been
 * destroyed.
 */
NautilusViewIdentifier *
nautilus_program_chooser_get_component (GnomeDialog *program_chooser)
{
	GtkCList *clist;

	g_return_val_if_fail (GNOME_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (nautilus_program_chooser_get_type (program_chooser)
			== GNOME_VFS_MIME_ACTION_TYPE_COMPONENT,
		 NULL);

	clist = GTK_CLIST (gtk_object_get_data (GTK_OBJECT (program_chooser), "list"));
	return (NautilusViewIdentifier *)gtk_clist_get_row_data 
		(clist, nautilus_gtk_clist_get_first_selected_row (clist));
}
