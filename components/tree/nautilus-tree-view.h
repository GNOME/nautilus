/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
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
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 *          Anders Carlsson <andersca@gnu.org> 
 */

/* nautilus-tree-view.h - tree view. */


#ifndef NAUTILUS_TREE_VIEW_H
#define NAUTILUS_TREE_VIEW_H

#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_TREE_VIEW	           (nautilus_tree_view_get_type ())
#define NAUTILUS_TREE_VIEW(obj)	           (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_VIEW, NautilusTreeView))
#define NAUTILUS_TREE_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_VIEW, NautilusTreeViewClass))
#define NAUTILUS_IS_TREE_VIEW(obj)	   (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_VIEW))
#define NAUTILUS_IS_TREE_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_VIEW))

typedef struct NautilusTreeViewDetails NautilusTreeViewDetails;

typedef struct {
	NautilusView parent;
	NautilusTreeViewDetails *details;
} NautilusTreeView;

typedef struct {
	NautilusViewClass parent_class;
} NautilusTreeViewClass;

GType nautilus_tree_view_get_type (void);

#endif /* NAUTILUS_TREE_VIEW_H */
