 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
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
 */

/* nautilus-tree-view.c - tree sidebar panel
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-model.h"
#include <bonobo/bonobo-control.h>
#include <eel/eel-gtk-macros.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvbox.h>

struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;
	
	GtkWidget *scrolled_window;
	GtkWidget *tree_view;
	NautilusTreeModel *model;
};

static void     nautilus_tree_view_class_init        (NautilusTreeViewClass *klass);
static void     nautilus_tree_view_init              (NautilusTreeView      *view);

EEL_CLASS_BOILERPLATE (NautilusTreeView,
				   nautilus_tree_view,
				   NAUTILUS_TYPE_VIEW)


static void
create_tree (NautilusTreeView *view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	
	view->details->model = nautilus_tree_model_new ("file:///");
	view->details->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->details->model));
	g_object_unref (view->details->model);

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

	gtk_tree_view_append_column (GTK_TREE_VIEW (view->details->tree_view), column);
	
	gtk_widget_show (view->details->tree_view);

	gtk_container_add (GTK_CONTAINER (view->details->scrolled_window),
			   view->details->tree_view);
}

static void
tree_activate_callback (BonoboControl *control, gboolean activating, gpointer user_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (user_data);
	g_assert (view != NULL);

	if (activating) {
		if (view->details->tree_view == NULL) {
			create_tree (view);
		}
	}
}

static void
dump_tree (GtkWidget *button, NautilusTreeView *view)
{
	if (view->details->model != NULL) {
		nautilus_tree_model_dump (view->details->model);
	}
}

static void
nautilus_tree_view_init (NautilusTreeView *view)
{
	BonoboControl *control;
	GtkWidget *button, *vbox;
	view->details = g_new0 (NautilusTreeViewDetails, 1);

	view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_widget_show (view->details->scrolled_window);
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), view->details->scrolled_window, TRUE, TRUE, 0);
	button = gtk_button_new_with_label ("Dump tree");
	g_signal_connect (button, "clicked", G_CALLBACK (dump_tree), view);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all (vbox);
	control = bonobo_control_new (vbox);
	nautilus_view_construct_from_bonobo_control (NAUTILUS_VIEW (view), control);

	g_signal_connect (control, "activate",
			  G_CALLBACK (tree_activate_callback), view);
}

static void
nautilus_tree_view_class_init (NautilusTreeViewClass *klass)
{
}
