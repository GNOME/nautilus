/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright C) 2000, 2001 Eazel, Inc
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
 * Authors: Anders Carlsson <andersca@gnu.org>
 *          Darin Adler <darin@bentspoon.com>
 */

/* nautilus-tree-model.c - model for the tree view */

#include <config.h>
#include "nautilus-tree-model.h"

#include <eel/eel-glib-extensions.h>
#include <libgnome/gnome-i18n.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <string.h>

typedef gboolean (* FilePredicate) (NautilusFile *);

/* The user_data of the GtkTreeIter is the TreeNode pointer.
 * It's NULL for the dummy node. If it's NULL, then user_data2
 * is the TreeNode pointer to the parent.
 */

typedef struct TreeNode TreeNode;

struct TreeNode {
	/* part of this node for the file itself */
	int ref_count;

	NautilusFile *file;
	char *display_name;
	GdkPixbuf *closed_pixbuf;
	GdkPixbuf *open_pixbuf;

	TreeNode *parent;
	TreeNode *next;
	TreeNode *prev;

	/* part of the node used only for directories */
	int dummy_child_ref_count;
	int all_children_ref_count;
	
	NautilusDirectory *directory;
	guint done_loading_id;
	guint files_added_id;
	guint files_changed_id;

	TreeNode *first_child;

	/* misc. flags */
	guint done_loading : 1;
	guint inserting_first_child : 1;
	guint inserted : 1;
};

struct NautilusTreeModelDetails {
	int stamp;
	GHashTable *file_to_node_map;
	
	TreeNode *root_node;	
	gboolean root_node_parented;

	guint monitoring_update_idle_id;

	gboolean show_hidden_files;
	gboolean show_backup_files;
	gboolean show_only_directories;
};

typedef struct {
	NautilusDirectory *directory;
	NautilusTreeModel *model;
} DoneLoadingParameters;

static GObjectClass *parent_class;

static void schedule_monitoring_update     (NautilusTreeModel *model);
static void destroy_node_without_reporting (NautilusTreeModel *model,
					    TreeNode          *node);

static void
object_unref_if_not_NULL (gpointer object)
{
	if (object == NULL) {
		return;
	}
	g_object_unref (object);
}

static TreeNode *
tree_node_new (NautilusFile *file)
{
	TreeNode *node;

	node = g_new0 (TreeNode, 1);
	node->file = nautilus_file_ref (file);
	return node;
}

static void
tree_node_unparent (TreeNode *node)
{
	TreeNode *parent, *next, *prev;

	parent = node->parent;
	next = node->next;
	prev = node->prev;

	if (parent == NULL) {
		g_assert (next == NULL);
		g_assert (prev == NULL);
		return;
	}

	if (next != NULL) {
		next->prev = prev;
	}
	if (prev == NULL) {
		g_assert (parent->first_child == node);
		parent->first_child = next;
	} else {
		prev->next = next;
	}

	node->parent = NULL;
	node->next = NULL;
	node->prev = NULL;
}

static void
tree_node_destroy (TreeNode *node)
{
	g_assert (node->first_child == NULL);
	g_assert (node->ref_count == 0);

	tree_node_unparent (node);

	g_object_unref (node->file);
	g_free (node->display_name);
	object_unref_if_not_NULL (node->closed_pixbuf);
	object_unref_if_not_NULL (node->open_pixbuf);

	g_assert (node->done_loading_id == 0);
	g_assert (node->files_added_id == 0);
	g_assert (node->files_changed_id == 0);
	object_unref_if_not_NULL (node->directory);

	g_free (node);
}

static void
tree_node_parent (TreeNode *node, TreeNode *parent)
{
	TreeNode *first_child;

	g_assert (parent != NULL);
	g_assert (node->parent == NULL);
	g_assert (node->prev == NULL);
	g_assert (node->next == NULL);

	first_child = parent->first_child;
	
	node->parent = parent;
	node->next = first_child;

	if (first_child != NULL) {
		g_assert (first_child->prev == NULL);
		first_child->prev = node;
	}

	parent->first_child = node;
}

static GdkPixbuf *
tree_node_get_pixbuf_from_factory (TreeNode *node,
				   const char *modifier)
{
	return nautilus_icon_factory_get_pixbuf_for_file
		(node->file, modifier, NAUTILUS_ICON_SIZE_FOR_MENUS);
}

static gboolean
tree_node_update_pixbuf (TreeNode *node,
			 GdkPixbuf **pixbuf_storage,
			 const char *modifier)
{
	GdkPixbuf *pixbuf;

	if (*pixbuf_storage == NULL) {
		return FALSE;
	}
	pixbuf = tree_node_get_pixbuf_from_factory (node, modifier);
	if (pixbuf == *pixbuf_storage) {
		g_object_unref (pixbuf);
		return FALSE;
	}
	g_object_unref (*pixbuf_storage);
	*pixbuf_storage = pixbuf;
	return TRUE;
}

static gboolean
tree_node_update_closed_pixbuf (TreeNode *node)
{
	return tree_node_update_pixbuf (node, &node->closed_pixbuf, NULL);
}

static gboolean
tree_node_update_open_pixbuf (TreeNode *node)
{
	return tree_node_update_pixbuf (node, &node->open_pixbuf, "accept");
}

static gboolean
tree_node_update_display_name (TreeNode *node)
{
	char *display_name;

	if (node->display_name == NULL) {
		return FALSE;
	}
	display_name = nautilus_file_get_display_name (node->file);
	if (strcmp (display_name, node->display_name) == 0) {
		g_free (display_name);
		return FALSE;
	}
	g_free (node->display_name);
	node->display_name = NULL;
	return TRUE;
}

static GdkPixbuf *
tree_node_get_closed_pixbuf (TreeNode *node)
{
	if (node->closed_pixbuf == NULL) {
		node->closed_pixbuf = tree_node_get_pixbuf_from_factory (node, NULL);
	}
	return node->closed_pixbuf;
}

static GdkPixbuf *
tree_node_get_open_pixbuf (TreeNode *node)
{
	if (node->open_pixbuf == NULL) {
		node->open_pixbuf = tree_node_get_pixbuf_from_factory (node, "accept");
	}
	return node->open_pixbuf;
}

static const char *
tree_node_get_display_name (TreeNode *node)
{
	if (node->display_name == NULL) {
		node->display_name = nautilus_file_get_display_name (node->file);
	}
	return node->display_name;
}

static gboolean
tree_node_has_dummy_child (TreeNode *node)
{
	return node->directory != NULL
		&& (!node->done_loading
		    || node->first_child == NULL
		    || node->inserting_first_child);
}

static int
tree_node_get_child_index (TreeNode *parent, TreeNode *child)
{
	int i;
	TreeNode *node;

	if (child == NULL) {
		g_assert (tree_node_has_dummy_child (parent));
		return 0;
	}

	i = tree_node_has_dummy_child (parent) ? 1 : 0;
	for (node = parent->first_child; node != NULL; node = node->next, i++) {
		if (child == node) {
			return i;
		}
	}

	g_assert_not_reached ();
	return 0;
}

static gboolean
make_iter_invalid (GtkTreeIter *iter)
{
	iter->stamp = 0;
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
	return FALSE;
}

static gboolean
make_iter_for_node (TreeNode *node, GtkTreeIter *iter, int stamp)
{
	if (node == NULL) {
		return make_iter_invalid (iter);
	}
	iter->stamp = stamp;
	iter->user_data = node;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
	return TRUE;
}

static gboolean
make_iter_for_dummy_row (TreeNode *parent, GtkTreeIter *iter, int stamp)
{
	g_assert (tree_node_has_dummy_child (parent));
	g_assert (parent != NULL);
	iter->stamp = stamp;
	iter->user_data = NULL;
	iter->user_data2 = parent;
	iter->user_data3 = NULL;
	return TRUE;
}

static TreeNode *
get_node_from_file (NautilusTreeModel *model, NautilusFile *file)
{
	return g_hash_table_lookup (model->details->file_to_node_map, file);
}

static TreeNode *
get_parent_node_from_file (NautilusTreeModel *model, NautilusFile *file)
{
	NautilusFile *parent_file;
	TreeNode *parent_node;
	
	parent_file = nautilus_file_get_parent (file);
	parent_node = get_node_from_file (model, parent_file);
	nautilus_file_unref (parent_file);
	return parent_node;
}

static TreeNode *
create_node_for_file (NautilusTreeModel *model, NautilusFile *file)
{
	TreeNode *node;

	g_assert (get_node_from_file (model, file) == NULL);
	node = tree_node_new (file);
	g_hash_table_insert (model->details->file_to_node_map, node->file, node);
	return node;
}

#if LOG_REF_COUNTS

static char *
get_node_uri (GtkTreeIter *iter)
{
	TreeNode *node, *parent;
	char *parent_uri, *node_uri;

	node = iter->user_data;
	if (node != NULL) {
		return nautilus_file_get_uri (node->file);
	}

	parent = iter->user_data2;
	parent_uri = nautilus_file_get_uri (parent->file);
	node_uri = g_strconcat (parent_uri, " -- DUMMY", NULL);
	g_free (parent_uri);
	return node_uri;
}

#endif

static void
decrement_ref_count (NautilusTreeModel *model, TreeNode *node, int count)
{
	node->all_children_ref_count -= count;
	if (node->all_children_ref_count == 0) {
		schedule_monitoring_update (model);
	}
}

static void
abandon_node_ref_count (NautilusTreeModel *model, TreeNode *node)
{
	if (node->parent != NULL) {
		decrement_ref_count (model, node->parent, node->ref_count);
#if LOG_REF_COUNTS
		if (node->ref_count != 0) {
			char *uri;

			uri = nautilus_file_get_uri (node->file);
			g_message ("abandoning %d ref of %s, count is now %d",
				   node->ref_count, uri, node->parent->all_children_ref_count);
			g_free (uri);
		}
#endif
	}
	node->ref_count = 0;
}

static void
abandon_dummy_row_ref_count (NautilusTreeModel *model, TreeNode *node)
{
	decrement_ref_count (model, node, node->dummy_child_ref_count);
	if (node->dummy_child_ref_count != 0) {
#if LOG_REF_COUNTS
		char *uri;

		uri = nautilus_file_get_uri (node->file);
		g_message ("abandoning %d ref of %s -- DUMMY, count is now %d",
			   node->dummy_child_ref_count, uri, node->all_children_ref_count);
		g_free (uri);
#endif
	}
	node->dummy_child_ref_count = 0;
}

static void
report_row_inserted (NautilusTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static void
report_row_contents_changed (NautilusTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static void
report_row_has_child_toggled (NautilusTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static GtkTreePath *
get_node_path (NautilusTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	make_iter_for_node (node, &iter, model->details->stamp);
	return gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
}

static void
report_dummy_row_inserted (NautilusTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;

	if (!parent->inserted) {
		return;
	}
	make_iter_for_dummy_row (parent, &iter, model->details->stamp);
	report_row_inserted (model, &iter);
}

static void
report_dummy_row_deleted (NautilusTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	abandon_dummy_row_ref_count (model, parent);
	if (!parent->inserted) {
		return;
	}
	make_iter_for_node (parent, &iter, model->details->stamp);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_path_append_index (path, 0);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static void
report_node_inserted (NautilusTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_inserted (model, &iter);
	node->inserted = TRUE;

	if (node->directory != NULL) {
		report_row_has_child_toggled (model, &iter);
       }
       if (tree_node_has_dummy_child (node)) {
               report_dummy_row_inserted (model, node);
	}
}

static void
report_node_contents_changed (NautilusTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	if (!node->inserted) {
		return;
	}
	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_contents_changed (model, &iter);
}

static void
report_node_has_child_toggled (NautilusTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	if (!node->inserted) {
		return;
	}
	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_has_child_toggled (model, &iter);
}

static void
report_dummy_row_contents_changed (NautilusTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;

	if (!parent->inserted) {
		return;
	}
	make_iter_for_dummy_row (parent, &iter, model->details->stamp);
	report_row_contents_changed (model, &iter);
}

static void
stop_monitoring_directory (NautilusTreeModel *model, TreeNode *node)
{
	NautilusDirectory *directory;

	if (node->done_loading_id == 0) {
		g_assert (node->files_added_id == 0);
		g_assert (node->files_changed_id == 0);
		return;
	}

	directory = node->directory;

	g_signal_handler_disconnect (node->directory, node->done_loading_id);
	g_signal_handler_disconnect (node->directory, node->files_added_id);
	g_signal_handler_disconnect (node->directory, node->files_changed_id);

	node->done_loading_id = 0;
	node->files_added_id = 0;
	node->files_changed_id = 0;

	nautilus_directory_file_monitor_remove (node->directory, model);
}

static void
destroy_children_without_reporting (NautilusTreeModel *model, TreeNode *parent)
{
	while (parent->first_child != NULL) {
		destroy_node_without_reporting (model, parent->first_child);
	}
}

static void
destroy_node_without_reporting (NautilusTreeModel *model, TreeNode *node)
{
	abandon_node_ref_count (model, node);
	stop_monitoring_directory (model, node);
	node->inserted = FALSE;
	destroy_children_without_reporting (model, node);
	g_hash_table_remove (model->details->file_to_node_map, node->file);
	tree_node_destroy (node);
}

static void
destroy_node (NautilusTreeModel *model, TreeNode *node)
{
	TreeNode *parent;
	gboolean parent_had_dummy_child;
	GtkTreePath *path;

	parent = node->parent;
	parent_had_dummy_child = tree_node_has_dummy_child (parent);

	path = get_node_path (model, node);

	destroy_node_without_reporting (model, node);

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);

	if (tree_node_has_dummy_child (parent)) {
		if (!parent_had_dummy_child) {
			report_dummy_row_inserted (model, parent);
		}
	} else {
		g_assert (!parent_had_dummy_child);
	}
}

static void
destroy_children (NautilusTreeModel *model, TreeNode *parent)
{
	while (parent->first_child != NULL) {
		destroy_node (model, parent->first_child);
	}
}

static void
destroy_children_by_function (NautilusTreeModel *model, TreeNode *parent, FilePredicate f)
{
	TreeNode *child, *next;

	for (child = parent->first_child; child != NULL; child = next) {
		next = child->next;
		if (f (child->file)) {
			destroy_node (model, child);
		} else {
			destroy_children_by_function (model, child, f);
		}
	}
}

static void
destroy_by_function (NautilusTreeModel *model, FilePredicate f)
{
	destroy_children_by_function (model, model->details->root_node, f);
}

static gboolean
update_node_without_reporting (NautilusTreeModel *model, TreeNode *node)
{
	gboolean changed;

	changed = FALSE;
	
	if (node->directory == NULL && nautilus_file_is_directory (node->file)) {
		node->directory = nautilus_directory_get_for_file (node->file);
	} else if (node->directory != NULL && !nautilus_file_is_directory (node->file)) {
		stop_monitoring_directory (model, node);
		destroy_children (model, node);
		nautilus_directory_unref (node->directory);
		node->directory = NULL;
	}

	changed |= tree_node_update_display_name (node);
	changed |= tree_node_update_closed_pixbuf (node);
	changed |= tree_node_update_open_pixbuf (node);

	return changed;
}

static void
insert_node (NautilusTreeModel *model, TreeNode *parent, TreeNode *node)
{
	gboolean parent_empty;

	parent_empty = parent->first_child == NULL;
	if (parent_empty) {
		parent->inserting_first_child = TRUE;
	}

	tree_node_parent (node, parent);

	update_node_without_reporting (model, node);
	report_node_inserted (model, node);

	if (parent_empty) {
		parent->inserting_first_child = FALSE;
		if (!tree_node_has_dummy_child (parent)) {
			report_dummy_row_deleted (model, parent);
		}
	}
}

static void
reparent_node (NautilusTreeModel *model, TreeNode *node)
{
	GtkTreePath *path;
	TreeNode *new_parent;

	new_parent = get_parent_node_from_file (model, node->file);
	if (new_parent == NULL || new_parent->directory == NULL) {
		destroy_node (model, node);
		return;
	}

	path = get_node_path (model, node);

	abandon_node_ref_count (model, node);
	tree_node_unparent (node);

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);

	insert_node (model, new_parent, node);
}

static gboolean
should_show_file (NautilusTreeModel *model, NautilusFile *file)
{
	gboolean should;

	should = nautilus_file_should_show (file,
					    model->details->show_hidden_files,
					    model->details->show_backup_files);

	if (should
	    && model->details->show_only_directories
	    &&! nautilus_file_is_directory (file)) {
		should = FALSE;
	}

	if (should && nautilus_file_is_gone (file)) {
		should = FALSE;
	}

	if (!should && model->details->root_node != NULL
	    && file == model->details->root_node->file) {
		should = TRUE;
	}

	return should;
}

static void
update_node (NautilusTreeModel *model, TreeNode *node)
{
	gboolean had_dummy_child, has_dummy_child;
	gboolean had_directory, has_directory;
	gboolean changed;

	if (!should_show_file (model, node->file)) {
		destroy_node (model, node);
		return;
	}

	if (node->parent != NULL && node->parent->directory != NULL
	    && !nautilus_directory_contains_file (node->parent->directory, node->file)) {
		reparent_node (model, node);
		return;
	}

	had_dummy_child = tree_node_has_dummy_child (node);
	had_directory = node->directory != NULL;

	changed = update_node_without_reporting (model, node);

	has_dummy_child = tree_node_has_dummy_child (node);
	has_directory = node->directory != NULL;
	
	if (had_dummy_child != has_dummy_child) {
		if (has_dummy_child) {
			report_dummy_row_inserted (model, node);
		} else {
			report_dummy_row_deleted (model, node);
		}
	}
	if (had_directory != has_directory) {
		report_node_has_child_toggled (model, node);
	}

	if (changed) {
		report_node_contents_changed (model, node);
	}
}

static void
process_file_change (NautilusTreeModel *model,
		     NautilusFile *file)
{
	TreeNode *node, *parent;

	node = get_node_from_file (model, file);
	if (node != NULL) {
		update_node (model, node);
		return;
	}

	if (!should_show_file (model, file)) {
		return;
	}

	parent = get_parent_node_from_file (model, file);
	if (parent == NULL) {
		return;
	}

	insert_node (model, parent, create_node_for_file (model, file));
}

static void
files_changed_callback (NautilusDirectory *directory,
			GList *changed_files,
			gpointer callback_data)
{
	NautilusTreeModel *model;
	GList *node;

	model = NAUTILUS_TREE_MODEL (callback_data);

	for (node = changed_files; node != NULL; node = node->next) {
		process_file_change (model, NAUTILUS_FILE (node->data));
	}
}

static void
set_done_loading (NautilusTreeModel *model, TreeNode *node, gboolean done_loading)
{
	gboolean had_dummy;

	if (node == NULL || node->done_loading == done_loading) {
		return;
	}

	had_dummy = tree_node_has_dummy_child (node);

	node->done_loading = done_loading;

	if (tree_node_has_dummy_child (node)) {
		if (had_dummy) {
			report_dummy_row_contents_changed (model, node);
		} else {
			report_dummy_row_inserted (model, node);
		}
	} else {
		if (had_dummy) {
			report_dummy_row_deleted (model, node);
		} else {
			g_assert_not_reached ();
		}
	}
}

static void
done_loading_callback (NautilusDirectory *directory,
		       NautilusTreeModel *model)
{
	NautilusFile *file;

	file = nautilus_directory_get_corresponding_file (directory);
	set_done_loading (model, get_node_from_file (model, file), TRUE);
	nautilus_file_unref (file);
}

static GList *
get_tree_monitor_attributes (void)
{
	GList *attrs;

	attrs = nautilus_icon_factory_get_required_file_attributes ();
	attrs = g_list_prepend (attrs, NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY);
	attrs = g_list_prepend (attrs, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
	return attrs;
}

static void
start_monitoring_directory (NautilusTreeModel *model, TreeNode *node)
{
	NautilusDirectory *directory;
	GList *attrs;

	if (node->done_loading_id != 0) {
		return;
	}

	g_assert (node->files_added_id == 0);
	g_assert (node->files_changed_id == 0);

	directory = node->directory;
	
	node->done_loading_id = g_signal_connect
		(directory, "done_loading",
		 G_CALLBACK (done_loading_callback), model);
	node->files_added_id = g_signal_connect 
		(directory, "files_added",
		 G_CALLBACK (files_changed_callback), model);
	node->files_changed_id = g_signal_connect 
		(directory, "files_changed",
		 G_CALLBACK (files_changed_callback), model);	

	set_done_loading (model, node, nautilus_directory_are_all_files_seen (directory));
	
	attrs = get_tree_monitor_attributes ();
	nautilus_directory_file_monitor_add (directory, model,
					     model->details->show_hidden_files,
					     model->details->show_backup_files,
					     attrs, files_changed_callback, model);
	g_list_free (attrs);
}

static int
nautilus_tree_model_get_n_columns (GtkTreeModel *model)
{
	return NAUTILUS_TREE_MODEL_NUM_COLUMNS;
}

static GType
nautilus_tree_model_get_column_type (GtkTreeModel *model, int index)
{
	switch (index) {
	case NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN:
		return G_TYPE_STRING;
	case NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		return GDK_TYPE_PIXBUF;
	case NAUTILUS_TREE_MODEL_OPEN_PIXBUF_COLUMN:
		return GDK_TYPE_PIXBUF;
	case NAUTILUS_TREE_MODEL_FONT_STYLE_COLUMN:
		return PANGO_TYPE_STYLE;
	default:
		g_assert_not_reached ();
	}
	
	return G_TYPE_INVALID;
}

static gboolean
iter_is_valid (NautilusTreeModel *model, const GtkTreeIter *iter)
{
	TreeNode *node, *parent;

	if (iter->stamp != model->details->stamp) {
		return FALSE;
	}

	node = iter->user_data;
	parent = iter->user_data2;
	if (node == NULL) {
		if (parent != NULL) {
			if (!NAUTILUS_IS_FILE (parent->file)) {
				return FALSE;
			}
			if (!tree_node_has_dummy_child (parent)) {
				return FALSE;
			}
		}
	} else {
		if (!NAUTILUS_IS_FILE (node->file)) {
			return FALSE;
		}
		if (parent != NULL) {
			return FALSE;
		}
	}
	if (iter->user_data3 != NULL) {
		return FALSE;
	}
	return TRUE;
}

static gboolean
nautilus_tree_model_get_iter (GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
	int *indices;
	GtkTreeIter parent;
	int depth, i;

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	if (! gtk_tree_model_iter_nth_child (model, iter, NULL, indices[0])) {
		return FALSE;
	}

	for (i = 1; i < depth; i++) {
		parent = *iter;

		if (! gtk_tree_model_iter_nth_child (model, iter, &parent, indices[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

static GtkTreePath *
nautilus_tree_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	NautilusTreeModel *tree_model;
	TreeNode *node, *parent;
	GtkTreePath *path;
	GtkTreeIter parent_iter;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), NULL);
	tree_model = NAUTILUS_TREE_MODEL (model);
	g_return_val_if_fail (iter_is_valid (tree_model, iter), NULL);

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		if (parent == NULL) {
			return gtk_tree_path_new ();
		}
	} else {
		parent = node->parent;
		if (parent == NULL) {
			g_assert (node == tree_model->details->root_node);
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, 0);
			return path;
		}
	}

	parent_iter.stamp = iter->stamp;
	parent_iter.user_data = parent;
	parent_iter.user_data2 = NULL;
	parent_iter.user_data3 = NULL;

	path = nautilus_tree_model_get_path (model, &parent_iter);
	
	gtk_tree_path_append_index (path, tree_node_get_child_index (parent, node));

	return path;
}

static void
nautilus_tree_model_get_value (GtkTreeModel *model, GtkTreeIter *iter, int column, GValue *value)
{
	TreeNode *node, *parent;

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter));
	
	node = iter->user_data;

	switch (column) {
	case NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN:
		g_value_init (value, G_TYPE_STRING);
		if (node == NULL) {
			parent = iter->user_data2;
			g_value_set_static_string (value, parent->done_loading
						   ? _("(Empty)") : _("Loading..."));
		} else {
			g_value_set_string (value, tree_node_get_display_name (node));
		}
		break;
	case NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_closed_pixbuf (node));
		break;
	case NAUTILUS_TREE_MODEL_OPEN_PIXBUF_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_open_pixbuf (node));
		break;
	case NAUTILUS_TREE_MODEL_FONT_STYLE_COLUMN:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node == NULL) {
			g_value_set_enum (value, PANGO_STYLE_ITALIC);
		} else {
			g_value_set_enum (value, PANGO_STYLE_NORMAL);
		}
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
nautilus_tree_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent, *next;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter), FALSE);
	
	node = iter->user_data;

	if (node == NULL) {
		parent = iter->user_data2;
		next = parent->first_child;
	} else {
		next = node->next;
	}

	return make_iter_for_node (next, iter, iter->stamp);
}

static gboolean
nautilus_tree_model_iter_children (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent_iter)
{
	TreeNode *parent;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), parent_iter), FALSE);
	
	parent = parent_iter->user_data;
	if (parent == NULL) {
		return make_iter_invalid (iter);
	}

	if (tree_node_has_dummy_child (parent)) {
		return make_iter_for_dummy_row (parent, iter, parent_iter->stamp);
	}
	return make_iter_for_node (parent->first_child, iter, parent_iter->stamp);
}

static gboolean
nautilus_tree_model_iter_parent (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child_iter)
{
	TreeNode *child, *parent;
	
	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), child_iter), FALSE);

	child = child_iter->user_data;

	if (child == NULL) {
		parent = child_iter->user_data2;
	} else {
		parent = child->parent;
	}

	return make_iter_for_node (parent, iter, child_iter->stamp);
}

static gboolean
nautilus_tree_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	gboolean has_child;
	TreeNode *node;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter), FALSE);

	node = iter->user_data;

	has_child = node != NULL && node->directory != NULL;

#if 0
	g_warning ("Node '%s' %s",
		   node && node->file ? nautilus_file_get_uri (node->file) : "no name",
		   has_child ? "has child" : "no child");
#endif
		   
	return has_child;
}

static int
nautilus_tree_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
	NautilusTreeModel *tree_model;
	TreeNode *parent, *node;
	int n;
	
	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter == NULL || iter_is_valid (NAUTILUS_TREE_MODEL (model), iter), FALSE);
	
	tree_model = NAUTILUS_TREE_MODEL (model);

	if (iter == NULL) {
		return 1;
	}

	parent = iter->user_data;
	if (parent == NULL) {
		return 0;
	}

	n = tree_node_has_dummy_child (parent) ? 1 : 0;
	for (node = parent->first_child; node != NULL; node = node->next) {
		n++;
	}

	return n;
}

static gboolean
nautilus_tree_model_iter_nth_child (GtkTreeModel *model, GtkTreeIter *iter,
				    GtkTreeIter *parent_iter, int n)
{
	NautilusTreeModel *tree_model;
	TreeNode *parent, *node;
	int i;
	
	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (parent_iter == NULL
			      || iter_is_valid (NAUTILUS_TREE_MODEL (model), parent_iter), FALSE);
	
	tree_model = NAUTILUS_TREE_MODEL (model);

	if (parent_iter == NULL) {
		if (n != 0) {
			return make_iter_invalid (iter);
		}
		return make_iter_for_node (tree_model->details->root_node,
					   iter, tree_model->details->stamp);
	}

	parent = parent_iter->user_data;
	if (parent == NULL) {
		return make_iter_invalid (iter);
	}

	i = tree_node_has_dummy_child (parent) ? 1 : 0;
	if (n == 0 && i == 1) {
		return make_iter_for_dummy_row (parent, iter, parent_iter->stamp);
	}
	for (node = parent->first_child; i != n; i++, node = node->next) {
		if (node == NULL) {
			return make_iter_invalid (iter);
		}
	}

	return make_iter_for_node (node, iter, parent_iter->stamp);	
}

static void
update_monitoring (NautilusTreeModel *model, TreeNode *node)
{
	TreeNode *child;

	if (node->all_children_ref_count == 0) {
		stop_monitoring_directory (model, node);
		destroy_children (model, node);
	} else {
		for (child = node->first_child; child != NULL; child = child->next) {
			update_monitoring (model, child);
		}
		start_monitoring_directory (model, node);
	}
}

static gboolean
update_monitoring_idle_callback (gpointer callback_data)
{
	NautilusTreeModel *model;

	model = NAUTILUS_TREE_MODEL (callback_data);
	model->details->monitoring_update_idle_id = 0;
	update_monitoring (model, model->details->root_node);
	return FALSE;
}

static void
schedule_monitoring_update (NautilusTreeModel *model)
{
	if (model->details->monitoring_update_idle_id == 0) {
		model->details->monitoring_update_idle_id =
			g_idle_add (update_monitoring_idle_callback, model);
	}
}

static void
stop_monitoring_directory_and_children (NautilusTreeModel *model, TreeNode *node)
{
	TreeNode *child;

	stop_monitoring_directory (model, node);
	for (child = node->first_child; child != NULL; child = child->next) {
		stop_monitoring_directory_and_children (model, child);
	}
}

static void
stop_monitoring (NautilusTreeModel *model)
{
	stop_monitoring_directory_and_children (model, model->details->root_node);
}

static void
nautilus_tree_model_ref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent;
#if LOG_REF_COUNTS
	char *uri;
#endif

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter));

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		g_assert (parent->dummy_child_ref_count >= 0);
		++parent->dummy_child_ref_count;
	} else {
		parent = node->parent;
		g_assert (node->ref_count >= 0);
		++node->ref_count;
	}

	if (parent == NULL) {
		g_assert (node == NAUTILUS_TREE_MODEL (model)->details->root_node);
	} else {
		g_assert (parent->all_children_ref_count >= 0);
		if (++parent->all_children_ref_count == 1) {
			if (parent->first_child == NULL) {
				parent->done_loading = FALSE;
			}
			schedule_monitoring_update (NAUTILUS_TREE_MODEL (model));
		}
#if LOG_REF_COUNTS
		uri = get_node_uri (iter);
		g_message ("ref of %s, count is now %d",
			   uri, parent->all_children_ref_count);
		g_free (uri);
#endif
	}
}

static void
nautilus_tree_model_unref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent;
#if LOG_REF_COUNTS
	char *uri;
#endif

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter));

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		g_assert (parent->dummy_child_ref_count > 0);
		--parent->dummy_child_ref_count;
	} else {
		parent = node->parent;
		g_assert (node->ref_count > 0);
		--node->ref_count;
	}

	if (parent == NULL) {
		g_assert (node == NAUTILUS_TREE_MODEL (model)->details->root_node);
	} else {
		g_assert (parent->all_children_ref_count > 0);
#if LOG_REF_COUNTS
		uri = get_node_uri (iter);
		g_message ("unref of %s, count is now %d",
			   uri, parent->all_children_ref_count - 1);
		g_free (uri);
#endif
		if (--parent->all_children_ref_count == 0) {
			schedule_monitoring_update (NAUTILUS_TREE_MODEL (model));
		}
	}
}

static void
root_node_file_changed_callback (NautilusFile *file, NautilusTreeModel *model)
{
	update_node (model, model->details->root_node);
}

void
nautilus_tree_model_set_root_uri (NautilusTreeModel *model, const char *root_uri)
{
	NautilusFile *file;
	TreeNode *node;
	GList *attrs;
	
	g_return_if_fail (model->details->root_node == NULL);
	
	file = nautilus_file_get (root_uri);

	node = create_node_for_file (model, file);
	model->details->root_node = node;

	g_signal_connect_object	(file, "changed",
				 G_CALLBACK (root_node_file_changed_callback), model, 0);

	attrs = get_tree_monitor_attributes ();
	nautilus_file_monitor_add (file, model, attrs);
	g_list_free (attrs);

	nautilus_file_unref (file);

	update_node_without_reporting (model, node);
	report_node_inserted (model, node);
}

NautilusTreeModel *
nautilus_tree_model_new (const char *opt_root_uri)
{
	NautilusTreeModel *model;

	model = g_object_new (NAUTILUS_TYPE_TREE_MODEL, NULL);
	if (opt_root_uri) {
		nautilus_tree_model_set_root_uri (model, opt_root_uri);
	}
	
	return model;
}

void
nautilus_tree_model_set_show_hidden_files (NautilusTreeModel *model,
					   gboolean show_hidden_files)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (show_hidden_files == FALSE || show_hidden_files == TRUE);

	show_hidden_files = show_hidden_files != FALSE;
	if (model->details->show_hidden_files == show_hidden_files) {
		return;
	}
	model->details->show_hidden_files = show_hidden_files;
	stop_monitoring (model);
	if (!show_hidden_files) {
		destroy_by_function (model, nautilus_file_is_hidden_file);
	}
	schedule_monitoring_update (model);
}

void
nautilus_tree_model_set_show_backup_files (NautilusTreeModel *model,
					   gboolean show_backup_files)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (show_backup_files == FALSE || show_backup_files == TRUE);

	show_backup_files = show_backup_files != FALSE;
	if (model->details->show_backup_files == show_backup_files) {
		return;
	}
	model->details->show_backup_files = show_backup_files;
	stop_monitoring (model);
	if (!show_backup_files) {
		destroy_by_function (model, nautilus_file_is_backup_file);
	}
	schedule_monitoring_update (model);
}

static gboolean
file_is_not_directory (NautilusFile *file)
{
	return !nautilus_file_is_directory (file);
}

void
nautilus_tree_model_set_show_only_directories (NautilusTreeModel *model,
					       gboolean show_only_directories)
{
	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (show_only_directories == FALSE || show_only_directories == TRUE);

	show_only_directories = show_only_directories != FALSE;
	if (model->details->show_only_directories == show_only_directories) {
		return;
	}
	model->details->show_only_directories = show_only_directories;
	stop_monitoring (model);
	if (show_only_directories) {
		destroy_by_function (model, file_is_not_directory);
	}
	schedule_monitoring_update (model);
}

NautilusFile *
nautilus_tree_model_iter_get_file (NautilusTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node;

	g_return_val_if_fail (NAUTILUS_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (iter_is_valid (NAUTILUS_TREE_MODEL (model), iter), 0);

	node = iter->user_data;
	return node == NULL ? NULL : nautilus_file_ref (node->file);
}

static void
nautilus_tree_model_init (NautilusTreeModel *model)
{
	model->details = g_new0 (NautilusTreeModelDetails, 1);

	do
		model->details->stamp = g_random_int ();
	while (model->details->stamp == 0);

	model->details->file_to_node_map = g_hash_table_new (NULL, NULL);
}

static void
nautilus_tree_model_finalize (GObject *object)
{
	NautilusTreeModel *model;
	TreeNode *root;

	model = NAUTILUS_TREE_MODEL (object);

	root = model->details->root_node;
	if (root != NULL) {
		nautilus_file_monitor_remove (root->file, model);
		destroy_node_without_reporting (model, root);
	}

	if (model->details->monitoring_update_idle_id != 0) {
		g_source_remove (model->details->monitoring_update_idle_id);
	}

	g_free (model->details);

	parent_class->finalize (object);
}

static void
nautilus_tree_model_class_init (NautilusTreeModelClass *class)
{
	parent_class = g_type_class_peek_parent (class);

	G_OBJECT_CLASS (class)->finalize = nautilus_tree_model_finalize;
}

static void
nautilus_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_n_columns = nautilus_tree_model_get_n_columns;
	iface->get_column_type = nautilus_tree_model_get_column_type;
	iface->get_iter = nautilus_tree_model_get_iter;
	iface->get_path = nautilus_tree_model_get_path;
	iface->get_value = nautilus_tree_model_get_value;
	iface->iter_next = nautilus_tree_model_iter_next;
	iface->iter_children = nautilus_tree_model_iter_children;
	iface->iter_has_child = nautilus_tree_model_iter_has_child;
	iface->iter_n_children = nautilus_tree_model_iter_n_children;
	iface->iter_nth_child = nautilus_tree_model_iter_nth_child;
	iface->iter_parent = nautilus_tree_model_iter_parent;
	iface->ref_node = nautilus_tree_model_ref_node;
	iface->unref_node = nautilus_tree_model_unref_node;
}

GType
nautilus_tree_model_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (NautilusTreeModelClass),
			NULL,
			NULL,
			(GClassInitFunc) nautilus_tree_model_class_init,
			NULL,
			NULL,
			sizeof (NautilusTreeModel),
			0,
			(GInstanceInitFunc) nautilus_tree_model_init,
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) nautilus_tree_model_tree_model_init,
			NULL,
			NULL
		};

		object_type = g_type_register_static (G_TYPE_OBJECT, "NautilusTreeModel", &object_info, 0);
		g_type_add_interface_static (object_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return object_type;
}
