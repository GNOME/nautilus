/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-column-chooser.h - A column chooser widget

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#include <config.h>
#include "nautilus-column-chooser.h"

#include <string.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>

#include "nautilus-column-utilities.h"

struct _NautilusColumnChooserDetails
{
	GtkTreeView *view;
	GtkListStore *store;

	GtkWidget *move_up_button;
	GtkWidget *move_down_button;
	GtkWidget *show_button;
	GtkWidget *hide_button;
	GtkWidget *use_default_button;
};

enum {
	COLUMN_VISIBLE,
	COLUMN_LABEL,
	COLUMN_NAME,
	NUM_COLUMNS
};

enum {
	CHANGED,
	USE_DEFAULT,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_column_chooser_class_init (NautilusColumnChooserClass *chooser_class);
static void nautilus_column_chooser_init       (NautilusColumnChooser *chooser);
static void nautilus_column_chooser_destroy    (GtkObject      *object);
static void nautilus_column_chooser_finalize   (GObject        *object);

EEL_CLASS_BOILERPLATE (NautilusColumnChooser, nautilus_column_chooser, GTK_TYPE_HBOX);

static void
nautilus_column_chooser_class_init (NautilusColumnChooserClass *chooser_class)
{
	G_OBJECT_CLASS (chooser_class)->finalize = nautilus_column_chooser_finalize;
	GTK_OBJECT_CLASS (chooser_class)->destroy = nautilus_column_chooser_destroy;

	signals[CHANGED] = g_signal_new
		("changed",
		 G_TYPE_FROM_CLASS (chooser_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusColumnChooserClass,
				  changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[USE_DEFAULT] = g_signal_new
		("use_default",
		 G_TYPE_FROM_CLASS (chooser_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusColumnChooserClass,
				  use_default),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	g_type_class_add_private (chooser_class, sizeof (NautilusColumnChooserDetails));
}

static void 
update_buttons (NautilusColumnChooser *chooser)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection (chooser->details->view);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gboolean visible;
		gboolean top;
		gboolean bottom;
		GtkTreePath *first;
		GtkTreePath *path;
		
		gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->store),
				    &iter, 
				    COLUMN_VISIBLE, &visible, 
				    -1);
		
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (chooser->details->store),
						&iter);
		first = gtk_tree_path_new_first ();
		
		top = (gtk_tree_path_compare (path, first) == 0);

		gtk_tree_path_free (path);
		gtk_tree_path_free (first);
		
		bottom = !gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->details->store),
						    &iter);

		gtk_widget_set_sensitive (chooser->details->move_up_button,
					  !top);
		gtk_widget_set_sensitive (chooser->details->move_down_button,
					  !bottom);
		gtk_widget_set_sensitive (chooser->details->show_button,
					  !visible);
		gtk_widget_set_sensitive (chooser->details->hide_button,
					  visible);
	} else {
		gtk_widget_set_sensitive (chooser->details->move_up_button,
					  FALSE);
		gtk_widget_set_sensitive (chooser->details->move_down_button,
					  FALSE);
		gtk_widget_set_sensitive (chooser->details->show_button,
					  FALSE);
		gtk_widget_set_sensitive (chooser->details->hide_button,
					  FALSE);
	}
}

static void
list_changed (NautilusColumnChooser *chooser) 
{
	update_buttons (chooser);
	g_signal_emit (chooser, signals[CHANGED], 0);
}

static void
visible_toggled_callback (GtkCellRendererToggle *cell, 
			  char *path_string,
			  gpointer user_data)
{
	NautilusColumnChooser *chooser;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean visible;
	
	chooser = NAUTILUS_COLUMN_CHOOSER (user_data);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->details->store), 
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->store),
			    &iter, COLUMN_VISIBLE, &visible, -1);
	gtk_list_store_set (chooser->details->store,
			    &iter, COLUMN_VISIBLE, !visible, -1);
	gtk_tree_path_free (path);
	list_changed (chooser);
}

static void
selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
	update_buttons (NAUTILUS_COLUMN_CHOOSER (user_data));
}

static void
row_deleted_callback (GtkTreeModel *model, 
		       GtkTreePath *path,
		       gpointer user_data)
{
	list_changed (NAUTILUS_COLUMN_CHOOSER (user_data));
}

static void
add_tree_view (NautilusColumnChooser *chooser)
{
	GtkWidget *scrolled;
	GtkWidget *view;
	GtkListStore *store;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	
	view = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
	
	store = gtk_list_store_new (NUM_COLUMNS,
				    G_TYPE_BOOLEAN,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), 
				 GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (view), TRUE);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (selection, "changed", 
			  G_CALLBACK (selection_changed_callback), chooser);

	cell = gtk_cell_renderer_toggle_new ();
	
	g_signal_connect (G_OBJECT (cell), "toggled",
			  G_CALLBACK (visible_toggled_callback), chooser);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
						     -1, NULL,
						     cell, 
						     "active", COLUMN_VISIBLE,
						     NULL);

	cell = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
						     -1, NULL,
						     cell, 
						     "text", COLUMN_LABEL,
						     NULL);

	chooser->details->view = GTK_TREE_VIEW (view);
	chooser->details->store = store;

	gtk_widget_show (view);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (GTK_WIDGET (scrolled));
	
	gtk_container_add (GTK_CONTAINER (scrolled), view);
	gtk_box_pack_start (GTK_BOX (chooser), scrolled, TRUE, TRUE, 0);
}

static void
set_selection_visible (NautilusColumnChooser *chooser, gboolean visible)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (chooser->details->view);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_list_store_set (chooser->details->store,
				    &iter, 
				    COLUMN_VISIBLE, visible, 
				    -1);
	}

	list_changed (chooser);
}

static void
move_up_clicked_callback (GtkWidget *button, gpointer user_data)
{
	NautilusColumnChooser *chooser;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	chooser = NAUTILUS_COLUMN_CHOOSER (user_data);
	
	selection = gtk_tree_view_get_selection (chooser->details->view);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkTreePath *path;
		GtkTreeIter prev;

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (chooser->details->store), &iter);
		gtk_tree_path_prev (path);
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->details->store), &prev, path)) {
			gtk_list_store_move_before (chooser->details->store,
						   &iter,
						   &prev);
		}
		gtk_tree_path_free (path);
	}

	list_changed (chooser);
}

static void
move_down_clicked_callback (GtkWidget *button, gpointer user_data)
{
	NautilusColumnChooser *chooser;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	chooser = NAUTILUS_COLUMN_CHOOSER (user_data);
	
	selection = gtk_tree_view_get_selection (chooser->details->view);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkTreeIter next;

		next = iter;
		
		if (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->details->store), &next)) {
			gtk_list_store_move_after (chooser->details->store,
						   &iter,
						   &next);
		}
	}

	list_changed (chooser);
}

static void
show_clicked_callback (GtkWidget *button, gpointer user_data)
{
	set_selection_visible (NAUTILUS_COLUMN_CHOOSER (user_data), TRUE);
}

static void
hide_clicked_callback (GtkWidget *button, gpointer user_data)
{
	set_selection_visible (NAUTILUS_COLUMN_CHOOSER (user_data), FALSE);
}

static void
use_default_clicked_callback (GtkWidget *button, gpointer user_data)
{
	g_signal_emit (NAUTILUS_COLUMN_CHOOSER (user_data), 
		       signals[USE_DEFAULT], 0);
}

static void
add_buttons (NautilusColumnChooser *chooser)
{
	GtkWidget *box;
	GtkWidget *separator;
	
	box = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (box);
	
	chooser->details->move_up_button = gtk_button_new_with_mnemonic ("Move _Up");
	g_signal_connect (chooser->details->move_up_button, 
			  "clicked",  G_CALLBACK (move_up_clicked_callback),
			  chooser);
	gtk_widget_show (chooser->details->move_up_button);
	gtk_widget_set_sensitive (chooser->details->move_up_button, FALSE);
	gtk_box_pack_start (GTK_BOX (box), chooser->details->move_up_button,
			    FALSE, FALSE, 0);

	chooser->details->move_down_button = gtk_button_new_with_mnemonic ("Move _Down");
	g_signal_connect (chooser->details->move_down_button, 
			  "clicked",  G_CALLBACK (move_down_clicked_callback),
			  chooser);
	gtk_widget_show (chooser->details->move_down_button);
	gtk_widget_set_sensitive (chooser->details->move_down_button, FALSE);
	gtk_box_pack_start (GTK_BOX (box), chooser->details->move_down_button,
			    FALSE, FALSE, 0);

	chooser->details->show_button = gtk_button_new_with_mnemonic ("_Show");
	g_signal_connect (chooser->details->show_button, 
			  "clicked",  G_CALLBACK (show_clicked_callback),
			  chooser);
			  
	gtk_widget_set_sensitive (chooser->details->show_button, FALSE);
	gtk_widget_show (chooser->details->show_button);
	gtk_box_pack_start (GTK_BOX (box), chooser->details->show_button,
			    FALSE, FALSE, 0);

	chooser->details->hide_button = gtk_button_new_with_mnemonic ("_Hide");
	g_signal_connect (chooser->details->hide_button, 
			  "clicked",  G_CALLBACK (hide_clicked_callback),
			  chooser);
	gtk_widget_set_sensitive (chooser->details->hide_button, FALSE);
	gtk_widget_show (chooser->details->hide_button);
	gtk_box_pack_start (GTK_BOX (box), chooser->details->hide_button,
			    FALSE, FALSE, 0);

	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);	

	chooser->details->use_default_button = gtk_button_new_with_mnemonic ("_Use Default");
	g_signal_connect (chooser->details->use_default_button, 
			  "clicked",  G_CALLBACK (use_default_clicked_callback),
			  chooser);
	gtk_widget_show (chooser->details->use_default_button);
	gtk_box_pack_start (GTK_BOX (box), chooser->details->use_default_button,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (chooser), box,
			    FALSE, FALSE, 0);
}

static void
populate_tree (NautilusColumnChooser *chooser)
{
	GList *columns;
	GList *l;
	
	columns = nautilus_get_all_columns ();
	
	for (l = columns; l != NULL; l = l->next) {
		GtkTreeIter iter;
		NautilusColumn *column;
		char *name;
		char *label;
		
		column = NAUTILUS_COLUMN (l->data);
		
		g_object_get (G_OBJECT (column), 
			      "name", &name, "label", &label, 
			      NULL);

		gtk_list_store_append (chooser->details->store, &iter);
		gtk_list_store_set (chooser->details->store, &iter,
				    COLUMN_VISIBLE, FALSE,
				    COLUMN_LABEL, label,
				    COLUMN_NAME, name,
				    -1);

		g_free (name);
		g_free (label);
	}

	nautilus_column_list_free (columns);
}

static void
nautilus_column_chooser_init (NautilusColumnChooser *chooser)
{	
	chooser->details = G_TYPE_INSTANCE_GET_PRIVATE ((chooser), NAUTILUS_TYPE_COLUMN_CHOOSER, NautilusColumnChooserDetails);

	g_object_set (G_OBJECT (chooser), 
		      "homogeneous", FALSE,
		      "spacing", 8,
		      NULL);

	add_tree_view (chooser);
	add_buttons (chooser);

	populate_tree (chooser);

	g_signal_connect (chooser->details->store, "row_deleted", 
			  G_CALLBACK (row_deleted_callback), chooser);
}

static void
nautilus_column_chooser_destroy (GtkObject      *object)
{
	NautilusColumnChooser *chooser;
	
	chooser = NAUTILUS_COLUMN_CHOOSER (object);
}

static void
nautilus_column_chooser_finalize (GObject        *object)
{
}

static void 
set_visible_columns (NautilusColumnChooser *chooser,
		     GList *visible_columns)
{
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->details->store),
					   &iter)) {
		do {
			char *name;
			gboolean visible;			
			
			gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->store),
					    &iter,
					    COLUMN_NAME, &name,
					    -1);

			visible = (eel_g_str_list_index (visible_columns, name) != -1);

			gtk_list_store_set (chooser->details->store,
					    &iter,
					    COLUMN_VISIBLE, visible,
					    -1);
			g_free (name);
			
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->details->store), &iter));
	}	
}

static GList *
get_column_names (NautilusColumnChooser *chooser, gboolean only_visible)
{
	
	GList *ret;
	GtkTreeIter iter;
	
	ret = NULL;
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->details->store),
					   &iter)) {
		do {
			char *name;
			gboolean visible;
			gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->store),
					    &iter,
					    COLUMN_VISIBLE, &visible,
					    COLUMN_NAME, &name,
					    -1);
			if (!only_visible || visible) {
				/* give ownership to the list */
				ret = g_list_prepend (ret, name);
			}
			
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->details->store), &iter));
	}

	return g_list_reverse (ret);
}

static gboolean
get_column_iter (NautilusColumnChooser *chooser, 
		 NautilusColumn *column,
		 GtkTreeIter *iter)
{
	char *column_name;

	g_object_get (NAUTILUS_COLUMN (column), "name", &column_name, NULL);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->details->store),
					   iter)) {
		do {
			char *name;

			
			gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->store),
					    iter,
					    COLUMN_NAME, &name,
					    -1);
			if (!strcmp (name, column_name)) {
				g_free (column_name);
				g_free (name);
				return TRUE;
			}

			g_free (name);
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->details->store), iter));
	}
	g_free (column_name);
	return FALSE;
}

static void
set_column_order (NautilusColumnChooser *chooser,
		  GList *column_order)

{
	GList *columns;
	GList *l;
	GtkTreePath *path;
	
	columns = nautilus_get_all_columns ();
	columns = nautilus_sort_columns (columns, column_order);

	g_signal_handlers_block_by_func (chooser->details->store,
					 G_CALLBACK (row_deleted_callback), 
					 chooser);

	path = gtk_tree_path_new_first ();
	for (l = columns; l != NULL; l = l->next) {
		GtkTreeIter iter;
		
		if (get_column_iter (chooser, NAUTILUS_COLUMN (l->data), &iter)) {
			GtkTreeIter before;
			if (path) {
				gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->details->store),
							 &before, path);
				gtk_list_store_move_after (chooser->details->store,
							   &iter, &before);
				gtk_tree_path_next (path);
				
			} else {		
				gtk_list_store_move_after (chooser->details->store,
							   &iter, NULL);
			}
		}
	}
	gtk_tree_path_free (path);
	g_signal_handlers_unblock_by_func (chooser->details->store, 
					   G_CALLBACK (row_deleted_callback), 
					   chooser);
	
	nautilus_column_list_free (columns);
}

void
nautilus_column_chooser_set_settings (NautilusColumnChooser *chooser,
				      GList *visible_columns,
				      GList *column_order)
{
	g_return_if_fail (NAUTILUS_IS_COLUMN_CHOOSER (chooser));
	g_return_if_fail (visible_columns != NULL);
	g_return_if_fail (column_order != NULL);

	set_visible_columns (chooser, visible_columns);
	set_column_order (chooser, column_order);

	list_changed (chooser);
}

void
nautilus_column_chooser_get_settings (NautilusColumnChooser *chooser,
				      GList **visible_columns,
				      GList **column_order)
{
	g_return_if_fail (NAUTILUS_IS_COLUMN_CHOOSER (chooser));
	g_return_if_fail (visible_columns != NULL);
	g_return_if_fail (column_order != NULL);
	
	*visible_columns = get_column_names (chooser, TRUE);
	*column_order = get_column_names (chooser, FALSE);
}

GtkWidget *
nautilus_column_chooser_new (void)
{
	return g_object_new (NAUTILUS_TYPE_COLUMN_CHOOSER, NULL);
}



