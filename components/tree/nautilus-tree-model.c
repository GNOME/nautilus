/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright C) 2000, 2001 Eazel, Inc
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
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 *          Anders Carlsson <andersca@gnu.org>
 */

/* nautilus-tree-model.c - model for the tree view */

#include <config.h>
#include "nautilus-tree-model.h"

#include <string.h>
#include <gtk/gtktreemodel.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-icon-factory.h>

/* A single tree node */
typedef struct NautilusTreeNode NautilusTreeNode;

typedef enum {
	NAUTILUS_DUMMY_TREE_NODE_LOADING,
	NAUTILUS_DUMMY_TREE_NODE_EMPTY,
} NautilusDummyTreeNodeType;

struct NautilusTreeNode {
	int ref_count;

	NautilusFile *file;
	NautilusDirectory *directory;
	
	GdkPixbuf *closed_pixbuf;

	NautilusTreeNode *parent;
	NautilusTreeNode *children;
	
	NautilusTreeNode *next;
	NautilusTreeNode *prev;
	
	guint done_loading_id;
	guint files_added_id;
	guint files_changed_id;

	gboolean is_dummy_node;
	NautilusDummyTreeNodeType dummy_type;
};

struct NautilusTreeModelDetails {
	GHashTable *file_to_node_map;
	
	int stamp;
	NautilusTreeNode *root_node;
	
	guint root_node_changed_signal_id;
	gboolean root_node_reported;
};


static void nautilus_tree_model_class_init      (NautilusTreeModelClass *klass);
static void nautilus_tree_model_init            (NautilusTreeModel      *model);
static void nautilus_tree_model_tree_model_init (GtkTreeModelIface      *iface);

static void
nautilus_tree_node_set_parent (NautilusTreeNode *node, NautilusTreeNode *parent)
{
	g_return_if_fail (node->parent == NULL);
	
	node->parent = parent;
	
	node->next = parent->children;

	if (parent->children != NULL) {
		parent->children->prev = node;
	}

	parent->children = node;
}

static NautilusTreeNode *
nautilus_tree_node_new (NautilusFile *file)
{
	NautilusTreeNode *node;

	node = g_new0 (NautilusTreeNode, 1);

	node->file = nautilus_file_ref (file);

	return node;
}

static NautilusTreeNode *
nautilus_dummy_tree_node_new (NautilusDummyTreeNodeType type)
{
	NautilusTreeNode *node;

	node = g_new0 (NautilusTreeNode, 1);

	node->is_dummy_node = TRUE;
	node->dummy_type = type;

	return node;
}

static void
nautilus_tree_node_update_icons (NautilusTreeNode *node)
{
	GdkPixbuf *closed_pixbuf;

	closed_pixbuf = nautilus_icon_factory_get_pixbuf_for_file
		(node->file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS);

	if (node->closed_pixbuf) {
		g_object_unref (node->closed_pixbuf);
	}

	node->closed_pixbuf = closed_pixbuf;
}

GType
nautilus_tree_model_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (NautilusTreeModelClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) nautilus_tree_model_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (NautilusTreeModel),
			0,
			(GInstanceInitFunc) nautilus_tree_model_init,
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) nautilus_tree_model_tree_model_init,
			NULL,
			NULL
		};

		object_type = g_type_register_static (G_TYPE_OBJECT, "NautilusTreeModel", &object_info, 0);
		g_type_add_interface_static (object_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return object_type;
}

static void
nautilus_tree_model_init (NautilusTreeModel *model)
{
	model->details = g_new0 (NautilusTreeModelDetails, 1);
	model->details->stamp = g_random_int ();

	model->details->file_to_node_map = g_hash_table_new (NULL, NULL);
}

static void
nautilus_tree_model_class_init (NautilusTreeModelClass *klass)
{
}


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


static NautilusTreeNode *
nautilus_tree_model_get_node_from_file (NautilusTreeModel *model, NautilusFile *file)
{
	return g_hash_table_lookup (model->details->file_to_node_map, file);
}

static NautilusTreeNode *
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

static void
report_node_changed (NautilusTreeModel *model, NautilusTreeNode *node)
{
	char *node_uri, *parent_uri;
	char *file_uri;
	NautilusTreeNode *parent_node, *dummy_node;
	GtkTreeIter iter;
	GtkTreePath *path;
	
	if (nautilus_file_get_file_type (node->file) == GNOME_VFS_FILE_TYPE_UNKNOWN) {
		return;
	}

	node_uri = nautilus_file_get_uri (node->file);
	
	if (nautilus_tree_model_get_node_from_file (model, node->file) == NULL) {
		/* The file has been added */

		parent_uri = uri_get_parent_text (node_uri);

		if (parent_uri != NULL) {
			parent_node = nautilus_tree_model_get_node (model, parent_uri);

			if (parent_node != NULL) {
				nautilus_tree_node_set_parent (node, parent_node);
			}
			else {
				/* FIXME: Add register unparented node here */
			}

			g_free (parent_uri);
		}

		g_hash_table_insert (model->details->file_to_node_map,
				     node->file,
				     node);

		nautilus_tree_node_update_icons (node);

		/* Tell the view that things have changed */
		iter.stamp = model->details->stamp;
		iter.user_data = node;
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
		gtk_tree_path_free (path);

		
		if (node->directory == NULL && nautilus_file_is_directory (node->file)) {
			node->directory = nautilus_directory_get (node_uri);

			dummy_node = nautilus_dummy_tree_node_new (NAUTILUS_DUMMY_TREE_NODE_LOADING);
			nautilus_tree_node_set_parent (dummy_node, node);
			
			/* Tell the view that things have changed */
			iter.stamp = model->details->stamp;
			iter.user_data = dummy_node;
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
			gtk_tree_path_free (path);
		}

	}
	else {
		/* The file has changed */

		file_uri = nautilus_file_get_uri (node->file);

		if (strcmp (file_uri, node_uri) == 0) {
			/* A normal change */

			nautilus_tree_node_update_icons (node);
			
			iter.stamp = model->details->stamp;
			iter.user_data = node;
			
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
			gtk_tree_path_free (path);
		}
		else {
			/* FIXME: Implement rename support */
		}
	}
}

static void
report_root_node_if_possible (NautilusTreeModel *model)
{
	if (nautilus_file_get_file_type (model->details->root_node->file)
	    != GNOME_VFS_FILE_TYPE_UNKNOWN) {

		model->details->root_node_reported = TRUE;
		
		report_node_changed (model, model->details->root_node);
	}
}

static void
process_file_change (NautilusTreeModel *model,
		     NautilusFile *file)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node_from_file (model, file);

	if (node == NULL) {
		node = nautilus_tree_node_new (file);
		
		report_node_changed (model, node);
	}
}

static void
nautilus_tree_model_directory_files_changed_callback (NautilusDirectory *directory,
						      GList *changed_files,
						      gpointer callback_data)
{
	NautilusTreeModel *model;
	NautilusFile *file;
	GList *node;

	model = NAUTILUS_TREE_MODEL (callback_data);

	for (node = changed_files; node; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		process_file_change (model, file);
	}
}

static void
nautilus_tree_model_directory_done_loading_callback (NautilusDirectory        *directory,
						     NautilusTreeModel        *model)
{
	NautilusFile *file;

	file = nautilus_directory_get_corresponding_file (directory);
}

static void
nautilus_tree_model_begin_monitoring_directory (NautilusTreeModel *model, NautilusTreeNode *node)
{
	GList *monitor_attributes;
	NautilusDirectory *directory;
	
	if (node->done_loading_id != 0) {
		return;
	}

	directory = node->directory;
	
	monitor_attributes = nautilus_icon_factory_get_required_file_attributes ();
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY);
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);

	node->done_loading_id = g_signal_connect
		(directory,
		 "done_loading",
		 G_CALLBACK (nautilus_tree_model_directory_done_loading_callback),
		 model);
	
	node->files_added_id = g_signal_connect 
		(directory,
		 "files_added",
		 G_CALLBACK (nautilus_tree_model_directory_files_changed_callback),
		 model);
	
	node->files_changed_id = g_signal_connect 
		(directory,
		 "files_changed",
		 G_CALLBACK (nautilus_tree_model_directory_files_changed_callback),
		 model);	

	nautilus_directory_file_monitor_add (directory, model,
					     TRUE, TRUE, monitor_attributes,
					     nautilus_tree_model_directory_files_changed_callback,
					     model);
	g_list_free (monitor_attributes);
}

static int
nautilus_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
	return NAUTILUS_TREE_MODEL_NUM_COLUMNS;
}

static GType
nautilus_tree_model_get_column_type (GtkTreeModel *tree_model, int index)
{
	switch (index) {
	case NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN:
		return G_TYPE_STRING;
	case NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		return GDK_TYPE_PIXBUF;
	default:
		g_assert_not_reached ();
	}
	
	return G_TYPE_INVALID;
}

static gboolean
nautilus_tree_model_get_iter (GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
	NautilusTreeModel *tree_model;
	int *indices;
	GtkTreeIter parent;
	int depth, i;

	tree_model = NAUTILUS_TREE_MODEL (model);
	
	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	parent.stamp = tree_model->details->stamp;
	parent.user_data = NULL;

	if (gtk_tree_model_iter_nth_child (model, iter, NULL, indices[0]) == FALSE) {
		return FALSE;
	}

	for (i = 1; i < depth; i++) {
		parent = *iter;

		if (gtk_tree_model_iter_nth_child (model, iter, &parent, indices[i]) == FALSE) {
			return FALSE;
		}
	}

	return TRUE;
}

static GtkTreePath *
nautilus_tree_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeIter tmp_iter;
	NautilusTreeNode *tmp_node;
	NautilusTreeModel *tree_model;
	int i;

	tree_model = NAUTILUS_TREE_MODEL (model);

	i = 0;
	tmp_node = iter->user_data;

	if (tmp_node->parent == NULL) {
		path = gtk_tree_path_new ();
		tmp_node = tree_model->details->root_node;
	}
	else {
		tmp_iter = *iter;

		tmp_iter.user_data = tmp_node->parent;

		path = nautilus_tree_model_get_path (model, &tmp_iter);

		tmp_node = ((NautilusTreeNode *)iter->user_data)->parent->children;
	}

	for (; tmp_node; tmp_node = tmp_node->next) {
		if (tmp_node == iter->user_data) {
			break;
		}

		i++;
	}

	gtk_tree_path_append_index (path, i);
	
	return path;
}

static void
nautilus_tree_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value)
{
	NautilusTreeNode *node;
	NautilusFile *file;

	g_return_if_fail (iter->stamp == NAUTILUS_TREE_MODEL (tree_model)->details->stamp);
	
	file = NULL;
	
	node = iter->user_data;

	if (node != NULL) {
		file = node->file;
	}
	
	switch (column) {
	case NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_dummy_node) {
			g_value_set_string (value,
					    node->dummy_type == NAUTILUS_DUMMY_TREE_NODE_LOADING ?
					    ("Loading...") :
					    ("(Empty)"));
		}
		else {
			g_value_set_string_take_ownership (value, nautilus_file_get_display_name (file));
		}
		break;
	case NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, node->closed_pixbuf);
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
nautilus_tree_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
	NautilusTreeNode *node;

	g_return_val_if_fail (iter->stamp == NAUTILUS_TREE_MODEL (model)->details->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	
	node = iter->user_data;

	if (node->next != NULL) {
		iter->user_data = node->next;

		return TRUE;
	}
	else {
		return FALSE;
	}
}

static gboolean
nautilus_tree_model_iter_children (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	NautilusTreeNode *parent_node;

	g_return_val_if_fail (parent == NULL || parent->stamp == NAUTILUS_TREE_MODEL (model)->details->stamp, FALSE);
	
	parent_node = parent->user_data;

	g_assert (parent_node->children != NULL);

	/* We can call this function more than once,
	 * but it'll only begin monitoring the first time it's called.
	 */
	nautilus_tree_model_begin_monitoring_directory (NAUTILUS_TREE_MODEL (model),
							parent_node);

	iter->stamp = NAUTILUS_TREE_MODEL (model)->details->stamp;
	iter->user_data = parent_node->children;

	return TRUE;
}

static gboolean
nautilus_tree_model_iter_parent (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child)
{
	NautilusTreeNode *node;
	
	g_return_val_if_fail (iter->stamp == NAUTILUS_TREE_MODEL (model)->details->stamp, FALSE);

	node = child->user_data;

	if (node->parent == NULL) {
		return FALSE;
	}

	iter->user_data = node->parent;
	iter->stamp = NAUTILUS_TREE_MODEL (model)->details->stamp;
	
	return TRUE;
}

static gboolean
nautilus_tree_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	NautilusTreeNode *node;

	g_return_val_if_fail (iter->stamp == NAUTILUS_TREE_MODEL (model)->details->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	node = iter->user_data;

	return node->children != NULL;

}

static gboolean
nautilus_tree_model_iter_nth_child (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent, int n)
{
	NautilusTreeModel *tree_model;
	NautilusTreeNode *child;
	int i;
	
	tree_model = NAUTILUS_TREE_MODEL (model);

	if (parent == NULL) {
		child = tree_model->details->root_node;
	}
	else {
		child = ((NautilusTreeNode *)parent->user_data)->children;
	}

	for (i = 0; i < n; i++) {
		if (child == NULL) {
			break;
		}

		child = child->next;
	}

	if (child != NULL) {
		iter->user_data = child;
		iter->stamp = tree_model->details->stamp;
		
		return TRUE;
	}
	else {
		return FALSE;
	}
	
}

static void
nautilus_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_n_columns = nautilus_tree_model_get_n_columns;
	iface->get_column_type = nautilus_tree_model_get_column_type;
	iface->get_iter = nautilus_tree_model_get_iter;
	iface->get_path = nautilus_tree_model_get_path;
	iface->get_value = nautilus_tree_model_get_value;
	iface->iter_next = nautilus_tree_model_iter_next;
	iface->iter_children = nautilus_tree_model_iter_children;
	iface->iter_has_child = nautilus_tree_model_iter_has_child;
	iface->iter_parent = nautilus_tree_model_iter_parent;
	iface->iter_nth_child = nautilus_tree_model_iter_nth_child;
}

static void
nautilus_tree_model_root_node_file_monitor (NautilusFile *file, NautilusTreeModel *model)
{
	if (!model->details->root_node_reported) {
		report_root_node_if_possible (model);
	}
	else {
		report_node_changed (model,
				     model->details->root_node);
	}
}


static void
nautilus_tree_model_set_root_uri (NautilusTreeModel *model, const char *root_uri)
{
	NautilusFile *file;
	GList *monitor_attributes;
	
	/* You can only set the root node once */
	g_return_if_fail (model->details->root_node == NULL);
	
	file = nautilus_file_get (root_uri);
	model->details->root_node = nautilus_tree_node_new (file);
	
	if (!model->details->root_node_reported) {
		report_root_node_if_possible (model);
	}
	
	/* Setup a file monitor for the root node */
	model->details->root_node_changed_signal_id = g_signal_connect
		(file,
		 "changed",
		 G_CALLBACK (nautilus_tree_model_root_node_file_monitor),
		 model);

	monitor_attributes = nautilus_icon_factory_get_required_file_attributes ();
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY);
	monitor_attributes = g_list_prepend (monitor_attributes, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
	nautilus_file_monitor_add (file,
				   model,
				   monitor_attributes);
	g_list_free (monitor_attributes);

	report_root_node_if_possible (model);
}

NautilusTreeModel *
nautilus_tree_model_new (const char *root_uri)
{
	NautilusTreeModel *model;

	model = g_object_new (NAUTILUS_TYPE_TREE_MODEL, NULL);
	nautilus_tree_model_set_root_uri (model, root_uri);
	
	return model;
}

static void
nautilus_tree_model_dump_helper (NautilusTreeNode *node, int indent)
{
	int i;
	NautilusTreeNode *tmp_node;
	
	for (i = 0; i < indent; i++)
		g_print (" ");

	if (node->is_dummy_node)
		g_print ("%p %s\n", node, node->dummy_type == NAUTILUS_DUMMY_TREE_NODE_LOADING ?
			 ("Loading...") :
			 ("(Empty)"));
	else
		g_print ("%p %s\n", node, node->file ? nautilus_file_get_name (node->file): "(unknown)");

	for (tmp_node = node->children; tmp_node; tmp_node = tmp_node->next) {
		nautilus_tree_model_dump_helper (tmp_node, indent + 1);
	}
}

void
nautilus_tree_model_dump (NautilusTreeModel *model)
{
	nautilus_tree_model_dump_helper (model->details->root_node, 0);
}
