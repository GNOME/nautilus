/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 1999 Helix Code, Inc.
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2000 Eazel, Inc
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
 * Author: Elliot Lee <sopwith@redhat.com>, based on bonobo-stream-fs.c by Miguel de Icaza.
 */

/*
 * bonobo-stream-vfs.c: Gnome VFS-based Stream implementation
 */

/* FIXME bugzilla.gnome.org 44400: There's another copy of this file,
 * with a few subtle differences, in the Bonobo sources, although it's
 * currently not being compiled.  
 */

#include <config.h>
#include "bonobo-stream-vfs.h"

#include <errno.h>
#include <fcntl.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <stdio.h>
#include <sys/stat.h>

#define READ_CHUNK_SIZE 65536

struct BonoboStreamVFSDetails {
	GnomeVFSHandle *handle;
};

static BonoboStreamClass *bonobo_stream_vfs_parent_class;
static POA_Bonobo_Stream__vepv vepv;


static void
bonobo_stream_vfs_storageinfo_from_file_info (Bonobo_StorageInfo *si,
					      GnomeVFSFileInfo   *fi)
{
	g_return_if_fail (si != NULL);
	g_return_if_fail (fi != NULL);

	si->name = CORBA_string_dup (fi->name);

	if (fi->flags & GNOME_VFS_FILE_INFO_FIELDS_SIZE)
		si->size = fi->size;
	else
		si->size = 0;

	if (fi->flags & GNOME_VFS_FILE_INFO_FIELDS_TYPE &&
	    fi->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		si->type = Bonobo_STORAGE_TYPE_DIRECTORY;
	else
		si->type = Bonobo_STORAGE_TYPE_REGULAR;

	if (fi->flags & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE &&
	    fi->mime_type)
		si->content_type = CORBA_string_dup (fi->mime_type);
	else
		si->content_type = CORBA_string_dup ("");
}

static Bonobo_StorageInfo *
vfs_get_info (BonoboStream *stream,
	      Bonobo_StorageInfoFields mask,
	      CORBA_Environment *ev)
{
	BonoboStreamVFS    *sfs = BONOBO_STREAM_VFS (stream);
	Bonobo_StorageInfo *si;
	GnomeVFSFileInfo    fi;
	GnomeVFSResult      result;

	if (mask & ~(Bonobo_FIELD_CONTENT_TYPE | Bonobo_FIELD_SIZE |
		     Bonobo_FIELD_TYPE)) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, 
				     ex_Bonobo_Storage_NotSupported, NULL);
		return CORBA_OBJECT_NIL;
	}

	result = gnome_vfs_get_file_info_from_handle (
			sfs->details->handle, &fi,
			(mask & Bonobo_FIELD_CONTENT_TYPE) ?
			GNOME_VFS_FILE_INFO_GET_MIME_TYPE :
			GNOME_VFS_FILE_INFO_DEFAULT);

	if (result != GNOME_VFS_OK) {
		if (result == GNOME_VFS_ERROR_ACCESS_DENIED)
			CORBA_exception_set 
				(ev, CORBA_USER_EXCEPTION,
				 ex_Bonobo_Stream_NoPermission, NULL);
		else
			CORBA_exception_set 
				(ev, CORBA_USER_EXCEPTION,
				 ex_Bonobo_Stream_IOError, NULL);
		return NULL;
	}

	si = Bonobo_StorageInfo__alloc ();

	bonobo_stream_vfs_storageinfo_from_file_info (si, &fi);

	return si;
}

static void
vfs_set_info (BonoboStream *stream,
	      const Bonobo_StorageInfo *info,
	      Bonobo_StorageInfoFields mask,
	      CORBA_Environment *ev)
{
	/* FIXME bugzilla.gnome.org 44403: Is it OK to have this
	 * unimplemented? 
	 */
	g_warning ("BonoboStreamVFS:set_info not implemented");
        CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                             ex_Bonobo_Stream_IOError, NULL);
}
	   
static void
vfs_write (BonoboStream *stream,
	   const Bonobo_Stream_iobuf *buffer,
	   CORBA_Environment *ev)
{
	BonoboStreamVFS *stream_vfs;
	GnomeVFSResult res;
	GnomeVFSFileSize written;

	stream_vfs = BONOBO_STREAM_VFS (stream);

	res = gnome_vfs_write (stream_vfs->details->handle, buffer->_buffer, buffer->_length, &written);
	if (res != GNOME_VFS_OK) {
		/* FIXME bugzilla.gnome.org 44396: We might need to
		 * distinguish NoPermission from IOError.  
		 */
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Stream_IOError, NULL);
	}
}

static void
vfs_read (BonoboStream *stream,
	  CORBA_long count,
	  Bonobo_Stream_iobuf **buffer,
	  CORBA_Environment *ev)
{
	BonoboStreamVFS *stream_vfs;
	CORBA_octet *data;
	GnomeVFSResult res;
	GnomeVFSFileSize nread = 0;
	
	stream_vfs = BONOBO_STREAM_VFS (stream);

	*buffer = Bonobo_Stream_iobuf__alloc ();
	CORBA_sequence_set_release (*buffer, TRUE);
	data = CORBA_sequence_CORBA_octet_allocbuf (count);

	res = gnome_vfs_read (stream_vfs->details->handle, data, count, &nread);

	if (res == GNOME_VFS_OK) {
		(*buffer)->_buffer = data;
		(*buffer)->_length = nread;
	} else if (res == GNOME_VFS_ERROR_EOF) {
		/* Bonobo returns a zero length buffer for end of file */
		(*buffer)->_buffer = data;
		(*buffer)->_length = 0;
	} else{
		CORBA_free (data);
		CORBA_free (*buffer);
		*buffer = NULL;
		/* FIXME bugzilla.gnome.org 44396: We might need to
		 * distinguish NoPermission from IOError.  
		 */
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Stream_IOError, NULL);
	}
}

static CORBA_long
vfs_seek (BonoboStream *stream,
	  CORBA_long offset, 
	  Bonobo_Stream_SeekType whence,
	  CORBA_Environment *ev)
{
	BonoboStreamVFS *stream_vfs;
	GnomeVFSSeekPosition vfs_whence;
	GnomeVFSFileSize pos;
	GnomeVFSResult res;

	stream_vfs = BONOBO_STREAM_VFS (stream);

	switch (whence) {
	case Bonobo_Stream_SEEK_CUR:
		vfs_whence = GNOME_VFS_SEEK_CURRENT;
		break;
	case Bonobo_Stream_SEEK_END:
		vfs_whence = GNOME_VFS_SEEK_END;
		break;
	default:
		vfs_whence = GNOME_VFS_SEEK_START;
		break;
	}

	res = gnome_vfs_seek (stream_vfs->details->handle, vfs_whence, offset);
	if (res != GNOME_VFS_OK) {
		pos = -1;
	} else {
		res = gnome_vfs_tell (stream_vfs->details->handle, &pos);
		if (res != GNOME_VFS_OK) {
			pos = -1;
		}
	}
	
	/* FIXME: Will munge >31-bit file positions, which can
	 * happen in gnome-vfs.
	 */
	return pos;
}

static void
vfs_truncate (BonoboStream *stream,
	      const CORBA_long new_size, 
	      CORBA_Environment *ev)
{
	BonoboStreamVFS *stream_vfs;

	stream_vfs = BONOBO_STREAM_VFS (stream);
	gnome_vfs_truncate_handle (stream_vfs->details->handle, new_size);
}

static void
vfs_copy_to  (BonoboStream *stream,
	      const CORBA_char *dest,
	      const CORBA_long bytes,
	      CORBA_long *read_bytes,
	      CORBA_long *written_bytes,
	      CORBA_Environment *ev)
{
	BonoboStreamVFS *stream_vfs;
	CORBA_octet *data;
	CORBA_unsigned_long more;
	GnomeVFSHandle *fd_out;
	GnomeVFSResult res;
	GnomeVFSFileSize rsize, wsize;

	stream_vfs = BONOBO_STREAM_VFS (stream);

	data = CORBA_sequence_CORBA_octet_allocbuf (READ_CHUNK_SIZE);

	*read_bytes = 0;
	*written_bytes = 0;

	res = gnome_vfs_create (&fd_out, dest, GNOME_VFS_OPEN_WRITE, FALSE, 0666);
	if (res != GNOME_VFS_OK) {
		/* FIXME bugzilla.gnome.org 44398: Need to set exception here. */
		return;
	}

	if (bytes == -1) {
		more = sizeof (data);
	} else {
		more = bytes;
	}

	do {
		res = gnome_vfs_read (stream_vfs->details->handle, data, MIN (READ_CHUNK_SIZE, more), &rsize);
		if (res != GNOME_VFS_OK) {
			/* FIXME bugzilla.gnome.org 44398: Need to set exception here. */
			break;
		}
		*read_bytes += rsize;

		res = gnome_vfs_write (fd_out, data, rsize, &wsize);
		if (res != GNOME_VFS_OK) {
			/* FIXME bugzilla.gnome.org 44398: Need to set exception here. */
			break;
		}
		*written_bytes += wsize;
		
		if (bytes != -1) {
			more -= rsize;
		}
	} while (more > 0 && rsize > 0);
	
	gnome_vfs_close (fd_out);
}

static void
vfs_commit (BonoboStream *stream,
	    CORBA_Environment *ev)
{
        CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                             ex_Bonobo_Stream_NotSupported, NULL);
}

static void
vfs_revert (BonoboStream *stream,
	    CORBA_Environment *ev)
{
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Stream_NotSupported, NULL);
}

static void
vfs_destroy (GtkObject *object)
{
	BonoboStreamVFS *stream_vfs;

	stream_vfs = BONOBO_STREAM_VFS (object);

	if (stream_vfs->details->handle != NULL) {
		gnome_vfs_close (stream_vfs->details->handle);
		/* FIXME bugzilla.gnome.org 44399: Errors that happen
		 * only at flush time are lost here. Many gnome-vfs
		 * modules return errors at close time about the
		 * remaining flushed writes.  
		 */
		stream_vfs->details->handle = NULL;
	}

	GTK_OBJECT_CLASS (bonobo_stream_vfs_parent_class)->destroy (object);
}

static void
bonobo_stream_vfs_class_init (BonoboStreamVFSClass *klass)
{
	BonoboStreamClass *stream_class;
	GtkObjectClass *object_class;

	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();
	vepv.Bonobo_Stream_epv = nautilus_bonobo_stream_get_epv ();

	stream_class = BONOBO_STREAM_CLASS (klass);
	object_class = GTK_OBJECT_CLASS (klass);

	bonobo_stream_vfs_parent_class = gtk_type_class (bonobo_stream_get_type ());

	stream_class->get_info = vfs_get_info;
	stream_class->set_info = vfs_set_info;
	stream_class->write    = vfs_write;
	stream_class->read     = vfs_read;
	stream_class->seek     = vfs_seek;
	stream_class->truncate = vfs_truncate;
	stream_class->copy_to  = vfs_copy_to;
	stream_class->commit   = vfs_commit;
	stream_class->revert   = vfs_revert;

	object_class->destroy = vfs_destroy;
}

static void
bonobo_stream_vfs_init (BonoboStreamVFS *stream)
{
	stream->details = g_new0 (BonoboStreamVFSDetails, 1);
}

/**
 * bonobo_stream_vfs_get_type:
 *
 * Returns the GtkType for the BonoboStreamVFS class.
 */
GtkType
bonobo_stream_vfs_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"IDL:GNOME/StreamVFS:1.0",
			sizeof (BonoboStreamVFS),
			sizeof (BonoboStreamVFSClass),
			(GtkClassInitFunc) bonobo_stream_vfs_class_init,
			(GtkObjectInitFunc) bonobo_stream_vfs_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_stream_get_type (), &info);
	}
	
	return type;
}

/**
 * bonobo_stream_vfs_construct:
 * @stream: The BonoboStreamVFS object to initialize.
 * @corba_stream: The CORBA server which implements the BonoboStreamVFS service.
 *
 * This function initializes an object of type BonoboStreamVFS using the
 * provided CORBA server @corba_stream.
 *
 * Returns the constructed BonoboStreamVFS @stream.
 */
BonoboStream *
bonobo_stream_vfs_construct (BonoboStreamVFS *stream,
			   Bonobo_Stream corba_stream)
{
	g_return_val_if_fail (stream != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_STREAM (stream), NULL);
	g_return_val_if_fail (corba_stream != CORBA_OBJECT_NIL, NULL);
	
	bonobo_object_construct (BONOBO_OBJECT (stream), corba_stream);
	
	return BONOBO_STREAM (stream);
}

static Bonobo_Stream
Bonobo_Stream_vfs__create (BonoboObject *object)
{
	POA_Bonobo_Stream *servant;
	CORBA_Environment ev;

	servant = (POA_Bonobo_Stream *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &vepv;
	
	CORBA_exception_init (&ev);

	POA_Bonobo_Stream__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
                g_free (servant);
		CORBA_exception_free (&ev);
                return CORBA_OBJECT_NIL;
        }

	CORBA_exception_free (&ev);
	return (Bonobo_Stream) bonobo_object_activate_servant (object, servant);
}

static BonoboStream *
bonobo_stream_vfs_new_internal (GnomeVFSHandle *handle)
{
	BonoboStreamVFS *stream_vfs;
	Bonobo_Stream corba_stream;
	CORBA_Environment ev;

	stream_vfs = BONOBO_STREAM_VFS (gtk_object_new (bonobo_stream_vfs_get_type (), NULL));
	
	stream_vfs->details->handle = handle;
	
	corba_stream = Bonobo_Stream_vfs__create (BONOBO_OBJECT (stream_vfs));
	
	CORBA_exception_init (&ev);
	
	if (CORBA_Object_is_nil (corba_stream, &ev)){
		gtk_object_unref (GTK_OBJECT (stream_vfs));
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return bonobo_stream_vfs_construct (stream_vfs, corba_stream);
}

/**
 * bonobo_stream_vfs_open:
 * @uri: The path to the file to be opened.
 * @mode: The mode with which the file should be opened.
 *
 * Creates a new BonoboStream object for the filename specified by
 * @path.  
 */
BonoboStream *
bonobo_stream_vfs_open (const char *uri, Bonobo_Storage_OpenMode mode)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult result;

	g_return_val_if_fail (uri != NULL, NULL);

	handle = NULL;
	if (mode == Bonobo_Storage_READ) {
		result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	} else if (mode == Bonobo_Storage_WRITE) {
		result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_WRITE);
	} else {
		/* FIXME bugzilla.gnome.org 44401: Do we need to
		 * support CREATE, FAILIFEXIST, COMPRESSED,
		 * TRANSACTED, or combinations?  
		 */
	   	result = GNOME_VFS_ERROR_NOT_SUPPORTED;
	}
	
	if (result != GNOME_VFS_OK || handle == NULL) {
		return NULL;
	}

	return bonobo_stream_vfs_new_internal (handle);
}

/**
 * bonobo_stream_vfs_new:
 * @uri: The path to the file to be opened.
 *
 * Creates a new BonoboStreamVFS object which is bound to the file
 * specified by @path.
 *
 * When data is read out of or written into the returned BonoboStream
 * object, the read() and write() operations are mapped to the
 * corresponding operations on the specified file.
 *
 * Returns: the constructed BonoboStream object which is bound to the specified file.
 */
BonoboStream *
bonobo_stream_vfs_new (const char *uri)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult result;

	g_return_val_if_fail (uri != NULL, NULL);

	handle = NULL;
	result = gnome_vfs_create (&handle, uri,
				   GNOME_VFS_OPEN_READ | GNOME_VFS_OPEN_WRITE,
				   FALSE, 0666);

	if (result != GNOME_VFS_OK || handle == NULL) {
		return NULL;
	}

	return bonobo_stream_vfs_new_internal (handle);
}
