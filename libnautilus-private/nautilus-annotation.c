/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-annotation.c: routines for getting and setting xml-based annotations associated
   with the digest of a file.
  
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to md5_init, call md5_update as
 * needed on buffers full of bytes, and then call md5_Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/* parts of this file are :
 * Written March 1993 by Branko Lankester
 * Modified June 1993 by Colin Plumb for altered md5.c.
 * Modified October 1995 by Erik Troan for RPM
 */


#include <config.h>
#include "nautilus-annotation.h"

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-preferences.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <librsvg/rsvg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* icon selection callback function. */
typedef void (* NautilusCalculateDigestCallback) (NautilusFile *file, const char *file_digest);

typedef struct {
	guint32 buf[4];
	guint32 bits[2];
	guchar in[64];
	int doByteReverse;
} MD5Context ;

struct NautilusDigestFileHandle {
	GnomeVFSAsyncHandle *handle;
	NautilusFile *file;
	char *buffer;
	MD5Context digest_context;
};

#undef _MD5_STANDALONE


static void md5_transform (guint32 buf[4], const guint32 in[16]);

static int _ie = 0x44332211;
static union _endian { int i; char b[4]; } *_endian = (union _endian *)&_ie;
#define	IS_BIG_ENDIAN()		(_endian->b[0] == '\x44')
#define	IS_LITTLE_ENDIAN()	(_endian->b[0] == '\x11')


/*
 * Note: this code is harmless on little-endian machines.
 */
static void 
_byte_reverse (guchar *buf, guint32 longs)
{
	guint32 t;
	do {
		t = (guint32) ((guint32) buf[3] << 8 | buf[2]) << 16 |
			((guint32) buf[1] << 8 | buf[0]);
		*(guint32 *) buf = t;
		buf += 4;
	} while (--longs);
}

/**
 * md5_init: Initialise an md5 context object
 * @ctx: md5 context 
 * 
 * Initialise an md5 buffer. 
 *
 **/
static void 
md5_init (MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;
	
	ctx->bits[0] = 0;
	ctx->bits[1] = 0;
	
	if (IS_BIG_ENDIAN())	
		ctx->doByteReverse = 1;		
	else 
		ctx->doByteReverse = 0;	
}



/**
 * md5_update: add a buffer to md5 hash computation
 * @ctx: conetxt object used for md5 computaion
 * @buf: buffer to add
 * @len: buffer length
 * 
 * Update context to reflect the concatenation of another buffer full
 * of bytes. Use this to progressively construct an md5 hash.
 **/
static void 
md5_update (MD5Context *ctx, const guchar *buf, guint32 len)
{
	guint32 t;
	
	/* Update bitcount */
	
	t = ctx->bits[0];
	if ((ctx->bits[0] = t + ((guint32) len << 3)) < t)
		ctx->bits[1]++;		/* Carry from low to high */
	ctx->bits[1] += len >> 29;
	
	t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */
	
	/* Handle any leading odd-sized chunks */
	
	if (t) {
		guchar *p = (guchar *) ctx->in + t;
		
		t = 64 - t;
		if (len < t) {
			memcpy (p, buf, len);
			return;
		}
		memcpy (p, buf, t);
		if (ctx->doByteReverse)
			_byte_reverse (ctx->in, 16);
		md5_transform (ctx->buf, (guint32 *) ctx->in);
		buf += t;
		len -= t;
	}
	/* Process data in 64-byte chunks */
	
	while (len >= 64) {
		memcpy (ctx->in, buf, 64);
		if (ctx->doByteReverse)
			_byte_reverse (ctx->in, 16);
		md5_transform (ctx->buf, (guint32 *) ctx->in);
		buf += 64;
		len -= 64;
	}
	
	/* Handle any remaining bytes of data. */
	
	memcpy (ctx->in, buf, len);
}





/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
/**
 * md5_final: copy the final md5 hash to a bufer
 * @digest: 16 bytes buffer
 * @ctx: context containing the calculated md5
 * 
 * copy the final md5 hash to a bufer
 **/
static void 
md5_final (MD5Context *ctx, guchar digest[16])
{
	guint32 count;
	guchar *p;
	
	/* Compute number of bytes mod 64 */
	count = (ctx->bits[0] >> 3) & 0x3F;
	
	/* Set the first char of padding to 0x80.  This is safe since there is
	   always at least one byte free */
	p = ctx->in + count;
	*p++ = 0x80;
	
	/* Bytes of padding needed to make 64 bytes */
	count = 64 - 1 - count;
	
	/* Pad out to 56 mod 64 */
	if (count < 8) {
		/* Two lots of padding:  Pad the first block to 64 bytes */
		memset (p, 0, count);
		if (ctx->doByteReverse)
			_byte_reverse (ctx->in, 16);
		md5_transform (ctx->buf, (guint32 *) ctx->in);
		
		/* Now fill the next block with 56 bytes */
		memset (ctx->in, 0, 56);
	} else {
		/* Pad block to 56 bytes */
		memset (p, 0, count - 8);
	}
	if (ctx->doByteReverse)
		_byte_reverse (ctx->in, 14);
	
	/* Append length in bits and transform */
	((guint32 *) ctx->in)[14] = ctx->bits[0];
	((guint32 *) ctx->in)[15] = ctx->bits[1];
	
	md5_transform (ctx->buf, (guint32 *) ctx->in);
	if (ctx->doByteReverse)
		_byte_reverse ((guchar *) ctx->buf, 4);
	memcpy (digest, ctx->buf, 16);
}




/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  md5_Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void 
md5_transform (guint32 buf[4], const guint32 in[16])
{
	register guint32 a, b, c, d;
	
	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];
	
	MD5STEP (F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP (F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP (F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP (F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP (F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP (F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP (F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP (F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP (F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP (F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP (F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP (F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP (F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP (F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP (F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP (F1, b, c, d, a, in[15] + 0x49b40821, 22);
	
	MD5STEP (F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP (F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP (F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP (F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP (F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP (F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP (F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP (F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP (F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP (F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP (F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP (F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP (F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP (F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP (F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP (F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);
	
	MD5STEP (F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP (F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP (F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP (F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP (F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP (F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP (F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP (F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP (F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP (F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP (F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP (F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP (F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP (F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP (F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP (F3, b, c, d, a, in[2] + 0xc4ac5665, 23);
	
	MD5STEP (F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP (F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP (F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP (F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP (F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP (F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP (F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP (F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP (F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP (F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP (F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP (F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP (F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP (F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP (F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP (F4, b, c, d, a, in[9] + 0xeb86d391, 21);
	
	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}



/* When close is complete, there's no more work to do. */
static void
digest_file_close_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  gpointer callback_data)
{
}

/* Close the file and then tell the caller we succeeded, handing off
 * the buffer to the caller.
 */
static void
read_file_succeeded (NautilusDigestFileHandle *digest_handle)
{
	char digest_result[16];
	gnome_vfs_async_close (digest_handle->handle,
				       digest_file_close_callback,
				       NULL);
	
	/* Invoke the callback to continue processing the annotation */
	md5_final (&ctx, &digest_result);
	(* digest_handle->callback) (digest_handle->file, &digest_result);
	
	g_free (digest_handle->buffer);
	g_free (digest_handle);
}

/* Tell the caller we failed. */
static void
read_file_failed (NautilusDigestFileHandle *digest_handle, GnomeVFSResult result)
{
	gnome_vfs_async_close (digest_handle->handle,
				       digest_file_close_callback,
				       NULL);
	
	g_free (digest_handle->buffer);
	
	(* digest_handle->callback) (digest_handle->file, NULL);
	g_free (digest_handle);
}

/* Here is the callback from the file read routine, where we actually accumulate the checksum */
static void
calculate_checksum_callback (GnomeVFSAsyncHandle *handle,
				GnomeVFSResult result,
				gpointer buffer,
				GnomeVFSFileSize bytes_requested,
				GnomeVFSFileSize bytes_read,
				gpointer callback_data)
{
	NautilusDigestFileHandle *digest_handle;
	gboolean read_more;

	/* Do a few reality checks. */
	g_assert (bytes_requested == READ_CHUNK_SIZE);
	
	digest_handle = callback_data;
	g_assert (digest_handle->handle == handle);
	g_assert (bytes_read <= bytes_requested);

	/* Check for a failure. */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		digest_file_failed (read_handle, result);
		return;
	}

	/* Check for the extremely unlikely case where the file size overflows. */
	if (digest_handle->bytes_read + bytes_read < digest_handle->bytes_read) {
		digest_file_failed (digest_handle, GNOME_VFS_ERROR_TOO_BIG);
		return;
	}

	/* accumulate the recently read data into the checksum */
	md5_update (&digest_handle->context, buffer, bytes_read);
	
	/* Read more unless we are at the end of the file. */
	if (bytes_read > 0 && result == GNOME_VFS_OK) {
		gnome_vfs_async_read (digest_handle->handle,
			      digest_handle->buffer,
			      READ_CHUNK_SIZE,
			      calculate_checksum_callback,
			      digest_handle);
	} else {
		digest_file_completed (digest_handle);
	}
}

/* Once the open is finished, read a first chunk. */
static void
read_file_open_callback (GnomeVFSAsyncHandle *handle,
			 GnomeVFSResult result,
			 gpointer callback_data)
{
	NautilusDigestFileHandle *digest_handle;
	
	digest_handle = callback_data;
	g_assert (digest_handle->handle == handle);

	/* Handle the failure case. */
	if (result != GNOME_VFS_OK) {
		digest_file_failed (digest_handle, result);
		return;
	}

	/* read in the first chunk of the file */
	gnome_vfs_async_read (digest_handle->handle,
			      digest_handle->buffer,
			      READ_CHUNK_SIZE,
			      calculate_checksum_callback,
			      digest_handle);
}


/* calculate the digest for the passed-in file asynchronously, invoking the passed in
 * callback when the calculation has been completed.
 */
static void
calculate_file_digest (NautilusFile *file, NautilusCalculateDigestCallback callback)
{
	NautilusDigestFileHandle *handle;
	char *uri;
	
	/* allocate a digest-handle structure to keep our state */

	handle = g_new0 (NautilusDigestFileHandle, 1);
	uri = nautilus_file_get_uri (file);
	
	handle->callback = callback;
	handle->file = file;

	/* allocate the buffer */
	
	/* initialize the MD5 stuff */
	md5_init(&handle->context);		
	
	/* open the file */
	gnome_vfs_async_open (&handle->handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      read_file_open_callback,
			      handle);
	g_free (uri);
	return handle;	
}

/* look up the passed-in digest in the local annotation cache */
static char *
look_up_local_annotation (NautilusFile *file, const char *digest)
{
	return NULL;
}

static gboolean
has_local_annotation (const char *digest)
{
	return FALSE;
}

/* ask the server for an annotation asynchrounously  */
static void
get_annotation_from_server (NautilusFile *file, const char *file_digest)
{
}

/* callback that's invokes when we've finished calculating the file's digest.  Remember
 * it in the metadata, and look up the associated annotation
 */
void got_file_digest (NautilusFile *file, const char *file_digest)
{
	/* save the digest in the file metadata */
	
	/* lookup the annotations associated with the file. If there is one, flag the change and we're done */
	if (has_local_annotation (file_digest)) {
		nautilus_file_changed (file);
		return;
	}

	/* there isn't a local annotation, so ask the server for one */
	get_annotation_from_server (file, file_digest);
	return NULL;	
}

/* return the annotation associated with a file. If we haven't inspected this file yet,
 * return NULL but queue a request for an annotation lookup, which will be processed
 * asynchronously and issue a "file_changed" signal if any is found.
 */
char	*nautilus_annotation_get_annotation (NautilusFile *file)
{
	char *digest, *annotations;
	
	/* see if there's a digest available in metadata */
	digest = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_FILE_DIGEST, NULL);
	
	/* there isn't a digest, so start a request for one going, and return NULL */
	if (digest == NULL) {
		calculate_file_digest (file, (NautilusCalculateDigestCallback) got_file_digest);
		return NULL;
	}
	
	/* there's a digest, so we if we have the annotations for the file cached locally */
	annotations = look_up_local_annotation (file, digest);
	if (annotations != NULL) {
		g_free (digest);
		return annotations;
	}
		
	/* we don't have a local annotation, so queue a request from the server */
	get_annotation_from_server (file, digest);
	g_free (digest);
	return NULL;	
}

/* return the number of annotations associated with the passed in file.  If we don't know,
 * return 0, but queue a request like above
 */
int	nautilus_annotation_has_annotation (NautilusFile *file)
{

}

/* add an annotation to a file */
void	nautilus_annotation_add_annotation  (NautilusFile *file, const char *new_annotation)
{
}

/* remove an annotation from a file */
void	nautilus_annotation_remove_annotation (NautilusFile *file, int which_annotation)
{
}

