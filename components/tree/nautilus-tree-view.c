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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-tree-view.c - tree content view
   component. This component displays a simple label of the URI
   and demonstrates merging menu items & toolbar buttons. 
   It should be a good basis for writing out-of-proc content views.
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>


/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;
};

#define TREE_SPACING 3

static void nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass);
static void nautilus_tree_view_initialize       (NautilusTreeView      *view);
static void nautilus_tree_view_destroy          (GtkObject                      *object);
static void tree_load_location_callback                 (NautilusView                   *nautilus_view,
							   const char                     *location,
							   NautilusTreeView      *view);
static void nautilus_tree_view_load_uri                   (NautilusTreeView               *view,
							   const char                     *uri);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view, GTK_TYPE_CTREE)
     
static void
nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_view_destroy;
}

static void
nautilus_tree_view_initialize (NautilusTreeView *view)
{
	view->details = g_new0 (NautilusTreeViewDetails, 1);
	
	
	gtk_ctree_construct (GTK_CTREE (view),
			     1, 
			     0,
			     NULL);

        gtk_clist_set_selection_mode (GTK_CLIST (view), GTK_SELECTION_BROWSE);
	gtk_clist_set_auto_sort (GTK_CLIST (view), TRUE);
	gtk_clist_set_sort_type (GTK_CLIST (view), GTK_SORT_ASCENDING);
	gtk_clist_set_column_auto_resize (GTK_CLIST (view), 0, TRUE);
	gtk_clist_columns_autosize (GTK_CLIST (view));

	gtk_clist_set_reorderable (GTK_CLIST (view), FALSE);

	gtk_ctree_set_expander_style (GTK_CTREE (view), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_ctree_set_line_style (GTK_CTREE (view), GTK_CTREE_LINES_NONE);

	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (tree_load_location_callback), 
			    view);


	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);
	
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
	
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



/**
 * nautilus_tree_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusTreeView to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_tree_view_get_nautilus_view (NautilusTreeView *view)
{
	return view->details->nautilus_view;
}

/**
 * nautilus_tree_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * 
 **/
void
nautilus_tree_view_load_uri (NautilusTreeView *view,
			     const char               *uri)
{
	char *texts[1];
	GtkCTreeNode *top_node;

	texts[0] = "/";

	top_node = gtk_ctree_insert_node (GTK_CTREE (view), NULL, NULL,
			       texts, TREE_SPACING,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       FALSE, TRUE);

	texts[0] = "usr";

	gtk_ctree_insert_node (GTK_CTREE (view), top_node, NULL,
			       texts, TREE_SPACING,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       FALSE, TRUE);


	texts[0] = "bin";

	gtk_ctree_insert_node (GTK_CTREE (view), top_node, NULL,
			       texts, TREE_SPACING,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       FALSE, TRUE);


	texts[0] = "tmp";

	gtk_ctree_insert_node (GTK_CTREE (view), top_node, NULL,
			       texts, TREE_SPACING,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       FALSE, TRUE);



	/* FIXME */
}
 
static void
tree_load_location_callback (NautilusView *nautilus_view, 
			       const char *location,
			       NautilusTreeView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	/* It's mandatory to send an underway message once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progress update here.
	 */
	nautilus_view_report_load_underway (nautilus_view);
	
	/* Do the actual load. */
	nautilus_tree_view_load_uri (view, location);
	
	/* It's mandatory to call report_load_complete once the
	 * component is done loading successfully, or
	 * report_load_failed if it completes unsuccessfully. In a
	 * real component, this will probably happen in some sort of
	 * callback from whatever loading mechanism it is using to
	 * load the data; this component loads no data, so it gives
	 * the progress update here.
	 */
	nautilus_view_report_load_complete (nautilus_view);
}



