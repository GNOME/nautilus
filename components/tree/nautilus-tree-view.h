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

/* nautilus-tree-view.h - sample content view
   component. This component just displays a simple label of the URI
   and does nothing else. It should be a good basis for writing
   out-of-proc content views.*/

#ifndef NAUTILUS_TREE_VIEW_H
#define NAUTILUS_TREE_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>

typedef struct NautilusTreeView NautilusTreeView;
typedef struct NautilusTreeViewClass NautilusTreeViewClass;

#define NAUTILUS_TYPE_TREE_VIEW	           (nautilus_tree_view_get_type ())
#define NAUTILUS_TREE_VIEW(obj)	           (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_VIEW, NautilusTreeView))
#define NAUTILUS_TREE_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_VIEW, NautilusTreeViewClass))
#define NAUTILUS_IS_TREE_VIEW(obj)	   (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_VIEW))
#define NAUTILUS_IS_TREE_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_VIEW))

typedef struct NautilusTreeViewDetails NautilusTreeViewDetails;

struct NautilusTreeView {
	GtkScrolledWindow parent;
	NautilusTreeViewDetails *details;
};

struct NautilusTreeViewClass {
	GtkScrolledWindowClass parent_class;
};

/* GtkObject support */
GtkType       nautilus_tree_view_get_type          (void);

/* Component embedding support */
NautilusView *nautilus_tree_view_get_nautilus_view (NautilusTreeView *view);


#endif /* NAUTILUS_TREE_VIEW_H */


