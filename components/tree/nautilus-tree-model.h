/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Bent Spoon Software
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
 * Author: Anders Carlsson <andersca@gnu.org>
 */

/* nautilus-tree-model.h - Model for the tree view */

#ifndef NAUTILUS_TREE_MODEL_H
#define NAUTILUS_TREE_MODEL_H

#include <glib-object.h>
#include <gtk/gtktreemodel.h>
#include <libnautilus-private/nautilus-file.h>

#define NAUTILUS_TYPE_TREE_MODEL	    (nautilus_tree_model_get_type ())
#define NAUTILUS_TREE_MODEL(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_MODEL, NautilusTreeModel))
#define NAUTILUS_TREE_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_MODEL, NautilusTreeModelClass))
#define NAUTILUS_IS_TREE_MODEL(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_MODEL))
#define NAUTILUS_IS_TREE_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_MODEL))

enum {
	NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN,
	NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
	NAUTILUS_TREE_MODEL_OPEN_PIXBUF_COLUMN,
	NAUTILUS_TREE_MODEL_FONT_STYLE_COLUMN,
	NAUTILUS_TREE_MODEL_NUM_COLUMNS
};

typedef struct NautilusTreeModelDetails NautilusTreeModelDetails;

typedef struct {
	GObject parent;
	NautilusTreeModelDetails *details;
} NautilusTreeModel;

typedef struct {
	GObjectClass parent_class;
} NautilusTreeModelClass;

GType              nautilus_tree_model_get_type                  (void);
NautilusTreeModel *nautilus_tree_model_new                       (const char        *opt_root_uri);
void               nautilus_tree_model_set_show_hidden_files     (NautilusTreeModel *model,
								  gboolean           show_hidden_files);
void               nautilus_tree_model_set_show_backup_files     (NautilusTreeModel *model,
								  gboolean           show_backup_files);
void               nautilus_tree_model_set_show_only_directories (NautilusTreeModel *model,
								  gboolean           show_only_directories);
NautilusFile *     nautilus_tree_model_iter_get_file             (NautilusTreeModel *model,
								  GtkTreeIter       *iter);
void               nautilus_tree_model_set_root_uri              (NautilusTreeModel *model,
								  const char        *root_uri);

void               nautilus_tree_model_set_theme                 (NautilusTreeModel *model);

#endif /* NAUTILUS_TREE_MODEL_H */
