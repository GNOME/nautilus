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

/* nautilus-tree-model.c - model for the tree view */

#include <config.h>
#include "nautilus-tree-model.h"
#include "nautilus-tree-node-private.h"
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs.h>

#include <stdio.h>

enum {
	NODE_ADDED,
	NODE_CHANGED,
	NODE_REMOVED,
	DONE_LOADING_CHILDREN,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


struct NautilusTreeModelDetails {
	GHashTable       *uri_to_node_map;

	GList            *monitor_clients;

	NautilusTreeNode *root_node;
	gboolean          root_node_reported;
	gint              root_node_changed_signal_id;
};




static void               nautilus_tree_model_destroy          (GtkObject   *object);
static void               nautilus_tree_model_initialize       (gpointer     object,
								gpointer     klass);
static void               nautilus_tree_model_initialize_class (gpointer     klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeModel, nautilus_tree_model, GTK_TYPE_OBJECT)



static void nautilus_tree_model_set_root_uri                   (NautilusTreeModel *model,
								const char        *root_uri);
static char *uri_get_parent_text                               (const char        *uri_text);
static void  report_root_node_if_possible                      (NautilusTreeModel *model);
static void  report_node_added                                 (NautilusTreeModel *model,
								NautilusTreeNode  *node);
static void  report_node_changed                               (NautilusTreeModel *model,
								NautilusTreeNode  *node);
static void  report_node_removed                               (NautilusTreeModel *model,
								NautilusTreeNode  *node);
static void  report_done_loading                               (NautilusTreeModel *model,
								NautilusTreeNode  *node);

static void  nautilus_tree_stop_monitoring_internal            (NautilusTreeModel *model);


/* signal/monitoring callbacks */
static void  nautilus_tree_model_root_node_file_monitor          (NautilusFile      *file,
								  NautilusTreeModel *model);
static void nautilus_tree_model_directory_files_changed_callback (NautilusDirectory        *directory,
								  GList                    *added_files,
								  NautilusTreeModel        *model);

static void nautilus_tree_model_directory_files_added_callback   (NautilusDirectory        *directory,
								  GList                    *added_files,
								  NautilusTreeModel        *model);

static void nautilus_tree_model_directory_done_loading_callback  (NautilusDirectory        *directory,
								  NautilusTreeModel        *model);


/* infrastructure stuff */

static void
nautilus_tree_model_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_model_destroy;

	signals[NODE_ADDED] =
		gtk_signal_new ("node_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusTreeModelClass, node_added),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

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

	model->details->uri_to_node_map = g_hash_table_new (g_str_hash, 
							    g_str_equal);
}

static void       
nautilus_tree_model_destroy (GtkObject *object)
{
	NautilusTreeModel *model;

	model = (NautilusTreeModel *) object;

	/* FIXME: Free all the key strings and unref all the value nodes in
           the uri_to_node_map */

	g_hash_table_destroy (model->details->uri_to_node_map);

	nautilus_tree_stop_monitoring_internal (model);

	if (model->details->root_node != NULL) {
		gtk_object_unref (GTK_OBJECT (model->details->root_node));
	}

	g_free (model->details->monitor_clients);

	g_free (model->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* public API */

NautilusTreeModel *
nautilus_tree_model_new (const char *root_uri)
{
	NautilusTreeModel *model;

	model = NAUTILUS_TREE_MODEL (gtk_type_new (NAUTILUS_TYPE_TREE_MODEL));

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


void
nautilus_tree_model_monitor_add (NautilusTreeModel         *model,
				 gconstpointer              client,
				 NautilusTreeModelCallback  initial_nodes_callback,
				 gpointer                   callback_data)
{
	NautilusTreeNode *current_node;
	GList *reporting_queue;
	GList *monitor_attributes;
	
	reporting_queue = NULL;

	/* If we just (re)started monitoring the whole tree, make sure
           to monitor the root node itself. */

	if (model->details->monitor_clients == NULL) {
		if (model->details->root_node_reported == FALSE) {
			report_root_node_if_possible (model);
		}

		model->details->root_node_changed_signal_id = gtk_signal_connect 
			(GTK_OBJECT (nautilus_tree_node_get_file (model->details->root_node)),
			 "changed",
			 nautilus_tree_model_root_node_file_monitor,
			 model);
		
		monitor_attributes = g_list_prepend (NULL, "is directory");

		nautilus_file_monitor_add (nautilus_tree_node_get_file (model->details->root_node),
					   model,
					   monitor_attributes,
					   FALSE);

		g_list_free (monitor_attributes);
	}

	if (! g_list_find (model->details->monitor_clients, (gpointer) client)) {
		model->details->monitor_clients = g_list_prepend (model->details->monitor_clients, (gpointer) client);
	}

	if (model->details->root_node_reported) {
		reporting_queue = g_list_prepend (reporting_queue, model->details->root_node);

		while (reporting_queue != NULL) {
			current_node = (NautilusTreeNode *) reporting_queue->data;
			reporting_queue = g_list_remove_link (reporting_queue, reporting_queue);

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
	model->details->monitor_clients = g_list_remove (model->details->monitor_clients, (gpointer) client);

	if (model->details->monitor_clients == NULL) {
		/* FIXME: stop monitoring root node file, dunno what else */
	}
}


void
nautilus_tree_model_monitor_node (NautilusTreeModel         *model,
				  NautilusTreeNode          *node,
				  gconstpointer              client)
{
	NautilusDirectory *directory;
	GList             *monitor_attributes;

	if (!nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
		report_done_loading (model, node);
		return;
	}

	if (node->details->monitor_clients == NULL) {
		/* we must connect to signals */
		directory = nautilus_tree_node_get_directory (node);
		
		node->details->files_added_id = gtk_signal_connect 
			(GTK_OBJECT (directory),
			 "files_added",
			 nautilus_tree_model_directory_files_added_callback,
			 model);
		
		node->details->files_changed_id = gtk_signal_connect 
			(GTK_OBJECT (directory),
			 "files_changed",
			 nautilus_tree_model_directory_files_changed_callback,
			 model);

		node->details->done_loading_id = gtk_signal_connect 
			(GTK_OBJECT (directory),
			 "done_loading",
			 nautilus_tree_model_directory_done_loading_callback,
			 model);
	}

	monitor_attributes = g_list_prepend (NULL, "is directory");
	
	node->details->provisional_children = node->details->children;
	node->details->children = NULL;

	nautilus_directory_file_monitor_add (directory,
					     model,
					     monitor_attributes,
					     FALSE,
					     TRUE,
					     (NautilusDirectoryCallback) nautilus_tree_model_directory_files_added_callback,
					     model);
	
	g_list_free (monitor_attributes);

	if (! g_list_find (node->details->monitor_clients, (gpointer) client)) {
		node->details->monitor_clients = g_list_prepend (node->details->monitor_clients, (gpointer) client);
	}
}


void
nautilus_tree_model_stop_monitoring_node (NautilusTreeModel *model,
					  NautilusTreeNode  *node,
					  gconstpointer      client)
{
	if (!nautilus_file_is_directory (nautilus_tree_node_get_file (node)) || node->details->monitor_clients == NULL) {
		return;
	}

	node->details->monitor_clients = g_list_remove (node->details->monitor_clients, (gpointer) client);

	if (node->details->monitor_clients == NULL) {
		gtk_signal_disconnect (GTK_OBJECT (node), node->details->files_added_id);
		gtk_signal_disconnect (GTK_OBJECT (node), node->details->files_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (node), node->details->done_loading_id);
		
		nautilus_directory_file_monitor_remove (node->details->directory,
							model);
	}
}


NautilusTreeNode *
nautilus_tree_model_get_node (NautilusTreeModel *model,
			      const char *uri)
{
	return g_hash_table_lookup (model->details->uri_to_node_map, uri);
}


#if 0

NautilusTreeNode  *nautilus_tree_model_get_nearest_parent_node  (NautilusTreeModel *model,
								 const char *uri);


NautilusTreeNode  *nautilus_tree_model_get_root_node            (NautilusTreeModel *model);

#endif


/* helper functions */

static char *uri_get_parent_text (const char *uri_text)
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
		
		report_node_added (model, model->details->root_node);
	}
}


static void
report_node_added (NautilusTreeModel *model,
		   NautilusTreeNode  *node)
{
	char *uri;
	char *parent_uri;
	NautilusTreeNode *parent_node;

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (g_hash_table_lookup (model->details->uri_to_node_map, uri) == NULL) {
		parent_uri = uri_get_parent_text (uri);

		if (parent_uri != NULL) {
			parent_node = g_hash_table_lookup (model->details->uri_to_node_map, parent_uri);
			
			if (parent_node != NULL) {
				nautilus_tree_node_set_parent (node,
							       parent_node);
			}

			g_free (parent_uri);
		}


		g_hash_table_insert (model->details->uri_to_node_map, 
				     g_strdup (nautilus_file_get_uri (nautilus_tree_node_get_file (node))),
				     node);

		gtk_signal_emit (GTK_OBJECT (model),
				 signals[NODE_ADDED],
				 node);
	} else {
		/* FIXME: reconstruct internals and report that it changed?? */
	}
}


static void
report_node_changed (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	char *uri;
	char *parent_uri;
	NautilusTreeNode *parent_node;

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (g_hash_table_lookup (model->details->uri_to_node_map, uri) == NULL) {
		/* Actually added, go figure */
		
		parent_uri = uri_get_parent_text (uri);

		if (parent_uri != NULL) {
			parent_node = g_hash_table_lookup (model->details->uri_to_node_map, parent_uri);
			
			if (parent_node != NULL) {
				nautilus_tree_node_set_parent (node,
							       parent_node);
			}

			g_free (parent_uri);
		}


		g_hash_table_insert (model->details->uri_to_node_map, 
				     g_strdup (nautilus_file_get_uri (nautilus_tree_node_get_file (node))),
				     node);

		gtk_signal_emit (GTK_OBJECT (model),
				 signals[NODE_ADDED],
				 node);
	} else {
		/* really changed */
		gtk_signal_emit (GTK_OBJECT (model),
				 signals[NODE_CHANGED],
				 node);
	}
	
	g_free (uri);
}

static void
report_node_removed (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	char *uri;
	gpointer orig_key;
	gpointer dummy;
	char *parent_uri;
	NautilusTreeNode *parent_node;

	if (node == NULL) {
		return;
	}

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (g_hash_table_lookup (model->details->uri_to_node_map, uri) != NULL) {
		parent_uri = uri_get_parent_text (uri);

		if (parent_uri != NULL) {
			parent_node = g_hash_table_lookup (model->details->uri_to_node_map, parent_uri);
			
			if (parent_node != NULL) {
				nautilus_tree_remove_from_parent (node);
			}

			g_free (parent_uri);
		}


		g_hash_table_lookup_extended (model->details->uri_to_node_map, 
					      nautilus_file_get_uri (nautilus_tree_node_get_file (node)),
					      &orig_key,
					      &dummy);

		g_hash_table_remove (model->details->uri_to_node_map, 
				     nautilus_file_get_uri (nautilus_tree_node_get_file (node)));
		
		g_free (orig_key);
				     
		gtk_signal_emit (GTK_OBJECT (model),
				 signals[NODE_REMOVED],
				 node);


		gtk_object_unref (GTK_OBJECT (node));
	} 

	g_free (uri);

}


static void
report_done_loading (NautilusTreeModel *model,
		     NautilusTreeNode  *node)
{
	gtk_signal_emit (GTK_OBJECT (model),
			 signals[DONE_LOADING_CHILDREN],
			 node);
}


static void
nautilus_tree_stop_monitoring_internal (NautilusTreeModel *model)
{
	/* FIXME: stop monitoring everything; we should probably
           forget everything in the hash table too. */

}

/* signal/monitoring callbacks */

static void
nautilus_tree_model_root_node_file_monitor (NautilusFile      *file,
					    NautilusTreeModel *model)
{
	if (model->details->root_node_reported == FALSE) {
		report_root_node_if_possible (model);
	} else {
		report_node_changed (model,
				     model->details->root_node);
	}
}


static void
nautilus_tree_model_directory_files_changed_callback (NautilusDirectory        *directory,
						      GList                    *added_files,
						      NautilusTreeModel        *model)
{
	GList *p;
	NautilusFile     *file;
	NautilusTreeNode *node;
	char *uri;

	for (p = added_files; p != NULL; p = p->next) {
		file = (NautilusFile *) p->data;
		
		uri = nautilus_file_get_uri (file);
		node = nautilus_tree_model_get_node (model, uri);
		g_free (uri);

		if (!nautilus_directory_contains_file (directory, file)) {
			report_node_removed (model, node);
		} else {			
			report_node_changed (model, node);
		}
	} 
}

static void
nautilus_tree_model_directory_files_added_callback (NautilusDirectory        *directory,
						    GList                    *added_files,
						    NautilusTreeModel        *model)
{
	GList *p;
	NautilusFile     *file;
	NautilusTreeNode *node;

	for (p = added_files; p != NULL; p = p->next) {
		file = (NautilusFile *) p->data;
		
		node = nautilus_tree_node_new (file);
		
		report_node_added (model, node);
	}
}


static void
nautilus_tree_model_directory_done_loading_callback (NautilusDirectory        *directory,
						     NautilusTreeModel        *model)
{
	char *uri;
	NautilusTreeNode *node;

	uri = nautilus_directory_get_uri (directory);
	node = nautilus_tree_model_get_node (model, uri);
	g_free (uri);

	report_done_loading (model, node);
}
