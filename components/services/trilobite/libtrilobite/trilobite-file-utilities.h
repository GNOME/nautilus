/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TRILOBITE_FILE_UTILITIES_H
#define TRILOBITE_FILE_UTILITIES_H

#include <libgnomevfs/gnome-vfs.h>

/* FIXME: cut and pasted from libnautilus-extensions/nautilus-file-utilities.c */

typedef void     (* TrilobiteReadFileCallback) (GnomeVFSResult result,
						GnomeVFSFileSize file_size,
						char *file_contents,
						gpointer callback_data);
typedef gboolean (* TrilobiteReadMoreCallback) (GnomeVFSFileSize file_size,
						const char *file_contents,
						gpointer callback_data);
typedef struct TrilobiteReadFileHandle TrilobiteReadFileHandle;

/* Read an entire file at once with gnome-vfs. */
GnomeVFSResult          trilobite_read_entire_file        (const char                *uri,
							   int                       *file_size,
							   char                     **file_contents);
TrilobiteReadFileHandle *trilobite_read_entire_file_async (const char                *uri,
							   TrilobiteReadFileCallback  callback,
							   gpointer                   callback_data);
TrilobiteReadFileHandle *trilobite_read_file_async        (const char                *uri,
							   TrilobiteReadFileCallback  callback,
							   TrilobiteReadMoreCallback  read_more_callback,
							   gpointer                   callback_data);
void                    trilobite_read_file_cancel        (TrilobiteReadFileHandle    *handle);



#endif /* TRILOBITE_FILE_UTILITIES_H */
