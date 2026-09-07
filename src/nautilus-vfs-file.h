/*
 * nautilus-vfs-file.h: Subclass of NautilusFile to implement the
 * the case of a VFS file.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "nautilus-file.h"

#define NAUTILUS_TYPE_VFS_FILE nautilus_vfs_file_get_type()
#define NAUTILUS_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_VFS_FILE, NautilusVFSFile))
#define NAUTILUS_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VFS_FILE, NautilusVFSFileClass))
#define NAUTILUS_IS_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_VFS_FILE))
#define NAUTILUS_IS_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VFS_FILE))
#define NAUTILUS_VFS_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_VFS_FILE, NautilusVFSFileClass))

typedef struct NautilusVFSFileDetails NautilusVFSFileDetails;

typedef struct {
	NautilusFile parent_slot;
} NautilusVFSFile;

typedef struct {
	NautilusFileClass parent_slot;
} NautilusVFSFileClass;

GType   nautilus_vfs_file_get_type (void);
