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

/* nautilus-tree-model.h - Model for the tree view */

#ifndef NAUTILUS_TREE_MODEL_H
#define NAUTILUS_TREE_MODEL_H

#include <gtk/gtkobject.h>
#include "nautilus-tree-node.h"

typedef struct NautilusTreeModel NautilusTreeModel;
typedef struct NautilusTreeModelClass NautilusTreeModelClass;

#define NAUTILUS_TYPE_TREE_MODEL	    (nautilus_tree_model_get_type ())
#define NAUTILUS_TREE_MODEL(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_MODEL, NautilusTreeModel))
#define NAUTILUS_TREE_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_MODEL, NautilusTreeModelClass))
#define NAUTILUS_IS_TREE_MODEL(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_MODEL))
#define NAUTILUS_IS_TREE_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_MODEL))

typedef struct NautilusTreeModelDetails NautilusTreeModelDetails;


struct NautilusTreeModel {
	GtkObject parent;
	NautilusTreeModelDetails *details;
};

struct NautilusTreeModelClass {
	GtkObjectClass parent_class;

	void         (*node_changed)          (NautilusTreeModel *model,
					       NautilusTreeNode *node);

	void         (*node_removed)          (NautilusTreeModel *model,
					       NautilusTreeNode *node);

	void         (*node_being_renamed)    (NautilusTreeModel *model,
					       const char *old_uri,
					       const char *new_uri);

	void         (*done_loading_children) (NautilusTreeModel *model,
					       NautilusTreeNode *node);
};

typedef void (*NautilusTreeModelCallback) (NautilusTreeModel *model,
					   NautilusTreeNode  *node,
					   gpointer           callback_data);


GtkType            nautilus_tree_model_get_type                 (void);

NautilusTreeModel *nautilus_tree_model_new                      (const char *root_uri);


void               nautilus_tree_model_monitor_add              (NautilusTreeModel         *model,
								 gconstpointer              client,
								 NautilusTreeModelCallback  initial_nodes_callback,
								 gpointer                   callback_data);

void               nautilus_tree_model_monitor_remove           (NautilusTreeModel         *model,
								 gconstpointer              client);

void               nautilus_tree_model_monitor_node             (NautilusTreeModel         *model,
								 NautilusTreeNode          *node,
								 gconstpointer              client,
								 gboolean                   force_reload); 

void               nautilus_tree_model_stop_monitoring_node     (NautilusTreeModel         *model,
								 NautilusTreeNode          *node,
								 gconstpointer              client);

void               nautilus_tree_model_stop_monitoring_node_recursive (NautilusTreeModel *model,
								       NautilusTreeNode  *node,
								       gconstpointer      client);

NautilusTreeNode  *nautilus_tree_model_get_node                 (NautilusTreeModel *model,
								 const char        *uri);


NautilusTreeNode  *nautilus_tree_model_get_node_from_file       (NautilusTreeModel *model,
								 NautilusFile      *file);

#if 0
NautilusTreeNode  *nautilus_tree_model_get_nearest_parent_node  (NautilusTreeModel *model,
								 NautilusFile      *file);


NautilusTreeNode  *nautilus_tree_model_get_root_node            (NautilusTreeModel *model);
#endif

void               nautilus_tree_model_set_defer_notifications  (NautilusTreeModel *model,
								 gboolean           defer);

/* Debugging */
void		   nautilus_tree_model_dump_files		(NautilusTreeModel *model);


#endif /* NAUTILUS_TREE_MODEL_H */

