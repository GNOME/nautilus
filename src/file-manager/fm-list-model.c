/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists. 

   Copyright (C) 2001, 2002 Anders Carlsson
   Copyright (C) 2003, Soeren Sandmann

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

   Authors: Anders Carlsson <andersca@gnu.org>, Soeren Sandmann (sandmann@daimi.au.dk)
*/

#include <config.h>
#include "fm-list-model.h"
#include <libegg/eggtreemultidnd.h>

#include <string.h>
#include <eel/eel-gtk-macros.h>
#include <gtk/gtktreednd.h>
#include <gtk/gtktreesortable.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <gsequence/gsequence.h>

static int fm_list_model_compare_func (gconstpointer a,
				       gconstpointer b,
				       gpointer      user_data);

static GObjectClass *parent_class;

struct FMListModelDetails {
	GSequence *files;
	GHashTable *reverse_map;	/* map from files to GSequencePtr's */

	int stamp;

	int sort_column_id;
	GtkSortType order;

	gboolean sort_directories_first;

	GtkTreeView *drag_view;
	int drag_begin_x;
	int drag_begin_y;
};

typedef struct {
	FMListModel *model;
	
	GList *path_list;
} DragDataGetInfo;

typedef struct {
	const char *attribute_name;
	int sort_column_id;
} AttributeEntry;

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL },
	{ NAUTILUS_ICON_DND_TEXT_TYPE, 0, NAUTILUS_ICON_DND_TEXT }
};

/*
 * Do not change the order of the type and size attributes, they 
 * have to be in this order so that the column_id to attribute mapping
 * works. This is needed to store the sorting preferences. This duplicate
 * entry is here to allow the ordering by icon (i think...)
 */

static const AttributeEntry attributes[] = {
	{ "name", FM_LIST_MODEL_NAME_COLUMN },
	{ "type", FM_LIST_MODEL_TYPE_COLUMN },
#ifdef GNOME2_CONVERSION_COMPLETE
	{ "emblems", FM_LIST_MODEL_EMBLEMS_COLUMN },
#endif
	{ "size", FM_LIST_MODEL_SIZE_COLUMN },
	{ "icon", FM_LIST_MODEL_TYPE_COLUMN },
	{ "date_modified", FM_LIST_MODEL_DATE_MODIFIED_COLUMN },
};

static GtkTargetList *drag_target_list = NULL;

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
	case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
	case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
	case FM_LIST_MODEL_SMALL_ICON_COLUMN:
	case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
	case FM_LIST_MODEL_LARGE_ICON_COLUMN:
	case FM_LIST_MODEL_LARGER_ICON_COLUMN:
	case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
		return GDK_TYPE_PIXBUF;
	case FM_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
		return G_TYPE_BOOLEAN;
	default:
		return G_TYPE_INVALID;
	}
}

static gboolean
fm_list_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	FMListModel *model;
	GSequencePtr ptr;
	int i;
	
	model = (FMListModel *)tree_model;

	i = gtk_tree_path_get_indices (path)[0];

	if (i >= g_sequence_get_length (model->details->files)) {
		return FALSE;
	}

	ptr = g_sequence_get_ptr_at_pos (model->details->files, i);

	iter->stamp = model->details->stamp;
	iter->user_data = ptr;

	g_assert (!g_sequence_ptr_is_end (iter->user_data));
	
	return TRUE;
}

static GtkTreePath *
fm_list_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	FMListModel *model;

	model = (FMListModel *)tree_model;
	
	g_return_val_if_fail (iter->stamp == model->details->stamp, NULL);

	if (g_sequence_ptr_is_end (iter->user_data)) {
		/* FIXME is this right? */
		return NULL;
	}
	
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, g_sequence_ptr_get_position (iter->user_data));

	return path;
}

static void
fm_list_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value)
{
	FMListModel *model;
	NautilusFile *file;
	char *str;
	GdkPixbuf *icon, *tmp;
	int icon_size;
	NautilusZoomLevel zoom_level;
	int width, height;

	model = (FMListModel *)tree_model;

	g_return_if_fail (model->details->stamp == iter->stamp);
	g_return_if_fail (!g_sequence_ptr_is_end (iter->user_data));

	file = g_sequence_ptr_get_data (iter->user_data);
	
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
	case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
	case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
	case FM_LIST_MODEL_SMALL_ICON_COLUMN:
	case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
	case FM_LIST_MODEL_LARGE_ICON_COLUMN:
	case FM_LIST_MODEL_LARGER_ICON_COLUMN:
	case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);

		zoom_level = fm_list_model_get_zoom_level_from_column_id (column);
		icon_size = nautilus_get_icon_size_for_zoom_level (zoom_level);
		icon = nautilus_icon_factory_get_pixbuf_for_file (file, NULL, icon_size);

		height = gdk_pixbuf_get_height (icon);
		if (height > icon_size) {
			width = gdk_pixbuf_get_width (icon) * icon_size / height;
			height = icon_size;
			tmp = gdk_pixbuf_scale_simple (icon, width, height, GDK_INTERP_BILINEAR);
			g_object_unref (icon);
			icon = tmp;
		}
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
	case FM_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
		g_value_init (value, G_TYPE_BOOLEAN);

		g_value_set_boolean (value, nautilus_file_can_rename (file));
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

	iter->user_data = g_sequence_ptr_next (iter->user_data);

	return !g_sequence_ptr_is_end (iter->user_data);
}

static gboolean
fm_list_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	FMListModel *model;

	model = (FMListModel *)tree_model;
	
	if (parent != NULL) {
		return FALSE;
	}

	if (g_sequence_get_length (model->details->files) == 0) {
		return FALSE;
	}

	iter->stamp = model->details->stamp;
	iter->user_data = g_sequence_get_begin_ptr (model->details->files);

	return TRUE;
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
		return g_sequence_get_length (model->details->files);
	}

	g_return_val_if_fail (model->details->stamp == iter->stamp, -1);

	return 0;
}

static gboolean
fm_list_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, int n)
{
	FMListModel *model;
	GSequencePtr child;

	model = (FMListModel *)tree_model;
	
	if (parent != NULL) {
		return FALSE;
	}

	child = g_sequence_get_ptr_at_pos (model->details->files, n);

	if (g_sequence_ptr_is_end (child)) {
		return FALSE;
	}

	iter->stamp = model->details->stamp;
	iter->user_data = child;

	return TRUE;
}

static gboolean
fm_list_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	return FALSE;
}

gboolean
fm_list_model_get_tree_iter_from_file (FMListModel *model, NautilusFile *file, GtkTreeIter *iter)
{
	GSequencePtr ptr;

	ptr = g_hash_table_lookup (model->details->reverse_map, file);

	if (!ptr) {
		return FALSE;
	}

	g_assert (!g_sequence_ptr_is_end (ptr));
	g_assert (g_sequence_ptr_get_data (ptr) == file);
	
	if (iter != NULL) {
		iter->stamp = model->details->stamp;
		iter->user_data = ptr;
	}
	
	return TRUE;
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

	file1 = (NautilusFile *)a;
	file2 = (NautilusFile *)b;

	result = nautilus_file_compare_for_sort (file1, file2,
						 fm_list_model_get_sort_type_from_sort_column_id (model->details->sort_column_id),
						 model->details->sort_directories_first,
						 (model->details->order == GTK_SORT_DESCENDING));

	return result;
}

static void
fm_list_model_sort (FMListModel *model)
{
	GSequence *files;
	GSequencePtr *old_order;
	GtkTreePath *path;
	int *new_order;
	int length;
	int i;

	files = model->details->files;
	length = g_sequence_get_length (files);

	if (length <= 1) {
		return;
	}
	
	/* generate old order of GSequencePtr's */
	old_order = g_new (GSequencePtr, length);
	for (i = 0; i < length; ++i) {
		GSequencePtr ptr = g_sequence_get_ptr_at_pos (files, i);

		old_order[i] = ptr;
	}

	/* sort */
	g_sequence_sort (files, fm_list_model_compare_func, model);

	/* generate new order */
	new_order = g_new (int, length);
	for (i = 0; i < length; ++i) {
		new_order[i] = g_sequence_ptr_get_position (old_order[i]);
	}

	/* Let the world know about our new order */
	path = gtk_tree_path_new ();

	g_assert (new_order != NULL);
	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
				       path, NULL, new_order);

	gtk_tree_path_free (path);
	g_free (old_order);
	g_free (new_order);
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
fm_list_model_multi_row_draggable (EggTreeMultiDragSource *drag_source, GList *path_list)
{
	return TRUE;
}

static void
each_path_get_data_binder (NautilusDragEachSelectedItemDataGet data_get,
			   gpointer context,
			   gpointer data)
{
	DragDataGetInfo *info;
	GList *l;
	NautilusFile *file;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	char *uri;
	GdkRectangle cell_area;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;

	info = context;

	g_return_if_fail (info->model->details->drag_view);

	column = gtk_tree_view_get_column (info->model->details->drag_view, 0);

	for (l = info->path_list; l != NULL; l = l->next) {
		row = l->data;

		path = gtk_tree_row_reference_get_path (row);

		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (info->model), 
					     &iter, path)) {
			gtk_tree_model_get (GTK_TREE_MODEL (info->model), 
					    &iter, 
					    FM_LIST_MODEL_FILE_COLUMN, &file,
					    -1);

			if (file) {
				gtk_tree_view_get_cell_area
					(info->model->details->drag_view,
					 path, 
					 column,
					 &cell_area);
				
				uri = nautilus_file_get_uri (file);
				
				(*data_get) (uri, 
					     0,
					     cell_area.y - info->model->details->drag_begin_y,
					     cell_area.width, cell_area.height, 
					     data);
				
				g_free (uri);
			}
		}
		
		gtk_tree_path_free (path);
	}
}

static gboolean
fm_list_model_multi_drag_data_get (EggTreeMultiDragSource *drag_source, 
				   GList *path_list, 
				   GtkSelectionData *selection_data)
{
	FMListModel *model;
	DragDataGetInfo context;
	guint target_info;
	
	model = FM_LIST_MODEL (drag_source);

	context.model = model;
	context.path_list = path_list;

	if (!drag_target_list) {
		drag_target_list = gtk_target_list_new 
			(drag_types, G_N_ELEMENTS (drag_types));

	}

	if (gtk_target_list_find (drag_target_list,
				  selection_data->target,
				  &target_info)) {
		nautilus_drag_drag_data_get (NULL,
					     NULL,
					     selection_data,
					     target_info,
					     GDK_CURRENT_TIME,
					     &context,
					     each_path_get_data_binder);
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
fm_list_model_multi_drag_data_delete (EggTreeMultiDragSource *drag_source, GList *path_list)
{
	return TRUE;
}

void
fm_list_model_add_file (FMListModel *model, NautilusFile *file)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GSequencePtr new_ptr;
	
	/* We may only add each file once. */
	if (fm_list_model_get_tree_iter_from_file (model, file, NULL)) {
		return;
	}

	nautilus_file_ref (file);
	
	new_ptr = g_sequence_insert_sorted (model->details->files, file,
					    fm_list_model_compare_func, model);

	g_hash_table_insert (model->details->reverse_map, file, new_ptr);
	
	iter.stamp = model->details->stamp;
	iter.user_data = new_ptr;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

void
fm_list_model_file_changed (FMListModel *model, NautilusFile *file)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GSequencePtr ptr;

	ptr = g_hash_table_lookup (model->details->reverse_map, file);
	if (!ptr) {
		return;
	}

	g_sequence_ptr_sort_changed (ptr, fm_list_model_compare_func, model);

	if (!fm_list_model_get_tree_iter_from_file (model, file, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

gboolean
fm_list_model_is_empty (FMListModel *model)
{
	return (g_sequence_get_length (model->details->files) == 0);
}

static void
fm_list_model_remove (FMListModel *model, GtkTreeIter *iter)
{
	GSequencePtr ptr;
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	ptr = iter->user_data;

	g_hash_table_remove (model->details->reverse_map, g_sequence_ptr_get_data (ptr));
	g_sequence_remove (ptr);

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

	g_return_if_fail (model != NULL);

	while (g_sequence_get_length (model->details->files) > 0) {
		iter.stamp = model->details->stamp;
		iter.user_data = g_sequence_get_begin_ptr (model->details->files);
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
			return attributes[i].sort_column_id;
		}
	}

	return -1;
}

char *
fm_list_model_get_attribute_from_sort_column_id (int sort_column_id)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (attributes); i++) {
		if (attributes[i].sort_column_id == sort_column_id) {
			return g_strdup (attributes[i].attribute_name);
		}
	}

	g_warning ("unknown sort column id: %d", sort_column_id);
	return g_strdup ("name");
}

int
fm_list_model_get_sort_column_id_from_sort_type (NautilusFileSortType sort_type)
{
	switch (sort_type) {
	case NAUTILUS_FILE_SORT_NONE:
		return -1;
	case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
		return FM_LIST_MODEL_NAME_COLUMN;
	case NAUTILUS_FILE_SORT_BY_TYPE:
		return FM_LIST_MODEL_TYPE_COLUMN;
	case NAUTILUS_FILE_SORT_BY_SIZE:
		return FM_LIST_MODEL_SIZE_COLUMN;
	case NAUTILUS_FILE_SORT_BY_MTIME:
		return FM_LIST_MODEL_DATE_MODIFIED_COLUMN;
	case NAUTILUS_FILE_SORT_BY_EMBLEMS:
	case NAUTILUS_FILE_SORT_BY_DIRECTORY:
		break;
	}

	g_return_val_if_reached (-1);
}

NautilusFileSortType
fm_list_model_get_sort_type_from_sort_column_id (int sort_column_id)
{
	switch (sort_column_id) {
	case FM_LIST_MODEL_NAME_COLUMN:
		return NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;
	case FM_LIST_MODEL_TYPE_COLUMN:
		return NAUTILUS_FILE_SORT_BY_TYPE;
	case FM_LIST_MODEL_SIZE_COLUMN:
		return NAUTILUS_FILE_SORT_BY_SIZE;
	case FM_LIST_MODEL_DATE_MODIFIED_COLUMN:
		return NAUTILUS_FILE_SORT_BY_MTIME;
	}

	g_return_val_if_reached (NAUTILUS_FILE_SORT_NONE);
}

NautilusZoomLevel
fm_list_model_get_zoom_level_from_column_id (int column)
{
	switch (column) {
	case FM_LIST_MODEL_SMALLEST_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_SMALLEST;
	case FM_LIST_MODEL_SMALLER_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_SMALLER;
	case FM_LIST_MODEL_SMALL_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_SMALL;
	case FM_LIST_MODEL_STANDARD_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_STANDARD;
	case FM_LIST_MODEL_LARGE_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_LARGE;
	case FM_LIST_MODEL_LARGER_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_LARGER;
	case FM_LIST_MODEL_LARGEST_ICON_COLUMN:
		return NAUTILUS_ZOOM_LEVEL_LARGEST;
	}

	g_return_val_if_reached (NAUTILUS_ZOOM_LEVEL_STANDARD);
}

int
fm_list_model_get_column_id_from_zoom_level (NautilusZoomLevel zoom_level)
{
	switch (zoom_level) {
	case NAUTILUS_ZOOM_LEVEL_SMALLEST:
		return FM_LIST_MODEL_SMALLEST_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_SMALLER:
		return FM_LIST_MODEL_SMALLER_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_SMALL:
		return FM_LIST_MODEL_SMALL_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_STANDARD:
		return FM_LIST_MODEL_STANDARD_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_LARGE:
		return FM_LIST_MODEL_LARGE_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_LARGER:
		return FM_LIST_MODEL_LARGER_ICON_COLUMN;
	case NAUTILUS_ZOOM_LEVEL_LARGEST:
		return FM_LIST_MODEL_LARGEST_ICON_COLUMN;
	}

	g_return_val_if_reached (FM_LIST_MODEL_STANDARD_ICON_COLUMN);
}

void
fm_list_model_set_drag_view (FMListModel *model,
			     GtkTreeView *view,
			     int drag_begin_x,
			     int drag_begin_y)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (FM_IS_LIST_MODEL (model));
	g_return_if_fail (!view || GTK_IS_TREE_VIEW (view));
	
	model->details->drag_view = view;
	model->details->drag_begin_x = drag_begin_x;
	model->details->drag_begin_y = drag_begin_y;
}

void
fm_list_model_get_drag_types (GtkTargetEntry **entries,
			      int *num_entries)
{
	*entries = drag_types;
	*num_entries = G_N_ELEMENTS (drag_types);
}

static void
fm_list_model_finalize (GObject *object)
{
	FMListModel *model;

	model = FM_LIST_MODEL (object);

	g_sequence_free (model->details->files);
	g_hash_table_destroy (model->details->reverse_map);
	g_free (model->details);
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
fm_list_model_init (FMListModel *model)
{
	model->details = g_new0 (FMListModelDetails, 1);
	model->details->files = g_sequence_new ((GDestroyNotify)nautilus_file_unref);
	model->details->reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->details->stamp = g_random_int ();
	model->details->sort_column_id = -1;
}

static void
fm_list_model_class_init (FMListModelClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *)klass;
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = fm_list_model_finalize;
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
fm_list_model_multi_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
	iface->row_draggable = fm_list_model_multi_row_draggable;
	iface->drag_data_get = fm_list_model_multi_drag_data_get;
	iface->drag_data_delete = fm_list_model_multi_drag_data_delete;
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

		static const GInterfaceInfo multi_drag_source_info = {
			(GInterfaceInitFunc) fm_list_model_multi_drag_source_init,
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
					     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
					     &multi_drag_source_info);
	}
	
	return object_type;
}
