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

/* nautilus-tree-model.h - sample content model
   component. This component just displays a simple label of the URI
   and does nothing else. It should be a good basis for writing
   out-of-proc content models.*/

#ifndef NAUTILUS_TREE_MODEL_H
#define NAUTILUS_TREE_MODEL_H

#include <gtk/gtkobject.h>

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
};

/* GtkObject support */
GtkType       nautilus_tree_model_get_type          (void);




#endif /* NAUTILUS_TREE_MODEL_H */
