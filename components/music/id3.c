/*********************************************************************
 * 
 *    Copyright (C) 1999,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3.c
 * Description:   Code for accessing ID3 tags.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Fri Feb  5 23:55:13 1999
 *                
 * $Id$
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 ********************************************************************/
#include <sys/types.h>
#include <sys/uio.h>
#include <glib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "id3.h"
#include "id3_header.h"



/*
**
** Functions for accessing the ID3 tag using a memory pointer.
**
*/

/*
 * Function id3_seek_mem (id3, offset)
 *
 *    Seek `offset' bytes forward in the indicated ID3-tag.  Return 0
 *    upon success, or -1 if an error occured.
 *
 */
static int id3_seek_mem(id3_t *id3, int offset)
{
    id3->s.me.id3_ptr = (char *) id3->s.me.id3_ptr + offset;
    id3->id3_pos += offset;

    if ( id3->id3_pos > id3->id3_tagsize ) {
	id3->id3_pos = id3->id3_tagsize;
	id3_error( id3, "seeking beyond tag boundary" );
	return -1;
    }

    return 0;
}


/*
 * Function id3_read_mem (id3, buf, size)
 *
 *    Read `size' bytes from indicated ID3-tag.  If `buf' is non-NULL,
 *    read into that buffer.  Return a pointer to the data which was
 *    read, or NULL upon error.
 *
 */
static void *id3_read_mem(id3_t *id3, void *buf, int size)
{
    void *ret = id3->s.me.id3_ptr;

    /*
     * Check boundary.
     */
    if ( id3->id3_pos + size > id3->id3_tagsize ) {
	size = id3->id3_tagsize - id3->id3_pos;
    }

    /*
     * If buffer is non-NULL, we have to copy the data.
     */
    if ( buf != NULL )
	memcpy( buf, id3->s.me.id3_ptr, size );

    /*
     * Update memory pointer.
     */
    id3->s.me.id3_ptr = (char *) id3->s.me.id3_ptr + size;
    id3->id3_pos += size;

    return ret;
}


/*
**
** Functions for accessing the ID3 tag using a file descriptor.
**
*/

/*
 * Function id3_seek_fd (id3, offset)
 *
 *    Seek `offset' bytes forward in the indicated ID3-tag.  Return 0
 *    upon success, or -1 if an error occured.
 *
 */
static int id3_seek_fd(id3_t *id3, int offset)
{
    if ( lseek( id3->s.fd.id3_fd, offset, SEEK_CUR ) == -1 ) {
	id3_error( id3, "seeking beyond tag boundary" );
	return -1;
    }
    id3->id3_pos += offset;

    return 0;
}


/*
 * Function id3_read_fd (id3, buf, size)
 *
 *    Read `size' bytes from indicated ID3-tag.  If `buf' is non-NULL,
 *    read into that buffer.  Return a pointer to the data which was
 *    read, or NULL upon error.
 *
 */
static void *id3_read_fd(id3_t *id3, void *buf, int size)
{
    int rsize = size > ID3_FD_BUFSIZE ? ID3_FD_BUFSIZE : size;
    int done = 0;

    /*
     * Check boundary.
     */
    if ( id3->id3_pos + size > id3->id3_tagsize ) {
	size = id3->id3_tagsize - id3->id3_pos;
    }

    /*
     * If buffer is NULL, we use the default buffer.
     */
    if ( buf == NULL )
	buf = id3->s.fd.id3_buf;

    /*
     * Read until we have slurped as much data as we wanted.
     */
    while ( done < rsize ) {
	char *buffer = (char *)buf + done;
	int ret;

	/*
	 * Try reading from file.
	 */
	ret = read( id3->s.fd.id3_fd, buffer, rsize );
	if ( ret == -1 ) {
	    id3_error( id3, "read(2) failed" );
	    return NULL;
	}

	/*
	 * Check if we've reached end-of-file.
	 */
	if ( ret == 0 )
	    return buf;

	id3->id3_pos += ret;
	done += ret;
    }

    return buf;
}


/*
**
** Functions for accessing the ID3 tag using a file pointer.
**
*/

/*
 * Function id3_seek_fp (id3, offset)
 *
 *    Seek `offset' bytes forward in the indicated ID3-tag.  Return 0
 *    upon success, or -1 if an error occured.
 *
 */
static int id3_seek_fp(id3_t *id3, int offset)
{
    if ( offset > 0 ) {
	/*
	 * If offset is positive, we use fread() instead of fseek().  This
	 * is more robust with respect to streams.
	 */
	char buf[64];
	int r, remain = offset;

	while ( remain > 0 ) {
	    r = fread( buf, 1, 64, id3->s.fp.id3_fp );
	    if ( r == -1 ) {
		id3_error(  id3, "fread() failed" );
		return -1;
	    }
	    remain -= r;
	}
    } else {
	/*
	 * If offset is negative, we ahve to use fseek().  Let us hope
	 * that it works.
	 */
	if ( fseek( id3->s.fp.id3_fp, offset, SEEK_CUR ) == -1 ) {
	    id3_error( id3, "seeking beyond tag boundary" );
	    return -1;
	}
    }
    id3->id3_pos += offset;

    return 0;
}


/*
 * Function id3_read_fp (id3, buf, size)
 *
 *    Read `size' bytes from indicated ID3-tag.  If `buf' is non-NULL,
 *    read into that buffer.  Return a pointer to the data which was
 *    read, or NULL upon error.
 *
 */
static void *id3_read_fp(id3_t *id3, void *buf, int size)
{
    int rsize = size > ID3_FD_BUFSIZE ? ID3_FD_BUFSIZE : size;
    int done = 0;

    /*
     * Check boundary.
     */
    if ( id3->id3_pos + size > id3->id3_tagsize ) {
	size = id3->id3_tagsize - id3->id3_pos;
    }

    /*
     * If buffer is NULL, we use the default buffer.
     */
    if ( buf == NULL )
	buf = id3->s.fp.id3_buf;

    /*
     * Read until we have slurped as much data as we wanted.
     */
    while ( done < rsize ) {
	char *buffer = (char *)buf + done;
	int ret;

	/*
	 * Try reading from file.
	 */
	ret = fread( buffer, 1, rsize, id3->s.fp.id3_fp );
	if ( ret == -1 ) {
	    id3_error( id3, "fread() failed" );
	    return NULL;
	}

	/*
	 * Check if we've reached end-of-file.
	 */
	if ( ret == 0 )
	    return buf;

	id3->id3_pos += ret;
	done += ret;
    }

    return buf;
}




/*
 * Function id3_open_mem (ptr, flags)
 *
 *    Open an ID3 tag using a memory pointer.  Return a pointer to a
 *    structure describing the ID3 tag, or NULL if an error occured.
 *
 */
id3_t *id3_open_mem(void *ptr, int flags)
{
    id3_t *id3;

    /*
     * Allocate ID3 structure.
     */
    id3 = malloc( sizeof(id3_t) );
    if ( id3 == NULL )
	return NULL;
    bzero( id3, sizeof(id3_t) );

    /*
     * Initialize access pointers.
     */
    id3->id3_seek = id3_seek_mem;
    id3->id3_read = id3_read_mem;

    id3->id3_oflags = flags;
    id3->id3_type = ID3_TYPE_MEM;
    id3->id3_pos = 0;
    id3->s.me.id3_ptr = ptr;

    /*
     * Try reading ID3 tag.
     */
    if ( id3_read_tag( id3 ) == -1 ) {
	if ( ~flags & ID3_OPENF_CREATE )
	    goto Return_NULL;
	id3_init_tag ( id3 );
    }

    return id3;

 Return_NULL:
    free( id3 );
    return NULL;
}


/*
 * Function id3_open_fd (fd, flags)
 *
 *    Open an ID3 tag using a file descriptor.  Return a pointer to a
 *    structure describing the ID3 tag, or NULL if an error occured.
 *
 */
id3_t *id3_open_fd(int fd, int flags)
{
    id3_t *id3;

    /*
     * Allocate ID3 structure.
     */
    id3 = malloc( sizeof(id3_t) );
    if ( id3 == NULL )
	return NULL;
    bzero( id3, sizeof(id3_t) );

    /*
     * Initialize access pointers.
     */
    id3->id3_seek = id3_seek_fd;
    id3->id3_read = id3_read_fd;

    id3->id3_oflags = flags;
    id3->id3_type = ID3_TYPE_FD;
    id3->id3_pos = 0;
    id3->s.fd.id3_fd = fd;

    /*
     * Allocate buffer to hold read data.
     */
    id3->s.fd.id3_buf = malloc( ID3_FD_BUFSIZE );
    if ( id3->s.fd.id3_buf == NULL ) {
	id3_error( id3, "malloc - no memory" );
	goto Error_nobuf;
    }

    /*
     * Try reading ID3 tag.
     */
    if ( id3_read_tag( id3 ) == -1 ) {
	if ( ~flags & ID3_OPENF_CREATE ) 
	    goto Return_NULL;
	id3_init_tag ( id3 );
    }

    return id3;

    /*
     * Cleanup code.
     */
 Return_NULL:
    free( id3->s.fd.id3_buf );
 Error_nobuf:
    free( id3 );
    return NULL;
}


/*
 * Function id3_open_fp (fp, flags)
 *
 *    Open an ID3 tag using a file pointer.  Return a pointer to a
 *    structure describing the ID3 tag, or NULL if an error occured.
 *
 */
id3_t *id3_open_fp(FILE *fp, int flags)
{
    id3_t *id3;

    /*
     * Allocate ID3 structure.
     */
    id3 = malloc( sizeof(id3_t) );
    if ( id3 == NULL )
	return NULL;
    bzero( id3, sizeof(id3_t) );

    /*
     * Initialize access pointers.
     */
    id3->id3_seek = id3_seek_fp;
    id3->id3_read = id3_read_fp;

    id3->id3_oflags = flags;
    id3->id3_type = ID3_TYPE_FP;
    id3->id3_pos = 0;
    id3->s.fp.id3_fp = fp;

    /*
     * Allocate buffer to hold read data.
     */
    id3->s.fp.id3_buf = malloc( ID3_FD_BUFSIZE );
    if ( id3->s.fp.id3_buf == NULL ) {
	id3_error( id3, "malloc - no memory" );
	goto Error_nobuf;
    }

    /*
     * Try reading ID3 tag.
     */
    if ( id3_read_tag( id3 ) == -1 ) {
	if ( ~flags & ID3_OPENF_CREATE ) 
	    goto Return_NULL;
	id3_init_tag ( id3 );
    }
    

    return id3;

    /*
     * Cleanup code.
     */
 Return_NULL:
    free( id3->s.fp.id3_buf );
 Error_nobuf:
    free( id3 );
    return NULL;
}


/*
 * Function id3_close (id3)
 *
 *    Free all resources assoicated with the ID3 tag.
 *
 */
int id3_close(id3_t *id3)
{
    int ret = 0;
    
    switch( id3->id3_type ) {
    case ID3_TYPE_MEM:
	break;
    case ID3_TYPE_FD:
	free( id3->s.fd.id3_buf );
	break;
    case ID3_TYPE_FP:
	free( id3->s.fp.id3_buf );
	break;
    case ID3_TYPE_NONE:
	id3_error( id3, "unknown ID3 type" );
	ret = -1;
    }
    free( id3 );

    return ret;
}


/*
 * Function id3_tell (id3)
 *
 *    Return the current position in ID3 tag.  This will always be
 *    directly after the tag.
 *
 */
int id3_tell(id3_t *id3)
{
    if ( id3->id3_newtag ) {
	return 0;
    } else {
	return id3->id3_tagsize + 3 + sizeof(id3_taghdr_t);
    }
}


/*
 * Function id3_alter_file (id3)
 *
 *    When altering a file, some ID3 tags should be discarded.  As the ID3
 *    library has no means of knowing when a file has been altered
 *    outside of the library, this function must be called manually
 *    whenever the file is altered.
 *
 */
int id3_alter_file(id3_t *id3)
{
    /*
     * List of frame classes that should be discarded whenever the
     * file is altered.
     */
    static guint32 discard_list[] = {
	ID3_ETCO, ID3_EQUA, ID3_MLLT, ID3_POSS, ID3_SYLT,
	ID3_SYTC, ID3_RVAD, ID3_TENC, ID3_TLEN, ID3_TSIZ,
	0
    };
    id3_frame_t *fr;
    guint32 id, i = 0;

    /*
     * Go through list of frame types that should be discarded.
     */
    while ( (id = discard_list[i++]) != 0 ) {
	/*
	 * Discard all frames of that type.
	 */
	while ( (fr = id3_get_frame(id3, id, 1)) ) {
	    id3_delete_frame( fr );
	}
    }
    
    return 0;
}


/*
 * Function safe_write (fd, buf, size)
 *
 *    Like write(2), except that the whole buffer will be written.
 *
 */
static int safe_write(int fd, void *buf, int size)
{
    int remaining = size;
    char *ptr = buf;
    int r;

    while ( remaining > 0 ) {
	if ( (r = write( fd, ptr, remaining )) == -1 )
	    return -1;
	remaining -= r;
	ptr += r;
    }

    return 0;
}


/*
 * Function id3_write_tag (id3, fd)
 *
 *    Wrtite the ID3 tag to the indicated file descriptor.  Return 0
 *    upon success, or -1 if an error occured.
 *
 */
int id3_write_tag(id3_t *id3, int fd)
{
    id3_frame_t *fr;
    id3_taghdr_t taghdr;
    int size = 0;

    /*
     * Calculate size of ID3 tag.
     */
    for ( fr = id3->id3_frame; fr != NULL; fr = fr->fr_next ) {
	size += fr->fr_size + sizeof(id3_framehdr_t);
    }

    /*
     * Write tag header.
     */
    taghdr.th_version = id3->id3_version;
    taghdr.th_revision = id3->id3_revision;
    taghdr.th_flags = id3->id3_flags;
    taghdr.th_size = g_htonl( ID3_SET_SIZE28(size) );

    if ( safe_write(fd, "ID3", 3) == -1 )
	return -1;
    if ( safe_write(fd, &taghdr, sizeof(taghdr)) == -1 )
	return -1;

    /*
     * TODO: Write extended header.
     */
#if 0
    if ( id3->id3_flags & ID3_THFLAG_EXT ) {
	id3_exthdr_t exthdr;
    }
#endif
	    
    for ( fr = id3->id3_frame; fr != NULL; fr = fr->fr_next ) {
	id3_framehdr_t fhdr;

	/*
	 * TODO: Support compressed headers, encoded
	 * headers, and grouping info.
	 */
	fhdr.fh_id = fr->fr_desc ? g_htonl(fr->fr_desc->fd_id) : 0;
	fhdr.fh_size = g_htonl( fr->fr_size );
	fhdr.fh_flags = g_ntohl( fr->fr_flags );

	if ( safe_write(fd, &fhdr, sizeof(fhdr)) == -1 )
	    return -1;

	if ( safe_write(fd, fr->fr_data, fr->fr_size) == -1 )
	    return -1;
    }
    return 0;
}
