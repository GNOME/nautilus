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

#include <config.h>
#include "fm-list-model.h"

#include <string.h>
#include <gtk/gtktreednd.h>
#include <gtk/gtktreesortable.h>
#include <libnautilus-private/nautilus-icon-factory.h>

#define G_SLIST(x) ((GSList *) x)

struct FMListModelDetails {
	GSList *files;
	GSList *tail;
	int length;

	int stamp;

	int sort_column_id;
	GtkSortType order;

	gboolean sort_directories_first;
};

typedef struct {
	const char *attribute_name;
	int sort_column_id;
} AttributeEntry;

static const AttributeEntry attributes[] = {
	{ "name", FM_LIST_MODEL_NAME_COLUMN },
	{ "icon", FM_LIST_MODEL_TYPE_COLUMN },
#ifdef GNOME2_CONVERSION_COMPLETE
	{ "emblems", FM_LIST_MODEL_EMBLEMS_COLUMN },
#endif
	{ "size", FM_LIST_MODEL_SIZE_COLUMN },
	{ "type", FM_LIST_MODEL_TYPE_COLUMN },
	{ "date_modified", FM_LIST_MODEL_DATE_MODIFIED_COLUMN },
};

static guint
fm_list_model_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static int
fm_list_model_get_n_columns (GtkTreeModel *tree_model)
{
	return FM_LIST_MODEL_NUM_COLUMNS;
}

static GType
fm_list_model_get_column_type (GtkTreeModel *tree_model, int index)
{
	switch (index) {
	case FM_LIST_MODEL_FILE_COLUMN:
		return NAUTILUS_TYPE_FILE;
	case FM_LIST_MODEL_NAME_COLUMN:
	case FM_LIST_MODEL_SIZE_COLUMN:
	case FM_LIST_MODEL_TYPE_COLUMN:
	case FM_LIST_MODEL_DATE_MODIFIED_COLUMN:
		return G_TYPE_STRING;
	case FM_LIST_MODEL_ICON_COLUMN:
		return GDK_TYPE_PIXBUF;
	default:
		return G_TYPE_INVALID;
	}
}

static gboolean
fm_list_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	FMListModel *model;
	GSList *list;
	int i;
	
	model = (FMListModel *)tree_model;

	i = gtk_tree_path_get_indices (path)[0];

	if (i >= model->details->length) {
		return FALSE;
	}

	list = g_slist_nth (model->details->files, i);

	iter->stamp = model->details->stamp;
	iter->user_data = list;

	return TRUE;
}

static GtkTreePath *
fm_list_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	FMListModel *model;
	GSList *list;
	int i;

	model = (FMListModel *)tree_model;
	
	g_return_val_if_fail (iter->stamp == model->details->stamp, NULL);
	
	i = 0;

	for (list = model->details->files; list; list = list->next) {
		if (list == iter->user_data) {
			break;
		}

		i++;
	}

	if (list == NULL) {
		return NULL;
	}

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, i);
	
	return path;
}


static void
fm_list_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value)
{
	FMListModel *model;
	NautilusFile *file;
	char *str;
	GdkPixbuf *icon;
	
	model = (FMListModel *)tree_model;

	g_return_if_fail (model->details->stamp == iter->stamp);

	file = G_SLIST (iter->user_data)->data;
	
	switch (column) {
	case FM_LIST_MODEL_FILE_COLUMN:
		g_value_init (value, NAUTILUS_TYPE_FILE);

		g_value_set_object (value, file);
		break;
	case FM_LIST_MODEL_NAME_COLUMN:
		g_value_init (value, G_TYPE_STRING);

		str = nautilus_file_get_string_attribute_with_default (file, "name");
		g_value_set_string_take_ownership (value, str);
		break;
	case FM_LIST_MODEL_ICON_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);

		icon = nautilus_icon_factory_get_pixbuf_for_file (file, NULL, NAUTILUS_ICON_SIZE_SMALLER);
		g_value_set_object (value, icon);
		g_object_unref (icon);
		break;
	case FM_LIST_MODEL_SIZE_COLUMN:
		g_value_init (value, G_TYPE_STRING);

		str = nautilus_file_get_string_attribute_with_default (file, "size");
		g_value_set_string_take_ownership (value, str);
		break;
	case FM_LIST_MODEL_TYPE_COLUMN:
		g_value_init (value, G_TYPE_STRING);

		str = nautilus_file_get_string_attribute_with_default (file, "type");
		g_value_set_string_take_ownership (value, str);
		break;
	case FM_LIST_MODEL_DATE_MODIFIED_COLUMN:
		g_value_init (value, G_TYPE_STRING);

		str = nautilus_file_get_string_attribute_with_default (file, "date_modified");
		g_value_set_string_take_ownership (value, str);
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
fm_list_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	FMListModel *model;

	model = (FMListModel *)tree_model;

	g_return_val_if_fail (model->details->stamp == iter->stamp, FALSE);

	iter->user_data = G_SLIST (iter->user_data)->next;

	return (iter->user_data != NULL);
}

static gboolean
fm_list_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	FMListModel *model;

	model = (FMListModel *)tree_model;
	
	if (parent != NULL) {
		return FALSE;
	}

	if (model->details->files != NULL) {
		iter->stamp = model->details->stamp;
		iter->user_data = model->details->files;

		return TRUE;
	}
	else {
		return FALSE;
	}
}

static gboolean
fm_list_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	return FALSE;
}

static int
fm_list_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	FMListModel *model;

	model = (FMListModel *)tree_model;

	if (iter == NULL) {
		return model->details->length;
	}

	g_return_val_if_fail (model->details->stamp == iter->stamp, -1);

	return 0;
}

static gboolean
fm_list_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, int n)
{
	FMListModel *model;
	GSList *child;

	model = (FMListModel *)tree_model;
	
	if (parent != NULL) {
		return FALSE;
	}

	child = g_slist_nth (model->details->files, n);

	if (child != NULL) {
		iter->stamp = model->details->stamp;
		iter->user_data = child;

		return TRUE;
	}
	else {
		return FALSE;
	}
}

static gboolean
fm_list_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	return FALSE;
}

gboolean
fm_list_model_get_tree_iter_from_file (FMListModel *model, NautilusFile *file, GtkTreeIter *iter)
{
	GSList *list;

	for (list = model->details->files; list; list = list->next) {
		if (list->data == file) {
			break;
		}
	}

	if (list == NULL) {
		return FALSE;
	}

	if (iter != NULL) {
		iter->stamp = model->details->stamp;
		iter->user_data = list;
	}
	
	return TRUE;
}

/* Sorting */
typedef struct _SortTuple
{
  gint offset;
  GSList *el;
} SortTuple;

static NautilusFileSortType
fm_list_model_get_nautilus_file_sort_type (FMListModel *model)
{
	if (model->details->sort_column_id == -1) {
		return NAUTILUS_FILE_SORT_NONE;
	}

	switch (model->details->sort_column_id) {
	case FM_LIST_MODEL_NAME_COLUMN:
		return NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;
	case FM_LIST_MODEL_SIZE_COLUMN:
		return NAUTILUS_FILE_SORT_BY_SIZE;
	case FM_LIST_MODEL_TYPE_COLUMN:
		return NAUTILUS_FILE_SORT_BY_TYPE;
	case FM_LIST_MODEL_DATE_MODIFIED_COLUMN:
		return NAUTILUS_FILE_SORT_BY_MTIME;
	default:
		g_assert_not_reached ();
	}

	/* Should not be reached */
	return NAUTILUS_FILE_SORT_NONE;
}

static int
fm_list_model_compare_func (gconstpointer a,
			    gconstpointer b,
			    gpointer      user_data)
{
	NautilusFile *file1;
	NautilusFile *file2;
	FMListModel *model;
	int result;
	
	model = (FMListModel *)user_data;

	file1 = ((SortTuple *)a)->el->data;
	file2 = ((SortTuple *)b)->el->data;

	result = nautilus_file_compare_for_sort (file1, file2,
						 fm_list_model_get_nautilus_file_sort_type (model),
						 model->details->sort_directories_first,
						 (model->details->order == GTK_SORT_DESCENDING));

	return result;
}

static void
fm_list_model_sort (FMListModel *model)
{
	GtkTreeIter iter;
	GArray *sort_array;
	gint i;
	gint *new_order;
	GSList *list;
	GtkTreePath *path;

	if (model->details->length <= 1) {
		return;
	}

	list = G_SLIST (model->details->files);
	
	sort_array = g_array_sized_new (FALSE, FALSE,
					sizeof (SortTuple),
					model->details->length);

	for (i = 0; i < model->details->length; i++) {
		SortTuple tuple;

		/* If this fails, we are in an inconsistent state.  Bad */
		g_return_if_fail (list != NULL);

		tuple.offset = i;
		tuple.el = list;
		g_array_append_val (sort_array, tuple);

		list = list->next;
	}

	g_array_sort_with_data (sort_array, fm_list_model_compare_func, model);
	
	for (i = 0; i < model->details->length - 1; i++) {
		g_array_index (sort_array, SortTuple, i).el->next = g_array_index (sort_array, SortTuple, i + 1).el;
	}
	g_array_index (sort_array, SortTuple, model->details->length - 1).el->next = NULL;
	model->details->files = g_array_index (sort_array, SortTuple, 0).el;

	/* Let the world know about our new order */
	new_order = g_new (int, model->details->length);
	for (i = 0; i < model->details->length; i++) {
		new_order[i] = g_array_index (sort_array, SortTuple, i).offset;
	}
	path = gtk_tree_path_new ();
	iter.stamp = model->details->stamp;
	iter.user_data = NULL;
	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
				       path, NULL, new_order);
	gtk_tree_path_free (path);
	g_free (new_order);
	g_array_free (sort_array, TRUE);
}

static gboolean
fm_list_model_get_sort_column_id (GtkTreeSortable *sortable,
				  gint            *sort_column_id,
				  GtkSortType     *order)
{
	FMListModel *model;

	model = (FMListModel *)sortable;

	if (model->details->sort_column_id == -1) {
		return FALSE;
	}

	if (sort_column_id != NULL) {
		*sort_column_id = model->details->sort_column_id;
	}

	if (order != NULL) {
		*order = model->details->order;
	}

	return TRUE;
}

static void
fm_list_model_set_sort_column_id (GtkTreeSortable *sortable, gint sort_column_id, GtkSortType order)
{
	FMListModel *model;

	model = (FMListModel *)sortable;

	if ((model->details->sort_column_id == sort_column_id) &&
	    (model->details->order == order)) {
		return;
	}

	model->details->sort_column_id = sort_column_id;
	model->details->order = order;

	fm_list_model_sort (model);
	gtk_tree_sortable_sort_column_changed (sortable);
}


static gboolean
fm_list_model_has_default_sort_func (GtkTreeSortable *sortable)
{
	return FALSE;
}

static gboolean
fm_list_model_row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *path)
{
	/* We always return FALSE here until we can fix the dnd support */
	return FALSE;
}

static gboolean
fm_list_model_drag_data_get (GtkTreeDragSource *drag_source, GtkTreePath *path, GtkSelectionData *selection_data)
{
	return FALSE;
}


void
fm_list_model_add_file (FMListModel *model, NautilusFile *file)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GSList *list, *tmp, *tmp_prev;
	NautilusFile *file1;
	int result;
	
	/* We may only add each file once. */
	if (fm_list_model_get_tree_iter_from_file (model, file, NULL) == TRUE) {
		return;
	}

	tmp_prev = NULL;
	
	list = g_slist_alloc ();
	list->data = nautilus_file_ref (file);
	
	iter.stamp = model->details->stamp;
	iter.user_data = list;

	if (model->details->tail == NULL) {
		model->details->files = iter.user_data;
		model->details->tail = iter.user_data;
	}
	else {
		for (tmp = model->details->files; tmp; tmp = tmp->next) {
			file1 = tmp->data;
			
			
			result = nautilus_file_compare_for_sort (file1, file,
								 fm_list_model_get_nautilus_file_sort_type (model),
								 model->details->sort_directories_first,
								 (model->details->order == GTK_SORT_DESCENDING));
			if (result < 0) {
				break;
			}

			tmp_prev = tmp;
		}

		if (tmp != NULL) {
			if (tmp == model->details->files) {
				list->next = model->details->files;
				model->details->files = list;
			}
			else {
				list->next = tmp;
			}
		}

		if (tmp_prev != NULL) {
			if (tmp_prev == model->details->tail) {
				model->details->tail->next = list;
				model->details->tail = list;
			}
			else {
				tmp_prev->next = list;
			}
		}

	}

	model->details->length += 1;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

void
fm_list_model_file_changed (FMListModel *model, NautilusFile *file)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GSList *list;

	for (list = model->details->files; list; list = list->next) {
		if (list->data == file) {
			break;
		}
	}

	if (list == NULL) {
		return;
	}
	
	iter.stamp = model->details->stamp;
	iter.user_data = list;
	
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

gboolean
fm_list_model_is_empty (FMListModel *model)
{
	return model->details->length == 0;
}

static GSList *
remove_link_saving_prev (GSList *list, GSList *link, GSList **prevp)
{
	GSList *node;
	GSList *prev;
	
	prev = NULL;

	for (node = list; node; node = node->next) {
		if (node == link) {
			if (prev != NULL) {
				prev->next = link->next;
			}
			
			if (list == link) {
				list = list->next;
			}
			
			link->next = NULL;
			break;
		}

		prev = node;
	}

	*prevp = prev;

	return list;
}

static void
fm_list_model_remove (FMListModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GSList *prev;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	nautilus_file_unref (NAUTILUS_FILE (G_SLIST (iter->user_data)->data));
	
	prev = NULL;
	model->details->files = remove_link_saving_prev (model->details->files,
							 iter->user_data,
							 &prev);
	model->details->length -= 1;

	if (iter->user_data == model->details->tail) {
		model->details->tail = prev;
	}

	model->details->stamp++;

	
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);

}

void
fm_list_model_remove_file (FMListModel *model, NautilusFile *file)
{
	GtkTreeIter iter;

	if (fm_list_model_get_tree_iter_from_file (model, file, &iter)) {
		fm_list_model_remove (model, &iter);
	}
}

void
fm_list_model_clear (FMListModel *model)
{
	GtkTreeIter iter;

	while (model->details->files != NULL) {
		iter.stamp = model->details->stamp;
		iter.user_data = model->details->files;
		fm_list_model_remove (model, &iter);
	}
}

void
fm_list_model_set_should_sort_directories_first (FMListModel *model, gboolean sort_directories_first)
{
	if (model->details->sort_directories_first == sort_directories_first) {
		return;
	}

	model->details->sort_directories_first = sort_directories_first;
	fm_list_model_sort (model);
}

int
fm_list_model_get_sort_column_id_from_attribute (const char *attribute)
{
	guint i;

	if (attribute == NULL) {
		return -1;
	}

	for (i = 0; i < G_N_ELEMENTS (attributes); i++) {
		if (strcmp (attributes[i].attribute_name, attribute) == 0) {
			return i;
		}
	}

	return -1;
}

int
fm_list_model_get_sort_column_id_from_sort_type (NautilusFileSortType sort_type)
{
	switch (sort_type) {
	case NAUTILUS_FILE_SORT_NONE:
		return -1;
	case NAUTILUS_FILE_SORT_BY_SIZE:
		return FM_LIST_MODEL_NAME_COLUMN;
	case NAUTILUS_FILE_SORT_BY_TYPE:
		return FM_LIST_MODEL_TYPE_COLUMN;
	case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
		return FM_LIST_MODEL_NAME_COLUMN;
	case NAUTILUS_FILE_SORT_BY_MTIME:
		return FM_LIST_MODEL_DATE_MODIFIED_COLUMN;
	case NAUTILUS_FILE_SORT_BY_EMBLEMS:
	case NAUTILUS_FILE_SORT_BY_DIRECTORY:
		break;
	}

	g_return_val_if_reached (-1);
}

static void
fm_list_model_init (FMListModel *model)
{
	model->details = g_new0 (FMListModelDetails, 1);
	model->details->stamp = g_random_int ();
	model->details->sort_column_id = -1;
}

static void
fm_list_model_class_init (FMListModelClass *klass)
{
}

static void
fm_list_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = fm_list_model_get_flags;
	iface->get_n_columns = fm_list_model_get_n_columns;
	iface->get_column_type = fm_list_model_get_column_type;
	iface->get_iter = fm_list_model_get_iter;
	iface->get_path = fm_list_model_get_path;
	iface->get_value = fm_list_model_get_value;
	iface->iter_next = fm_list_model_iter_next;
	iface->iter_children = fm_list_model_iter_children;
	iface->iter_has_child = fm_list_model_iter_has_child;
	iface->iter_n_children = fm_list_model_iter_n_children;
	iface->iter_nth_child = fm_list_model_iter_nth_child;
	iface->iter_parent = fm_list_model_iter_parent;
}

static void
fm_list_model_sortable_init (GtkTreeSortableIface *iface)
{
	iface->get_sort_column_id = fm_list_model_get_sort_column_id;
	iface->set_sort_column_id = fm_list_model_set_sort_column_id;
	iface->has_default_sort_func = fm_list_model_has_default_sort_func;
}

static void
fm_list_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = fm_list_model_row_draggable;
	iface->drag_data_get = fm_list_model_drag_data_get;
}

GType
fm_list_model_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (FMListModelClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) fm_list_model_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (FMListModel),
			0,
			(GInstanceInitFunc) fm_list_model_init,
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) fm_list_model_tree_model_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo sortable_info = {
			(GInterfaceInitFunc) fm_list_model_sortable_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo drag_source_info = {
			(GInterfaceInitFunc) fm_list_model_drag_source_init,
			NULL,
			NULL
		};
		
		object_type = g_type_register_static (G_TYPE_OBJECT, "FMListModel", &object_info, 0);
		g_type_add_interface_static (object_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (object_type,
					     GTK_TYPE_TREE_SORTABLE,
					     &sortable_info);
		g_type_add_interface_static (object_type,
					     GTK_TYPE_TREE_DRAG_SOURCE,
					     &drag_source_info);
	}
	
	return object_type;
}
