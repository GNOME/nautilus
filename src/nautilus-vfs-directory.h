/*
 * nautilus-vfs-directory.h: Subclass of NautilusDirectory to implement the
 * the case of a VFS directory.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "nautilus-directory.h"

#define NAUTILUS_TYPE_VFS_DIRECTORY nautilus_vfs_directory_get_type()
#define NAUTILUS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_VFS_DIRECTORY, NautilusVFSDirectory))
#define NAUTILUS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VFS_DIRECTORY, NautilusVFSDirectoryClass))
#define NAUTILUS_IS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_VFS_DIRECTORY))
#define NAUTILUS_IS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VFS_DIRECTORY))
#define NAUTILUS_VFS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_VFS_DIRECTORY, NautilusVFSDirectoryClass))

typedef struct NautilusVFSDirectoryDetails NautilusVFSDirectoryDetails;

typedef struct {
	NautilusDirectory parent_slot;
} NautilusVFSDirectory;

typedef struct {
	NautilusDirectoryClass parent_slot;
} NautilusVFSDirectoryClass;

GType   nautilus_vfs_directory_get_type (void);
