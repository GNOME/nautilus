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



struct NautilusTreeModelDetails {
	GHashTable *uri_to_node_map;

	NautilusTreeNode *root_node;
};


#if 0
	void         (*node_added)            (NautilusTreeModel *model,
					       NautilusTreeNode *node);

	void         (*node_changed)          (NautilusTreeModel *model,
					       NautilusTreeNode *node);

	void         (*node_removed)          (NautilusTreeModel *model,
					       NautilusTreeNode *node);

	void         (*done_loading_children) (NautilusTreeModel *model,
					       NautilusTreeNode *node);
#endif

GtkType            nautilus_tree_model_get_type                 (void);

NautilusTreeModel *nautilus_tree_model_new                      (const char *root_uri);


NautilusTreeModel *nautilus_tree_model_set_root_uri             (const char *root_uri);


void               nautilus_tree_model_load_children            (NautilusTreeModel *model,
								 NautilusTreeNode  *node);

void               nautilus_tree_model_stop_monitoring_children (NautilusTreeModel *model,
								 NautilusTreeNode  *node);

NautilusTreeNode  *nautilus_tree_model_get_node                 (NautilusTreeModel *model,
								 const char *uri);

NautilusTreeNode  *nautilus_tree_model_get_nearest_parent_node  (NautilusTreeModel *model,
								 const char *uri);


NautilusTreeNode  *nautilus_tree_model_get_root_node            (NautilusTreeModel *model);


