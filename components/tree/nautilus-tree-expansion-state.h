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

/* nautilus-tree-expansion-state.h - Class to track what nodes are
   expanded for the tree view */

#ifndef NAUTILUS_TREE_EXPANSION_STATE_H
#define NAUTILUS_TREE_EXPANSION_STATE_H

#include <gtk/gtkobject.h>

typedef struct NautilusTreeExpansionState NautilusTreeExpansionState;
typedef struct NautilusTreeExpansionStateClass NautilusTreeExpansionStateClass;

#define NAUTILUS_TYPE_TREE_EXPANSION_STATE	    (nautilus_tree_expansion_state_get_type ())
#define NAUTILUS_TREE_EXPANSION_STATE(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_EXPANSION_STATE, NautilusTreeExpansionState))
#define NAUTILUS_TREE_EXPANSION_STATE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_EXPANSION_STATE, NautilusTreeExpansionStateClass))
#define NAUTILUS_IS_TREE_EXPANSION_STATE(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_EXPANSION_STATE))
#define NAUTILUS_IS_TREE_EXPANSION_STATE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_EXPANSION_STATE))

typedef struct NautilusTreeExpansionStateDetails NautilusTreeExpansionStateDetails;


struct NautilusTreeExpansionState {
	GtkObject parent;
	NautilusTreeExpansionStateDetails *details;
};

struct NautilusTreeExpansionStateClass {
	GtkObjectClass parent_class;
};



GtkType                     nautilus_tree_expansion_state_get_type          (void);
NautilusTreeExpansionState *nautilus_tree_expansion_state_new               (void);

gboolean                    nautilus_tree_expansion_state_is_node_expanded  (NautilusTreeExpansionState *expansion_state,
									     const char                 *uri);
gboolean                    nautilus_tree_expansion_state_was_ever_expanded (NautilusTreeExpansionState *expansion_state,
									     const char                 *uri);
void                        nautilus_tree_expansion_state_expand_node       (NautilusTreeExpansionState *expansion_state,
									     const char                 *uri);
void                        nautilus_tree_expansion_state_collapse_node     (NautilusTreeExpansionState *expansion_state,
									     const char                 *uri);
void                        nautilus_tree_expansion_state_remove_node       (NautilusTreeExpansionState *expansion_state,
									     const char                 *uri);
void                        nautilus_tree_expansion_state_save              (NautilusTreeExpansionState *expansion_state);




#endif /* NAUTILUS_TREE_EXPANSION_STATE_H */

