/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2000 Red Hat, Inc.
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
 *
 * Author: Elliot Lee <sopwith@redhat.com>, based on bonobo-stream-fs.h by Miguel de Icaza.
 * bonobo-stream-vfs.h: Gnome VFS-based Stream interface
 */
#ifndef _BONOBO_STREAM_VFS_H_
#define _BONOBO_STREAM_VFS_H_

#include <bonobo/bonobo-stream.h>
#include <libgnomevfs/gnome-vfs.h>

BEGIN_GNOME_DECLS

struct BonoboStreamVFS;
typedef struct BonoboStreamVFS BonoboStreamVFS;


#define BONOBO_STREAM_VFS_TYPE        (bonobo_stream_vfs_get_type ())
#define BONOBO_STREAM_VFS(o)          (GTK_CHECK_CAST ((o), BONOBO_STREAM_VFS_TYPE, BonoboStreamVFS))
#define BONOBO_STREAM_VFS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), BONOBO_STREAM_VFS_TYPE, BonoboStreamVFSClass))
#define BONOBO_IS_STREAM_VFS(o)       (GTK_CHECK_TYPE ((o), BONOBO_STREAM_VFS_TYPE))
#define BONOBO_IS_STREAM_VFS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), BONOBO_STREAM_VFS_TYPE))

typedef struct BonoboStreamVFSDetails BonoboStreamVFSDetails;

struct BonoboStreamVFS {
	BonoboStream stream;

	BonoboStreamVFSDetails *details;
};

typedef struct {
	BonoboStreamClass parent_class;
} BonoboStreamVFSClass;

GtkType         bonobo_stream_vfs_get_type     (void);
BonoboStream *  bonobo_stream_vfs_new          (const char              *uri);
BonoboStream *  bonobo_stream_vfs_construct    (BonoboStreamVFS         *stream,
						Bonobo_Stream            corba_stream);
BonoboStream *  bonobo_stream_vfs_open         (const char              *uri, 
						Bonobo_Storage_OpenMode  mode);

END_GNOME_DECLS

#endif /* _BONOBO_STREAM_VFS_H_ */
