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


#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-directory.h>




NautilusTreeNode *
nautilus_tree_node_get_parent (NautilusTreeNode   *node)
{
	return node->details->parent;
}

NautilusTreeNode *
nautilus_tree_node_get_children  (NautilusTreeNode   *node)
{
	return node->details->children;
}

NautilusTreeNode *
nautilus_tree_node_get_file      (NautilusTreeNode   *node)
{
	return node->details->file;
}

NautilusTreeNode *
nautilus_tree_node_get_directory (NautilusTreeNode   *node)
{
	return node->details->directory;
}


void
nautilus_tree_node_set_user_data (NautilusTreeNode   *node,
				  gpointer           user_data,
				  GDestroyNotifyFunc destroy_notify)
{
	if (node->details->user_data != NULL) {
		/* FIXME: call old destroy_notify */
	}

	node->details->user_data = user_data;
	node->details->destroy_notify = destroy_notify;
}

gpointer
nautilus_tree_node_get_user_data (NautilusTreeNode *node)
{
	return node->details->user_data;

}


