/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.c - implementation for window that lets user choose 
                                a program from a list

   Copyright (C) 2000, 2001 Eazel, Inc.

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

#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include "nautilus-global-preferences.h"
#include "nautilus-mime-actions.h"
#include "nautilus-program-choosing.h"
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include "nautilus-view-identifier.h"
#include <gtk/gtkclist.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>

#define RESPONSE_CHOOSE 1000

enum {
	PROGRAM_LIST_NAME_COLUMN,
	PROGRAM_LIST_STATUS_COLUMN,
	PROGRAM_LIST_COLUMN_COUNT
};

typedef enum {
	PROGRAM_STATUS_UNKNOWN,
	PROGRAM_NOT_IN_PREFERRED_LIST,
	PROGRAM_IN_PREFERRED_LIST_FOR_SUPERTYPE,
	PROGRAM_IN_PREFERRED_LIST_FOR_TYPE,
	PROGRAM_IN_PREFERRED_LIST_FOR_FILE,
	PROGRAM_DEFAULT_FOR_SUPERTYPE,
	PROGRAM_DEFAULT_FOR_TYPE,
	PROGRAM_DEFAULT_FOR_FILE,
} ProgramFileStatus;

typedef struct {
	NautilusViewIdentifier *view_identifier;
	GnomeVFSMimeApplication *application;
	GnomeVFSMimeActionType action_type;
	NautilusFile *file;
	ProgramFileStatus status;
} ProgramFilePair;

/* Scrolling list has no idea how tall to make itself. Its
 * "natural height" is just enough to draw the scroll bar controls.
 * Hardwire an initial window size here, but let user resize
 * bigger or smaller.
 */
#define PROGRAM_CHOOSER_DEFAULT_HEIGHT	 374

/* If we let the automatic column sizing code run, it
 * makes the Status column much wider than the Name column,
 * which looks bad. Since there's no way to say "give each
 * column the same amount of space", we'll hardwire a width.
 */
#define NAME_COLUMN_INITIAL_WIDTH	 200

/* Program name of the mime type capplet */
#define FILE_TYPES_CAPPLET_NAME 	"file-types-capplet"

/* This number controls a maximum character count for a file name that is
 * displayed as part of a dialog (beyond this it will be truncated). 
 * It's fairly arbitrary -- big enough to allow most "normal" names to display 
 * in full, but small enough to prevent the dialog from getting insanely wide.
 */
#define MAX_DISPLAYED_FILE_NAME_LENGTH 40

/* Forward declarations as needed */
static gboolean program_file_pair_is_default_for_file_type 	 (ProgramFilePair *pair);
static gboolean program_file_pair_is_default_for_file 		 (ProgramFilePair *pair);
static gboolean program_file_pair_is_in_short_list_for_file_type (ProgramFilePair *pair);
static gboolean program_file_pair_is_in_short_list_for_file 	 (ProgramFilePair *pair);

static gboolean
program_file_pair_compute_status (ProgramFilePair *pair)
{
	ProgramFileStatus new_status;

	/* FIXME bugzilla.gnome.org 41459: Need to check whether it's the default or in short list for the supertype */
	if (program_file_pair_is_default_for_file_type (pair)) {
		new_status = PROGRAM_DEFAULT_FOR_TYPE;
	} else if (program_file_pair_is_default_for_file (pair)) {
		new_status = PROGRAM_DEFAULT_FOR_FILE;
	} else if (program_file_pair_is_in_short_list_for_file_type (pair)) {
		new_status = PROGRAM_IN_PREFERRED_LIST_FOR_TYPE;
	} else if (program_file_pair_is_in_short_list_for_file (pair)) {
		new_status = PROGRAM_IN_PREFERRED_LIST_FOR_FILE;
	} else {
		new_status = PROGRAM_NOT_IN_PREFERRED_LIST;
	}

	if (new_status == pair->status) {
		return FALSE;
	}

	pair->status = new_status;
	return TRUE;
}

static void
program_file_pair_set_file (ProgramFilePair *pair, NautilusFile *file)
{
	if (pair->file == file) {
		return;
	}
	nautilus_file_unref (pair->file);
	nautilus_file_ref (file);
	pair->file = file;
	program_file_pair_compute_status (pair);
}

static ProgramFilePair *
program_file_pair_new_from_content_view (Bonobo_ServerInfo *content_view, NautilusFile *file)
{
	ProgramFilePair *new_pair;

	new_pair = g_new0 (ProgramFilePair, 1);
	new_pair->view_identifier = nautilus_view_identifier_new_from_content_view (content_view);
	new_pair->action_type = GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;

	program_file_pair_set_file (new_pair, file);

	return new_pair;
}

static ProgramFilePair *
program_file_pair_new_from_application (GnomeVFSMimeApplication *application, NautilusFile *file)
{
	ProgramFilePair *new_pair;

	new_pair = g_new0 (ProgramFilePair, 1);
	new_pair->application = gnome_vfs_mime_application_copy (application);
	new_pair->action_type = GNOME_VFS_MIME_ACTION_TYPE_APPLICATION;

	program_file_pair_set_file (new_pair, file);

	return new_pair;
}

static void
program_file_pair_free (ProgramFilePair *pair)
{
	nautilus_view_identifier_free (pair->view_identifier);
	gnome_vfs_mime_application_free (pair->application);
	nautilus_file_unref (pair->file);
	
	g_free (pair);
}

static char *
program_file_pair_get_program_name_for_display (ProgramFilePair *pair)
{
	g_assert (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION 
		  || pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
	g_assert (pair->action_type != GNOME_VFS_MIME_ACTION_TYPE_APPLICATION 
		  || pair->application != NULL);
	g_assert (pair->action_type != GNOME_VFS_MIME_ACTION_TYPE_COMPONENT 
		  || pair->view_identifier != NULL);

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		return g_strdup (_(pair->view_identifier->view_as_label));
	}

	return g_strdup (_(pair->application->name));
}

static char *
get_supertype_from_file (NautilusFile *file)
{
	/* FIXME bugzilla.gnome.org 41459: Needs implementation */
	return nautilus_file_get_string_attribute (file, "type");
}

static char *
program_file_pair_get_short_status_text (ProgramFilePair *pair)
{
	char *file_type;
	char *supertype;
	char *result;

	file_type = nautilus_file_get_string_attribute_with_default (pair->file, "type");
	supertype = get_supertype_from_file (pair->file);

	switch (pair->status) {
	default:
	case PROGRAM_STATUS_UNKNOWN:
		g_assert_not_reached ();
		result = "error";
		break;
	case PROGRAM_NOT_IN_PREFERRED_LIST:
		result = g_strdup (_("not in menu"));
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_FILE:
		result = g_strdup (_("in menu for this file"));				
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_TYPE:
		result = g_strdup_printf (_("in menu for \"%s\""), file_type);	
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_SUPERTYPE:
		result = g_strdup_printf (_("in menu for \"%s\""), supertype);	
		break;
	case PROGRAM_DEFAULT_FOR_FILE:
		result = g_strdup (_("default for this file"));				
		break;
	case PROGRAM_DEFAULT_FOR_TYPE:
		result = g_strdup_printf (_("default for \"%s\""), file_type);	
		break;
	case PROGRAM_DEFAULT_FOR_SUPERTYPE:
		result = g_strdup_printf (_("default for \"%s\""), supertype);	
		break;
	}
	
	g_free (file_type);
	g_free (supertype);

	return result;
}

static char *
get_file_name_for_display (NautilusFile *file)
{
	char *full_name;
	char *truncated_name;

	g_assert (NAUTILUS_IS_FILE (file));

	full_name = nautilus_file_get_display_name (file);
	truncated_name = eel_str_middle_truncate
		(full_name, MAX_DISPLAYED_FILE_NAME_LENGTH);
	g_free (full_name);

	return truncated_name;
}

static char *
program_file_pair_get_long_status_text (ProgramFilePair *pair)
{
	char *file_type;
	char *file_name;
	char *supertype;
	char *result;

	file_type = nautilus_file_get_string_attribute_with_default (pair->file, "type");
	supertype = get_supertype_from_file (pair->file);
	file_name = get_file_name_for_display (pair->file);

	switch (pair->status) {
	default:
	case PROGRAM_STATUS_UNKNOWN:
		g_assert_not_reached ();
		result = "error";
		break;
	case PROGRAM_NOT_IN_PREFERRED_LIST:
		result = g_strdup_printf (_("Is not in the menu for \"%s\" items."), file_type);
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_FILE:
		result = g_strdup_printf (_("Is in the menu for \"%s\"."), file_name);				
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_TYPE:
		result = g_strdup_printf (_("Is in the menu for \"%s\" items."), file_type);	
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_SUPERTYPE:
		result = g_strdup_printf (_("Is in the menu for all \"%s\" items."), supertype);	
		break;
	case PROGRAM_DEFAULT_FOR_FILE:
		result = g_strdup_printf (_("Is the default for \"%s\"."), file_name);				
		break;
	case PROGRAM_DEFAULT_FOR_TYPE:
		result = g_strdup_printf (_("Is the default for \"%s\" items."), file_type);	
		break;
	case PROGRAM_DEFAULT_FOR_SUPERTYPE:
		result = g_strdup_printf (_("Is the default for all \"%s\" items."), supertype);	
		break;
	}
	
	g_free (file_type);
	g_free (file_name);
	g_free (supertype);

	return result;
}

static GnomeVFSMimeActionType
nautilus_program_chooser_get_type (GtkDialog *program_chooser)
{
	GnomeVFSMimeActionType type;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (program_chooser), "type"));

	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT ||
		  type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

	return type;
}

static void
repopulate_program_list (GtkDialog *program_chooser,
		         NautilusFile *file,
		         GtkCList *clist)
{
	char **text;
	GList *programs, *program;
	ProgramFilePair *pair;
	int new_row;
	GnomeVFSMimeActionType type;

	type = nautilus_program_chooser_get_type (program_chooser);

	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		  || type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
	

	programs = type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		? nautilus_mime_get_all_components_for_file (file)
		: nautilus_mime_get_all_applications_for_file (file);

	gtk_clist_clear (clist);
		
	for (program = programs; program != NULL; program = program->next) {
		/* One extra slot so it's NULL-terminated */
		text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);

		if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
			pair = program_file_pair_new_from_content_view
				((Bonobo_ServerInfo *)program->data, file);
		} else {
			pair = program_file_pair_new_from_application
				((GnomeVFSMimeApplication *)program->data, file);
		}
		
		text[PROGRAM_LIST_NAME_COLUMN] = 
			program_file_pair_get_program_name_for_display (pair);			
		text[PROGRAM_LIST_STATUS_COLUMN] = 
			program_file_pair_get_short_status_text (pair);

		new_row = gtk_clist_append (clist, text);

		gtk_clist_set_row_data_full 
			(clist, new_row, pair, 
			 (GtkDestroyNotify)program_file_pair_free);
		
		g_strfreev (text);
	}

	if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		gnome_vfs_mime_component_list_free (programs);
	} else {
		gnome_vfs_mime_application_list_free (programs);
	}

	gtk_clist_sort (clist);

	/* Start with first item selected, rather than some arbitrary item */
	gtk_clist_select_row (clist, 0, 0);
}

static NautilusFile *
nautilus_program_chooser_get_file (GtkDialog *chooser)
{
	return NAUTILUS_FILE (g_object_get_data (G_OBJECT (chooser), "file"));
}

static GtkCList *
nautilus_program_chooser_get_clist (GtkDialog *chooser)
{
	return GTK_CLIST (g_object_get_data (G_OBJECT (chooser), "clist"));
}

static GtkFrame *
nautilus_program_chooser_get_frame (GtkDialog *chooser)
{
	return GTK_FRAME (g_object_get_data (G_OBJECT (chooser), "frame"));
}

static GtkLabel *
nautilus_program_chooser_get_status_label (GtkDialog *chooser)
{
	return GTK_LABEL (g_object_get_data (G_OBJECT (chooser), "status_label"));
}

static void
nautilus_program_chooser_set_is_cancellable (GtkDialog *chooser, gboolean cancellable)
{
#if GNOME2_CONVERSION_COMPLETE
	GtkButton *done_button, *cancel_button;

	cancel_button = eel_gtk_dialog_get_button_by_index 
		(chooser, GNOME_CANCEL);
	done_button = eel_gtk_dialog_get_button_by_index 
		(chooser, GNOME_CANCEL+1);

	if (cancellable) {
		gtk_widget_hide (GTK_WIDGET (done_button));
		gtk_widget_show (GTK_WIDGET (cancel_button));
	} else {
		gtk_widget_hide (GTK_WIDGET (cancel_button));
		gtk_widget_show (GTK_WIDGET (done_button));
	}
#endif
}

static void
nautilus_program_chooser_set_file (GtkDialog *chooser, NautilusFile *file)
{
	nautilus_file_ref (file);
	g_object_set_data_full (G_OBJECT (chooser), 
				"file", 
				file, 
				(GtkDestroyNotify) nautilus_file_unref);
}

static void
nautilus_program_chooser_set_clist (GtkDialog *chooser, GtkCList *clist)
{
	g_object_set_data (G_OBJECT (chooser), "clist", clist);
}

static void
nautilus_program_chooser_set_frame (GtkDialog *chooser, GtkFrame *frame)
{
	g_object_set_data (G_OBJECT (chooser), "frame", frame);
}

static void
nautilus_program_chooser_set_status_label (GtkDialog *chooser, GtkLabel *status_label)
{
	g_object_set_data (G_OBJECT (chooser), "status_label", status_label);
}

static gint
compare_mime_applications (GnomeVFSMimeApplication *app_1, GnomeVFSMimeApplication *app_2)
{
	return strcmp (app_1->id, app_2->id);
}

static gint
compare_component_with_view (Bonobo_ServerInfo *info, NautilusViewIdentifier *identifier)
{
	return strcmp (info->iid, identifier->iid);
}

static gboolean
is_application_default_for_type (GnomeVFSMimeApplication *application, const char *mime_type)
{
	GnomeVFSMimeApplication *default_application;
	gboolean result;

	g_assert (application != NULL);

	default_application = gnome_vfs_mime_get_default_application (mime_type);
	result = (default_application != NULL && strcmp (default_application->id, application->id) == 0);

	gnome_vfs_mime_application_free (default_application);

	return result;
}

static gboolean
is_component_default_for_type (NautilusViewIdentifier *identifier, const char *mime_type)
{
	Bonobo_ServerInfo *default_component;
	gboolean result;

	g_assert (identifier != NULL);

	default_component = gnome_vfs_mime_get_default_component (mime_type);
	result = (default_component != NULL && strcmp (default_component->iid, identifier->iid) == 0);

	CORBA_free (default_component);

	return result;
}

static gboolean
is_application_default_for_file (GnomeVFSMimeApplication *application, 
				 NautilusFile *file)
{
	GnomeVFSMimeApplication *default_application;
	gboolean result;

	g_assert (application != NULL);

	default_application = nautilus_mime_get_default_application_for_file (file);
	result = (default_application != NULL && strcmp (default_application->id, application->id) == 0);

	gnome_vfs_mime_application_free (default_application);

	return result;
}

static gboolean
is_component_default_for_file (NautilusViewIdentifier *identifier, NautilusFile *file)
{
	Bonobo_ServerInfo *default_component;
	gboolean result;

	g_assert (identifier != NULL);

	default_component = nautilus_mime_get_default_component_for_file (file);
	result = (default_component != NULL && strcmp (default_component->iid, identifier->iid) == 0);

	CORBA_free (default_component);

	return result;
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
is_component_in_short_list_for_file (NautilusViewIdentifier *identifier, 
				     NautilusFile *file)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_short_list_components_for_file (file);
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
is_application_in_short_list_for_file (GnomeVFSMimeApplication *application, NautilusFile *file)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_short_list_applications_for_file (file);
	result = g_list_find_custom (list, 
				     application, 
				     (GCompareFunc)compare_mime_applications) 
		 != NULL;
	gnome_vfs_mime_application_list_free (list);
	
	return result;
}

static gboolean
program_file_pair_is_default_for_file_type (ProgramFilePair *pair)
{
	char *mime_type;
	gboolean result;
	
	g_assert (pair != NULL);
	g_assert (NAUTILUS_IS_FILE (pair->file));

	mime_type = nautilus_file_get_mime_type (pair->file);

	if (pair->action_type != gnome_vfs_mime_get_default_action_type (mime_type)) {
		return FALSE;
	}

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_default_for_type (pair->view_identifier, mime_type);
	} else {
		result = is_application_default_for_type (pair->application, mime_type);
	}

	g_free (mime_type);

	return result;
}

static gboolean
program_file_pair_is_default_for_file (ProgramFilePair *pair)
{
	gboolean result;

	g_assert (pair != NULL);
	g_assert (NAUTILUS_IS_FILE (pair->file));

	if (pair->action_type != nautilus_mime_get_default_action_type_for_file (pair->file)) {
		return FALSE;
	}

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_default_for_file (pair->view_identifier, pair->file);
	} else {
		result = is_application_default_for_file (pair->application, pair->file);
	}

	return result;
}

static gboolean
program_file_pair_is_in_short_list_for_file_type (ProgramFilePair *pair) 
{
	char *mime_type;
	gboolean result;
	
	g_assert (pair != NULL);
	g_assert (NAUTILUS_IS_FILE (pair->file));

	mime_type = nautilus_file_get_mime_type (pair->file);

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_in_short_list (pair->view_identifier, mime_type);
	} else {
		result = is_application_in_short_list (pair->application, mime_type);
	}

	g_free (mime_type);

	return result;
}

static gboolean
program_file_pair_is_in_short_list_for_file (ProgramFilePair *pair) 
{
	gboolean result;

	g_assert (pair != NULL);
	g_assert (NAUTILUS_IS_FILE (pair->file));

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = is_component_in_short_list_for_file (pair->view_identifier, pair->file);
	} else {
		result = is_application_in_short_list_for_file (pair->application, pair->file);
	}

	return result;
}

static ProgramFilePair *
get_program_file_pair_from_row_data (GtkCList *clist, int row)
{
	g_assert (row < clist->rows);
	return (ProgramFilePair *)gtk_clist_get_row_data (clist, row);
}

static ProgramFilePair *
get_selected_program_file_pair (GtkDialog *dialog)
{
	GtkCList *clist;
	int selected_row;

	clist = nautilus_program_chooser_get_clist (dialog);
	selected_row = eel_gtk_clist_get_first_selected_row (clist);
	
	if (selected_row < 0) {
		return NULL;
	}
	
	return get_program_file_pair_from_row_data (clist, selected_row);
}

static void
update_selected_item_details (GtkDialog *dialog)
{
	GtkFrame *frame;
	GtkLabel *status_label;
	char *frame_label_text, *status_label_text;
	ProgramFilePair *pair;

	frame = nautilus_program_chooser_get_frame (dialog);
	status_label = nautilus_program_chooser_get_status_label (dialog);

	pair = get_selected_program_file_pair (dialog);

	if (pair != NULL) {
		frame_label_text = program_file_pair_get_program_name_for_display (pair);
		status_label_text = program_file_pair_get_long_status_text (pair);
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
update_all_status (GtkDialog *dialog)
{
	GtkCList *clist;
	ProgramFilePair *pair;
	char *status_text;
	int row;
	gboolean anything_changed;

	clist = nautilus_program_chooser_get_clist (dialog);	
	anything_changed = FALSE;
	for (row = 0; row < clist->rows; ++row) {
		pair = get_program_file_pair_from_row_data (clist, row);
		if (program_file_pair_compute_status (pair)) {
			/* Status has changed, update text in list */
			anything_changed = TRUE;
			status_text = program_file_pair_get_short_status_text (pair);
			gtk_clist_set_text (clist, row, PROGRAM_LIST_STATUS_COLUMN, status_text);
			g_free (status_text);
		}
	}

	if (anything_changed) {
		gtk_clist_sort (clist);
	}
}

static void
program_list_selection_changed_callback (GtkCList *clist, 
					 gint row, 
					 gint column, 
					 GdkEventButton *event, 
					 gpointer user_data)
{
	g_assert (GTK_IS_CLIST (clist));

	update_selected_item_details (GTK_DIALOG (user_data));
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
add_to_short_list_for_file (ProgramFilePair *pair)
{
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		nautilus_mime_add_application_to_short_list_for_file (pair->file, pair->application->id);
	} else {
		nautilus_mime_add_component_to_short_list_for_file (pair->file, pair->view_identifier->iid);
	}
}

static void
remove_from_short_list_for_file (ProgramFilePair *pair)
{
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		nautilus_mime_remove_application_from_short_list_for_file (pair->file, pair->application->id);
	} else {
		nautilus_mime_remove_component_from_short_list_for_file (pair->file, pair->view_identifier->iid);
	}
}

static void
add_to_short_list_for_type (ProgramFilePair *pair)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (pair->file);
	
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		gnome_vfs_mime_add_application_to_short_list (mime_type, pair->application->id);
	} else {
		gnome_vfs_mime_add_component_to_short_list (mime_type, pair->view_identifier->iid);
	}

	g_free (mime_type);
}

static void
remove_from_short_list_for_type (ProgramFilePair *pair)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (pair->file);
	
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		gnome_vfs_mime_remove_application_from_short_list (mime_type, pair->application->id);
	} else {
		gnome_vfs_mime_remove_component_from_short_list (mime_type, pair->view_identifier->iid);
	}

	g_free (mime_type);
}

static void
remove_default_for_type (ProgramFilePair *pair)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (pair->file);
	
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
	    	if (is_application_default_for_type (pair->application, mime_type)) {
			gnome_vfs_mime_set_default_application (mime_type, NULL);
	        }
	} else {
		if (is_component_default_for_type (pair->view_identifier, mime_type)) {
			gnome_vfs_mime_set_default_component (mime_type, NULL);
		}
	}

	g_free (mime_type);
}

static void
remove_default_for_item (ProgramFilePair *pair)
{
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
	    	/* If the default is just falling through to the default for this type,
	    	 * don't do anything here.
	    	 */
	    	if (nautilus_mime_is_default_application_for_file_user_chosen (pair->file)) {
	    		if (is_application_default_for_file (pair->application, pair->file)) {
				nautilus_mime_set_default_application_for_file (pair->file, NULL);
		        }
	    	}
	} else {
	    	/* If the default is just falling through to the default for this type,
	    	 * don't do anything here.
	    	 */
	    	if (nautilus_mime_is_default_component_for_file_user_chosen (pair->file)) {
	    		if (is_component_default_for_file (pair->view_identifier, pair->file)) {
				nautilus_mime_set_default_component_for_file (pair->file, NULL);
		        }
	    	}
	}
}

static void
set_default_for_type (ProgramFilePair *pair)
{
	char *mime_type;

	mime_type = nautilus_file_get_mime_type (pair->file);

	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		gnome_vfs_mime_set_default_application (mime_type, pair->application->id);
	} else {
		if (g_ascii_strcasecmp (mime_type, "x-directory/normal") == 0) {
			nautilus_global_preferences_set_default_folder_viewer (pair->view_identifier->iid);
		} else {
			gnome_vfs_mime_set_default_component (mime_type, pair->view_identifier->iid);
		}
	}

	gnome_vfs_mime_set_default_action_type (mime_type, pair->action_type);
	
	g_free (mime_type);
}

static void
set_default_for_item (ProgramFilePair *pair)
{
	if (pair->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
		nautilus_mime_set_default_application_for_file (pair->file, pair->application->id);
	} else {
		nautilus_mime_set_default_component_for_file (pair->file, pair->view_identifier->iid);
	}

	nautilus_mime_set_default_action_type_for_file (pair->file, pair->action_type);
}

static void
launch_mime_capplet (const char *mime_type)
{
	char *command;

	command = g_strconcat (FILE_TYPES_CAPPLET_NAME, " ", mime_type, NULL);
	nautilus_launch_application_from_command (FILE_TYPES_CAPPLET_NAME, command, NULL, FALSE);
	g_free (command);
}

static void
launch_mime_capplet_on_ok (GtkDialog *dialog, int response, gpointer callback_data)
{
	g_assert (GTK_IS_DIALOG (dialog));
	
	if (response == GTK_RESPONSE_OK) {
		launch_mime_capplet (callback_data);
	}
}

static void
launch_mime_capplet_and_close_dialog (GtkButton *button, gpointer callback_data)
{
	ProgramFilePair *file_pair;
	char *mime_type;
	
	g_assert (GTK_IS_BUTTON (button));

	file_pair = get_selected_program_file_pair (GTK_DIALOG (callback_data));
	mime_type = nautilus_file_get_mime_type (file_pair->file);
	launch_mime_capplet (mime_type);

	/* Don't leave a nested modal dialogs in the wake of switching
	 * user's attention to the capplet.
	 */	
	gtk_dialog_response (GTK_DIALOG (callback_data), GTK_RESPONSE_OK);
	
	g_free (mime_type);
}

static void
run_program_configurator_callback (GtkWidget *button, gpointer callback_data)
{
	GtkDialog *program_chooser;
	NautilusFile *file;
	GtkCList *clist;
	GtkWidget *dialog;
	GtkWidget *radio_buttons_frame, *framed_vbox;
	GtkRadioButton *type_radio_button, *type_default_radio_button, *item_radio_button, *item_default_radio_button, *none_radio_button;
	GtkRadioButton *old_active_button;
	char *radio_button_text;
	char *file_type, *file_name;
	char *program_display_name;
	char *title;
	ProgramFilePair *pair;

	program_chooser = GTK_DIALOG (callback_data);
	
	file = nautilus_program_chooser_get_file (program_chooser);
	clist = nautilus_program_chooser_get_clist (program_chooser);

	file_type = nautilus_file_get_string_attribute_with_default (file, "type");
	file_name = get_file_name_for_display (file);

	pair = get_selected_program_file_pair (program_chooser);
	if (pair == NULL) {
		/* No valid selected item, don't do anything. The UI
		 * should prevent this.
		 */
		return;
	}

	program_display_name = program_file_pair_get_program_name_for_display (pair);

	title = g_strdup_printf (_("Modify \"%s\""), program_display_name);
	
	dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (program_chooser),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      NULL);
	g_free (title);
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "program_chooser", "Nautilus");

	/* Labeled frame to avoid repeating text in each radio button,
	 * and to look nice.
	 */
	radio_buttons_frame = gtk_frame_new (program_display_name);
	gtk_widget_show (radio_buttons_frame);
  	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), radio_buttons_frame, FALSE, FALSE, 0);

	g_free (program_display_name);

  	framed_vbox = gtk_vbox_new (FALSE, GNOME_PAD);
  	gtk_widget_show (framed_vbox);
  	gtk_container_add (GTK_CONTAINER (radio_buttons_frame), framed_vbox);
  	gtk_container_set_border_width (GTK_CONTAINER (framed_vbox), GNOME_PAD);


	/* Radio button for adding to short list for file type. */
	radio_button_text = g_strdup_printf (_("Include in the menu for \"%s\" items"), 
					     file_type);
	type_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, NULL);
	g_free (radio_button_text);
	

	/* Radio button for setting default for file type. */
	radio_button_text = g_strdup_printf (_("Use as default for \"%s\" items"), 
					     file_type);
	type_default_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, type_radio_button);
	g_free (radio_button_text);
	

	/* Radio button for adding to short list for specific file. */
	radio_button_text = g_strdup_printf (_("Include in the menu just for \"%s\""), 
					     file_name);
	item_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, type_radio_button);
	g_free (radio_button_text);

	/* Radio button for setting default for specific file. */
	radio_button_text = g_strdup_printf (_("Use as default just for \"%s\""), 
					     file_name);
	item_default_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, type_radio_button);
	g_free (radio_button_text);
	

	/* Radio button for not including program in short list for type or file. */
	radio_button_text = g_strdup_printf (_("Don't include in the menu for \"%s\" items"), 
					     file_type);
	none_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, type_radio_button);
	g_free (radio_button_text);

	g_free (file_type);
	g_free (file_name);

	/* Activate the correct radio button. */
	switch (pair->status) {
	case PROGRAM_DEFAULT_FOR_TYPE: 
		old_active_button = type_default_radio_button;
		break;
	case PROGRAM_DEFAULT_FOR_FILE: 
		old_active_button = item_default_radio_button;
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_TYPE: 
		old_active_button = type_radio_button;
		break;
	case PROGRAM_IN_PREFERRED_LIST_FOR_FILE: 
		old_active_button = item_radio_button;
		break;
	default:
		g_warning ("unhandled program status %d", pair->status);
	case PROGRAM_NOT_IN_PREFERRED_LIST:
		old_active_button = none_radio_button;
		break;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (old_active_button), TRUE);

  	/* Make OK button the default. */
  	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (old_active_button))) {
			/* Selected button has changed, update stuff as necessary. 
			 * Rather than keep track of a whole bunch of transitions, we
			 * start by removing this program from everything and then
			 * add it back in as appropriate based on the new setting.
			 */
			if (old_active_button == item_radio_button) {
				remove_from_short_list_for_type (pair);
				remove_from_short_list_for_file (pair);
			} else if (old_active_button == item_default_radio_button) {
				remove_from_short_list_for_type (pair);
				remove_from_short_list_for_file (pair);
				remove_default_for_item (pair);
			} else if (old_active_button == type_radio_button) {
				remove_from_short_list_for_type (pair);
			} else if (old_active_button == type_default_radio_button) {
				remove_from_short_list_for_type (pair);
				remove_default_for_type (pair);
			} else {
				g_assert (old_active_button == none_radio_button);
				/* Nothing to remove anywhere for this case. */
			}

			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item_radio_button))) {
				add_to_short_list_for_file (pair);
			} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item_default_radio_button))) {
				add_to_short_list_for_file (pair);
				set_default_for_item (pair);
			} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_radio_button))) {
				add_to_short_list_for_type (pair);
				/* To remove it from the "removed" list if necessary. */
				add_to_short_list_for_file (pair);
			} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_default_radio_button))) {
				add_to_short_list_for_type (pair);
				/* To remove it from the "removed" list if necessary. */
				add_to_short_list_for_type (pair);
				set_default_for_type (pair);
			} else {
				g_assert (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (none_radio_button)));
				/* Nothing to add anywhere for this case. */
			}

			/* Change made in sub-dialog; now the main dialog can't be "cancel"ed, 
			 * it can only be closed with no further changes.
			 */
			nautilus_program_chooser_set_is_cancellable (program_chooser, FALSE);

			/* Update displayed text about selected program. */
			update_selected_item_details (program_chooser);

			/* Update text in list too, since a changed item might change
			 * other items as side-effect (like changing the default).
			 */
			update_all_status (program_chooser);
		}
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}

static int
compare_program_file_pairs (GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	GtkCListRow *row1, *row2;
	ProgramFilePair *pair1, *pair2;
	char *name1, *name2;
	gint result;

	g_assert (GTK_IS_CLIST (clist));

	row1 = (GtkCListRow *) ptr1;
	row2 = (GtkCListRow *) ptr2;

	pair1 = (ProgramFilePair *) row1->data;
	pair2 = (ProgramFilePair *) row2->data;

	switch (clist->sort_column) {
		case PROGRAM_LIST_STATUS_COLUMN:
			if (pair1->status > pair2->status) {
				result = -1;
			} else if (pair1->status < pair2->status) {
				result = +1;
			} else {
				result = 0;
			}
			break;
		case PROGRAM_LIST_NAME_COLUMN:
			name1 = program_file_pair_get_program_name_for_display (pair1);
			name2 = program_file_pair_get_program_name_for_display (pair2);
			result = strcmp (name1, name2);
			g_free (name1);
			g_free (name2);
			break;
		default:
			g_warning ("unhandled sort column %d", clist->sort_column);
			result = 0;
			break;
	}

	return result;	
}

static void
switch_sort_column (GtkCList *clist, gint column, gpointer user_data)
{
	g_assert (GTK_IS_CLIST (clist));

	gtk_clist_set_sort_column (clist, column);
	gtk_clist_sort (clist);
}

static GtkWidget *
create_program_clist ()
{
	GtkCList *clist;

	clist = GTK_CLIST (gtk_clist_new (PROGRAM_LIST_COLUMN_COUNT));

	gtk_clist_set_column_title (clist, PROGRAM_LIST_NAME_COLUMN, _("Name"));
	gtk_clist_set_column_width (clist, PROGRAM_LIST_NAME_COLUMN, NAME_COLUMN_INITIAL_WIDTH);

	gtk_clist_set_column_title (clist, PROGRAM_LIST_STATUS_COLUMN, _("Status"));
	/* This column will get all the rest of the width */

	gtk_clist_set_selection_mode (clist, GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_show (clist);
	gtk_widget_show (GTK_WIDGET (clist));

	gtk_clist_set_sort_column (clist, PROGRAM_LIST_NAME_COLUMN);
	/* Do not use autosort. The list changes only at well-defined times
	 * that we control, and autosort has that nasty bug where you can't
	 * use row data in the compare function because the row is sorted
	 * on insert before the row data has been added.
	 */
	gtk_clist_set_compare_func (clist, compare_program_file_pairs);

	g_signal_connect (G_OBJECT (clist),
			    "click_column",
			    G_CALLBACK (switch_sort_column),
			    NULL);
			    

	return GTK_WIDGET (clist);
}

GtkDialog *
nautilus_program_chooser_new (GnomeVFSMimeActionType action_type,
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
	GtkWidget *capplet_button_frame, *capplet_hbox;
	GtkWidget *capplet_button, *caption, *capplet_button_vbox;
	char *file_name, *prompt;
	const char *title;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	file_name = get_file_name_for_display (file);

	switch (action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		title = _("Open with Other");
		prompt = g_strdup_printf (_("Choose an application with which to open \"%s\"."), file_name);
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
	default:
		title = _("View as Other");
		prompt = g_strdup_printf (_("Choose a view for \"%s\"."), file_name);
		break;
	}

	g_free (file_name);
	
	window = gtk_dialog_new_with_buttons (title, NULL, 0,
					      _("Choose"), RESPONSE_CHOOSE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      _("Done"), GTK_RESPONSE_OK,
					      NULL);

	nautilus_program_chooser_set_is_cancellable (GTK_DIALOG (window), TRUE);

  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (window), -1, PROGRAM_CHOOSER_DEFAULT_HEIGHT);
	gtk_window_set_wmclass (GTK_WINDOW (window), "program_chooser", "Nautilus");

	g_object_set_data (G_OBJECT (window), "type", GINT_TO_POINTER (action_type));

	dialog_vbox = GTK_DIALOG (window)->vbox;

	/* Prompt at top of dialog. */
	prompt_label = gtk_label_new (prompt);
	gtk_widget_show (prompt_label);
	/* Move prompt to left edge */
	gtk_misc_set_alignment (GTK_MISC (prompt_label), 0, 0.5);
  	g_free (prompt);

  	gtk_box_pack_start (GTK_BOX (dialog_vbox), prompt_label, FALSE, FALSE, 0);

	/* Scrolling list to hold choices. */
	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (window)->vbox), list_scroller);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), 
					GTK_POLICY_NEVER, 
					GTK_POLICY_AUTOMATIC);	  

	clist = create_program_clist ();

	gtk_container_add (GTK_CONTAINER (list_scroller), clist);	  
#if GNOME2_CONVERSION_COMPLETE
	eel_gtk_clist_set_double_click_button 
		(GTK_CLIST (clist), 
		 eel_gtk_dialog_get_button_by_index 
			(GTK_DIALOG (window), GNOME_OK));
#endif
	g_object_set_data (G_OBJECT (window), "list", clist);

	repopulate_program_list (GTK_DIALOG (window), file, GTK_CLIST (clist));

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

  	change_button = gtk_button_new_with_mnemonic (_("_Modify..."));

  	gtk_widget_show (change_button);
  	gtk_box_pack_end (GTK_BOX (change_button_holder), change_button, TRUE, FALSE, 0);

  	g_signal_connect (G_OBJECT (change_button),
			  "clicked",
			  G_CALLBACK (run_program_configurator_callback),
			  window);

	/* Framed area with button to launch mime type editing capplet. */
	capplet_button_frame = gtk_frame_new (_("File Types and Programs"));
	gtk_widget_show (capplet_button_frame);
  	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), capplet_button_frame, FALSE, FALSE, 0);

  	capplet_hbox = gtk_hbox_new (FALSE, GNOME_PAD_BIG);
  	gtk_widget_show (capplet_hbox);
  	gtk_container_add (GTK_CONTAINER (capplet_button_frame), capplet_hbox);
  	gtk_container_set_border_width (GTK_CONTAINER (capplet_hbox), GNOME_PAD);

	capplet_button_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (capplet_button_vbox);
	gtk_box_pack_end (GTK_BOX (capplet_hbox), capplet_button_vbox, FALSE, FALSE, 0);
	capplet_button = gtk_button_new_with_mnemonic (_("_Go There"));	 

	g_signal_connect (G_OBJECT (capplet_button),
			  "clicked",
			  G_CALLBACK (launch_mime_capplet_and_close_dialog),
			  window);
	gtk_widget_show (capplet_button);
	gtk_box_pack_start (GTK_BOX (capplet_button_vbox), capplet_button, TRUE, FALSE, 0);

	caption = gtk_label_new (_("You can configure which programs are offered "
				   "for which file types in the GNOME Control Center."));
	gtk_widget_show (caption);
	gtk_label_set_line_wrap (GTK_LABEL (caption), TRUE);
	gtk_box_pack_start (GTK_BOX (capplet_hbox), caption, FALSE, FALSE, 0);				    

  	/* Make confirmation button the default. */
  	gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_OK);

	/* Load up the dialog object with info other functions will need. */
	nautilus_program_chooser_set_file (GTK_DIALOG (window), file);
	nautilus_program_chooser_set_clist (GTK_DIALOG (window), GTK_CLIST (clist));
	nautilus_program_chooser_set_frame (GTK_DIALOG (window), GTK_FRAME (frame));
	nautilus_program_chooser_set_status_label (GTK_DIALOG (window), GTK_LABEL (status_label));
	
	/* Fill in initial info about the selected item. */
  	update_selected_item_details (GTK_DIALOG (window));

  	/* Update selected item info whenever selection changes. */
  	g_signal_connect (G_OBJECT (clist),
  			    "select_row",
  			    G_CALLBACK (program_list_selection_changed_callback),
  			    window);

  	return GTK_DIALOG (window);
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
nautilus_program_chooser_get_application (GtkDialog *program_chooser)
{
	ProgramFilePair *pair;

	g_return_val_if_fail (GTK_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (nautilus_program_chooser_get_type (program_chooser)
			      == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION,
			      NULL);

	pair = get_selected_program_file_pair (program_chooser);
	if (pair == NULL) {
		return NULL;
	}

	return pair->application;
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
nautilus_program_chooser_get_component (GtkDialog *program_chooser)
{
	ProgramFilePair *pair;

	g_return_val_if_fail (GTK_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (nautilus_program_chooser_get_type (program_chooser)
			      == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT,
			      NULL);
	
	pair = get_selected_program_file_pair (program_chooser);
	if (pair == NULL) {
		return NULL;
	}

	return pair->view_identifier;
}

void
nautilus_program_chooser_show_no_choices_message (GnomeVFSMimeActionType action_type,
						  NautilusFile *file, 
						  GtkWindow *parent_window)
{
	char *prompt;
	char *unavailable_message;
	char *file_name;
	char *dialog_title;
	GtkDialog *dialog;

	file_name = get_file_name_for_display (file);

	if (action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		unavailable_message = g_strdup_printf (_("No viewers are available for \"%s\"."), file_name);
		dialog_title = g_strdup (_("No Viewers Available"));
	} else {
		g_assert (action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
		unavailable_message = g_strdup_printf (_("No applications are available for \"%s\"."), file_name);
		dialog_title = g_strdup (_("No Applications Available"));
	}

	/* Note: This might be misleading in the components case, since the
	 * user can't add components to the complete list even from the capplet.
	 * (They can add applications though.)
	 */
	prompt = g_strdup_printf (_("%s\n\n"
				    "You can configure which programs are offered "
				    "for which file types with the \"File Types and "
				    "Programs\" part of the GNOME Control Center. Do "
				    "you want to go there now?"),
				  unavailable_message);
	dialog = eel_show_yes_no_dialog 
		(prompt, dialog_title, GTK_STOCK_OK, GTK_STOCK_CANCEL, parent_window);

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (launch_mime_capplet_on_ok),
			  nautilus_file_get_mime_type (file));

	g_free (unavailable_message);
	g_free (file_name);
	g_free (prompt);
	g_free (dialog_title);
}
