/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   eel-open-with-dialog.c: an open-with dialog

   Copyright (C) 2004 Novell, Inc.
   Copyright (C) 2007 Red Hat, Inc.

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

   Authors: Dave Camp <dave@novell.com>
            Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-open-with-dialog.h"
#include "nautilus-signaller.h"

#include <eel/eel-stock-dialogs.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkexpander.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkvbox.h>
#include <gio/gio.h>

#define sure_string(s)                    ((const char *)((s)!=NULL?(s):""))
#define DESKTOP_ENTRY_GROUP		  "Desktop Entry"

struct _NautilusOpenWithDialogDetails {
	GAppInfo *selected_app_info;
	
	char *content_type;
	char *extension;
	char *type_description;

	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;

	GtkWidget *desc_label;

	GtkWidget *open_label;

	GtkWidget     *program_list;
	GtkListStore  *program_list_store;
	GSList	      *add_icon_paths;
	gint	       add_items_idle_id;
	gint	       add_icons_idle_id;
};

enum {
        COLUMN_APP_INFO,
        COLUMN_ICON,
        COLUMN_GICON,
        COLUMN_NAME,
        COLUMN_COMMENT,
        COLUMN_EXEC,
        NUM_COLUMNS
};

enum {
	RESPONSE_OPEN
};

enum {
	APPLICATION_SELECTED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

static void
nautilus_open_with_dialog_finalize (GObject *object)
{
	NautilusOpenWithDialog *dialog;

	dialog = NAUTILUS_OPEN_WITH_DIALOG (object);

	if (dialog->details->add_icons_idle_id) {
		g_source_remove (dialog->details->add_icons_idle_id);
	}

	if (dialog->details->add_items_idle_id) {
		g_source_remove (dialog->details->add_items_idle_id);
	}

	if (dialog->details->selected_app_info) {
		g_object_unref (dialog->details->selected_app_info);
	}
	g_free (dialog->details->content_type);
	g_free (dialog->details->extension);
	g_free (dialog->details->type_description);

	g_free (dialog->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_open_with_dialog_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/* An application is valid if:
 *
 * 1) The file exists
 * 2) The user has permissions to run the file
 */
static gboolean
check_application (NautilusOpenWithDialog *dialog)
{
	char *command;
	char *path = NULL;
	char **argv = NULL;
	int argc;
	GError *error = NULL;
	gint retval = TRUE;

	command = NULL;
	if (dialog->details->selected_app_info != NULL) {
		command = g_strdup (g_app_info_get_executable (dialog->details->selected_app_info));
	}

	if (command == NULL) {
		command = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->details->entry)));
	}
	
	g_shell_parse_argv (command, &argc, &argv, &error);
	if (error) {
		eel_show_error_dialog (_("Could not run application"),
				       error->message,
				       GTK_WINDOW (dialog));
		g_error_free (error);
		retval = FALSE;
		goto cleanup;
	}

	path = g_find_program_in_path (argv[0]);
	if (!path) {
		char *error_message;

		error_message = g_strdup_printf (_("Could not find '%s'"),
						 argv[0]);

		eel_show_error_dialog (_("Could not find application"),
				       error_message,
				       GTK_WINDOW (dialog));
		g_free (error_message);
		retval = FALSE;
		goto cleanup;
	}

 cleanup:
	g_strfreev (argv);
	g_free (path);
	g_free (command);

	return retval;
}

/* Only called for non-desktop files */
static char *
get_app_name (const char *commandline, GError **error)
{
	char *basename;
	char *unquoted;
	char **argv;
	int argc;

	if (!g_shell_parse_argv (commandline,
				 &argc, &argv, error)) {
		return NULL;
	}
	
	unquoted = g_shell_unquote (argv[0], NULL);
	if (unquoted) {
		basename = g_path_get_basename (unquoted);
	} else {
		basename = g_strdup (argv[0]);
	}

	g_free (unquoted);
	g_strfreev (argv);

	return basename;
}

/* This will check if the application the user wanted exists will return that
 * application.  If it doesn't exist, it will create one and return that.
 * It also sets the app info as the default for this type.
 */
static GAppInfo *
add_or_find_application (NautilusOpenWithDialog *dialog)
{
	GAppInfo *app;
	char *app_name;
	const char *commandline;
	GError *error;
	gboolean success;
	char *message;

	error = NULL;
	app = NULL;
	if (dialog->details->selected_app_info) {
		app = g_object_ref (dialog->details->selected_app_info);
	} else {
		commandline = gtk_entry_get_text (GTK_ENTRY (dialog->details->entry));
		app_name = get_app_name (commandline, &error);
		if (app_name != NULL) {
			app = g_app_info_create_from_commandline (commandline,
								  app_name,
								  G_APP_INFO_CREATE_NONE,
								  &error);
		}
	}

	if (app == NULL) {
		message = g_strdup_printf (_("Could not add application to the application database: %s"), error->message);
		eel_show_error_dialog (_("Could not add application"),
				       message,
				       GTK_WINDOW (dialog));
		g_free (message);
		g_error_free (error);
		return NULL;
	}


	if (dialog->details->content_type) {
		success = g_app_info_set_as_default_for_type (app,
							      dialog->details->content_type,
							      &error);
	} else {
		success = g_app_info_set_as_default_for_extension (app,
								   dialog->details->extension,
								   &error);
	}
	
	if (!success) {
		message = g_strdup_printf (_("Could not set application as the default: %s"), error->message);
		eel_show_error_dialog (_("Could not set as default application"),
				       message,
				       GTK_WINDOW (dialog));
		g_free (message);
		g_error_free (error);
	}

	g_signal_emit_by_name (nautilus_signaller_get_current (),
			       "mime_data_changed");

	return app;
}

static void
emit_application_selected (NautilusOpenWithDialog *dialog,
			   GAppInfo *application)
{
	g_signal_emit (G_OBJECT (dialog), signals[APPLICATION_SELECTED], 0,
		       application);
}

static void
response_cb (NautilusOpenWithDialog *dialog,
	     int response_id,
	     gpointer data)
{
	GAppInfo *application;

	switch (response_id) {
	case RESPONSE_OPEN:
		if (check_application (dialog)) {
			application = add_or_find_application (dialog);

			if (application) {
				emit_application_selected (dialog, application);
				g_object_unref (application);

				gtk_widget_destroy (GTK_WIDGET (dialog));
			}
		}

		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	default :
		g_assert_not_reached ();
	}

}


static void
nautilus_open_with_dialog_class_init (NautilusOpenWithDialogClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = nautilus_open_with_dialog_finalize;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = nautilus_open_with_dialog_destroy;

	signals[APPLICATION_SELECTED] =
		g_signal_new ("application_selected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusOpenWithDialogClass,
					       application_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
chooser_response_cb (GtkFileChooser *chooser,
		     int response,
		     gpointer user_data)
{
	NautilusOpenWithDialog *dialog;

	dialog = NAUTILUS_OPEN_WITH_DIALOG (user_data);

	if (response == GTK_RESPONSE_OK) {
		char *filename;

		filename = gtk_file_chooser_get_filename (chooser);

		if (filename) {
			char *quoted_text;

			quoted_text = g_shell_quote (filename);

			gtk_entry_set_text (GTK_ENTRY (dialog->details->entry),
					    quoted_text);
			gtk_entry_set_position (GTK_ENTRY (dialog->details->entry), -1);
			g_free (quoted_text);
			g_free (filename);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
browse_clicked_cb (GtkWidget *button,
		   gpointer user_data)
{
	NautilusOpenWithDialog *dialog;
	GtkWidget *chooser;

	dialog = NAUTILUS_OPEN_WITH_DIALOG (user_data);

	chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
					       GTK_WINDOW (dialog),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL,
					       GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN,
					       GTK_RESPONSE_OK,
					       NULL);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);
	g_signal_connect (chooser, "response",
			  G_CALLBACK (chooser_response_cb), dialog);
	gtk_dialog_set_default_response (GTK_DIALOG (chooser),
					 GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser),
					      FALSE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     "/usr/bin");

	gtk_widget_show (chooser);
}

static void
entry_changed_cb (GtkWidget *entry,
		  NautilusOpenWithDialog *dialog)
{
	/* We are writing in the entry, so we are not using a known appinfo anymore */
	if (dialog->details->selected_app_info != NULL) {
		g_object_unref (dialog->details->selected_app_info);
		dialog->details->selected_app_info = NULL;
	}

	if (gtk_entry_get_text (GTK_ENTRY (dialog->details->entry))[0] == '\000') {
		gtk_widget_set_sensitive (dialog->details->button, FALSE);
	} else {
		gtk_widget_set_sensitive (dialog->details->button, TRUE);
	}
}

static GdkPixbuf *
get_pixbuf_for_icon (GIcon *icon)
{
	GdkPixbuf  *pixbuf;
	char *filename;

	pixbuf = NULL;
	if (G_IS_FILE_ICON (icon)) {
		filename = g_file_get_path (g_file_icon_get_file (G_FILE_ICON (icon)));
		if (filename) {
			pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 24, 24, NULL);
		}
		g_free (filename);
	} else if (G_IS_THEMED_ICON (icon)) {
		const char * const *names;
		char *icon_no_extension;
		char *p;
		
		names = g_themed_icon_get_names (G_THEMED_ICON (icon));
		
		if (names != NULL && names[0] != NULL) {
			icon_no_extension = g_strdup (names[0]);
			p = strrchr (icon_no_extension, '.');
			if (p &&
			    (strcmp (p, ".png") == 0 ||
			     strcmp (p, ".xpm") == 0 ||
			     strcmp (p, ".svg") == 0)) {
				*p = 0;
			}
			pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
							   icon_no_extension, 24, 0, NULL);
			g_free (icon_no_extension);
		}
	}
	return pixbuf;
}

static gboolean
nautilus_open_with_dialog_add_icon_idle (NautilusOpenWithDialog *dialog)
{
	GtkTreeIter   iter;
	GtkTreePath  *path;
	GdkPixbuf    *pixbuf;
	GIcon *icon;
	gboolean      long_operation;

	long_operation = FALSE;
	do {
		if (!dialog->details->add_icon_paths) {
			dialog->details->add_icons_idle_id = 0;
			return FALSE;
		}

		path = dialog->details->add_icon_paths->data;
		dialog->details->add_icon_paths->data = NULL;
		dialog->details->add_icon_paths = g_slist_delete_link (dialog->details->add_icon_paths,
								       dialog->details->add_icon_paths);

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->details->program_list_store),
					      &iter, path)) {
			gtk_tree_path_free (path);
			continue;
		}
		
		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (dialog->details->program_list_store), &iter,
				    COLUMN_GICON, &icon, -1);

		if (icon == NULL) {
			continue;
		}

		pixbuf = get_pixbuf_for_icon (icon);
		if (pixbuf) {
			long_operation = TRUE;
			gtk_list_store_set (dialog->details->program_list_store, &iter, COLUMN_ICON, pixbuf, -1);
			g_object_unref (pixbuf);
		}
		
	/* don't go back into the main loop if this wasn't very hard to do */
	} while (!long_operation);

	return TRUE;
}


static gboolean
nautilus_open_with_search_equal_func (GtkTreeModel *model,
				 int column,
				 const char *key,
				 GtkTreeIter *iter,
				 gpointer user_data)
{
	char *normalized_key;
	char *name, *normalized_name;
	char *path, *normalized_path;
	char *basename, *normalized_basename;
	gboolean ret;

	if (key != NULL) {
		normalized_key = g_utf8_casefold (key, -1);
		g_assert (normalized_key != NULL);

		ret = TRUE;

		gtk_tree_model_get (model, iter,
				    COLUMN_NAME, &name,
				    COLUMN_EXEC, &path,
				    -1);

		if (name != NULL) {
			normalized_name = g_utf8_casefold (name, -1);
			g_assert (normalized_name != NULL);

			if (strncmp (normalized_name, normalized_key, strlen (normalized_key)) == 0) {
				ret = FALSE;
			}

			g_free (normalized_name);
		}

		if (ret && path != NULL) {
			normalized_path = g_utf8_casefold (path, -1);
			g_assert (normalized_path != NULL);

			basename = g_path_get_basename (path);
			g_assert (basename != NULL);

			normalized_basename = g_utf8_casefold (basename, -1);
			g_assert (normalized_basename != NULL);

			if (strncmp (normalized_path, normalized_key, strlen (normalized_key)) == 0 ||
			    strncmp (normalized_basename, normalized_key, strlen (normalized_key)) == 0) {
				ret = FALSE;
			}

			g_free (basename);
			g_free (normalized_basename);
			g_free (normalized_path);
		}

		g_free (name);
		g_free (path);
		g_free (normalized_key);

		return ret;
	} else {
		return TRUE;
	}
}



static gboolean
nautilus_open_with_dialog_add_items_idle (NautilusOpenWithDialog *dialog)
{
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GList            *all_applications;
	GList            *l;
	const char        *prev_name;

	/* create list store */
	dialog->details->program_list_store = gtk_list_store_new (NUM_COLUMNS,
								  G_TYPE_APP_INFO,
								  GDK_TYPE_PIXBUF,
								  G_TYPE_ICON,
								  G_TYPE_STRING,
								  G_TYPE_STRING,
								  G_TYPE_STRING);

	all_applications = g_app_info_get_all ();
	
	prev_name = NULL;
	for (l = all_applications; l; l = l->next) {
		GAppInfo *app = l->data;
		GtkTreeIter     iter;
		GtkTreePath    *path;

		if (!g_app_info_supports_uris (app) &&
		    !g_app_info_supports_files (app))
			continue;

		gtk_list_store_append (dialog->details->program_list_store, &iter);
		gtk_list_store_set (dialog->details->program_list_store, &iter,
				    COLUMN_APP_INFO,  app,
				    COLUMN_ICON,      NULL,
				    COLUMN_GICON,     g_app_info_get_icon (app),
				    COLUMN_NAME,      g_app_info_get_name (app),
				    COLUMN_COMMENT,   g_app_info_get_description (app),
				    COLUMN_EXEC,      g_app_info_get_executable,
				    -1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->details->program_list_store), &iter);
		if (path != NULL) {
			dialog->details->add_icon_paths = g_slist_prepend (dialog->details->add_icon_paths, path);
		}
	}
	g_list_free (all_applications);

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->details->program_list), 
				 GTK_TREE_MODEL (dialog->details->program_list_store));
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (dialog->details->program_list),
					     nautilus_open_with_search_equal_func,
					     NULL, NULL);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", COLUMN_ICON,
                                             NULL);
        
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", COLUMN_NAME,
                                             NULL);
					          
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->details->program_list), column);

	dialog->details->add_icon_paths = g_slist_reverse (dialog->details->add_icon_paths);

	if (!dialog->details->add_icons_idle_id) {
		dialog->details->add_icons_idle_id =
			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, (GSourceFunc) nautilus_open_with_dialog_add_icon_idle,
					 dialog, NULL);
	}

	dialog->details->add_items_idle_id = 0;					 
	return FALSE;
}

static void
program_list_selection_changed (GtkTreeSelection  *selection,
				NautilusOpenWithDialog *dialog)
{
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GAppInfo *info;
		
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (dialog->details->button, FALSE);
		return;
	}

	info = NULL;
	gtk_tree_model_get (model, &iter,
			    COLUMN_APP_INFO, &info,
			    -1);
				  
	if (info == NULL) {
		return;
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->details->entry),
			    sure_string (g_app_info_get_executable (info)));
	gtk_label_set_text (GTK_LABEL (dialog->details->desc_label),
			    sure_string (g_app_info_get_description (info)));
	gtk_widget_set_sensitive (dialog->details->button, TRUE);

	if (dialog->details->selected_app_info) {
		g_object_unref (dialog->details->selected_app_info);
	}
	
	dialog->details->selected_app_info = info;
}

static void
program_list_selection_activated (GtkTreeView       *view,
				  GtkTreePath       *path,
				  GtkTreeViewColumn *column,
				  NautilusOpenWithDialog *dialog)
{
	GtkTreeSelection *selection;

	/* update the entry with the info from the selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->details->program_list));	
	program_list_selection_changed (selection, dialog);
	
	gtk_dialog_response (GTK_DIALOG (&dialog->parent), RESPONSE_OPEN);
}

static void
expander_toggled (GtkWidget *expander, NautilusOpenWithDialog *dialog)
{
	if (gtk_expander_get_expanded (GTK_EXPANDER (expander)) == TRUE) {
		gtk_widget_grab_focus (dialog->details->entry);
		gtk_window_resize (GTK_WINDOW (dialog), 400, 1);
	} else {
		GtkTreeSelection *selection;

		gtk_widget_grab_focus (dialog->details->program_list);
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->details->program_list));
		program_list_selection_changed (selection, dialog);
	}
}

static void
nautilus_open_with_dialog_instance_init (NautilusOpenWithDialog *dialog)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *align;
	GtkWidget *scrolled_window;
	GtkWidget *expander;
	GtkTreeSelection *selection;

	dialog->details = g_new0 (NautilusOpenWithDialogDetails, 1);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Open With"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);

	dialog->details->label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (dialog->details->label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (dialog->details->label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox2), dialog->details->label,
			    FALSE, FALSE, 0);
	gtk_widget_show (dialog->details->label);


	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request (scrolled_window, 400, 300);
	
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	dialog->details->program_list = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->details->program_list),
					   FALSE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), dialog->details->program_list);
	
	gtk_box_pack_start (GTK_BOX (vbox2), scrolled_window, TRUE, TRUE, 0);

	dialog->details->desc_label = gtk_label_new (_("Select an application to view its description."));
	gtk_misc_set_alignment (GTK_MISC (dialog->details->desc_label), 0.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (dialog->details->desc_label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (dialog->details->desc_label), TRUE);
	gtk_label_set_single_line_mode (GTK_LABEL (dialog->details->desc_label), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox2), dialog->details->desc_label, FALSE, FALSE, 0);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->details->program_list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (program_list_selection_changed),
			  dialog);
	g_signal_connect (dialog->details->program_list, "row-activated",
			  G_CALLBACK (program_list_selection_activated),
			  dialog);

	dialog->details->add_items_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
						     (GSourceFunc) nautilus_open_with_dialog_add_items_idle,
						      dialog, NULL);

	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);


	expander = gtk_expander_new_with_mnemonic (_("_Use a custom command"));
	gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, FALSE, 0);
	g_signal_connect_after (expander, "activate", G_CALLBACK (expander_toggled), dialog);

	gtk_widget_show (expander);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (expander), hbox);
	gtk_widget_show (hbox);

	dialog->details->entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->entry), TRUE);

	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->entry,
			    TRUE, TRUE, 0);
	gtk_widget_show (dialog->details->entry);

	dialog->details->button = gtk_button_new_with_mnemonic (_("_Browse..."));
	g_signal_connect (dialog->details->button, "clicked",
			  G_CALLBACK (browse_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->button, FALSE, FALSE, 0);
	gtk_widget_show (dialog->details->button);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);


	/* Create a custom stock icon */
	dialog->details->button = gtk_button_new ();

	/* Hook up the entry to the button */
	gtk_widget_set_sensitive (dialog->details->button, FALSE);
	g_signal_connect (G_OBJECT (dialog->details->entry), "changed",
			  G_CALLBACK (entry_changed_cb), dialog);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (hbox);

	label = gtk_label_new_with_mnemonic (_("_Open"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (dialog->details->button));
	gtk_widget_show (label);
	dialog->details->open_label = label;

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (align);

	gtk_widget_show (dialog->details->button);
	GTK_WIDGET_SET_FLAGS (dialog->details->button, GTK_CAN_DEFAULT);


	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_container_add (GTK_CONTAINER (dialog->details->button), align);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      dialog->details->button, RESPONSE_OPEN);


	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 RESPONSE_OPEN);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_cb),
			  dialog);
}

static char *
get_extension (const char *basename)
{
	char *p;

	p = strrchr (basename, '.');

	if (p && *(p + 1) != '\0') {
		return g_strdup (p + 1);
	} else {
		return NULL;
	}
}

static void
set_uri_and_type (NautilusOpenWithDialog *dialog,
		  const char *uri,
		  const char *mime_type,
		  const char *passed_extension)
{
	char *label;
	char *emname;
	char *name, *extension;
	
	const char *description;
	
	name = NULL;
	extension = NULL;
	
	if (uri != NULL) {
		GFile *file;

		file = g_file_new_for_uri (uri);
		name = g_file_get_basename (file);
		g_object_unref (file);
	}
	if (passed_extension == NULL && name != NULL) {
		extension = get_extension (name);
	} else {
		extension = g_strdup (passed_extension);
	}

	if (extension != NULL &&
	    g_content_type_is_unknown (mime_type)) {
		dialog->details->extension = g_strdup (extension);
		/* the %s here is a file extension */
		dialog->details->type_description =
			g_strdup_printf (_("%s document"), extension);
	} else {
		dialog->details->content_type = g_strdup (mime_type);
		description = g_content_type_get_description (mime_type);

		if (description == NULL) {
			description = _("Unknown");
		}

		dialog->details->type_description = g_strdup (description);
	}
	g_free (extension);

	if (name != NULL) {
		emname = g_strdup_printf ("<i>%s</i>", name);
		label = g_strdup_printf (_("Open %s and other files of type \"%s\" with:"), emname, dialog->details->type_description);
		g_free (emname);
	} else {
		label = g_strdup_printf (_("Open all files of type \"%s\" with:"),
					 dialog->details->type_description);
	}

	gtk_label_set_markup (GTK_LABEL (dialog->details->label), label);

	g_free (label);
	g_free (name);
}

GtkWidget *
nautilus_open_with_dialog_new (const char *uri,
			       const char *mime_type,
			       const char *extension)
{
	GtkWidget *dialog;

	dialog = gtk_widget_new (NAUTILUS_TYPE_OPEN_WITH_DIALOG, NULL);

	set_uri_and_type (NAUTILUS_OPEN_WITH_DIALOG (dialog), uri, mime_type, extension);

	return dialog;
}

GtkWidget* 
nautilus_add_application_dialog_new (const char *uri,
				const char *mime_type)
{
	NautilusOpenWithDialog *dialog;
	
	dialog = NAUTILUS_OPEN_WITH_DIALOG (nautilus_open_with_dialog_new (uri, mime_type, NULL));
	
	gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->details->open_label),
					  _("_Add"));
	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Application"));

	return GTK_WIDGET (dialog);
}

GtkWidget* 
nautilus_add_application_dialog_new_for_multiple_files (const char *extension,
							const char *mime_type)
{
	NautilusOpenWithDialog *dialog;
	
	dialog = NAUTILUS_OPEN_WITH_DIALOG (nautilus_open_with_dialog_new (NULL, mime_type, extension));
	
	gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->details->open_label),
					  _("_Add"));
	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Application"));

	return GTK_WIDGET (dialog);
}

GType
nautilus_open_with_dialog_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusOpenWithDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)nautilus_open_with_dialog_class_init,
			NULL,
			NULL,
			sizeof (NautilusOpenWithDialog),
			0,
			(GInstanceInitFunc)nautilus_open_with_dialog_instance_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "NautilusOpenWithDialog",
					       &info, 0);
	}

	return type;
}
