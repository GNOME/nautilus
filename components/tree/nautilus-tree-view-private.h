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
 * Authors: 
 *       Maciej Stachowiak <mjs@eazel.com>
 */

#ifndef NAUTILUS_TREE_VIEW_PRIVATE_H
#define NAUTILUS_TREE_VIEW_PRIVATE_H

#include "nautilus-tree-view.h"
#include "nautilus-tree-change-queue.h"
#include "nautilus-tree-expansion-state.h"
#include "nautilus-tree-model.h"

#include <libnautilus-extensions/nautilus-ctree.h>
#include <libnautilus/nautilus-view.h>

typedef struct NautilusTreeViewDndDetails NautilusTreeViewDndDetails;

typedef void (*TreeViewCallback) (NautilusTreeView *view);

#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))
				


/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;

	GtkWidget *tree;

	NautilusTreeModel *model;

	GHashTable *file_to_node_map;

	gboolean show_hidden_files;
	gboolean show_backup_files;
	gboolean show_non_directories;

	NautilusTreeExpansionState *expansion_state;
	char *selected_uri;
	char *current_main_view_uri;

	NautilusTreeChangeQueue *change_queue;
	guint pending_idle_id;

	TreeViewCallback root_seen_callback;
	char *wait_uri;
	NautilusTreeNode *wait_node;
	TreeViewCallback uri_loaded_or_parent_done_loading;
        GList *in_progress_select_uris;
        gboolean root_seen;

	gboolean got_first_size_allocate;

	NautilusFile *activation_uri_wait_file;

	NautilusTreeViewDndDetails *dnd;
};

NautilusTreeNode  *nautilus_tree_view_node_to_model_node      (NautilusTreeView  *view,
							       NautilusCTreeNode *node);
NautilusFile      *nautilus_tree_view_node_to_file            (NautilusTreeView  *view,
							       NautilusCTreeNode *node);
NautilusCTreeNode *nautilus_tree_view_model_node_to_view_node (NautilusTreeView  *view,
							       NautilusTreeNode  *node);
void               nautilus_tree_view_init_dnd                (NautilusTreeView *view);
void               nautilus_tree_view_free_dnd                (NautilusTreeView *view);


#endif /* NAUTILUS_TREE_VIEW_PRIVATE_H */
