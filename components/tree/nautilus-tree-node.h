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

/* nautilus-tree-node.h - A single node for the tree model. */

#ifndef NAUTILUS_TREE_NODE_H
#define NAUTILUS_TREE_NODE_H

#include <gtk/gtkobject.h>
#include <glib.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>

typedef struct NautilusTreeNode NautilusTreeNode;
typedef struct NautilusTreeNodeClass NautilusTreeNodeClass;

#define NAUTILUS_TYPE_TREE_NODE	    (nautilus_tree_node_get_type ())
#define NAUTILUS_TREE_NODE(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_NODE, NautilusTreeNode))
#define NAUTILUS_TREE_NODE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_NODE, NautilusTreeNodeClass))
#define NAUTILUS_IS_TREE_NODE(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_NODE))
#define NAUTILUS_IS_TREE_NODE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_NODE))

typedef struct NautilusTreeNodeDetails NautilusTreeNodeDetails;

struct NautilusTreeNode {
	GtkObject parent;
	NautilusTreeNodeDetails *details;
};

struct NautilusTreeNodeClass {
	GtkObjectClass parent_class;
};


GtkType            nautilus_tree_node_get_type      (void);

NautilusTreeNode  *nautilus_tree_node_get_parent    (NautilusTreeNode   *node);
GList             *nautilus_tree_node_get_children  (NautilusTreeNode   *node);
NautilusFile      *nautilus_tree_node_get_file      (NautilusTreeNode   *node);
NautilusDirectory *nautilus_tree_node_get_directory (NautilusTreeNode   *node);

void              nautilus_tree_node_set_user_data (NautilusTreeNode   *node,
						    gpointer           user_data,
						    GDestroyNotify     destroy_notify);

gpointer          nautilus_tree_node_get_user_data (NautilusTreeNode *node);
						    

#endif /* NAUTILUS_TREE_NODE_H */


