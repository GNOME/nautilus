/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists. 

   Copyright (C) 2001, 2002 Anders Carlsson

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <gtk/gtktreemodel.h>
#include <libnautilus-private/nautilus-file.h>

#ifndef FM_LIST_MODEL_H
#define FM_LIST_MODEL_H

#define FM_TYPE_LIST_MODEL		(fm_list_model_get_type ())
#define FM_LIST_MODEL(obj)		(GTK_CHECK_CAST ((obj), FM_TYPE_LIST_MODEL, FMListModel))
#define FM_LIST_MODEL_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_LIST_MODEL, FMListModelClass))
#define FM_IS_LIST_MODEL(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_LIST_MODEL))
#define FM_IS_LIST_MODEL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_LIST_MODEL))

enum {
	FM_LIST_MODEL_FILE_COLUMN,
	FM_LIST_MODEL_ICON_COLUMN,
	FM_LIST_MODEL_NAME_COLUMN,
	FM_LIST_MODEL_SIZE_COLUMN,
	FM_LIST_MODEL_TYPE_COLUMN,
	FM_LIST_MODEL_DATE_MODIFIED_COLUMN,
	FM_LIST_MODEL_NUM_COLUMNS
};

typedef struct FMListModelDetails FMListModelDetails;

typedef struct FMListModel {
	GObject parent_instance;
	FMListModelDetails *details;
} FMListModel;

typedef struct {
	GObjectClass parent_class;
} FMListModelClass;

GType    fm_list_model_get_type                          (void);
void     fm_list_model_add_file                          (FMListModel          *model,
							  NautilusFile         *file);
void     fm_list_model_file_changed                      (FMListModel          *model,
							  NautilusFile         *file);
gboolean fm_list_model_is_empty                          (FMListModel          *model);
void     fm_list_model_remove_file                       (FMListModel          *model,
							  NautilusFile         *file);
void     fm_list_model_clear                             (FMListModel          *model);
gboolean fm_list_model_get_tree_iter_from_file           (FMListModel          *model,
							  NautilusFile         *file,
							  GtkTreeIter          *iter);
void     fm_list_model_set_should_sort_directories_first (FMListModel          *model,
							  gboolean              sort_directories_first);
int      fm_list_model_get_sort_column_id_from_attribute (const char           *attribute);
int      fm_list_model_get_sort_column_id_from_sort_type (NautilusFileSortType  sort_type);

#endif /* FM_LIST_MODEL_H */
