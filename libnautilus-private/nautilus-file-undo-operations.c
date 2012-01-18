/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-operations.c - Manages undo/redo of file operations
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-file-undo-operations.h"

#include <glib/gi18n.h>

#include "nautilus-file-operations.h"
#include "nautilus-file.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-undo-types.h"

G_DEFINE_TYPE (NautilusFileUndoInfo, nautilus_file_undo_info, G_TYPE_OBJECT)

enum {
	PROP_TYPE = 1,
	PROP_ITEM_COUNT,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

struct _NautilusFileUndoInfoDetails {
	NautilusFileUndoData *undo_data;

	NautilusFileUndoDataType type;

	NautilusFileUndoFinishCallback callback;
	gpointer callback_user_data;

	NautilusFileUndoManager *manager;
	guint is_valid : 1;
	guint locked : 1;	/* True if the action is being undone/redone */
	guint freed : 1;	/* True if the action must be freed after undo/redo */
	guint count;		/* Number of items */

	void (* undo_func) (NautilusFileUndoData *data,
			    GtkWindow            *parent_window);
	void (* redo_func) (NautilusFileUndoData *data,
			    GtkWindow            *parent_window);

	void (* strings_func) (NautilusFileUndoData *data,
			       guint count,
			       gchar **labels,
			       gchar **descriptions);

	void (* finalize_func) (NautilusFileUndoData *data);

	gchar *undo_label;
	gchar *redo_label;
	gchar *undo_description;
	gchar *redo_description;
};

void
nautilus_file_undo_data_free (NautilusFileUndoData *action)
{
	g_return_if_fail (action != NULL);
	
	g_free (action->undo_label);
	g_free (action->undo_description);
	g_free (action->redo_label);
	g_free (action->redo_description);
	
	action->finalize_func (action);
}

static char *
get_first_target_short_name (NautilusFileUndoDataExt *eaction)
{
	GList *targets_first;
	char *file_name = NULL;

	targets_first = g_list_first (eaction->destinations);

	if (targets_first != NULL &&
	    targets_first->data != NULL) {
		file_name = g_file_get_basename (targets_first->data);
	}

	return file_name;
}

static GList *
uri_list_to_gfile_list (GList * urilist)
{
	const GList *l;
	GList *file_list = NULL;
	GFile *file;

	for (l = urilist; l != NULL; l = l->next) {
		file = g_file_new_for_uri (l->data);
		file_list = g_list_append (file_list, file);
	}

	return file_list;
}

/* TODO: Synch-I/O, error handling */
static GHashTable *
retrieve_files_to_restore (GHashTable * trashed)
{
	GFileEnumerator *enumerator;
	GHashTable *to_restore;
	GFile *trash;

	to_restore = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, g_free);

	trash = g_file_new_for_uri ("trash:///");

	enumerator = g_file_enumerate_children (trash,
			G_FILE_ATTRIBUTE_STANDARD_NAME","
			G_FILE_ATTRIBUTE_TIME_MODIFIED","
			G_FILE_ATTRIBUTE_TRASH_ORIG_PATH,
			G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			NULL, NULL);

	if (enumerator) {
		GFileInfo *info;
		guint64 *mtime;
		gpointer lookupvalue;
		GFile *item;
		guint64 mtime_item;
		char *origpath;
		GFile *origfile;
		char *origuri;

		mtime = 0;
		while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
			/* Retrieve the original file uri */
			origpath = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH);
			origfile = g_file_new_for_path (origpath);
			origuri = g_file_get_uri (origfile);
			g_object_unref (origfile);
			g_free (origpath);

			lookupvalue = g_hash_table_lookup (trashed, origuri);

			if (lookupvalue) {
				mtime = (guint64 *)lookupvalue;
				mtime_item = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
				if (*mtime == mtime_item) {
					/* File in the trash */
					item = g_file_get_child (trash, g_file_info_get_name (info));
					g_hash_table_insert (to_restore, item, origuri);
				}
			} else {
				g_free (origuri);
			}

		}
		g_file_enumerator_close (enumerator, FALSE, NULL);
		g_object_unref (enumerator);
	}
	g_object_unref (trash);

	return to_restore;
}

static void
undo_redo_done_transfer_callback (GHashTable * debuting_uris,
				  gboolean success,
                                  gpointer data)
{
	NautilusFileUndoData *action = data;

	action->callback (action, success, action->callback_user_data);
}

static void
undo_redo_done_rename_callback (NautilusFile * file,
				GFile * result_location,
				GError * error,
				gpointer callback_data)
{
	undo_redo_done_transfer_callback (NULL, (error != NULL), callback_data);
}

static void
undo_redo_done_create_callback (GFile * new_file,
				gboolean success,
				gpointer callback_data)
{
	undo_redo_done_transfer_callback (NULL, success, callback_data);
}

static void
undo_redo_done_delete_callback (GHashTable *debuting_uris,
                                gboolean user_cancel,
                                gpointer callback_data)
{
	undo_redo_done_transfer_callback (debuting_uris, !user_cancel, callback_data);
}

static void
undo_redo_recursive_permissions_callback (gboolean success,
					  gpointer callback_data)
{
	undo_redo_done_transfer_callback (NULL, success, callback_data);
}

/* undo helpers */

static void
delete_files (NautilusFileUndoManager *self,
	      GtkWindow *parent_window,
	      GList *files,
	      NautilusFileUndoData *action)
{
	nautilus_file_operations_delete (files, parent_window,
					 undo_redo_done_delete_callback, action);
}

static void
create_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *files = NULL;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	files = g_list_append (files, g_object_ref (eaction->target_file));
	delete_files (action->manager, parent_window,
		      files, action);

	g_list_free_full (files, g_object_unref);
}

static void
copy_or_link_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *files;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	files = g_list_copy (eaction->destinations);
	files = g_list_reverse (files); /* Deleting must be done in reverse */

	delete_files (action->manager, parent_window,
		      files, action);

	g_list_free (files);
}

static void
restore_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;
	nautilus_file_operations_trash_or_delete (eaction->destinations, parent_window,
						  undo_redo_done_delete_callback, action);
}

static void
trash_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFileUndoManagerPrivate *priv = action->manager->priv;
	GHashTable *files_to_restore;
	NautilusFileUndoDataTrash* eaction = (NautilusFileUndoDataTrash*) action;

	/* Internally managed op, clear the undo_redo_flag.
	 * Same as calling nautilus_file_undo_manager_is_undo_redo()
	 * minus the function call and unused return val.
	 */

	priv->undo_redo_flag = FALSE;
	files_to_restore = retrieve_files_to_restore (eaction->trashed);

	if (g_hash_table_size (files_to_restore) > 0) {
		GList *gfiles_in_trash, *l;
		GFile *item;
		GFile *dest;
		char *value;

		gfiles_in_trash = g_hash_table_get_keys (files_to_restore);

		for (l = gfiles_in_trash; l != NULL; l = l->next) {
			item = l->data;
			value = g_hash_table_lookup (files_to_restore, item);
			dest = g_file_new_for_uri (value);
			g_file_move (item, dest, G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL, NULL, NULL, NULL);
			g_object_unref (dest);
		}

		g_list_free (gfiles_in_trash);
	}

	g_hash_table_destroy (files_to_restore);

	/* Here we must do what's necessary for the callback */
	undo_redo_done_transfer_callback (NULL, TRUE, action);
}

static void
move_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;
	nautilus_file_operations_move (eaction->destinations, NULL,
				       eaction->src_dir, parent_window,
				       undo_redo_done_transfer_callback, action);
}

static void
rename_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	gchar *new_name;
	NautilusFile *file;
	NautilusFileUndoDataRename* eaction = (NautilusFileUndoDataRename*) action;

	new_name = g_file_get_basename (eaction->old_file);
	file = nautilus_file_get (eaction->new_file);

	nautilus_file_rename (file, new_name,
			      undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
	g_free (new_name);
}

static void
set_permissions_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataPermissions* eaction = (NautilusFileUndoDataPermissions*) action;

	file = nautilus_file_get (eaction->target_file);

	nautilus_file_set_permissions (file,
				       eaction->current_permissions,
				       undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
}

static void
recursive_permissions_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFileUndoManagerPrivate *priv = action->manager->priv;
	NautilusFileUndoDataRecursivePermissions* eaction = (NautilusFileUndoDataRecursivePermissions*) action;

	/* Internally managed op, clear the undo_redo_flag. */
	priv->undo_redo_flag = FALSE;

	if (g_hash_table_size (eaction->original_permissions) > 0) {
		GList *gfiles_list;
		guint32 *perm;
		GList *l;
		GFile *dest;
		char *item;

		gfiles_list = g_hash_table_get_keys (eaction->original_permissions);
		for (l = gfiles_list; l != NULL; l = l->next) {
			item = l->data;
			perm = g_hash_table_lookup (eaction->original_permissions, item);
			dest = g_file_new_for_uri (item);
			g_file_set_attribute_uint32 (dest,
						     G_FILE_ATTRIBUTE_UNIX_MODE,
						     *perm, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
			g_object_unref (dest);
		}

		g_list_free (gfiles_list);
		/* Here we must do what's necessary for the callback */
		undo_redo_done_transfer_callback (NULL, TRUE, action);
	}
}

static void
change_group_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	file = nautilus_file_get (eaction->target_file);

	nautilus_file_set_group (file,
				 eaction->original_ownership,
				 undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
}

static void
change_owner_undo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	file = nautilus_file_get (eaction->target_file);

	nautilus_file_set_owner (file,
				 eaction->original_ownership,
				 undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
}

/* redo helpers */

static void
copy_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *locations;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	locations = eaction->sources;
	nautilus_file_operations_copy (locations, NULL,
				       eaction->dest_dir, parent_window,
				       undo_redo_done_transfer_callback, action);
}

static void
create_from_template_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GFile *parent;
	gchar *parent_uri, *new_name;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	parent = g_file_get_parent (eaction->target_file);
	parent_uri = g_file_get_uri (parent);
	new_name = g_file_get_parse_name (eaction->target_file);
	nautilus_file_operations_new_file_from_template (NULL, NULL,
							 parent_uri, new_name,
							 eaction->template,
							 undo_redo_done_create_callback, action);

	g_free (parent_uri);
	g_free (new_name);
	g_object_unref (parent);
}

static void
duplicate_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *locations;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	locations = eaction->sources;
	nautilus_file_operations_duplicate (locations, NULL, NULL,
					    undo_redo_done_transfer_callback, action);
}

static void
move_restore_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *locations;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	locations = eaction->sources;
	nautilus_file_operations_move (locations, NULL,
				       eaction->dest_dir, NULL,
				       undo_redo_done_transfer_callback, action);
}

static void
rename_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	gchar *new_name;
	NautilusFile *file;
	NautilusFileUndoDataRename* eaction = (NautilusFileUndoDataRename*) action;

	new_name = g_file_get_basename (eaction->new_file);
	file = nautilus_file_get (eaction->old_file);
	nautilus_file_rename (file, new_name,
			      undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
	g_free (new_name);
}

static void
create_empty_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GFile *parent;
	gchar *parent_uri;
	gchar *new_name;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	parent = g_file_get_parent (eaction->target_file);
	parent_uri = g_file_get_uri (parent);
	new_name = g_file_get_parse_name (eaction->target_file);
	nautilus_file_operations_new_file (NULL, NULL, parent_uri,
					   new_name,
					   eaction->template,
					   action->count, undo_redo_done_create_callback, action);

	g_free (parent_uri);
	g_free (new_name);
	g_object_unref (parent);
}

static void
create_folder_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GFile *parent;
	gchar *parent_uri;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	parent = g_file_get_parent (eaction->target_file);
	parent_uri = g_file_get_uri (parent);
	nautilus_file_operations_new_folder (NULL, NULL, parent_uri,
					     undo_redo_done_create_callback, action);

	g_free (parent_uri);
	g_object_unref (parent);
}

static void
trash_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFileUndoDataTrash* eaction = (NautilusFileUndoDataTrash*) action;
	
	if (g_hash_table_size (eaction->trashed) > 0) {
		GList *uri_to_trash, *locations;

		uri_to_trash = g_hash_table_get_keys (eaction->trashed);
		locations = uri_list_to_gfile_list (uri_to_trash);
		nautilus_file_operations_trash_or_delete (locations, parent_window,
							  undo_redo_done_delete_callback, action);
		g_list_free (uri_to_trash);
		g_list_free_full (locations, g_object_unref);
	}
}

static void
create_link_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	GList *locations;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	locations = eaction->sources;
	nautilus_file_operations_link (locations, NULL,
				       eaction->dest_dir, parent_window,
				       undo_redo_done_transfer_callback, action);
}

static void
set_permissions_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataPermissions* eaction = (NautilusFileUndoDataPermissions*) action;

	file = nautilus_file_get (eaction->target_file);
	nautilus_file_set_permissions (file, eaction->new_permissions,
				       undo_redo_done_rename_callback, action);

	nautilus_file_unref (file);
}

static void
recursive_permissions_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	gchar *parent_uri;
	NautilusFileUndoDataRecursivePermissions* eaction = (NautilusFileUndoDataRecursivePermissions*) action;

	parent_uri = g_file_get_uri (eaction->dest_dir);
	nautilus_file_set_permissions_recursive (parent_uri,
						 eaction->file_permissions,
						 eaction->file_mask,
						 eaction->dir_permissions,
						 eaction->dir_mask,
						 undo_redo_recursive_permissions_callback,
						 action);
	g_free (parent_uri);
}

static void
change_group_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	file = nautilus_file_get (eaction->target_file);
	
	nautilus_file_set_group (file,
				 eaction->new_ownership,
				 undo_redo_done_rename_callback,
				 action);

	nautilus_file_unref (file);
}

static void
change_owner_redo_func (NautilusFileUndoData *action, GtkWindow *parent_window)
{
	NautilusFile *file;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	file = nautilus_file_get (eaction->target_file);
	nautilus_file_set_owner (file,
				 eaction->new_ownership,
				 undo_redo_done_rename_callback,
				 action);

	nautilus_file_unref (file);
}

/* description helpers */

static void
copy_description_func (NautilusFileUndoData *action,
		       guint count,
		       gchar **labels,
		       gchar **descriptions)
{
	gchar *destination;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	destination = g_file_get_path (eaction->dest_dir);

	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Delete %d copied items"), count);
		descriptions[1] = g_strdup_printf (_("Copy %d items to '%s'"), count, destination);

		labels[0] = g_strdup_printf (_("_Undo Copy %d items"), count);
		labels[1] = g_strdup_printf (_("_Redo Copy %d items"), count);
	} else {
		gchar *name;

		name = get_first_target_short_name (eaction);
		descriptions[0] = g_strdup_printf (_("Delete '%s'"), name);
		descriptions[1] = g_strdup_printf (_("Copy '%s' to '%s'"), name, destination);

		labels[0] = g_strdup (_("_Undo Copy"));
		labels[1] = g_strdup (_("_Redo Copy"));

		g_free (name);
	}
}

static void
duplicate_description_func (NautilusFileUndoData *action,
			    guint count,
			    gchar **labels,
			    gchar **descriptions)
{
	gchar *destination;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

	destination = g_file_get_path (eaction->dest_dir);

	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Delete %d duplicated items"), count);
		descriptions[1] = g_strdup_printf (_("Duplicate of %d items in '%s'"),
						   count, destination);

		labels[0] = g_strdup_printf (_("_Undo Duplicate %d items"), count);
		labels[1] = g_strdup_printf (_("_Redo Duplicate %d items"), count);
	} else {
		char *name;

		name = get_first_target_short_name (eaction);
		descriptions[0] = g_strdup_printf (_("Delete '%s'"), name);
		descriptions[1] = g_strdup_printf (_("Duplicate '%s' in '%s'"),
						   name, destination);

		labels[0] = g_strdup (_("_Undo Duplicate"));
		labels[1] = g_strdup (_("_Redo Duplicate"));

		g_free (name);
	}

}

static void
move_description_func (NautilusFileUndoData *action,
		       guint count,
		       gchar **labels,
		       gchar **descriptions)
{
	gchar *source, *destination;
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;

    source = g_file_get_path (eaction->src_dir);
	destination = g_file_get_path (eaction->dest_dir);

	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Move %d items back to '%s'"), count, source);
		descriptions[1] = g_strdup_printf (_("Move %d items to '%s'"), count, destination);

		labels[0] = g_strdup_printf (_("_Undo Move %d items"), count);
		labels[1] = g_strdup_printf (_("_Redo Move %d items"), count);
	} else {
		char *name;

		name = get_first_target_short_name (eaction);
		descriptions[0] = g_strdup_printf (_("Move '%s' back to '%s'"), name, source);
		descriptions[1] = g_strdup_printf (_("Move '%s' to '%s'"), name, destination);

		labels[0] = g_strdup (_("_Undo Move"));
		labels[1] = g_strdup (_("_Redo Move"));

		g_free (name);
	}
}

static void
rename_description_func (NautilusFileUndoData *action,
			 guint count,
			 gchar **labels,
			 gchar **descriptions)
{
	gchar *new_name, *old_name;
	NautilusFileUndoDataRename* eaction = (NautilusFileUndoDataRename*) action;

	new_name = g_file_get_parse_name (eaction->new_file);
	old_name = g_file_get_parse_name (eaction->old_file);

	descriptions[0] = g_strdup_printf (_("Rename '%s' as '%s'"), new_name, old_name);
	descriptions[1] = g_strdup_printf (_("Rename '%s' as '%s'"), old_name, new_name);

	labels[0] = g_strdup (_("_Undo Rename"));
	labels[1] = g_strdup (_("_Redo Rename"));

	g_free (old_name);
	g_free (new_name);
}

static void
create_undo_common (NautilusFileUndoDataCreate *eaction,
		    guint count,
		    gchar **descriptions)
{
	char *name;

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[0] = g_strdup_printf (_("Delete '%s'"), name);
	
	g_free (name);
}

static void
create_empty_description_func (NautilusFileUndoData *action,
			       guint count,
			       gchar **labels,
			       gchar **descriptions)
{
	gchar *name;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	create_undo_common (eaction, count, descriptions);

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[1] = g_strdup_printf (_("Create an empty file '%s'"), name);

	labels[0] = g_strdup (_("_Undo Create Empty File"));
	labels[1] = g_strdup (_("_Redo Create Empty File"));

	g_free (name);
}

static void
create_from_template_description_func (NautilusFileUndoData *action,
				       guint count,
				       gchar **labels,
				       gchar **descriptions)
{
	char *name;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	create_undo_common (eaction, count, descriptions);

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[1] = g_strdup_printf (_("Create new file '%s' from template "), name);
	g_free (name);

	labels[0] = g_strdup (_("_Undo Create from Template"));
	labels[1] = g_strdup (_("_Redo Create from Template"));
}

static void
create_folder_description_func (NautilusFileUndoData *action,
				guint count,
				gchar **labels,
				gchar **descriptions)
{
	char *name;
	NautilusFileUndoDataCreate* eaction = (NautilusFileUndoDataCreate*) action;

	create_undo_common (eaction, count, descriptions);

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[1] = g_strdup_printf (_("Create a new folder '%s'"), name);

	labels[0] = g_strdup (_("_Undo Create Folder"));
	labels[1] = g_strdup (_("_Redo Create Folder"));

	g_free (name);
}

static void
trash_description_func (NautilusFileUndoData *action,
			guint count,
			gchar **labels,
			gchar **descriptions)
{
	NautilusFileUndoDataTrash* eaction = (NautilusFileUndoDataTrash*) action;
	count = g_hash_table_size (eaction->trashed);

	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Restore %d items from trash"), count);
		descriptions[1] = g_strdup_printf (_("Move %d items to trash"), count);
	} else {
		GList *keys;
		char *name, *orig_path;
		GFile *file;

		keys = g_hash_table_get_keys (eaction->trashed);
		file = g_file_new_for_commandline_arg (keys->data);
		name = g_file_get_basename (file);
		orig_path = g_file_get_path (file);
		descriptions[0] = g_strdup_printf (_("Restore '%s' to '%s'"), name, orig_path);

		g_free (name);
		g_free (orig_path);
		g_list_free (keys);

		name = g_file_get_parse_name (file);
		descriptions[1] = g_strdup_printf (_("Move '%s' to trash"), name);

		g_free (name);
		g_object_unref (file);

		labels[0] = g_strdup (_("_Undo Trash"));
		labels[1] = g_strdup (_("_Redo Trash"));
	}
}

static void
restore_description_func (NautilusFileUndoData *action,
			  guint count,
			  gchar **labels,
			  gchar **descriptions)
{
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;
	
	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Move %d items back to trash"), count);
		descriptions[1] = g_strdup_printf (_("Restore %d items from trash"), count);
	} else {
		char *name;

		name = get_first_target_short_name (eaction);
		descriptions[0] = g_strdup_printf (_("Move '%s' back to trash"), name);
		descriptions[1] = g_strdup_printf (_("Restore '%s' from trash"), name);
		
		g_free (name);

		labels[0] = g_strdup (_("_Undo Restore from Trash"));
		labels[1] = g_strdup (_("_Redo Restore from Trash"));
	}
}

static void
create_link_description_func (NautilusFileUndoData *action,
			      guint count,
			      gchar **labels,
			      gchar **descriptions)
{
	NautilusFileUndoDataExt* eaction = (NautilusFileUndoDataExt*) action;
	
	if (count != 1) {
		descriptions[0] = g_strdup_printf (_("Delete links to %d items"), count);
		descriptions[1] = g_strdup_printf (_("Create links to %d items"), count);
	} else {
		char *name;

		name = get_first_target_short_name (eaction);
		descriptions[0] = g_strdup_printf (_("Delete link to '%s'"), name);
		descriptions[1] = g_strdup_printf (_("Create link to '%s'"), name);

		labels[0] = g_strdup (_("_Undo Create Link"));
		labels[1] = g_strdup (_("_Redo Create Link"));
 
		g_free (name);
	}
}

static void
recursive_permissions_description_func (NautilusFileUndoData *action,
					guint count,
					gchar **labels,
					gchar **descriptions)
{
	char *name;
	NautilusFileUndoDataRecursivePermissions* eaction = (NautilusFileUndoDataRecursivePermissions*) action;

	name = g_file_get_path (eaction->dest_dir);

	descriptions[0] = g_strdup_printf (_("Restore original permissions of items enclosed in '%s'"), name);
	descriptions[1] = g_strdup_printf (_("Set permissions of items enclosed in '%s'"), name);

	labels[0] = g_strdup (_("_Undo Change Permissions"));
	labels[1] = g_strdup (_("_Redo Change Permissions"));
	
	g_free (name);
}

static void
set_permissions_description_func (NautilusFileUndoData *action,
				  guint count,
				  gchar **labels,
				  gchar **descriptions)
{
	char *name;
	NautilusFileUndoDataPermissions* eaction = (NautilusFileUndoDataPermissions*) action;

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[0] = g_strdup_printf (_("Restore original permissions of '%s'"), name);
	descriptions[1] = g_strdup_printf (_("Set permissions of '%s'"), name);

	labels[0] = g_strdup (_("_Undo Change Permissions"));
	labels[1] = g_strdup (_("_Redo Change Permissions"));
	
	g_free (name);
}

static void
change_group_description_func (NautilusFileUndoData *action,
			       guint count,
			       gchar **labels,
			       gchar **descriptions)
{
	gchar *name;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[0] = g_strdup_printf (_("Restore group of '%s' to '%s'"),
					   name, eaction->original_ownership);
	descriptions[1] = g_strdup_printf (_("Set group of '%s' to '%s'"),
					   name, eaction->new_ownership);

	labels[0] = g_strdup (_("_Undo Change Group"));
	labels[1] = g_strdup (_("_Redo Change Group"));

	g_free (name);
}

static void
change_owner_description_func (NautilusFileUndoData *action,
			       guint count,
			       gchar **labels,
			       gchar **descriptions)
{
	gchar *name;
	NautilusFileUndoDataOwnership* eaction = (NautilusFileUndoDataOwnership*) action;

	name = g_file_get_parse_name (eaction->target_file);
	descriptions[0] = g_strdup_printf (_("Restore owner of '%s' to '%s'"),
					   name, eaction->original_ownership);
	descriptions[1] = g_strdup_printf (_("Set owner of '%s' to '%s'"),
					   name, eaction->new_ownership);

	labels[0] = g_strdup (_("_Undo Change Owner"));
	labels[1] = g_strdup (_("_Redo Change Owner"));

	g_free (name);
}

static void
finalize_undo_data_ext (NautilusFileUndoData* data)
{
	NautilusFileUndoDataExt* extdata = (NautilusFileUndoDataExt*) data;
	if (extdata->sources) {
		g_list_foreach (extdata->sources, (GFunc) g_free, NULL);
		g_list_free (extdata->sources);
	}
	if (extdata->destinations) {
		g_list_foreach (extdata->destinations, (GFunc) g_free, NULL);
		g_list_free (extdata->destinations);
	}
	if (extdata->src_dir) {
		g_object_unref (extdata->src_dir);
	}
	if (extdata->dest_dir) {
		g_object_unref (extdata->dest_dir);
	}
	g_slice_free (NautilusFileUndoDataExt, extdata);
}

static void
finalize_undo_data_rename (NautilusFileUndoData* data)
{
	NautilusFileUndoDataRename* extdata = (NautilusFileUndoDataRename*) data;
	if (extdata->old_file) {
		g_object_unref (extdata->old_file);
	}
	if (extdata->new_file) { 
		g_free (extdata->new_file);
	}
	g_slice_free (NautilusFileUndoDataRename, extdata);	
}

static void
finalize_undo_data_create (NautilusFileUndoData* data)
{
	NautilusFileUndoDataCreate* extdata = (NautilusFileUndoDataCreate*) data;
	if (extdata->target_file) {
		g_object_unref (extdata->target_file);
	}
	g_free (extdata->template);	
	g_slice_free (NautilusFileUndoDataCreate, extdata);
}

static void
finalize_undo_data_trash (NautilusFileUndoData* data)
{
	NautilusFileUndoDataTrash* extdata = (NautilusFileUndoDataTrash*) data;
	if (extdata->trashed) {
		g_hash_table_destroy (extdata->trashed);
	}
	g_slice_free (NautilusFileUndoDataTrash, extdata);
}

static void
finalize_undo_data_permissions (NautilusFileUndoData* data)
{
	NautilusFileUndoDataPermissions* extdata = (NautilusFileUndoDataPermissions*) data;
	if (extdata->target_file) {
		g_object_unref (extdata->target_file);
	}
	g_slice_free (NautilusFileUndoDataPermissions, extdata);
}

static void
finalize_undo_data_recursivepermissions (NautilusFileUndoData* data)
{
	NautilusFileUndoDataRecursivePermissions* extdata = (NautilusFileUndoDataRecursivePermissions*) data;
	if (extdata->original_permissions) {
		g_hash_table_destroy (extdata->original_permissions);
	}
	if (extdata->dest_dir) {
		g_object_unref (extdata->dest_dir);
	}
	g_slice_free (NautilusFileUndoDataRecursivePermissions, extdata);
}

static void
finalize_undo_data_ownership (NautilusFileUndoData* data)
{
	NautilusFileUndoDataOwnership* extdata = (NautilusFileUndoDataOwnership*) data;
	if (extdata->target_file) {
		g_object_unref (extdata->target_file);
	}
	g_free (extdata->original_ownership);
	g_free (extdata->new_ownership);
	g_slice_free (NautilusFileUndoDataOwnership, extdata);
}


static NautilusFileUndoData *
create_from_type (NautilusFileUndoDataType type)
{
	struct {
		void (* undo_func)               (NautilusFileUndoData *data,
						  GtkWindow            *parent_window);
		void (* redo_func)               (NautilusFileUndoData *data,
						  GtkWindow            *parent_window);
		void (* strings_func)            (NautilusFileUndoData *data,
						  guint count,
						  gchar **labels,
						  gchar **descriptions);
		void (* finalize_func)           (NautilusFileUndoData *data);
		gsize alloc_size;
	} const mappings[NAUTILUS_FILE_UNDO_NUM_TYPES] = {
		/* copy action */
		{ copy_or_link_undo_func, copy_redo_func,
		  copy_description_func, finalize_undo_data_ext,
		  sizeof(NautilusFileUndoDataExt) },
		/* duplicate action */
		{ copy_or_link_undo_func, duplicate_redo_func,
		  duplicate_description_func, finalize_undo_data_ext,
		  sizeof(NautilusFileUndoDataExt) },
		/* move action */
		{ move_undo_func, move_restore_redo_func,
		  move_description_func, finalize_undo_data_ext,
		  sizeof(NautilusFileUndoDataExt) },
		/* rename action */
		{ rename_undo_func, rename_redo_func,
		  rename_description_func, finalize_undo_data_rename,
		  sizeof(NautilusFileUndoDataRename) },
		/* create empty action */
		{ create_undo_func, create_empty_redo_func,
		  create_empty_description_func, finalize_undo_data_create,
		  sizeof(NautilusFileUndoDataCreate) },
		/* create from template action */
		{ create_undo_func, create_from_template_redo_func,
		  create_from_template_description_func, finalize_undo_data_create,
		  sizeof(NautilusFileUndoDataCreate) },
		/* create folder action */
		{ create_undo_func, create_folder_redo_func,
		  create_folder_description_func, finalize_undo_data_create,
		  sizeof(NautilusFileUndoDataCreate) },
		/* move to trash action */
		{ trash_undo_func, trash_redo_func,
		  trash_description_func, finalize_undo_data_trash,
		  sizeof(NautilusFileUndoDataTrash) },
		/* restore from trash action */
		{ restore_undo_func, move_restore_redo_func,
		  restore_description_func, finalize_undo_data_ext,
		  sizeof(NautilusFileUndoDataExt) },
		/* create link action */
		{ create_undo_func, create_link_redo_func,
		  create_link_description_func, finalize_undo_data_ext,
		  sizeof(NautilusFileUndoDataExt) },
		/* recursive permissions action */
		{ recursive_permissions_undo_func, recursive_permissions_redo_func,
		  recursive_permissions_description_func, finalize_undo_data_recursivepermissions,
		  sizeof(NautilusFileUndoDataRecursivePermissions) },
		/* set permissions action */
		{ set_permissions_undo_func, set_permissions_redo_func,
		  set_permissions_description_func , finalize_undo_data_permissions,
		  sizeof(NautilusFileUndoDataPermissions) },
		/* change group action */
		{ change_group_undo_func, change_group_redo_func,
		  change_group_description_func, finalize_undo_data_ownership,
		  sizeof(NautilusFileUndoDataOwnership) },
		/* change owner action */
		{ change_owner_undo_func, change_owner_redo_func,
		  change_owner_description_func, finalize_undo_data_ownership,
		  sizeof(NautilusFileUndoDataOwnership) },
	};

	NautilusFileUndoData *retval;

	retval = g_slice_alloc0 (mappings[type].alloc_size);
	retval->undo_func = mappings[type].undo_func;
	retval->redo_func = mappings[type].redo_func;
	retval->strings_func = mappings[type].strings_func;
	retval->finalize_func = mappings[type].finalize_func;

	return retval;
}

/* functions to manipulate the action data */
NautilusFileUndoData *
nautilus_file_undo_data_new (NautilusFileUndoDataType type,
			     gint                     items_count)
{
	NautilusFileUndoData *data;

	data = create_from_type (type);
	data->type = type;
	data->count = items_count;

	return data;
}

static void
nautilus_file_undo_info_init (NautilusFileUndoInfo *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_FILE_UNDO_INFO,
						  NautilusFileUndoInfoDetails);
}

static void
nautilus_file_undo_info_get_property (GObject *object,
				      guint property_id,
				      GValue *value,
				      GParamSpec *pspec)
{

}

static void
nautilus_file_undo_info_set_property (GObject *object,
				      guint property_id,
				      const GValue *value,
				      GParamSpec *pspec)
{

}

static void
nautilus_file_undo_info_constructed (GObject *obj)
{
	NautilusFileUndoInfo *self = NAUTILUS_FILE_UNDO_INFO (obj);
}

static void
nautilus_file_undo_info_finalize (GObject *obj)
{
	NautilusFileUndoInfo *self = NAUTILUS_FILE_UNDO_INFO (obj);

	if (self->priv->undo_data != NULL) {
		nautilus_file_undo_data_free (self->priv->undo_data);
		self->priv->undo_data = NULL;
	}

	G_OBJECT_CLASS (nautilus_file_undo_info_parent_class)->finalize (obj);
}

static void
nautilus_file_undo_info_class_init (NautilusFileUndoInfoClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->finalize = nautilus_file_undo_info_finalize;
	oclass->constructed = nautilus_file_undo_info_constructed;
	oclass->get_property = nautilus_file_undo_info_get_property;
	oclass->set_property = nautilus_file_undo_info_set_property;

	properties[PROP_TYPE] =
		g_param_spec_int ("type",
				  "Undo info type",
				  "Type of undo operation",
				  0, NAUTILUS_FILE_UNDO_NUM_TYPES - 1, 0,
				  G_PARAM_READWRITE |
				  G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_ITEM_COUNT] =
		g_param_spec_int ("item-count",
				  "Number of items",
				  "Number of items",
				  0, G_MAXINT, 0,
				  G_PARAM_READWRITE |
				  G_PARAM_CONSTRUCT_ONLY);

	g_type_class_add_private (klass, sizeof (NautilusFileUndoInfoDetails));
	g_object_class_install_properties (oclass, N_PROPERTIES, properties);
}

NautilusFileUndoInfo *
nautilus_file_undo_info_new (NautilusFileUndoDataType type,
			     gint item_count)
{
	NautilusFileUndoInfo *retval;
	NautilusFileUndoData *data;

	retval = g_object_new (NAUTILUS_TYPE_FILE_UNDO_INFO, 
			       "type", type,
			       "item-count", item_count,
			       NULL);

	return retval;
}

void
nautilus_file_undo_data_set_src_dir (NautilusFileUndoData        *action_data,
				     GFile                       *src)
{
	NautilusFileUndoDataExt* eaction_data = (NautilusFileUndoDataExt*) action_data;
	eaction_data->src_dir = g_object_ref (src);
}

void
nautilus_file_undo_data_set_dest_dir (NautilusFileUndoData        *action_data,
				      GFile                       *dest)
{
	NautilusFileUndoDataExt* eaction_data = (NautilusFileUndoDataExt*) action_data;
	eaction_data->dest_dir = g_object_ref (dest);
}

void
nautilus_file_undo_data_add_origin_target_pair (NautilusFileUndoData        *action_data,
						GFile                       *origin,
						GFile                       *target)
{
	NautilusFileUndoDataExt* eaction_data = (NautilusFileUndoDataExt*) action_data;
	eaction_data->sources =
		g_list_append (eaction_data->sources, g_object_ref (origin));
	eaction_data->destinations =
		g_list_append (eaction_data->destinations, g_object_ref (target));

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_add_trashed_file (NautilusFileUndoData        *action_data,
					  GFile                       *file,
					  guint64                      mtime)
{
	guint64 *modification_time;
	char *original_uri;
	NautilusFileUndoDataTrash* eaction_data = (NautilusFileUndoDataTrash*) action_data;

	if (eaction_data->trashed == NULL) {
		eaction_data->trashed =
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	}

	modification_time = g_new (guint64, 1);
	*modification_time = mtime;

	original_uri = g_file_get_uri (file);

	g_hash_table_insert (eaction_data->trashed, original_uri, modification_time);

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_add_file_permissions (NautilusFileUndoData        *action_data,
					      GFile                       *file,
					      guint32                      permission)
{
	guint32 *current_permissions;
	char *original_uri;
	NautilusFileUndoDataRecursivePermissions* eaction_data = (NautilusFileUndoDataRecursivePermissions*) action_data;

	if (eaction_data->original_permissions == NULL) {
		eaction_data->original_permissions =
			g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	}

	current_permissions = g_new (guint32, 1);
	*current_permissions = permission;

	original_uri = g_file_get_uri (file);

	g_hash_table_insert (eaction_data->original_permissions, original_uri, current_permissions);

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_file_permissions (NautilusFileUndoData        *action_data,
					      GFile                       *file,
					      guint32                      current_permissions,
					      guint32                      new_permissions)
{
	NautilusFileUndoDataPermissions* eaction_data = (NautilusFileUndoDataPermissions*) action_data;
	eaction_data->target_file = g_object_ref (file);
	eaction_data->current_permissions = current_permissions;
	eaction_data->new_permissions = new_permissions;

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_owner_change_information (NautilusFileUndoData        *action_data,
						      GFile                       *file,
						      const char                  *current_user,
						      const char                  *new_user)
{
	NautilusFileUndoDataOwnership* eaction_data = (NautilusFileUndoDataOwnership*) action_data;
	eaction_data->target_file = g_object_ref (file);
	eaction_data->original_ownership = g_strdup (current_user);
	eaction_data->new_ownership = g_strdup (new_user);

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_group_change_information (NautilusFileUndoData        *action_data,
						      GFile                       *file,
						      const char                  *current_group,
						      const char                  *new_group)
{
	NautilusFileUndoDataOwnership* eaction_data = (NautilusFileUndoDataOwnership*) action_data;
	eaction_data->target_file = g_object_ref (file);
	eaction_data->original_ownership = g_strdup (current_group);
	eaction_data->new_ownership = g_strdup (new_group);

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_recursive_permissions (NautilusFileUndoData         *action_data,
						   guint32                      file_permissions,
						   guint32                      file_mask,
						   guint32                      dir_permissions,
						   guint32                      dir_mask)
{
	NautilusFileUndoDataRecursivePermissions* eaction_data = (NautilusFileUndoDataRecursivePermissions*) action_data;
	eaction_data->file_permissions = file_permissions;
	eaction_data->file_mask = file_mask;
	eaction_data->dir_permissions = dir_permissions;
	eaction_data->dir_mask = dir_mask;

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_recursive_permissions_dest_dir (NautilusFileUndoData        *action_data,
                                                            GFile                       *dest)
{
	NautilusFileUndoDataRecursivePermissions* eaction_data = (NautilusFileUndoDataRecursivePermissions*) action_data;
	eaction_data->dest_dir = g_object_ref (dest);
}

void
nautilus_file_undo_data_set_create_data (NautilusFileUndoData        *action_data,
                                         GFile                       *file,
                                         const char                  *template)
{
	NautilusFileUndoDataCreate* eaction_data = (NautilusFileUndoDataCreate*) action_data;
	eaction_data->target_file = g_object_ref (file);
	eaction_data->template = g_strdup (template);

	action_data->is_valid = TRUE;
}

void
nautilus_file_undo_data_set_rename_information (NautilusFileUndoData        *action_data,
						GFile                       *old_file,
						GFile                       *new_file)
{
	NautilusFileUndoDataRename* eaction_data = (NautilusFileUndoDataRename*) action_data;
	eaction_data->old_file = g_object_ref (old_file);
	eaction_data->new_file = g_object_ref (new_file);

	action_data->is_valid = TRUE;
}
