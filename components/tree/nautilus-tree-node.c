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
#include "nautilus-tree-node.h"

#include "nautilus-tree-node-private.h"
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file.h>
#include <eel/eel-gtk-macros.h>


static void               nautilus_tree_node_destroy          (GtkObject   *object);
static void               nautilus_tree_node_initialize       (gpointer     object,
								gpointer     klass);
static void               nautilus_tree_node_initialize_class (gpointer     klass);



EEL_DEFINE_CLASS_BOILERPLATE (NautilusTreeNode, nautilus_tree_node, GTK_TYPE_OBJECT)


static void
nautilus_tree_node_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_node_destroy;
}

static void
nautilus_tree_node_initialize (gpointer object, gpointer klass)
{
	NautilusTreeNode *node;

	node = NAUTILUS_TREE_NODE (object);

	node->details = g_new0 (NautilusTreeNodeDetails, 1);
}


static void
nautilus_tree_node_destroy (GtkObject *object)
{
	NautilusTreeNode *node;

	node = NAUTILUS_TREE_NODE (object);

	if (node->details->children != NULL) {
		g_warning ("Destroyed tree node still has children");
		nautilus_tree_node_remove_children (node);
	}

	nautilus_directory_unref (node->details->directory);
	nautilus_file_unref (node->details->file);
	g_free (node->details->uri);
	g_free (node->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusTreeNode *
nautilus_tree_node_new (NautilusFile *file)
{
	NautilusTreeNode *node;

	node = NAUTILUS_TREE_NODE (gtk_object_new (NAUTILUS_TYPE_TREE_NODE, NULL));
	gtk_object_ref (GTK_OBJECT (node));
	gtk_object_sink (GTK_OBJECT (node));

	node->details->file = nautilus_file_ref (file);
	nautilus_tree_node_update_uri (node);

	return node;
}



NautilusTreeNode *
nautilus_tree_node_get_parent (NautilusTreeNode   *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_NODE (node), NULL);

	return node->details->parent;
}

GList *
nautilus_tree_node_get_children  (NautilusTreeNode   *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_NODE (node), NULL);

	return node->details->children;
}

NautilusFile *
nautilus_tree_node_get_file      (NautilusTreeNode   *node)
{
	/* This is assumed to always return non-null, so have to crash.. */
	g_assert (NAUTILUS_IS_TREE_NODE (node));

	return node->details->file;
}

char *
nautilus_tree_node_get_uri      (NautilusTreeNode   *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_NODE (node), NULL);

	return g_strdup (node->details->uri);
}

void
nautilus_tree_node_update_uri (NautilusTreeNode *node)
{
	char *uri, *parent_uri;

	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	uri = nautilus_file_get_uri (node->details->file);

	g_free (node->details->uri);
	node->details->uri = uri;

	parent_uri = nautilus_file_get_parent_uri (node->details->file);
	node->details->is_toplevel = (parent_uri == NULL || *parent_uri == '\0');
	g_free (parent_uri);
}


NautilusDirectory *
nautilus_tree_node_get_directory (NautilusTreeNode   *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_NODE (node), NULL);

	return node->details->directory;
}

void
nautilus_tree_node_set_parent (NautilusTreeNode   *node,
			       NautilusTreeNode   *parent)
{
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (parent));
	g_return_if_fail (node->details->parent == NULL);

	node->details->parent = parent;
	parent->details->children = g_list_prepend (parent->details->children, node);
}

gboolean
nautilus_tree_node_is_toplevel (NautilusTreeNode *node)
{
	g_return_val_if_fail (NAUTILUS_IS_TREE_NODE (node), FALSE);

	return node->details->is_toplevel;
}


void
nautilus_tree_node_remove_from_parent (NautilusTreeNode *node)
{
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	if (node->details->parent != NULL) {
		node->details->parent->details->children = g_list_remove
			(node->details->parent->details->children, node);
		node->details->parent = NULL;
	}
}

void
nautilus_tree_node_remove_children (NautilusTreeNode *node)
{
	GList *p;
	NautilusTreeNode *child;

	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	for (p = node->details->children; p != NULL; p = p->next) {
		child = p->data;
		child->details->parent = NULL;
	}

	g_list_free (node->details->children);
	node->details->children = NULL;
}
