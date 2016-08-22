
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

#ifndef __NAUTILUS_FILE_UNDO_OPERATIONS_H__
#define __NAUTILUS_FILE_UNDO_OPERATIONS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-autoar/gnome-autoar.h>

typedef enum {
	NAUTILUS_FILE_UNDO_OP_COPY,
	NAUTILUS_FILE_UNDO_OP_DUPLICATE,
	NAUTILUS_FILE_UNDO_OP_MOVE,
	NAUTILUS_FILE_UNDO_OP_RENAME,
	NAUTILUS_FILE_UNDO_OP_BATCH_RENAME,
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

#define NAUTILUS_TYPE_FILE_UNDO_INFO         (nautilus_file_undo_info_get_type ())
#define NAUTILUS_FILE_UNDO_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO, NautilusFileUndoInfo))
#define NAUTILUS_FILE_UNDO_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO, NautilusFileUndoInfoClass))
#define NAUTILUS_IS_FILE_UNDO_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO))
#define NAUTILUS_IS_FILE_UNDO_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO))
#define NAUTILUS_FILE_UNDO_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO, NautilusFileUndoInfoClass))

typedef struct _NautilusFileUndoInfo      NautilusFileUndoInfo;
typedef struct _NautilusFileUndoInfoClass NautilusFileUndoInfoClass;
typedef struct _NautilusFileUndoInfoDetails NautilusFileUndoInfoDetails;

struct _NautilusFileUndoInfo {
	GObject parent;
	NautilusFileUndoInfoDetails *priv;
};

struct _NautilusFileUndoInfoClass {
	GObjectClass parent_class;

	void (* undo_func) (NautilusFileUndoInfo *self,
			    GtkWindow            *parent_window);
	void (* redo_func) (NautilusFileUndoInfo *self,
			    GtkWindow            *parent_window);

	void (* strings_func) (NautilusFileUndoInfo *self,
			       gchar **undo_label,
			       gchar **undo_description,
			       gchar **redo_label,
			       gchar **redo_description);
};

GType nautilus_file_undo_info_get_type (void) G_GNUC_CONST;

void nautilus_file_undo_info_apply_async (NautilusFileUndoInfo *self,
					  gboolean undo,
					  GtkWindow *parent_window,
					  GAsyncReadyCallback callback,
					  gpointer user_data);
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
#define NAUTILUS_TYPE_FILE_UNDO_INFO_EXT         (nautilus_file_undo_info_ext_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_EXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXT, NautilusFileUndoInfoExt))
#define NAUTILUS_FILE_UNDO_INFO_EXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_EXT, NautilusFileUndoInfoExtClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_EXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXT))
#define NAUTILUS_IS_FILE_UNDO_INFO_EXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_EXT))
#define NAUTILUS_FILE_UNDO_INFO_EXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXT, NautilusFileUndoInfoExtClass))

typedef struct _NautilusFileUndoInfoExt      NautilusFileUndoInfoExt;
typedef struct _NautilusFileUndoInfoExtClass NautilusFileUndoInfoExtClass;
typedef struct _NautilusFileUndoInfoExtDetails NautilusFileUndoInfoExtDetails;

struct _NautilusFileUndoInfoExt {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoExtDetails *priv;
};

struct _NautilusFileUndoInfoExtClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_ext_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_ext_new (NautilusFileUndoOp op_type,
						       gint item_count,
						       GFile *src_dir,
						       GFile *target_dir);
void nautilus_file_undo_info_ext_add_origin_target_pair (NautilusFileUndoInfoExt *self,
							 GFile                   *origin,
							 GFile                   *target);

/* create new file/folder */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE         (nautilus_file_undo_info_create_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_CREATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE, NautilusFileUndoInfoCreate))
#define NAUTILUS_FILE_UNDO_INFO_CREATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE, NautilusFileUndoInfoCreateClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_CREATE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE))
#define NAUTILUS_IS_FILE_UNDO_INFO_CREATE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE))
#define NAUTILUS_FILE_UNDO_INFO_CREATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_CREATE, NautilusFileUndoInfoCreateClass))

typedef struct _NautilusFileUndoInfoCreate      NautilusFileUndoInfoCreate;
typedef struct _NautilusFileUndoInfoCreateClass NautilusFileUndoInfoCreateClass;
typedef struct _NautilusFileUndoInfoCreateDetails NautilusFileUndoInfoCreateDetails;

struct _NautilusFileUndoInfoCreate {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoCreateDetails *priv;
};

struct _NautilusFileUndoInfoCreateClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_create_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_create_new (NautilusFileUndoOp op_type);
void nautilus_file_undo_info_create_set_data (NautilusFileUndoInfoCreate *self,
					      GFile                      *file,
					      const char                 *template,
					      gint                        length);

/* rename */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME         (nautilus_file_undo_info_rename_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_RENAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME, NautilusFileUndoInfoRename))
#define NAUTILUS_FILE_UNDO_INFO_RENAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME, NautilusFileUndoInfoRenameClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_RENAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME))
#define NAUTILUS_IS_FILE_UNDO_INFO_RENAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME))
#define NAUTILUS_FILE_UNDO_INFO_RENAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_RENAME, NautilusFileUndoInfoRenameClass))

typedef struct _NautilusFileUndoInfoRename      NautilusFileUndoInfoRename;
typedef struct _NautilusFileUndoInfoRenameClass NautilusFileUndoInfoRenameClass;
typedef struct _NautilusFileUndoInfoRenameDetails NautilusFileUndoInfoRenameDetails;

struct _NautilusFileUndoInfoRename {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoRenameDetails *priv;
};

struct _NautilusFileUndoInfoRenameClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_rename_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_rename_new (void);
void nautilus_file_undo_info_rename_set_data_pre (NautilusFileUndoInfoRename *self,
						  GFile                      *old_file,
						  gchar                      *old_display_name,
						  gchar                      *new_display_name);
void nautilus_file_undo_info_rename_set_data_post (NautilusFileUndoInfoRename *self,
						   GFile                      *new_file);

/* batch rename */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME         (nautilus_file_undo_info_batch_rename_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_BATCH_RENAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME, NautilusFileUndoInfoBatchRename))
#define NAUTILUS_FILE_UNDO_INFO_BATCH_RENAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME, NautilusFileUndoInfoBatchRenameClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_BATCH_RENAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME))
#define NAUTILUS_IS_FILE_UNDO_INFO_BATCH_RENAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME))
#define NAUTILUS_FILE_UNDO_INFO_BATCH_RENAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_BATCH_RENAME, NautilusFileUndoInfoBatchRenameClass))

typedef struct _NautilusFileUndoInfoBatchRename      NautilusFileUndoInfoBatchRename;
typedef struct _NautilusFileUndoInfoBatchRenameClass NautilusFileUndoInfoBatchRenameClass;
typedef struct _NautilusFileUndoInfoBatchRenameDetails NautilusFileUndoInfoBatchRenameDetails;

struct _NautilusFileUndoInfoBatchRename {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoBatchRenameDetails *priv;
};

struct _NautilusFileUndoInfoBatchRenameClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_batch_rename_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_batch_rename_new (gint item_count);
void nautilus_file_undo_info_batch_rename_set_data_pre (NautilusFileUndoInfoBatchRename *self,
						        GList                           *old_files);
void nautilus_file_undo_info_batch_rename_set_data_post (NautilusFileUndoInfoBatchRename *self,
						         GList                           *new_files);

/* trash */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH         (nautilus_file_undo_info_trash_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_TRASH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH, NautilusFileUndoInfoTrash))
#define NAUTILUS_FILE_UNDO_INFO_TRASH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH, NautilusFileUndoInfoTrashClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_TRASH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH))
#define NAUTILUS_IS_FILE_UNDO_INFO_TRASH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH))
#define NAUTILUS_FILE_UNDO_INFO_TRASH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_TRASH, NautilusFileUndoInfoTrashClass))

typedef struct _NautilusFileUndoInfoTrash      NautilusFileUndoInfoTrash;
typedef struct _NautilusFileUndoInfoTrashClass NautilusFileUndoInfoTrashClass;
typedef struct _NautilusFileUndoInfoTrashDetails NautilusFileUndoInfoTrashDetails;

struct _NautilusFileUndoInfoTrash {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoTrashDetails *priv;
};

struct _NautilusFileUndoInfoTrashClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_trash_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_trash_new (gint item_count);
void nautilus_file_undo_info_trash_add_file (NautilusFileUndoInfoTrash *self,
					     GFile                     *file);
GList *nautilus_file_undo_info_trash_get_files (NautilusFileUndoInfoTrash *self);

/* recursive permissions */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS         (nautilus_file_undo_info_rec_permissions_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_REC_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NautilusFileUndoInfoRecPermissions))
#define NAUTILUS_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NautilusFileUndoInfoRecPermissionsClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_REC_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NAUTILUS_IS_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NAUTILUS_FILE_UNDO_INFO_REC_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NautilusFileUndoInfoRecPermissionsClass))

typedef struct _NautilusFileUndoInfoRecPermissions      NautilusFileUndoInfoRecPermissions;
typedef struct _NautilusFileUndoInfoRecPermissionsClass NautilusFileUndoInfoRecPermissionsClass;
typedef struct _NautilusFileUndoInfoRecPermissionsDetails NautilusFileUndoInfoRecPermissionsDetails;

struct _NautilusFileUndoInfoRecPermissions {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoRecPermissionsDetails *priv;
};

struct _NautilusFileUndoInfoRecPermissionsClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_rec_permissions_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_rec_permissions_new (GFile   *dest,
								   guint32 file_permissions,
								   guint32 file_mask,
								   guint32 dir_permissions,
								   guint32 dir_mask);
void nautilus_file_undo_info_rec_permissions_add_file (NautilusFileUndoInfoRecPermissions *self,
						       GFile                              *file,
						       guint32                             permission);

/* single file change permissions */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS         (nautilus_file_undo_info_permissions_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS, NautilusFileUndoInfoPermissions))
#define NAUTILUS_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS, NautilusFileUndoInfoPermissionsClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NAUTILUS_IS_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NAUTILUS_FILE_UNDO_INFO_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_PERMISSIONS, NautilusFileUndoInfoPermissionsClass))

typedef struct _NautilusFileUndoInfoPermissions      NautilusFileUndoInfoPermissions;
typedef struct _NautilusFileUndoInfoPermissionsClass NautilusFileUndoInfoPermissionsClass;
typedef struct _NautilusFileUndoInfoPermissionsDetails NautilusFileUndoInfoPermissionsDetails;

struct _NautilusFileUndoInfoPermissions {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoPermissionsDetails *priv;
};

struct _NautilusFileUndoInfoPermissionsClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_permissions_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_permissions_new (GFile   *file,
							       guint32  current_permissions,
							       guint32  new_permissions);

/* group and owner change */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP         (nautilus_file_undo_info_ownership_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_OWNERSHIP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP, NautilusFileUndoInfoOwnership))
#define NAUTILUS_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP, NautilusFileUndoInfoOwnershipClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_OWNERSHIP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NAUTILUS_IS_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NAUTILUS_FILE_UNDO_INFO_OWNERSHIP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_OWNERSHIP, NautilusFileUndoInfoOwnershipClass))

typedef struct _NautilusFileUndoInfoOwnership      NautilusFileUndoInfoOwnership;
typedef struct _NautilusFileUndoInfoOwnershipClass NautilusFileUndoInfoOwnershipClass;
typedef struct _NautilusFileUndoInfoOwnershipDetails NautilusFileUndoInfoOwnershipDetails;

struct _NautilusFileUndoInfoOwnership {
	NautilusFileUndoInfo parent;
	NautilusFileUndoInfoOwnershipDetails *priv;
};

struct _NautilusFileUndoInfoOwnershipClass {
	NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_ownership_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo *nautilus_file_undo_info_ownership_new (NautilusFileUndoOp  op_type,
							     GFile              *file,
							     const char         *current_data,
							     const char         *new_data);

/* extract */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT         (nautilus_file_undo_info_extract_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_EXTRACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT, NautilusFileUndoInfoExtract))
#define NAUTILUS_FILE_UNDO_INFO_EXTRACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT, NautilusFileUndoInfoExtractClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_EXTRACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT))
#define NAUTILUS_IS_FILE_UNDO_INFO_EXTRACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT))
#define NAUTILUS_FILE_UNDO_INFO_EXTRACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_EXTRACT, NautilusFileUndoInfoExtractClass))

typedef struct _NautilusFileUndoInfoExtract        NautilusFileUndoInfoExtract;
typedef struct _NautilusFileUndoInfoExtractClass   NautilusFileUndoInfoExtractClass;
typedef struct _NautilusFileUndoInfoExtractDetails NautilusFileUndoInfoExtractDetails;

struct _NautilusFileUndoInfoExtract {
        NautilusFileUndoInfo parent;
        NautilusFileUndoInfoExtractDetails *priv;
};

struct _NautilusFileUndoInfoExtractClass {
        NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_extract_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo * nautilus_file_undo_info_extract_new (GList *sources,
                                                            GFile *destination_directory);
void nautilus_file_undo_info_extract_set_outputs (NautilusFileUndoInfoExtract *self,
                                                  GList                       *outputs);

/* compress */
#define NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS         (nautilus_file_undo_info_compress_get_type ())
#define NAUTILUS_FILE_UNDO_INFO_COMPRESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS, NautilusFileUndoInfoCompress))
#define NAUTILUS_FILE_UNDO_INFO_COMPRESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS, NautilusFileUndoInfoCompressClass))
#define NAUTILUS_IS_FILE_UNDO_INFO_COMPRESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS))
#define NAUTILUS_IS_FILE_UNDO_INFO_COMPRESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS))
#define NAUTILUS_FILE_UNDO_INFO_COMPRESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_UNDO_INFO_COMPRESS, NautilusFileUndoInfoCompressClass))

typedef struct _NautilusFileUndoInfoCompress        NautilusFileUndoInfoCompress;
typedef struct _NautilusFileUndoInfoCompressClass   NautilusFileUndoInfoCompressClass;
typedef struct _NautilusFileUndoInfoCompressDetails NautilusFileUndoInfoCompressDetails;

struct _NautilusFileUndoInfoCompress {
        NautilusFileUndoInfo parent;
        NautilusFileUndoInfoCompressDetails *priv;
};

struct _NautilusFileUndoInfoCompressClass {
        NautilusFileUndoInfoClass parent_class;
};

GType nautilus_file_undo_info_compress_get_type (void) G_GNUC_CONST;
NautilusFileUndoInfo * nautilus_file_undo_info_compress_new (GList        *sources,
                                                             GFile        *output,
                                                             AutoarFormat  format,
                                                             AutoarFilter  filter);


#endif /* __NAUTILUS_FILE_UNDO_OPERATIONS_H__ */
