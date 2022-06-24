
/* nautilus-file-undo-operations.h - Manages undo/redo of file operations
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-autoar/gnome-autoar.h>

#include "nautilus-file-operations-dbus-data.h"

typedef enum
{
    NAUTILUS_FILE_UNDO_OP_INVALID,
    NAUTILUS_FILE_UNDO_OP_COPY,
    NAUTILUS_FILE_UNDO_OP_DUPLICATE,
    NAUTILUS_FILE_UNDO_OP_MOVE,
    NAUTILUS_FILE_UNDO_OP_RENAME,
    NAUTILUS_FILE_UNDO_OP_BATCH_RENAME,
    NAUTILUS_FILE_UNDO_OP_STARRED,
    NAUTILUS_FILE_UNDO_OP_CREATE_EMPTY_FILE,
    NAUTILUS_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE,
    NAUTILUS_FILE_UNDO_OP_CREATE_FOLDER,
    NAUTILUS_FILE_UNDO_OP_EXTRACT,
    NAUTILUS_FILE_UNDO_OP_COMPRESS,
    NAUTILUS_FILE_UNDO_OP_MOVE_TO_TRASH,
    NAUTILUS_FILE_UNDO_OP_RESTORE_FROM_TRASH,
    NAUTILUS_FILE_UNDO_OP_CREATE_LINK,
    NAUTILUS_FILE_UNDO_OP_RECURSIVE_SET_PERMISSIONS,
    NAUTILUS_FILE_UNDO_OP_SET_PERMISSIONS,
    NAUTILUS_FILE_UNDO_OP_CHANGE_GROUP,
    NAUTILUS_FILE_UNDO_OP_CHANGE_OWNER,
    NAUTILUS_FILE_UNDO_OP_NUM_TYPES,
} NautilusFileUndoOp;

#define NAUTILUS_TYPE_FILE_UNDO_INFO nautilus_file_undo_info_get_type ()
G_DECLARE_DERIVABLE_TYPE (NautilusFileUndoInfo, nautilus_file_undo_info,
                          NAUTILUS, FILE_UNDO_INFO,
                          GObject)

struct _NautilusFileUndoInfoClass
{
    GObjectClass parent_class;

    void (* undo_func) (NautilusFileUndoInfo           *self,
                        GtkWindow                      *parent_window,
                        NautilusFileOperationsDBusData *dbus_data);
    void (* redo_func) (NautilusFileUndoInfo           *self,
                        GtkWindow                      *parent_window,
                        NautilusFileOperationsDBusData *dbus_data);

    void (* strings_func) (NautilusFileUndoInfo *self,
                           gchar **undo_label,
                           gchar **undo_description,
                           gchar **redo_label,
                           gchar **redo_description);
};

void nautilus_file_undo_info_apply_async (NautilusFileUndoInfo           *self,
                                          gboolean                        undo,
                                          GtkWindow                      *parent_window,
                                          NautilusFileOperationsDBusData *dbus_data,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
gboolean nautilus_file_undo_info_apply_finish (NautilusFileUndoInfo *self,
                                               GAsyncResult *res,
                                               gboolean *user_cancel,
                                               GError **error);

void nautilus_file_undo_info_get_strings (NautilusFileUndoInfo *self,
                                          gchar **undo_label,
                                          gchar **undo_description,
                                          gchar **redo_label,
                                          gchar **redo_description);

NautilusFileUndoOp nautilus_file_undo_info_get_op_type (NautilusFileUndoInfo *self);

/* copy/move/duplicate/link/restore from trash */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_EXT nautilus_file_undo_info_ext_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoExt, nautilus_file_undo_info_ext,
                      NAUTILUS, FILE_UNDO_INFO_EXT,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_ext_new (NautilusFileUndoOp op_type,
                                                       gint item_count,
                                                       GFile *src_dir,
                                                       GFile *target_dir);
void nautilus_file_undo_info_ext_add_origin_target_pair (NautilusFileUndoInfoExt *self,
                                                         GFile                   *origin,
                                                         GFile                   *target);

/* create new file/folder */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE nautilus_file_undo_info_create_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoCreate, nautilus_file_undo_info_create,
                      NAUTILUS, FILE_UNDO_INFO_CREATE,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_create_new (NautilusFileUndoOp op_type);
void nautilus_file_undo_info_create_set_data (NautilusFileUndoInfoCreate *self,
                                              GFile                      *file,
                                              const char                 *template,
                                              gsize                       length);

/* rename */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME nautilus_file_undo_info_rename_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoRename, nautilus_file_undo_info_rename,
                      NAUTILUS, FILE_UNDO_INFO_RENAME,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_rename_new (void);
void nautilus_file_undo_info_rename_set_data_pre (NautilusFileUndoInfoRename *self,
                                                  GFile                      *old_file,
                                                  const char                 *old_display_name,
                                                  const char                 *new_display_name);
void nautilus_file_undo_info_rename_set_data_post (NautilusFileUndoInfoRename *self,
                                                   GFile                      *new_file);

/* batch rename */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME nautilus_file_undo_info_batch_rename_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoBatchRename, nautilus_file_undo_info_batch_rename,
                      NAUTILUS, FILE_UNDO_INFO_BATCH_RENAME,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_batch_rename_new (gint item_count);
void nautilus_file_undo_info_batch_rename_set_data_pre (NautilusFileUndoInfoBatchRename *self,
                                                        GList                           *old_files);
void nautilus_file_undo_info_batch_rename_set_data_post (NautilusFileUndoInfoBatchRename *self,
                                                         GList                           *new_files);

/* starred files */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_STARRED         (nautilus_file_undo_info_starred_get_type ())
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoStarred, nautilus_file_undo_info_starred,
                      NAUTILUS, FILE_UNDO_INFO_STARRED,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_starred_new (GList   *files,
                                                           gboolean starred);
GList *nautilus_file_undo_info_starred_get_files (NautilusFileUndoInfoStarred *self);
gboolean nautilus_file_undo_info_starred_is_starred (NautilusFileUndoInfoStarred *self);

/* trash */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH         (nautilus_file_undo_info_trash_get_type ())
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoTrash, nautilus_file_undo_info_trash,
                      NAUTILUS, FILE_UNDO_INFO_TRASH,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_trash_new (gint item_count);
void nautilus_file_undo_info_trash_add_file (NautilusFileUndoInfoTrash *self,
                                             GFile                     *file);
GList *nautilus_file_undo_info_trash_get_files (NautilusFileUndoInfoTrash *self);

/* recursive permissions */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS nautilus_file_undo_info_rec_permissions_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoRecPermissions, nautilus_file_undo_info_rec_permissions,
                      NAUTILUS, FILE_UNDO_INFO_REC_PERMISSIONS,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_rec_permissions_new (GFile   *dest,
                                                                   guint32 file_permissions,
                                                                   guint32 file_mask,
                                                                   guint32 dir_permissions,
                                                                   guint32 dir_mask);
void nautilus_file_undo_info_rec_permissions_add_file (NautilusFileUndoInfoRecPermissions *self,
                                                       GFile                              *file,
                                                       guint32                             permission);

/* single file change permissions */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS nautilus_file_undo_info_permissions_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoPermissions, nautilus_file_undo_info_permissions,
                      NAUTILUS, FILE_UNDO_INFO_PERMISSIONS,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_permissions_new (GFile   *file,
                                                               guint32  current_permissions,
                                                               guint32  new_permissions);

/* group and owner change */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP nautilus_file_undo_info_ownership_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoOwnership, nautilus_file_undo_info_ownership,
                      NAUTILUS, FILE_UNDO_INFO_OWNERSHIP,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo *nautilus_file_undo_info_ownership_new (NautilusFileUndoOp  op_type,
                                                             GFile              *file,
                                                             const char         *current_data,
                                                             const char         *new_data);

/* extract */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT nautilus_file_undo_info_extract_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoExtract, nautilus_file_undo_info_extract,
                      NAUTILUS, FILE_UNDO_INFO_EXTRACT,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo * nautilus_file_undo_info_extract_new (GList *sources,
                                                            GFile *destination_directory);
void nautilus_file_undo_info_extract_set_outputs (NautilusFileUndoInfoExtract *self,
                                                  GList                       *outputs);

/* compress */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS nautilus_file_undo_info_compress_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFileUndoInfoCompress, nautilus_file_undo_info_compress,
                      NAUTILUS, FILE_UNDO_INFO_COMPRESS,
                      NautilusFileUndoInfo)

NautilusFileUndoInfo * nautilus_file_undo_info_compress_new (GList        *sources,
                                                             GFile        *output,
                                                             AutoarFormat  format,
                                                             AutoarFilter  filter,
                                                             const gchar  *passphrase);
