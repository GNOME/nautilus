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
 * Author: Elliot Lee <sopwith@redhat.com>, based on bonobo-stream-fs.c by Miguel de Icaza.
 * bonobo-stream-vfs.c: Gnome VFS-based Stream implementation
 */
#include <config.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <gnome.h>
#include "bonobo-stream-vfs.h"
#include <errno.h>

static BonoboStreamClass *bonobo_stream_vfs_parent_class;
static POA_Bonobo_Stream__vepv vepv;

static CORBA_long
vfs_write (BonoboStream *stream, const Bonobo_Stream_iobuf *buffer,
	  CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	GnomeVFSResult res;
	GnomeVFSFileSize written = 0;

	res = gnome_vfs_write(sfs->fd, buffer->_buffer, buffer->_length, &written);

	if (res != GNOME_VFS_OK) {
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION,
				    ex_Bonobo_Storage_NameExists, NULL);
		return 0;
	}

	return written;
}

static void
vfs_read (BonoboStream *stream, CORBA_long count,
	  Bonobo_Stream_iobuf ** buffer,
	  CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	CORBA_octet *data;
	GnomeVFSResult res;
	GnomeVFSFileSize nread = 0;
	
	*buffer = Bonobo_Stream_iobuf__alloc ();
	CORBA_sequence_set_release (*buffer, TRUE);
	data = CORBA_sequence_CORBA_octet_allocbuf (count);

	res = gnome_vfs_read(sfs->fd, data, count, &nread);

	if (res == GNOME_VFS_OK){
		(*buffer)->_buffer = data;
		(*buffer)->_length = nread;
	} else {
		CORBA_free (data);
		CORBA_free (*buffer);
		*buffer = NULL;
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Stream_IOError, NULL);
	}
	sfs->got_eof = (res == GNOME_VFS_ERROR_EOF);
}

static CORBA_long
vfs_seek (BonoboStream *stream,
	 CORBA_long ofvfset, Bonobo_Stream_SeekType whence,
	 CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	GnomeVFSSeekPosition gwhence;
	GnomeVFSFileSize pos;
	GnomeVFSResult res;

	switch(whence)
	  {
	  case Bonobo_Stream_SEEK_CUR:
	    gwhence = GNOME_VFS_SEEK_CURRENT;
	    break;
	  case Bonobo_Stream_SEEK_END:
	    gwhence = GNOME_VFS_SEEK_END;
	    break;
	  default:
	    gwhence = GNOME_VFS_SEEK_START;
	    break;
	  }

	res = gnome_vfs_seek(sfs->fd, gwhence, ofvfset);
	if(res != GNOME_VFS_OK)
	  {
	    pos = -1;
	    goto out;
	  }

	res = gnome_vfs_tell(sfs->fd, &pos);
	if(res != GNOME_VFS_OK)
	  {
	    pos = -1;
	    goto out;
	  }

 out:
	sfs->got_eof = (res == GNOME_VFS_ERROR_EOF);
	return pos;
}

static void
vfs_truncate (BonoboStream *stream,
	     const CORBA_long new_size, 
	     CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	gnome_vfs_truncate_handle(sfs->fd, new_size);
}

static void
vfs_copy_to  (BonoboStream *stream,
	      const CORBA_char *dest,
	      const CORBA_long bytes,
	      CORBA_long *read_bytes,
	      CORBA_long *written_bytes,
	      CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	CORBA_octet *data;
	CORBA_unsigned_long more = bytes;
	GnomeVFSHandle *fd_out;
	GnomeVFSResult res;
	GnomeVFSFileSize rsize, wsize;

#define READ_CHUNK_SIZE 65536

	data = CORBA_sequence_CORBA_octet_allocbuf (READ_CHUNK_SIZE);

	*read_bytes = 0;
	*written_bytes = 0;

	res = gnome_vfs_create(&fd_out, dest, GNOME_VFS_OPEN_WRITE, FALSE, 0666);
	if(res != GNOME_VFS_OK)
	  return;

	do {
	  res = gnome_vfs_read(sfs->fd, data, MIN(READ_CHUNK_SIZE, more), &rsize);

	  sfs->got_eof = (res == GNOME_VFS_ERROR_EOF);

	  if (res == GNOME_VFS_OK) {
	    *read_bytes += rsize;
	    more -= rsize;
	    res = gnome_vfs_write(fd_out, data, rsize, &wsize);
	    if (res == GNOME_VFS_OK)
	      *written_bytes += wsize;
	    else
	      break;
	  } else
	    rsize = 0;
	} while(more > 0 && rsize > 0);

	gnome_vfs_close(fd_out);
}

static void
vfs_commit   (BonoboStream *stream,
	     CORBA_Environment *ev)
{
	g_warning ("Commit NI");
}

static void
vfs_close (BonoboStream *stream,
	  CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);

	if (gnome_vfs_close (sfs->fd) != GNOME_VFS_OK)
		g_warning ("Close failed");

	sfs->fd = NULL;
	sfs->got_eof = FALSE;
}

static CORBA_boolean
vfs_eos (BonoboStream *stream,
	CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);

	return sfs->got_eof;
}
	
static CORBA_long
vfs_length (BonoboStream *stream,
	   CORBA_Environment *ev)
{
	BonoboStreamVFS *sfs = BONOBO_STREAM_VFS (stream);
	GnomeVFSFileInfo fi;
	CORBA_long retval;

	gnome_vfs_file_info_init(&fi);
	if(gnome_vfs_get_file_info_from_handle(sfs->fd, &fi, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK)
	  return 0;
	retval = fi.size;
	gnome_vfs_file_info_clear(&fi);

	return retval;
}
	   

static void
bonobo_stream_vfs_class_init (BonoboStreamVFSClass *klass)
{
	BonoboStreamClass *sclass = BONOBO_STREAM_CLASS (klass);
	
	bonobo_stream_vfs_parent_class = gtk_type_class (bonobo_stream_get_type ());

	vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv.Bonobo_Stream_epv = bonobo_stream_get_epv ();

	sclass->write    = vfs_write;
	sclass->read     = vfs_read;
	sclass->seek     = vfs_seek;
	sclass->truncate = vfs_truncate;
	sclass->copy_to  = vfs_copy_to;
	sclass->commit   = vfs_commit;
	sclass->close    = vfs_close;
	sclass->eos      = vfs_eos;
	sclass->length   = vfs_length;
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
			(GtkObjectInitFunc) NULL,
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
create_bonobo_stream_vfs (BonoboObject *object)
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
bonobo_stream_create (GnomeVFSHandle *fd)
{
	BonoboStreamVFS *stream_vfs;
	Bonobo_Stream corba_stream;
	CORBA_Environment ev;

	stream_vfs = gtk_type_new (bonobo_stream_vfs_get_type ());
	if (stream_vfs == NULL)
		return NULL;
	
	stream_vfs->fd = fd;
	
	corba_stream = create_bonobo_stream_vfs (
		BONOBO_OBJECT (stream_vfs));

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
	GnomeVFSHandle *fd = NULL;
	GnomeVFSResult res;

	g_return_val_if_fail (uri != NULL, NULL);

	if (mode == Bonobo_Storage_READ)
		res = gnome_vfs_open(&fd, uri, GNOME_VFS_OPEN_READ);
	else if (mode == Bonobo_Storage_WRITE)
		res = gnome_vfs_open(&fd, uri, GNOME_VFS_OPEN_WRITE);

	if(fd && res == GNOME_VFS_OK)
		return bonobo_stream_create (fd);
	else
		return NULL;
}

/**
 * bonobo_stream_vfs_create:
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
bonobo_stream_vfs_create (const char *uri)
{
	GnomeVFSHandle *fd = NULL;
	GnomeVFSResult res;

	g_return_val_if_fail (uri != NULL, NULL);

	res = gnome_vfs_create(&fd, uri, GNOME_VFS_OPEN_READ|GNOME_VFS_OPEN_WRITE, FALSE, 0666);

	if(fd && res == GNOME_VFS_OK)
		return bonobo_stream_create (fd);
	else
		return NULL;
}



