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
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>


static void               nautilus_tree_node_destroy          (GtkObject   *object);
static void               nautilus_tree_node_initialize       (gpointer     object,
								gpointer     klass);
static void               nautilus_tree_node_initialize_class (gpointer     klass);



NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeNode, nautilus_tree_node, GTK_TYPE_OBJECT)


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

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusTreeNode *
nautilus_tree_node_new (NautilusFile *file)
{
	NautilusTreeNode *node;

	node = NAUTILUS_TREE_NODE (gtk_object_new (NAUTILUS_TYPE_TREE_NODE, NULL));
	gtk_object_ref (GTK_OBJECT (node));
	gtk_object_sink (GTK_OBJECT (node));

	node->details->file = nautilus_file_ref (file);
	node->details->uri = nautilus_file_get_uri (file);

	return node;
}



NautilusTreeNode *
nautilus_tree_node_get_parent (NautilusTreeNode   *node)
{
	return node->details->parent;
}

GList *
nautilus_tree_node_get_children  (NautilusTreeNode   *node)
{
	return node->details->children;
}

NautilusFile *
nautilus_tree_node_get_file      (NautilusTreeNode   *node)
{
	return node->details->file;
}

char *
nautilus_tree_node_get_uri      (NautilusTreeNode   *node)
{
	return g_strdup (node->details->uri);
}


NautilusDirectory *
nautilus_tree_node_get_directory (NautilusTreeNode   *node)
{
	return node->details->directory;
}

void
nautilus_tree_node_set_parent (NautilusTreeNode   *node,
			       NautilusTreeNode   *parent)
{
	g_return_if_fail (node->details->parent == NULL);

	node->details->parent = parent;
	parent->details->children = g_list_append (parent->details->children, node);
}


void
nautilus_tree_node_remove_from_parent (NautilusTreeNode *node)
{
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

	for (p = node->details->children; p != NULL; p = p->next) {
		child = p->data;
		child->details->parent = NULL;
	}

	g_list_free (node->details->children);
	node->details->children = NULL;
}
