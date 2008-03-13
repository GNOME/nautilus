/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nautilus-mime-application-chooser.c: an mime-application chooser
 
   Copyright (C) 2004 Novell, Inc.
   Copyright (C) 2007 Red Hat, Inc.
 
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dave Camp <dave@novell.com>
            Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-mime-application-chooser.h"

#include "nautilus-open-with-dialog.h"
#include "nautilus-signaller.h"
#include "nautilus-file.h"
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkalignment.h> 
#include <gtk/gtkbox.h> 
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkvbox.h>
#include <gio/gio.h>

struct _NautilusMimeApplicationChooserDetails {
	char *uri;

	char *content_type;
	char *extension;
	char *type_description;
	char *orig_mime_type;
	
	guint refresh_timeout;

	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *treeview;
	GtkWidget *remove_button;
	
	gboolean for_multiple_files;

	GtkListStore *model;
	GtkCellRenderer *toggle_renderer;
};

enum {
	COLUMN_APPINFO,
	COLUMN_DEFAULT,
	COLUMN_ICON,
	COLUMN_NAME,
	NUM_COLUMNS
};

static void refresh_model             (NautilusMimeApplicationChooser *chooser);
static void refresh_model_soon        (NautilusMimeApplicationChooser *chooser);
static void mime_type_data_changed_cb (GObject                        *signaller,
				       gpointer                        user_data);

static gpointer parent_class;

static void
nautilus_mime_application_chooser_finalize (GObject *object)
{
	NautilusMimeApplicationChooser *chooser;

	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (object);

	if (chooser->details->refresh_timeout) {
		g_source_remove (chooser->details->refresh_timeout);
	}

	g_signal_handlers_disconnect_by_func (nautilus_signaller_get_current (),
					      G_CALLBACK (mime_type_data_changed_cb),
					      chooser);
	
	
	g_free (chooser->details->uri);
	g_free (chooser->details->content_type);
	g_free (chooser->details->extension);
	g_free (chooser->details->type_description);
	g_free (chooser->details->orig_mime_type);
	
	g_free (chooser->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_mime_application_chooser_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
nautilus_mime_application_chooser_class_init (NautilusMimeApplicationChooserClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = nautilus_mime_application_chooser_finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = nautilus_mime_application_chooser_destroy;
}

static void
default_toggled_cb (GtkCellRendererToggle *renderer,
		    const char *path_str,
		    gpointer user_data)
{
	NautilusMimeApplicationChooser *chooser;
	GtkTreeIter iter;
	GtkTreePath *path;
	GError *error;
	
	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);
	
	path = gtk_tree_path_new_from_string (path_str);
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->details->model),
				     &iter, path)) {
		gboolean is_default;
		gboolean success;
		GAppInfo *info;
		char *message;
		
		gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->model),
				    &iter,
				    COLUMN_DEFAULT, &is_default,
				    COLUMN_APPINFO, &info,
				    -1);
		
		if (!is_default && info != NULL) {
			error = NULL;
			if (chooser->details->extension) {
				success = g_app_info_set_as_default_for_extension (info,
										   chooser->details->extension,
										   &error);
			} else {
				success = g_app_info_set_as_default_for_type (info,
									      chooser->details->content_type,
									      &error);
			}

			if (!success) {
				message = g_strdup_printf (_("Could not set application as the default: %s"), error->message);
				eel_show_error_dialog (_("Could not set as default application"),
						       message,
						       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
				g_free (message);
				g_error_free (error);
			}

			g_signal_emit_by_name (nautilus_signaller_get_current (),
					       "mime_data_changed");
		}
		g_object_unref (info);
	}
	gtk_tree_path_free (path);
}

static GAppInfo *
get_selected_application (NautilusMimeApplicationChooser *chooser)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GAppInfo *info;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->details->treeview));

	info = NULL;
	if (gtk_tree_selection_get_selected (selection, 
					     NULL,
					     &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->model),
				    &iter,
				    COLUMN_APPINFO, &info,
				    -1);
	}
	
	return info;
}

static void
selection_changed_cb (GtkTreeSelection *selection, 
		      gpointer user_data)
{
	NautilusMimeApplicationChooser *chooser;
	GAppInfo *info;
	
	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);
	
	info = get_selected_application (chooser);
	if (info) {
		gtk_widget_set_sensitive (chooser->details->remove_button,
					  g_app_info_can_remove_supports_type (info));
		
		g_object_unref (info);
	} else {
		gtk_widget_set_sensitive (chooser->details->remove_button,
					  FALSE);
	}
}

static GtkWidget *
create_tree_view (NautilusMimeApplicationChooser *chooser)
{
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	
	treeview = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	
	store = gtk_list_store_new (NUM_COLUMNS,
				    G_TYPE_APP_INFO,
				    G_TYPE_BOOLEAN,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
				 GTK_TREE_MODEL (store));
	chooser->details->model = store;
	
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", 
			  G_CALLBACK (default_toggled_cb), 
			  chooser);
	gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (renderer),
					    TRUE);
	
	column = gtk_tree_view_column_new_with_attributes (_("Default"),
							   renderer,
							   "active",
							   COLUMN_DEFAULT,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Icon"),
							   renderer,
							   "pixbuf",
							   COLUMN_ICON,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	chooser->details->toggle_renderer = renderer;
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Name"),
							   renderer,
							   "markup",
							   COLUMN_NAME,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	g_signal_connect (selection, "changed", 
			  G_CALLBACK (selection_changed_cb), 
			  chooser);	

	return treeview;
}

static void
add_clicked_cb (GtkButton *button,
		gpointer user_data)
{
	NautilusMimeApplicationChooser *chooser;
	GtkWidget *dialog;
	
	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);
	
	if (chooser->details->for_multiple_files) {
		dialog = nautilus_add_application_dialog_new_for_multiple_files (chooser->details->extension,
										 chooser->details->orig_mime_type);
	} else {
		dialog = nautilus_add_application_dialog_new (chooser->details->uri,
							      chooser->details->orig_mime_type);
	}
	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (chooser)));
	gtk_widget_show (dialog);
}

static void
remove_clicked_cb (GtkButton *button, 
		   gpointer user_data)
{
	NautilusMimeApplicationChooser *chooser;
	GError *error;
	GAppInfo *info;
	
	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);
	
	info = get_selected_application (chooser);

	if (info) {
		error = NULL;
		if (!g_app_info_remove_supports_type (info,
						      chooser->details->content_type,
						      &error)) {
			eel_show_error_dialog (_("Could not remove application"),
					       error->message,
					       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
			g_error_free (error);
			
		}
		g_signal_emit_by_name (nautilus_signaller_get_current (),
				       "mime_data_changed");
		g_object_unref (info);
	}
}

static void
mime_type_data_changed_cb (GObject *signaller,
			   gpointer user_data)
{
	NautilusMimeApplicationChooser *chooser;

	chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);

	refresh_model_soon (chooser);
}

static void
nautilus_mime_application_chooser_instance_init (NautilusMimeApplicationChooser *chooser)
{
	GtkWidget *box;
	GtkWidget *scrolled;
	GtkWidget *button;
	
	chooser->details = g_new0 (NautilusMimeApplicationChooserDetails, 1);

	chooser->details->for_multiple_files = FALSE;
	gtk_container_set_border_width (GTK_CONTAINER (chooser), 8);
	gtk_box_set_spacing (GTK_BOX (chooser), 0);
	gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);

	chooser->details->label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (chooser->details->label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (chooser->details->label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (chooser->details->label),
				      PANGO_WRAP_WORD_CHAR);
	gtk_box_pack_start (GTK_BOX (chooser), chooser->details->label, 
			    FALSE, FALSE, 0);

	gtk_widget_show (chooser->details->label);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_IN);
	
	gtk_widget_show (scrolled);
	gtk_box_pack_start (GTK_BOX (chooser), scrolled, TRUE, TRUE, 6);

	chooser->details->treeview = create_tree_view (chooser);
	gtk_widget_show (chooser->details->treeview);
	
	gtk_container_add (GTK_CONTAINER (scrolled), 
			   chooser->details->treeview);

	box = gtk_hbutton_box_new ();
	gtk_box_set_spacing (GTK_BOX (box), 6);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (chooser), box, FALSE, FALSE, 6);
	gtk_widget_show (box);

	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (add_clicked_cb),
			  chooser);

	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (box), button);

	button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (remove_clicked_cb),
			  chooser);
	
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (box), button);
	
	chooser->details->remove_button = button;

	g_signal_connect (nautilus_signaller_get_current (),
			  "mime_data_changed",
			  G_CALLBACK (mime_type_data_changed_cb),
			  chooser);
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
refresh_model_timeout (gpointer data)
{
	NautilusMimeApplicationChooser *chooser = data;
	
	chooser->details->refresh_timeout = 0;

	refresh_model (chooser);

	return FALSE;
}

/* This adds a slight delay so that we're sure the mime data is
   done writing */
static void
refresh_model_soon (NautilusMimeApplicationChooser *chooser)
{
	if (chooser->details->refresh_timeout != 0)
		return;

	chooser->details->refresh_timeout =
		g_timeout_add (300,
			       refresh_model_timeout,
			       chooser);
}

static void
refresh_model (NautilusMimeApplicationChooser *chooser)
{
	GList *applications;
	GAppInfo *default_app;
	GList *l;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (chooser->details->treeview), 0);
	gtk_tree_view_column_set_visible (column, TRUE);
	
	gtk_list_store_clear (chooser->details->model);

	applications = g_app_info_get_all_for_type (chooser->details->content_type);
	default_app = g_app_info_get_default_for_type (chooser->details->content_type, FALSE);
	
	for (l = applications; l != NULL; l = l->next) {
		GtkTreeIter iter;
		gboolean is_default;
		GAppInfo *application;
		char *escaped;
		GIcon *icon;
		GdkPixbuf *pixbuf;

		pixbuf = NULL;

		application = l->data;
		
		is_default = default_app && g_app_info_equal (default_app, application);

		escaped = g_markup_escape_text (g_app_info_get_name (application), -1);

		icon = g_app_info_get_icon (application);

		if (icon != NULL) {
			pixbuf = get_pixbuf_for_icon (icon);
		}

		gtk_list_store_append (chooser->details->model, &iter);
		gtk_list_store_set (chooser->details->model, &iter,
				    COLUMN_APPINFO, application,
				    COLUMN_DEFAULT, is_default,
				    COLUMN_ICON, pixbuf,
				    COLUMN_NAME, escaped,
				    -1);

		g_free (escaped);
		if (pixbuf != NULL) {
			g_object_unref (pixbuf);
		}
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->details->treeview));
	
	if (applications) {
		g_object_set (chooser->details->toggle_renderer,
			      "visible", TRUE, 
			      NULL);
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	} else {
		GtkTreeIter iter;
		char *name;

		gtk_tree_view_column_set_visible (column, FALSE);
		gtk_list_store_append (chooser->details->model, &iter);
		name = g_strdup_printf ("<i>%s</i>", _("No applications selected"));
		gtk_list_store_set (chooser->details->model, &iter,
				    COLUMN_NAME, name,
				    COLUMN_APPINFO, NULL,
				    -1);
		g_free (name);

		gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
	}
	
	if (default_app) {
		g_object_unref (default_app);
	}
	
	eel_g_object_list_free (applications);
}

static void
set_extension_and_description (NautilusMimeApplicationChooser *chooser,
			       const char *extension,
			       const char *mime_type)
{
	if (extension != NULL &&
	    g_content_type_is_unknown (mime_type)) {
		chooser->details->extension = g_strdup (extension);
		    chooser->details->content_type = g_strdup_printf ("application/x-extension-%s", extension);
		    /* the %s here is a file extension */
		    chooser->details->type_description =
			    g_strdup_printf (_("%s document"), extension);
	    } else {
		    char *description;

		    chooser->details->content_type = g_strdup (mime_type);
		    description = g_content_type_get_description (mime_type);
		    if (description == NULL) {
			    description = g_strdup (_("Unknown"));
		    }
		    
		    chooser->details->type_description = description;
	    }
}

static gboolean
set_uri_and_type (NautilusMimeApplicationChooser *chooser, 
		  const char *uri,
		  const char *mime_type)
{
	char *label;
	char *name;
	char *emname;
	char *extension;
	GFile *file;

	chooser->details->uri = g_strdup (uri);

	file = g_file_new_for_uri (uri);
	name = g_file_get_basename (file);
	g_object_unref (file);
	
	chooser->details->orig_mime_type = g_strdup (mime_type);

	extension = get_extension (name);
	set_extension_and_description (NAUTILUS_MIME_APPLICATION_CHOOSER (chooser),
				       extension, mime_type);
	g_free (extension);

	/* first %s is filename, second %s is mime-type description */
	emname = g_strdup_printf ("<i>%s</i>", name);
	label = g_strdup_printf (_("Select an application to open %s and other files of type \"%s\""),
				 emname, chooser->details->type_description);
	g_free (emname);
	
	gtk_label_set_markup (GTK_LABEL (chooser->details->label), label);

	g_free (label);
	g_free (name);

	refresh_model (chooser);

	return TRUE;
}

static char *
get_extension_from_file (NautilusFile *nfile)
{
	char *name;
	char *extension;

	name = nautilus_file_get_name (nfile);
	extension = get_extension (name);
	
	g_free (name);
	
	return extension;
}

static gboolean
set_uri_and_type_for_multiple_files (NautilusMimeApplicationChooser *chooser,
				     GList *uris,
				     const char *mime_type)
{
	char *label;
	char *first_extension;
	gboolean same_extension;
	GList *iter;
	
	chooser->details->for_multiple_files = TRUE;
	chooser->details->uri = NULL;
	chooser->details->orig_mime_type = g_strdup (mime_type);
	same_extension = TRUE;
	first_extension = get_extension_from_file (NAUTILUS_FILE (uris->data));
	iter = uris->next;

	while (iter != NULL) {
		char *extension_current;

		extension_current = get_extension_from_file (NAUTILUS_FILE (iter->data));
		if (eel_strcmp (first_extension, extension_current)) {
			same_extension = FALSE;
			g_free (extension_current);
			break;
		}
		iter = iter->next;

		g_free (extension_current);
	}
	if (!same_extension) {
		set_extension_and_description (NAUTILUS_MIME_APPLICATION_CHOOSER (chooser),
					       NULL, mime_type);
	} else {
		set_extension_and_description (NAUTILUS_MIME_APPLICATION_CHOOSER (chooser),
					       first_extension, mime_type);
	}

	g_free (first_extension);

	label = g_strdup_printf (_("Open all files of type \"%s\" with:"),
				 chooser->details->type_description);
	gtk_label_set_markup (GTK_LABEL (chooser->details->label), label);

	g_free (label);

	refresh_model (chooser);

	return TRUE;		
}

GtkWidget *
nautilus_mime_application_chooser_new (const char *uri,
				  const char *mime_type)
{
	GtkWidget *chooser;

	chooser = gtk_widget_new (NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER, NULL);

	set_uri_and_type (NAUTILUS_MIME_APPLICATION_CHOOSER (chooser), uri, mime_type);

	return chooser;
}

GtkWidget *
nautilus_mime_application_chooser_new_for_multiple_files (GList *uris,
							  const char *mime_type)
{
	GtkWidget *chooser;
	
	chooser = gtk_widget_new (NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER, NULL);
	
	set_uri_and_type_for_multiple_files (NAUTILUS_MIME_APPLICATION_CHOOSER (chooser),
					     uris, mime_type);
	
	return chooser;
}

GType
nautilus_mime_application_chooser_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusMimeApplicationChooserClass),
			NULL, 
			NULL,
			(GClassInitFunc)nautilus_mime_application_chooser_class_init,
			NULL,
			NULL,
			sizeof (NautilusMimeApplicationChooser),
			0,
			(GInstanceInitFunc)nautilus_mime_application_chooser_instance_init,
		};
		
		type = g_type_register_static (GTK_TYPE_VBOX, 
					       "NautilusMimeApplicationChooser",
					       &info, 0);
	}
	
	return type;		       
}
