/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Darin Adler
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
 * Authors: 
 *       Maciej Stachowiak <mjs@eazel.com>
 *       Anders Carlsson <andersca@gnu.org>
 *       Darin Adler <darin@bentspoon.com>
 */

/* nautilus-tree-view.c - tree sidebar panel
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-model.h"
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtktreeview.h>
#include <libnautilus-private/nautilus-file-attributes.h>

#define NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE "tree-sidebar-panel/expansion_state"

struct NautilusTreeViewDetails {
	GtkWidget *scrolled_window;
	GtkTreeView *tree_widget;
#if SORT_MODEL_WORKS
	GtkTreeModelSort *sort_model;
#else
	NautilusTreeModel *sort_model;
#endif
	NautilusTreeModel *child_model;

	NautilusFile *activation_file;
	guint save_expansion_state_idle_id;
};

typedef struct {
	GList *uris;
	NautilusTreeView *view;
} PrependURIParameters;

BONOBO_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
load_expansion_state (NautilusTreeView *view)
{
	EelStringList *uris;

        uris = eel_preferences_get_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE);
        /* eel_string_list_for_each (uris, expansion_state_load_callback, expansion_state); */
        eel_string_list_free (uris);

	/* FIXME: Need to expand nodes as they get loaded -- connect to the row_inserted signal on the model */
}

static NautilusFile *
path_to_file (NautilusTreeView *view, GtkTreePath *path)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->sort_model), &iter, path)) {
		return NULL;
	}
#if SORT_MODEL_WORKS
	gtk_tree_model_sort_convert_iter_to_child_iter (view->details->sort_model, &iter, &iter);
#endif
	return nautilus_tree_model_iter_get_file (view->details->child_model, &iter);
}

static void
prepend_one_uri (GtkTreeView *tree_view,
		 GtkTreePath *path,
		 gpointer callback_data)
{
	PrependURIParameters *p;
	NautilusFile *file;

	p = callback_data;
	file = path_to_file (p->view, path);
	if (file == NULL) {
		return;
	}
	p->uris = g_list_prepend (p->uris, nautilus_file_get_uri (file));
	nautilus_file_unref (file);
}

static void
save_expansion_state (NautilusTreeView *view)
{
	PrependURIParameters p;
        EelStringList *uris;

	p.uris = NULL;
	p.view = view;
        gtk_tree_view_map_expanded_rows (view->details->tree_widget, prepend_one_uri, &p);
        p.uris = g_list_sort (p.uris, eel_strcmp_compare_func);
        uris = eel_string_list_new_from_g_list (p.uris, TRUE);
	g_list_free (p.uris);
        eel_preferences_set_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE, uris);
        eel_string_list_free (uris);
}

static gboolean
save_expansion_state_idle_callback (gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);
	view->details->save_expansion_state_idle_id = 0;
	save_expansion_state (view);
	return FALSE;
}

static void
schedule_save_expansion_state_callback (NautilusTreeView *view)
{
	g_assert (NAUTILUS_IS_TREE_VIEW (view));
	if (view->details->save_expansion_state_idle_id == 0) {
		view->details->save_expansion_state_idle_id =
			g_idle_add (save_expansion_state_idle_callback, view);
	}
}

static void
got_activation_uri_callback (NautilusFile *file, gpointer callback_data)
{
        char *uri;
        NautilusTreeView *view;
	
        view = NAUTILUS_TREE_VIEW (callback_data);
	
        g_assert (file == view->details->activation_file);

	uri = nautilus_file_get_activation_uri (file);
	if (uri != NULL
	    /* FIXME: reenable && !eel_uris_match_ignore_fragments (view->details->current_main_view_uri, uri) */
	    && strncmp (uri, "command:", strlen ("command:")) != 0) {
		nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), uri);
	}
	g_free (uri);
	
	/* FIXME: show_file (view, file); */
	
	nautilus_file_unref (view->details->activation_file);
	view->details->activation_file = NULL;
}

static void
cancel_activation (NautilusTreeView *view)
{
        if (view->details->activation_file == NULL) {
		return;
	}
	
	nautilus_file_cancel_call_when_ready
		(view->details->activation_file,
		 got_activation_uri_callback, view);
	nautilus_file_unref (view->details->activation_file);
        view->details->activation_file = NULL;
}

static void
row_activated_callback (GtkTreeView *tree_widget,
			GtkTreePath *path,
			GtkTreeViewColumn *column,
			NautilusTreeView *view)
{
	GList *attrs;

        cancel_activation (view);

        view->details->activation_file = nautilus_file_ref (path_to_file (view, path));

        attrs = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
        nautilus_file_call_when_ready (view->details->activation_file, attrs,
                                       got_activation_uri_callback, view);
	g_list_free (attrs);
}

static void
create_tree (NautilusTreeView *view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	
	view->details->child_model = nautilus_tree_model_new ("file:///");
#if SORT_MODEL_WORKS
	view->details->sort_model = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (view->details->child_model)));
	g_object_unref (view->details->child_model);
#else
	view->details->sort_model = view->details->child_model;
#endif
	view->details->tree_widget = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->details->sort_model)));
	g_object_unref (view->details->sort_model);

#if SORT_MODEL_WORKS
	/* gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sort_model), func, data); */
#endif

	gtk_tree_view_set_headers_visible (view->details->tree_widget, FALSE);

	/* Create column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "pixbuf", NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN,
					     NULL);

	gtk_tree_view_append_column (view->details->tree_widget, column);
	
	gtk_widget_show (GTK_WIDGET (view->details->tree_widget));

	gtk_container_add (GTK_CONTAINER (view->details->scrolled_window),
			   GTK_WIDGET (view->details->tree_widget));

	g_signal_connect_swapped (view->details->tree_widget, "row_expanded",
				  G_CALLBACK (schedule_save_expansion_state_callback), view);
	g_signal_connect_swapped (view->details->tree_widget, "row_collapsed",
				  G_CALLBACK (schedule_save_expansion_state_callback), view);
	g_signal_connect (view->details->tree_widget, "row_activated",
			  G_CALLBACK (row_activated_callback), view);
}

static void
tree_activate_callback (BonoboControl *control, gboolean activating, gpointer user_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (user_data);

	if (activating && view->details->tree_widget == NULL) {
		create_tree (view);
		load_expansion_state (view);
	}
}

static void
nautilus_tree_view_instance_init (NautilusTreeView *view)
{
	BonoboControl *control;
	
	view->details = g_new0 (NautilusTreeViewDetails, 1);
	
	view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_widget_show (view->details->scrolled_window);

	control = bonobo_control_new (view->details->scrolled_window);
	g_signal_connect_object (control, "activate",
				 G_CALLBACK (tree_activate_callback), view, 0);

	nautilus_view_construct_from_bonobo_control (NAUTILUS_VIEW (view), control);
}

static void
nautilus_tree_view_finalize (GObject *object)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (object);

	cancel_activation (view);
	if (view->details->save_expansion_state_idle_id != 0) {
		g_source_remove (view->details->save_expansion_state_idle_id);
	}

	g_free (view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_tree_view_class_init (NautilusTreeViewClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_tree_view_finalize;
}
