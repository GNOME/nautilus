/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.c - implementation for window that lets user choose 
                                a program from a list

   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
   
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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
*/

#include <config.h>
#include "nautilus-program-chooser.h"

#include "nautilus-global-preferences.h"
#include "nautilus-mime-actions.h"
#include "nautilus-program-choosing.h"
#include "nautilus-view-identifier.h"
#include "nautilus-multihead-hacks.h"
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-help.h>

#define RESPONSE_CHOOSE 1000

enum {
	PROGRAM_LIST_NAME_COLUMN,
	PROGRAM_LIST_STATUS_COLUMN,
	PROGRAM_LIST_PROGRAM_PAIR_COLUMN,
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

struct NautilusProgramChooserDetails {
	GtkWidget *prompt_label;
	GtkWidget *frame;
	GtkWidget *status_label;

	GtkWidget *tree_view;
	GtkListStore *list_store;

	NautilusFile *file;
	GnomeVFSMimeActionType action_type;

	/* Buttons in the dialog box */
	GtkWidget *cancel_button;
	GtkWidget *done_button;
};

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
#define FILE_TYPES_CAPPLET_NAME 	"gnome-file-types-properties"

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

#define PROGRAM_FILE_PAIR_TYPE (program_file_pair_get_type ())

static GType program_file_pair_get_type (void);

GNOME_CLASS_BOILERPLATE (NautilusProgramChooser, nautilus_program_chooser,
			 GtkDialog, GTK_TYPE_DIALOG);

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
help_cb (GtkWidget *button, NautilusProgramChooser *program_chooser)
{
	GError *error = NULL;
	gchar *section;

	switch (program_chooser->details->action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		section = "gosnautilus-75";
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
	default:
		section = "gosnautilus-111";
		break;
	}

	gnome_help_display_desktop (NULL,
				    "user-guide",
				    "wgosnautilus.xml",
				    section,
				    &error);

	if (error) {
		GtkWidget *err_dialog;
		err_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (button)),
						     GTK_DIALOG_MODAL,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     _("There was an error displaying help: %s"),
						     error->message);

		g_signal_connect (G_OBJECT (err_dialog),
				  "response", G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (err_dialog), FALSE);
		gtk_widget_show (err_dialog);
		g_error_free (error);
	}
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

static ProgramFilePair *
program_file_pair_copy (const ProgramFilePair *pair)
{
	ProgramFilePair *new_pair;

	new_pair = g_new0 (ProgramFilePair, 1);
	new_pair->view_identifier = nautilus_view_identifier_copy (pair->view_identifier);
	new_pair->application = gnome_vfs_mime_application_copy (pair->application);
	new_pair->action_type = pair->action_type;

	program_file_pair_set_file (new_pair, pair->file);

	new_pair->status = pair->status;

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

/* Boxed type, for use in GtkListStore */
static GType
program_file_pair_get_type (void)
{
	static GType type = 0;

	program_file_pair_get_type ();
	
	if (type == 0) {
		type = g_boxed_type_register_static ("NautilusProgramFilePair",
						     (GBoxedCopyFunc)program_file_pair_copy,
						     (GBoxedFreeFunc)program_file_pair_free);
	}

	return type;

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

static void
repopulate_program_list (NautilusProgramChooser *program_chooser)
{
	GList *programs, *program;
	ProgramFilePair *pair;
	GnomeVFSMimeActionType type;
	GtkListStore *list_store;
	gchar *program_name, *status_text;
	GtkTreeIter iter;
	GtkTreePath *path;
	
	type = program_chooser->details->action_type;

	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		  || type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
	

	programs = type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT
		? nautilus_mime_get_all_components_for_file (program_chooser->details->file)
		: nautilus_mime_get_all_applications_for_file (program_chooser->details->file);

	list_store = program_chooser->details->list_store;
	gtk_list_store_clear (list_store);
		
	for (program = programs; program != NULL; program = program->next) {

		if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
			pair = program_file_pair_new_from_content_view
				((Bonobo_ServerInfo *)program->data, program_chooser->details->file);
		} else {
			pair = program_file_pair_new_from_application
				((GnomeVFSMimeApplication *)program->data, program_chooser->details->file);
		}

		program_name = program_file_pair_get_program_name_for_display (pair);
		status_text = program_file_pair_get_short_status_text (pair);
		
		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (list_store, &iter,
				    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, pair,
				    PROGRAM_LIST_NAME_COLUMN, program_name,
				    PROGRAM_LIST_STATUS_COLUMN, status_text,
				    -1);
		g_free (program_name);
		g_free (status_text);
	}

	if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		gnome_vfs_mime_component_list_free (programs);
	} else {
		gnome_vfs_mime_application_list_free (programs);
	}
	
	/* Start with first item selected, rather than some arbitrary item */
	path = gtk_tree_path_new_root ();
	gtk_tree_selection_select_path (
		gtk_tree_view_get_selection (GTK_TREE_VIEW (program_chooser->details->tree_view)),
		path);
	gtk_tree_path_free (path);
}

static void
nautilus_program_chooser_set_is_cancellable (NautilusProgramChooser *program_chooser, gboolean cancellable)
{
	if (cancellable) {
		gtk_widget_hide (program_chooser->details->done_button);
		gtk_widget_show (program_chooser->details->cancel_button);
	} else {
		gtk_widget_hide (program_chooser->details->cancel_button);
		gtk_widget_show (program_chooser->details->done_button);
	}
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
get_selected_program_file_pair (NautilusProgramChooser *program_chooser)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gboolean selected;
	GValue value = { 0 };
	ProgramFilePair *pair;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (program_chooser->details->tree_view));	
	selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	if (selected == FALSE) {
		return NULL;
	}

	gtk_tree_model_get_value (GTK_TREE_MODEL (program_chooser->details->list_store),
				  &iter, PROGRAM_LIST_PROGRAM_PAIR_COLUMN,
				  &value);

	pair = g_value_get_pointer (&value);
	g_value_unset (&value);
	
	return pair;
}

static void
update_selected_item_details (NautilusProgramChooser *program_chooser)
{
	char *frame_label_text, *status_label_text;
	ProgramFilePair *pair;

	pair = get_selected_program_file_pair (program_chooser);

	if (pair != NULL) {
		frame_label_text = program_file_pair_get_program_name_for_display (pair);
		status_label_text = program_file_pair_get_long_status_text (pair);
	} else {
		frame_label_text = NULL;
		status_label_text = NULL;
	}

	gtk_frame_set_label (GTK_FRAME (program_chooser->details->frame), frame_label_text);
	gtk_label_set_text (GTK_LABEL (program_chooser->details->status_label), status_label_text);

	g_free (frame_label_text);
	g_free (status_label_text);
}

static void
update_all_status (NautilusProgramChooser *program_chooser)
{
	ProgramFilePair *pair;
	GtkTreeIter iter;
	GtkTreeModel *model;
	char *status_text;
	gboolean found;
	gboolean anything_changed;
	
	anything_changed = FALSE;
	model = GTK_TREE_MODEL (program_chooser->details->list_store);

	found = gtk_tree_model_get_iter_root (model, &iter);

	while (found) {
		gtk_tree_model_get (model, &iter,
				    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair,
				    -1);

		if (program_file_pair_compute_status (pair)) {
			/* Status has changed, update text in list */
			anything_changed = TRUE;
			status_text = program_file_pair_get_short_status_text (pair);

			gtk_list_store_set (GTK_LIST_STORE (model),
					    &iter,
					    PROGRAM_LIST_STATUS_COLUMN, status_text,
					    -1);
			g_free (status_text);
		}

		found = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
program_list_selection_changed_callback (GtkTreeSelection *selection,
					 gpointer user_data)
{
	update_selected_item_details (NAUTILUS_PROGRAM_CHOOSER (user_data));
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
launch_mime_capplet (NautilusFile *file,
		     GtkDialog    *parent_dialog)
{
	GdkScreen *screen;
	char *command, *tmp, *mime_type, *file_name;

	screen = gtk_window_get_screen (GTK_WINDOW (parent_dialog));

	tmp = nautilus_file_get_mime_type (file);
	mime_type = g_shell_quote (tmp);
	g_free (tmp);
	tmp = nautilus_file_get_name (file);
	file_name = g_shell_quote (tmp);
	g_free (tmp);

	command = g_strconcat (FILE_TYPES_CAPPLET_NAME, " ", mime_type, " ", file_name, NULL);
	nautilus_launch_application_from_command (screen, FILE_TYPES_CAPPLET_NAME, command, NULL, FALSE);

	g_free (command);
	g_free (file_name);
	g_free (mime_type);
}

static void
launch_mime_capplet_on_ok (GtkDialog *dialog, int response, gpointer callback_data)
{
	g_assert (GTK_IS_DIALOG (dialog));

	if (response == GTK_RESPONSE_YES) {
		launch_mime_capplet (callback_data, dialog);
	}
	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
launch_mime_capplet_and_close_dialog (GtkButton *button, gpointer callback_data)
{
	ProgramFilePair *file_pair;

	g_assert (GTK_IS_BUTTON (button));

	file_pair = get_selected_program_file_pair (NAUTILUS_PROGRAM_CHOOSER (callback_data));
 	launch_mime_capplet (file_pair->file, GTK_DIALOG (callback_data));
	gtk_dialog_response (GTK_DIALOG (callback_data),
		GTK_RESPONSE_DELETE_EVENT);
}

static void
run_program_configurator_callback (GtkWidget *button, gpointer callback_data)
{
	NautilusProgramChooser *program_chooser;
	NautilusFile *file;
	GtkWidget *dialog;
	GtkWidget *radio_buttons_frame, *framed_vbox;
	GtkRadioButton *type_radio_button, *type_default_radio_button, *item_radio_button, *item_default_radio_button, *none_radio_button;
	GtkRadioButton *old_active_button;
	char *radio_button_text;
	char *file_type, *file_name;
	char *program_display_name;
	char *title;
	ProgramFilePair *pair;

	program_chooser = NAUTILUS_PROGRAM_CHOOSER (callback_data);
	
	file = program_chooser->details->file;

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
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
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
	radio_button_text = g_strdup_printf (_("Include in the menu for \"%s\" only"), 
					     file_name);
	item_radio_button = pack_radio_button (GTK_BOX (framed_vbox), radio_button_text, type_radio_button);
	g_free (radio_button_text);

	/* Radio button for setting default for specific file. */
	radio_button_text = g_strdup_printf (_("Use as default for \"%s\" only"), 
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
name_column_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	ProgramFilePair *pair1, *pair2;
	char *name1, *name2;
	gint result;

	gtk_tree_model_get (model, a,
			    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair1,
			    -1);

	gtk_tree_model_get (model, b,
			    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair2,
			    -1);

	name1 = program_file_pair_get_program_name_for_display (pair1);
	name2 = program_file_pair_get_program_name_for_display (pair2);
	result = g_ascii_strcasecmp (name1, name2);
	g_free (name1);
	g_free (name2);

	return result;
}

static int
status_column_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	ProgramFilePair *pair1, *pair2;
	gint result;
	
	gtk_tree_model_get (model, a,
			    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair1,
			    -1);
	
	gtk_tree_model_get (model, b,
			    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair2,
			    -1);
	
	if (pair1->status > pair2->status) {
		result = -1;
	} else if (pair1->status < pair2->status) {
		result = +1;
	} else {
		result = 0;
	}

	return result;
}

static void
tree_view_row_activated_callback (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, NautilusProgramChooser *program_chooser)
{
	gtk_dialog_response (GTK_DIALOG (program_chooser), GTK_RESPONSE_OK);
}

static void
create_and_set_up_tree_view (NautilusProgramChooser *program_chooser)
{
	GtkTreeViewColumn *column;
	
	program_chooser->details->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (program_chooser->details->list_store));
	gtk_widget_show (program_chooser->details->tree_view);

	column = gtk_tree_view_column_new_with_attributes (_("Name"),
							   gtk_cell_renderer_text_new (),
							   "text", PROGRAM_LIST_NAME_COLUMN,
							   NULL);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, NAME_COLUMN_INITIAL_WIDTH);
	gtk_tree_view_column_set_sort_column_id (column, PROGRAM_LIST_NAME_COLUMN);	
	gtk_tree_view_append_column (GTK_TREE_VIEW (program_chooser->details->tree_view), column);

	column = gtk_tree_view_column_new_with_attributes (_("Status"),
							   gtk_cell_renderer_text_new (),
							   "text", PROGRAM_LIST_STATUS_COLUMN,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, PROGRAM_LIST_STATUS_COLUMN);
	gtk_tree_view_append_column (GTK_TREE_VIEW (program_chooser->details->tree_view), column);

	/* Update selected item info whenever selection changes. */
  	g_signal_connect_object (gtk_tree_view_get_selection (GTK_TREE_VIEW (program_chooser->details->tree_view)),
				 "changed",
				 G_CALLBACK (program_list_selection_changed_callback), program_chooser, 0);

	g_signal_connect_object (program_chooser->details->tree_view, "row_activated",
				 G_CALLBACK (tree_view_row_activated_callback), program_chooser, 0);
}

static gboolean
tree_model_destroy_program_file_pair (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	ProgramFilePair *pair;

	gtk_tree_model_get (model, iter,
			    PROGRAM_LIST_PROGRAM_PAIR_COLUMN, &pair,
			    -1);

	/* We don't zero out the pointer in the tree view
	 * because this funcion will only be called at
	 * finalize time anyway.
	 */
	program_file_pair_free (pair);

	return FALSE;
}

static void
nautilus_program_chooser_finalize (GObject *object)
{
	NautilusProgramChooser *program_chooser;

	program_chooser = NAUTILUS_PROGRAM_CHOOSER (object);

	/* Free the ProgramFilePairs */
	gtk_tree_model_foreach (GTK_TREE_MODEL (program_chooser->details->list_store),
				tree_model_destroy_program_file_pair, NULL);
	g_object_unref (program_chooser->details->list_store);

	nautilus_file_unref (program_chooser->details->file);

	g_free (program_chooser->details);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_program_chooser_class_init (NautilusProgramChooserClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = (GObjectClass *)klass;

	gobject_class->finalize = nautilus_program_chooser_finalize;
}

static void
nautilus_program_chooser_instance_init (NautilusProgramChooser *program_chooser)
{
	GtkWidget *dialog_vbox, *scrolled_window;
	GtkWidget *framed_hbox;
	GtkWidget *help_button;
	GtkWidget *change_button_holder, *change_button;
	GtkWidget *capplet_button_frame, *capplet_hbox;
	GtkWidget *capplet_button, *caption, *capplet_button_vbox;
	
	program_chooser->details = g_new0 (NautilusProgramChooserDetails, 1);

	/* This is a slight hack - we add our own help button to the dialog's
	   button box. We don't want it to go through the normal response
	   callback. */
	help_button = gtk_button_new_from_stock (GTK_STOCK_HELP);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (program_chooser)->action_area),
			  help_button, FALSE, TRUE, 0);
	gtk_widget_show (help_button);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (program_chooser)->action_area), help_button, TRUE);

	g_signal_connect_object (help_button, "clicked",
				 G_CALLBACK (help_cb), program_chooser, 0);

	program_chooser->details->cancel_button = gtk_dialog_add_button (GTK_DIALOG (program_chooser),
									 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (GTK_DIALOG (program_chooser),
			       _("C_hoose"), GTK_RESPONSE_OK);

	program_chooser->details->done_button = gtk_dialog_add_button (GTK_DIALOG (program_chooser),
								       _("Done"), GTK_RESPONSE_CANCEL);

	gtk_container_set_border_width (GTK_CONTAINER (program_chooser), GNOME_PAD);

	gtk_window_set_resizable (GTK_WINDOW (program_chooser), TRUE);
	gtk_window_set_default_size (GTK_WINDOW (program_chooser), -1, PROGRAM_CHOOSER_DEFAULT_HEIGHT);
	gtk_window_set_wmclass (GTK_WINDOW (program_chooser), "program_chooser", "Nautilus");

	dialog_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (program_chooser)->vbox),
			    dialog_vbox, TRUE, TRUE, 5);
	gtk_widget_show (dialog_vbox);

	/* Prompt at top of dialog. */
	program_chooser->details->prompt_label = gtk_label_new (NULL);
	gtk_widget_show (program_chooser->details->prompt_label);
	/* Move prompt to left edge */
	gtk_misc_set_alignment (GTK_MISC (program_chooser->details->prompt_label), 0, 0.5);

	gtk_box_pack_start (GTK_BOX (dialog_vbox), program_chooser->details->prompt_label, FALSE, FALSE, 0);

	/* Create scrolled window */
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), 
					GTK_POLICY_NEVER, 
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_widget_show (scrolled_window);

	gtk_box_pack_start_defaults (GTK_BOX (dialog_vbox), scrolled_window);

	/* Create list store */
	program_chooser->details->list_store = gtk_list_store_new (PROGRAM_LIST_COLUMN_COUNT,
								   G_TYPE_STRING, /* PROGRAM_LIST_NAME_COLUMN */
								   G_TYPE_STRING, /* PROGRAM_LIST_STATUS_COLUMN */
								   G_TYPE_POINTER /* PROGRAM_LIST_PROGRAM_PAIR_COLUMN */);

	/* Set up sort functions */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (program_chooser->details->list_store),
					 PROGRAM_LIST_NAME_COLUMN, name_column_sort_func,
					 NULL, NULL);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (program_chooser->details->list_store),
					 PROGRAM_LIST_STATUS_COLUMN, status_column_sort_func,
					 NULL, NULL);

	/* Sort ascending by name */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (program_chooser->details->list_store),
					      PROGRAM_LIST_NAME_COLUMN, GTK_SORT_ASCENDING);
	
	/* Create and setup tree view */
	create_and_set_up_tree_view (program_chooser);
	gtk_container_add (GTK_CONTAINER (scrolled_window), program_chooser->details->tree_view);

	/* Framed area with selection-specific details */
	program_chooser->details->frame = gtk_frame_new (NULL);
	gtk_widget_show (program_chooser->details->frame);
  	gtk_box_pack_start (GTK_BOX (dialog_vbox), program_chooser->details->frame, FALSE, FALSE, 0);

	framed_hbox = gtk_hbox_new (FALSE, GNOME_PAD);
  	gtk_widget_show (framed_hbox);
  	gtk_container_add (GTK_CONTAINER (program_chooser->details->frame), framed_hbox);
  	gtk_container_set_border_width (GTK_CONTAINER (framed_hbox), GNOME_PAD);

	program_chooser->details->status_label = gtk_label_new (NULL);
  	gtk_label_set_justify (GTK_LABEL (program_chooser->details->status_label), GTK_JUSTIFY_LEFT);
  	gtk_widget_show (program_chooser->details->status_label);
  	gtk_box_pack_start (GTK_BOX (framed_hbox), program_chooser->details->status_label, FALSE, FALSE, 0);

	change_button_holder = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (change_button_holder);
  	gtk_box_pack_end (GTK_BOX (framed_hbox), change_button_holder, FALSE, FALSE, 0);

  	change_button = gtk_button_new_with_mnemonic (_("_Modify..."));

	g_signal_connect_object (change_button, "clicked",
				 G_CALLBACK (run_program_configurator_callback), program_chooser, 0);

  	gtk_widget_show (change_button);
  	gtk_box_pack_end (GTK_BOX (change_button_holder), change_button, TRUE, FALSE, 0);

	/* Framed area with button to launch mime type editing capplet. */
	capplet_button_frame = gtk_frame_new (_("File Types and Programs"));
	gtk_widget_show (capplet_button_frame);
  	gtk_box_pack_start (GTK_BOX (dialog_vbox), capplet_button_frame, FALSE, FALSE, 0);

  	capplet_hbox = gtk_hbox_new (FALSE, GNOME_PAD_BIG);
  	gtk_widget_show (capplet_hbox);
  	gtk_container_add (GTK_CONTAINER (capplet_button_frame), capplet_hbox);
  	gtk_container_set_border_width (GTK_CONTAINER (capplet_hbox), GNOME_PAD);

	capplet_button_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (capplet_button_vbox);
	gtk_box_pack_end (GTK_BOX (capplet_hbox), capplet_button_vbox, FALSE, FALSE, 0);
	capplet_button = gtk_button_new_with_mnemonic (_("_Go There"));	 

	g_signal_connect_object (capplet_button, "clicked",
				 G_CALLBACK (launch_mime_capplet_and_close_dialog), program_chooser, 0);
	gtk_widget_show (capplet_button);
	gtk_box_pack_start (GTK_BOX (capplet_button_vbox), capplet_button, TRUE, FALSE, 0);

	caption = gtk_label_new (_("You can configure which programs are offered "
				   "for which file types in the File Types and Programs dialog."));
	gtk_widget_show (caption);
	gtk_label_set_line_wrap (GTK_LABEL (caption), TRUE);
	gtk_box_pack_start (GTK_BOX (capplet_hbox), caption, FALSE, FALSE, 0);				    

  	/* Make confirmation button the default. */
  	gtk_dialog_set_default_response (GTK_DIALOG (program_chooser), GTK_RESPONSE_OK);

	/* We don't need the separator as we use frames. */
  	gtk_dialog_set_has_separator (GTK_DIALOG (program_chooser), FALSE);
}


GtkWidget *
nautilus_program_chooser_new (GnomeVFSMimeActionType action_type,
			      NautilusFile *file)
{
	NautilusProgramChooser *program_chooser;
	char *file_name, *prompt;
	const char *title;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	/* Create the program chooser */
	program_chooser = g_object_new (NAUTILUS_TYPE_PROGRAM_CHOOSER, NULL);

	program_chooser->details->action_type = action_type;
	program_chooser->details->file = nautilus_file_ref (file);

	file_name = get_file_name_for_display (file);

	switch (action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		title = _("Open with Other Application");
		prompt = g_strdup_printf (_("Choose an application with which to open \"%s\":"), file_name);
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
	default:
		title = _("Open with Other Viewer");
		prompt = g_strdup_printf (_("Choose a view for \"%s\":"), file_name);
		break;
	}

	g_free (file_name);
	
	gtk_window_set_title (GTK_WINDOW (program_chooser), title);
	gtk_label_set_text (GTK_LABEL (program_chooser->details->prompt_label), prompt);
	
	nautilus_program_chooser_set_is_cancellable (program_chooser, TRUE);

  	g_free (prompt);

	repopulate_program_list (program_chooser);

  	return GTK_WIDGET (program_chooser);
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
nautilus_program_chooser_get_application (NautilusProgramChooser *program_chooser)
{
	ProgramFilePair *pair;

	g_return_val_if_fail (GTK_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (program_chooser->details->action_type
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
nautilus_program_chooser_get_component (NautilusProgramChooser *program_chooser)
{
	ProgramFilePair *pair;

	g_return_val_if_fail (GTK_IS_DIALOG (program_chooser), NULL);

	g_return_val_if_fail (program_chooser->details->action_type
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
		unavailable_message = g_strdup_printf (_("There is no application associated with \"%s\"."), file_name);
		dialog_title = g_strdup (_("No Application Associated"));
	}

	/* Note: This might be misleading in the components case, since the
	 * user can't add components to the complete list even from the capplet.
	 * (They can add applications though.)
	 */
	prompt = g_strdup_printf (_("%s\n\n"
				    "You can configure GNOME to associate applications "
				    "with file types.  Do you want to associate an "
				    "application with this file type now?"),
				  unavailable_message);
	dialog = eel_show_yes_no_dialog 
		(prompt, dialog_title, _("Associate Application"), GTK_STOCK_CANCEL, parent_window);

	g_signal_connect_object (dialog, "response",
		G_CALLBACK (launch_mime_capplet_on_ok),
		file, 0);

	g_free (unavailable_message);
	g_free (file_name);
	g_free (prompt);
	g_free (dialog_title);
}
