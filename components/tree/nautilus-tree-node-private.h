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

/* nautilus-tree-node-private.h - Private header shared between tree
   node and model. */


#ifndef NAUTILUS_TREE_NODE_PRIVATE_H
#define NAUTILUS_TREE_NODE_PRIVATE_H

#include "nautilus-tree-node.h"

struct NautilusTreeNodeDetails {
	NautilusFile *file;
	NautilusDirectory *directory;
	
	char *uri;

	guint files_added_id;
	guint files_changed_id;
	guint done_loading_id;

	GList *monitor_clients;

	NautilusTreeNode *parent;
	GList *children;

	gboolean is_toplevel;
};

NautilusTreeNode *nautilus_tree_node_new (NautilusFile *file);

void		  nautilus_tree_node_update_uri (NautilusTreeNode *node);

void              nautilus_tree_node_set_parent (NautilusTreeNode   *node,
						 NautilusTreeNode   *parent);

void  		  nautilus_tree_node_remove_from_parent (NautilusTreeNode *node);

void		  nautilus_tree_node_remove_children (NautilusTreeNode *node);

#endif /* NAUTILUS_TREE_NODE_PRIVATE_H */



