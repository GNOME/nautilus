/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
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

/* nautilus-tree-model.c - model for the tree view */

#include <config.h>
#include "nautilus-tree-model.h"

#include "nautilus-tree-node-private.h"
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <stdio.h>
#include <string.h>

enum {
	NODE_CHANGED,
	NODE_REMOVED,
	NODE_BEING_RENAMED,
	DONE_LOADING_CHILDREN,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


struct NautilusTreeModelDetails {
	GHashTable       *file_to_node_map;

	GList            *monitor_clients;
	GList		 *unparented_nodes;

	NautilusTreeNode *root_node;
	gboolean          root_node_reported;
	gint              root_node_changed_signal_id;

	GHashTable       *changed_files;
	GHashTable       *done_loading_files;
};




static void nautilus_tree_model_destroy                          (GtkObject         *object);
static void nautilus_tree_model_initialize                       (gpointer           object,
								  gpointer           klass);
static void nautilus_tree_model_initialize_class                 (gpointer           klass);
static void remove_all_nodes                                     (NautilusTreeModel *model);
static void nautilus_tree_model_set_root_uri                     (NautilusTreeModel *model,
								  const char        *root_uri);
static void report_root_node_if_possible                         (NautilusTreeModel *model);
static void report_node_changed                                  (NautilusTreeModel *model,
								  NautilusTreeNode  *node);
static void report_node_removed_internal                         (NautilusTreeModel *model,
								  NautilusTreeNode  *node,
								  gboolean           signal);
static void report_node_removed                                  (NautilusTreeModel *model,
								  NautilusTreeNode  *node);
static void report_done_loading                                  (NautilusTreeModel *model,
								  NautilusTreeNode  *node);


/* signal/monitoring callbacks */
static void nautilus_tree_model_root_node_file_monitor           (NautilusFile      *file,
								  NautilusTreeModel *model);
static void nautilus_tree_model_directory_files_changed_callback (NautilusDirectory *directory,
								  GList             *added_files,
								  gpointer           callback_data);
static void nautilus_tree_model_directory_done_loading_callback  (NautilusDirectory *directory,
								  NautilusTreeModel *model);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusTreeModel, nautilus_tree_model, GTK_TYPE_OBJECT)

/* infrastructure stuff */

static void
nautilus_tree_model_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_model_destroy;

	signals[NODE_CHANGED] =
		gtk_signal_new ("node_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusTreeModelClass, node_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[NODE_REMOVED] =
		gtk_signal_new ("node_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusTreeModelClass, node_removed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[NODE_BEING_RENAMED] =
		gtk_signal_new ("node_being_renamed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusTreeModelClass, node_removed),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_STRING, GTK_TYPE_STRING);

	signals[DONE_LOADING_CHILDREN] =
		gtk_signal_new ("done_loading_children",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusTreeModelClass, done_loading_children),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);


	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_tree_model_initialize (gpointer object, gpointer klass)
{
	NautilusTreeModel *model;

	model = NAUTILUS_TREE_MODEL (object);

	model->details = g_new0 (NautilusTreeModelDetails, 1);

	model->details->file_to_node_map = g_hash_table_new (NULL, NULL);
}

static void
destroy_file_hash (GHashTable *hash)
{
	if (hash == NULL) {
		return;
	}
	eel_g_hash_table_destroy_deep_custom
		(hash,
		 (GFunc) nautilus_file_unref, NULL,
		 NULL, NULL);
}

static void
nautilus_tree_model_destroy (GtkObject *object)
{
	NautilusTreeModel *model;

	model = NAUTILUS_TREE_MODEL (object);

	if (model->details->root_node_changed_signal_id != 0) {
		gtk_signal_disconnect (GTK_OBJECT (nautilus_tree_node_get_file (model->details->root_node)),
				       model->details->root_node_changed_signal_id);
	}

	remove_all_nodes (model);

	g_list_free (model->details->monitor_clients);
	g_list_free (model->details->unparented_nodes);

	g_hash_table_destroy (model->details->file_to_node_map);

	destroy_file_hash (model->details->changed_files);
	destroy_file_hash (model->details->done_loading_files);

	g_free (model->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


/* public API */

NautilusTreeModel *
nautilus_tree_model_new (const char *root_uri)
{
	NautilusTreeModel *model;

	model = NAUTILUS_TREE_MODEL (gtk_object_new (NAUTILUS_TYPE_TREE_MODEL, NULL));
	gtk_object_ref (GTK_OBJECT (model));
	gtk_object_sink (GTK_OBJECT (model));

	nautilus_tree_model_set_root_uri (model, root_uri);

	return model;
}


static void
nautilus_tree_model_set_root_uri (NautilusTreeModel *model,
				  const char *root_uri)
{
	NautilusFile *file;

	/* You can only set the root node once */
	g_return_if_fail (model->details->root_node == NULL);
	
	file = nautilus_file_get (root_uri);
	model->details->root_node = nautilus_tree_node_new (file);
	nautilus_file_unref (file);
}


static void
nautilus_tree_model_unref_callback (NautilusTreeModel *model,
				    NautilusTreeNode  *node,
				    gpointer           callback_data)
{
	report_node_removed_internal (model, node, FALSE);
}

static void
nautilus_tree_model_for_each_postorder (NautilusTreeModel  *model,
					NautilusTreeModelCallback  callback,
					gpointer                   callback_data)
{
	NautilusTreeNode *current_node;
	GList *reporting_queue, *link;

	if (model->details->root_node_reported) {
		reporting_queue = g_list_prepend (NULL, model->details->root_node);
		
		while (reporting_queue != NULL) {
			current_node = (NautilusTreeNode *) reporting_queue->data;

			if (nautilus_tree_node_get_children (current_node) == NULL) {
				NautilusDirectory *tmp;

				link = reporting_queue;
				reporting_queue = g_list_remove_link (reporting_queue, link);
				g_list_free_1 (link);
				
				
#ifdef DEBUG_TREE
				printf ("XXX unrefing: %s\n", nautilus_tree_node_get_uri
					(current_node));
#endif

				tmp = current_node->details->directory;

				(*callback) (model, current_node, callback_data);
				
#ifdef DEBUG_TREE
				printf ("XXX refs: %d\n", ((GtkObject *) tmp)->ref_count);
#endif
			} else {
#ifdef DEBUG_TREE
				printf ("XXX adding children of: %s\n", nautilus_tree_node_get_uri
					(current_node));
#endif
				reporting_queue = g_list_concat (g_list_copy (nautilus_tree_node_get_children (current_node)),
								 reporting_queue);
			}
#ifdef DEBUG_TREE
			printf ("XXX queue length: %d\n", g_list_length (reporting_queue));
#endif
		}
	}
}

static void
remove_all_nodes (NautilusTreeModel *model)
{
	nautilus_tree_model_for_each_postorder (model,
						nautilus_tree_model_unref_callback,
						NULL);

	if (model->details->root_node != NULL) {
		gtk_object_unref (GTK_OBJECT (model->details->root_node));
		model->details->root_node = NULL;
	}
	model->details->root_node_reported = FALSE;

	g_list_free (model->details->unparented_nodes);
	model->details->unparented_nodes = NULL;
}

void
nautilus_tree_model_monitor_add (NautilusTreeModel         *model,
				 gconstpointer              client,
				 NautilusTreeModelCallback  initial_nodes_callback,
				 gpointer                   callback_data)
{
	NautilusTreeNode *current_node;
	GList *reporting_queue, *link;
	GList *monitor_attributes;
	
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (initial_nodes_callback != NULL);

	reporting_queue = NULL;

	/* If we just (re)started monitoring the whole tree, make sure
           to monitor the root node itself. */

	if (model->details->monitor_clients == NULL) {
		if (!model->details->root_node_reported) {
			report_root_node_if_possible (model);
		}

		model->details->root_node_changed_signal_id = gtk_signal_connect 
			(GTK_OBJECT (nautilus_tree_node_get_file (model->details->root_node)),
			 "changed",
			 nautilus_tree_model_root_node_file_monitor,
			 model);

		monitor_attributes = nautilus_icon_factory_get_required_file_attributes ();
		monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY);
		monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
		nautilus_file_monitor_add (nautilus_tree_node_get_file (model->details->root_node),
					   model,
					   monitor_attributes);
		g_list_free (monitor_attributes);
	}

	if (! g_list_find (model->details->monitor_clients, (gpointer) client)) {
		model->details->monitor_clients = g_list_prepend (model->details->monitor_clients, (gpointer) client);
	}

	if (model->details->root_node_reported) {
		reporting_queue = g_list_prepend (reporting_queue, model->details->root_node);

		while (reporting_queue != NULL) {
			current_node = (NautilusTreeNode *) reporting_queue->data;

			link = reporting_queue;
			reporting_queue = g_list_remove_link (reporting_queue, link);
			g_list_free_1 (link);

			(*initial_nodes_callback) (model, current_node, callback_data);

			/* We are doing a depth-first scan here, we
                           could do breadth-first instead by reversing
                           the args to the g_list_concat call
                           below. */
			reporting_queue = g_list_concat (g_list_copy (nautilus_tree_node_get_children (current_node)),
							 reporting_queue);
		}
	}

}


void
nautilus_tree_model_monitor_remove (NautilusTreeModel         *model,
				    gconstpointer              client)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));

	model->details->monitor_clients = g_list_remove (model->details->monitor_clients, (gpointer) client);

	if (model->details->root_node_reported) {
		nautilus_tree_model_stop_monitoring_node_recursive (model,
								    model->details->root_node,
								    client);
	}


	if (model->details->monitor_clients == NULL) {
		if (model->details->root_node_reported) {
			nautilus_file_monitor_remove
				(nautilus_tree_node_get_file (model->details->root_node),
				 model);
		}
	}
}


static gboolean
nautilus_tree_model_node_has_monitor_clients (NautilusTreeModel         *model,
					      NautilusTreeNode          *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);

	return (node->details->monitor_clients != NULL);
}


static void
nautilus_tree_model_node_begin_monitoring_no_connect (NautilusTreeModel         *model,
						      NautilusTreeNode          *node,
						      gboolean                   force_reload)
{
	GList             *monitor_attributes;
	NautilusDirectory *directory;

	directory = nautilus_tree_node_get_directory (node);

	if (force_reload) {
		nautilus_directory_force_reload (directory);
	}
	
	monitor_attributes = nautilus_icon_factory_get_required_file_attributes ();
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY);
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
	nautilus_directory_file_monitor_add (directory,
					     model,
					     TRUE, TRUE,
					     monitor_attributes,
					     nautilus_tree_model_directory_files_changed_callback,
					     model);
	g_list_free (monitor_attributes);
}


				 
static void
nautilus_tree_model_node_begin_monitoring (NautilusTreeModel         *model,
					   NautilusTreeNode          *node,
					   gboolean                   force_reload)
{
	NautilusDirectory *directory;

	directory = nautilus_tree_node_get_directory (node);

	node->details->done_loading_id = gtk_signal_connect 
		(GTK_OBJECT (directory),
		 "done_loading",
		 nautilus_tree_model_directory_done_loading_callback,
		 model);

	nautilus_tree_model_node_begin_monitoring_no_connect (model, node, force_reload);

	node->details->files_added_id = gtk_signal_connect 
		(GTK_OBJECT (directory),
		 "files_added",
		 nautilus_tree_model_directory_files_changed_callback,
		 model);
	
	node->details->files_changed_id = gtk_signal_connect 
		(GTK_OBJECT (directory),
		 "files_changed",
		 nautilus_tree_model_directory_files_changed_callback,
		 model);	
}

static void
nautilus_tree_model_node_end_monitoring (NautilusTreeModel         *model,
					 NautilusTreeNode          *node)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	gtk_signal_disconnect (GTK_OBJECT (node->details->directory), node->details->files_added_id);
	gtk_signal_disconnect (GTK_OBJECT (node->details->directory), node->details->files_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (node->details->directory), node->details->done_loading_id);

	node->details->files_added_id = 0;
	node->details->files_changed_id = 0;
	node->details->done_loading_id = 0;

	nautilus_directory_file_monitor_remove (node->details->directory,
						model);
}



void
nautilus_tree_model_monitor_node (NautilusTreeModel         *model,
				  NautilusTreeNode          *node,
				  gconstpointer              client,
				  gboolean                   force_reload)
{
	GList *p;
	GList *lost_nodes;
	NautilusDirectory *directory;

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	if (!nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
		report_done_loading (model, node);
		return;
	}

	if (! nautilus_tree_model_node_has_monitor_clients (model, node)) {
		nautilus_tree_model_node_begin_monitoring (model, node, force_reload);
	} else if (force_reload) {
		nautilus_tree_model_node_begin_monitoring_no_connect (model, node, force_reload);
	}

	if (! g_list_find (node->details->monitor_clients, (gpointer) client)) {
		node->details->monitor_clients = g_list_prepend (node->details->monitor_clients, 
								 (gpointer) client);
	}

	/* Check if any files got moved out of this directory
	 * while the node was collapsed.
	 */
	directory = nautilus_tree_node_get_directory (node);
	lost_nodes = NULL;
	for (p = nautilus_tree_node_get_children (node); p != NULL; p = p->next) {
		if (!nautilus_directory_contains_file (directory, nautilus_tree_node_get_file (p->data))) {
			lost_nodes = g_list_prepend (lost_nodes, p->data);
		}
	}
	for (p = lost_nodes; p != 0; p = p->next) {
		report_node_removed (model, p->data);
	}
	g_list_free (lost_nodes);
}


void
nautilus_tree_model_stop_monitoring_node (NautilusTreeModel *model,
					  NautilusTreeNode  *node,
					  gconstpointer      client)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	if (!nautilus_file_is_directory (nautilus_tree_node_get_file (node)) || node->details->monitor_clients == NULL) {
		return;
	}

	if (g_list_find (node->details->monitor_clients, (gpointer) client) == NULL) {
		return;
	}

	node->details->monitor_clients = g_list_remove (node->details->monitor_clients, (gpointer) client);

	if (!nautilus_tree_model_node_has_monitor_clients (model, node)) {
		nautilus_tree_model_node_end_monitoring (model, node);
	}
}


void
nautilus_tree_model_stop_monitoring_node_recursive (NautilusTreeModel *model,
						    NautilusTreeNode  *node,
						    gconstpointer      client)
{
	GList *p;

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	nautilus_tree_model_stop_monitoring_node (model, node, client);

	for (p = nautilus_tree_node_get_children (node); p != NULL; p = p->next) {
		nautilus_tree_model_stop_monitoring_node_recursive (model, (NautilusTreeNode *) p->data, client);
	}
}


NautilusTreeNode *
nautilus_tree_model_get_node_from_file (NautilusTreeModel *model,
					NautilusFile      *file)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	
	return g_hash_table_lookup (model->details->file_to_node_map, file);
}

NautilusTreeNode *
nautilus_tree_model_get_node (NautilusTreeModel *model,
			      const char        *uri)
{
	NautilusFile *file;
	NautilusTreeNode *node;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	file = nautilus_file_get (uri);
	
	if (file == NULL) {
		return NULL;
	}

	node = nautilus_tree_model_get_node_from_file (model, file);
	nautilus_file_unref (file);

	return node;
}

/* Debugging functions to dump current contents of file_to_node_map */

static void
dump_one_file_node (gpointer key, gpointer value, gpointer user_data)
{
	guint *file_number;
	char *uri;

	g_assert (NAUTILUS_IS_FILE (key));
	g_assert (NAUTILUS_IS_TREE_NODE (value));
	g_assert (user_data != NULL);

	file_number = (guint *) user_data;
	uri = nautilus_file_get_uri (NAUTILUS_FILE (key));

	g_print ("%d: %s (%p)|\n", ++(*file_number), uri, key);
	
	g_free (uri);
}

void
nautilus_tree_model_dump_files (NautilusTreeModel *model)
{
	guint file_number;

	file_number = 0;

	g_print ("nautilus_tree_model_dump_files: %d files in tree view file_to_node_map hash table:\n", 
		   g_hash_table_size (model->details->file_to_node_map));
	g_hash_table_foreach (model->details->file_to_node_map, 
			      dump_one_file_node, 
			      &file_number);
}


/* helper functions */

static char *
uri_get_parent_text (const char *uri_text)
{
	GnomeVFSURI *uri;
	GnomeVFSURI *parent;
	char *parent_text;

	uri = gnome_vfs_uri_new (uri_text);
	parent = gnome_vfs_uri_get_parent (uri);
	gnome_vfs_uri_unref (uri);

	if (parent == NULL) {
		return NULL;
	}

	parent_text = gnome_vfs_uri_to_string (parent, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent);

	return parent_text;
}

static void
report_root_node_if_possible (NautilusTreeModel *model)
{
	if (nautilus_file_get_file_type (nautilus_tree_node_get_file (model->details->root_node))
	    != GNOME_VFS_FILE_TYPE_UNKNOWN) {
		model->details->root_node_reported = TRUE;
		
		report_node_changed (model, model->details->root_node);
	}
}

static void
register_unparented_node (NautilusTreeModel *model, NautilusTreeNode *node)
{
	if (!nautilus_tree_node_is_toplevel (node) && g_list_find (model->details->unparented_nodes, node) == NULL) {
		model->details->unparented_nodes = g_list_prepend (model->details->unparented_nodes, node);
	}
}

static void
forget_unparented_node (NautilusTreeModel *model, NautilusTreeNode *node)
{
	model->details->unparented_nodes = g_list_remove (model->details->unparented_nodes, node);
}

static void
connect_unparented_nodes (NautilusTreeModel *model, NautilusTreeNode *parent)
{
	NautilusDirectory *parent_directory;
	char *parent_uri;
	NautilusTreeNode *node;
	GList *p, *to_parent;

	parent_uri = nautilus_file_get_uri (parent->details->file);
	parent_directory = nautilus_directory_get (parent_uri);
	g_free (parent_uri);

	if (parent_directory != NULL) {
		to_parent = NULL;

		for (p = model->details->unparented_nodes; p != NULL; p = p->next) {
			node = p->data;
			if (nautilus_directory_contains_file (parent_directory, node->details->file)) {
				to_parent = g_list_prepend (to_parent, node);
			}
		}

		for (p = to_parent; p != NULL; p = p->next) {
			node = p->data;
			nautilus_tree_node_set_parent (node, parent);
			model->details->unparented_nodes = g_list_remove (model->details->unparented_nodes, node);
		}

		g_list_free (to_parent);

		nautilus_directory_unref (parent_directory);
	}
}


static void
report_node_changed (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	char *parent_uri;
	NautilusTreeNode *parent_node;
	char *node_uri;
	char *file_uri;

	/* Bail out if we don't have all the info we need yet (we'll
	 * end up reporting the change later, once the info is
	 * ready). 
	 */

	if (nautilus_file_get_file_type (node->details->file) == GNOME_VFS_FILE_TYPE_UNKNOWN) {
		return;
	}

	node_uri = nautilus_tree_node_get_uri (node);

	if (node->details->directory == NULL && nautilus_file_is_directory (node->details->file)) {
		node->details->directory = nautilus_directory_get (node_uri);
	
#if 0
		if (nautilus_tree_model_node_has_monitor_clients (model, node)) {
			nautilus_tree_model_node_begin_monitoring (model, node);
		}
#endif
	}

	if (nautilus_tree_model_get_node_from_file (model,
						    nautilus_tree_node_get_file (node)) == NULL) {
		/* Actually added, go figure */
		
		parent_uri = uri_get_parent_text (node_uri);

		if (parent_uri != NULL) {
			parent_node = nautilus_tree_model_get_node (model, parent_uri);
			
			if (parent_node != NULL) {
				nautilus_tree_node_set_parent (node,
							       parent_node);
			} else {
				register_unparented_node (model, node);
			}

			g_free (parent_uri);
		}

		gtk_object_ref (GTK_OBJECT (node));
		g_hash_table_insert (model->details->file_to_node_map, 
				     nautilus_tree_node_get_file (node),
				     node);

		gtk_signal_emit (GTK_OBJECT (model),
				 signals[NODE_CHANGED],
				 node);

		connect_unparented_nodes (model, node);

	} else {
		/* really changed */

		file_uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));

		if (strcmp (file_uri, node_uri) == 0) {
			/* A normal change */
			gtk_signal_emit (GTK_OBJECT (model),
					 signals[NODE_CHANGED],
					 node);
		} else {
			/* A move or rename - model it as a remove followed by an add */

			gtk_object_ref (GTK_OBJECT (node));


			/* Let the view know that the rename is happening, this allows
			 * it to propagate the expansion state from the old name to the
			 * new name
			 */
			gtk_signal_emit (GTK_OBJECT (model),
					 signals[NODE_BEING_RENAMED],
					 node->details->uri, file_uri);

			report_node_removed (model, node);

			nautilus_tree_node_update_uri (node);
			
#if 0
			if (node->details->directory != NULL) {
				/* Stop monitoring the old directory */
				if (nautilus_tree_model_node_has_monitor_clients (model, node)) {
					nautilus_tree_model_node_end_monitoring (model, node);
				}
				nautilus_directory_unref (node->details->directory);
				node->details->directory = NULL;
			}
#endif

			report_node_changed (model, node);

			gtk_object_unref (GTK_OBJECT (node));
		}

		g_free (file_uri);
	}
	
	g_free (node_uri);
}

static void
report_node_removed_internal (NautilusTreeModel *model,
			      NautilusTreeNode  *node,
			      gboolean signal)
{
	NautilusTreeNode *parent_node;
	GList *p;

	if (node == NULL) {
		return;
	}

	if (nautilus_tree_model_get_node_from_file 
	    (model,nautilus_tree_node_get_file (node)) != NULL) {

		parent_node = nautilus_tree_node_get_parent (node);
			
		if (parent_node != NULL) {
			nautilus_tree_node_remove_from_parent (node);
		}

		for (p = node->details->children; p != NULL; p = p->next) {
			register_unparented_node (model, p->data);
		}
		nautilus_tree_node_remove_children (node);

		g_hash_table_remove (model->details->file_to_node_map, 
				     nautilus_tree_node_get_file (node));
	
		forget_unparented_node (model, node);

		if (signal) {
			gtk_signal_emit (GTK_OBJECT (model),
					 signals[NODE_REMOVED],
					 node);
		}


		gtk_object_unref (GTK_OBJECT (node));
	} 
}

static void
report_node_removed (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	report_node_removed_internal (model, node, TRUE);
}


static void
report_done_loading (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	gtk_signal_emit (GTK_OBJECT (model),
			 signals[DONE_LOADING_CHILDREN],
			 node);
}


/* signal/monitoring callbacks */

static void
nautilus_tree_model_root_node_file_monitor (NautilusFile      *file,
					    NautilusTreeModel *model)
{
	if (!model->details->root_node_reported) {
		report_root_node_if_possible (model);
	} else {
		report_node_changed (model,
				     model->details->root_node);
	}
}

static void
process_file_change (NautilusTreeModel *model,
		     NautilusFile *file)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node_from_file (model, file);
	
	if (nautilus_file_is_gone (file)) {
		if (node != NULL) {
			report_node_removed (model, node);
		}
	} else {
		if (node == NULL) {
			node = nautilus_tree_node_new (file);
		} else {
			gtk_object_ref (GTK_OBJECT (node));
		}
		report_node_changed (model, node);
		gtk_object_unref (GTK_OBJECT (node));
	}
}

static void
process_directory_done_loading (NautilusTreeModel *model,
				NautilusFile *file)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node_from_file (model, file);
	if (node != NULL) {
		report_done_loading (model, node);
	}
}

static void
add_file_to_hash (GHashTable *hash,
		  NautilusFile *file)
{
	if (g_hash_table_lookup (hash, file) == NULL) {
		nautilus_file_ref (file);
		g_hash_table_insert (hash, file, file);
	}
}

static void
nautilus_tree_model_directory_files_changed_callback (NautilusDirectory *directory,
						      GList *changed_files,
						      gpointer callback_data)
{
	NautilusTreeModel *model;
	GList *node;
	NautilusFile *file;

	model = NAUTILUS_TREE_MODEL (callback_data);

	for (node = changed_files; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		if (model->details->changed_files != NULL) {
			add_file_to_hash (model->details->changed_files, file);
		} else {
			process_file_change (model, file);
		}
	} 
}

static void
nautilus_tree_model_directory_done_loading_callback (NautilusDirectory        *directory,
						     NautilusTreeModel        *model)
{
	NautilusFile *file;

	file = nautilus_directory_get_corresponding_file (directory);

	if (model->details->done_loading_files != NULL) {
		add_file_to_hash (model->details->done_loading_files, file);
	} else {
		process_directory_done_loading (model, file);
	}

	nautilus_file_unref (file);
}

static void
process_one_changed_file (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (key));
	g_assert (key == value);
	g_assert (NAUTILUS_IS_TREE_MODEL (callback_data));

	process_file_change (callback_data, key);
}

static void
process_one_done_loading_file (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (key));
	g_assert (key == value);
	g_assert (NAUTILUS_IS_TREE_MODEL (callback_data));

	process_directory_done_loading (callback_data, key);
}

void
nautilus_tree_model_set_defer_notifications  (NautilusTreeModel *model,
					      gboolean defer)
{
	gboolean already_deferring;
	GHashTable *changed, *done_loading;

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (defer == FALSE || defer == TRUE);

	already_deferring = model->details->changed_files != NULL;
	if (already_deferring == defer) {
		return;
	}

	if (defer) {
		model->details->changed_files = g_hash_table_new (NULL, NULL);
		model->details->done_loading_files = g_hash_table_new (NULL, NULL);
	} else {
		changed = model->details->changed_files;
		done_loading = model->details->done_loading_files;

		model->details->changed_files = NULL;
		model->details->done_loading_files = NULL;

		g_hash_table_foreach (changed,
				      process_one_changed_file,
				      model);
		g_hash_table_foreach (done_loading,
				      process_one_done_loading_file,
				      model);

		destroy_file_hash (changed);
		destroy_file_hash (done_loading);
	}
}
