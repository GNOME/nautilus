/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-annotation.c: routines for getting and setting xml-based annotations associated
   with the digest of a file.
  
   Copyright (C) 2001 Eazel, Inc.
  
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
#include "nautilus-file-private.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-preferences.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* icon selection callback function. */
typedef void (* NautilusCalculateDigestCallback) (NautilusFile *file, char *file_digest);
typedef struct NautilusDigestFileHandle NautilusDigestFileHandle;

typedef struct {
	guint32 buf[4];
	guint32 bits[2];
	guchar in[64];
	int doByteReverse;
} MD5Context ;

struct NautilusDigestFileHandle {
	GnomeVFSAsyncHandle *handle;
	NautilusCalculateDigestCallback callback;
	NautilusFile *file;
	char *buffer;
	gboolean opened;
	MD5Context digest_context;
};

#define READ_CHUNK_SIZE 65536
#define MAX_DIGESTS_IN_PROGRESS 16
#define SERVER_URI_TEMPLATE		"http://dellbert.differnet.com/get_notes.cgi?ids=%s"

static int open_count = 0;
static int close_count = 0;
static int digests_in_progress = 0;

static GList* digest_request_queue = NULL;
static GList* annotation_request_queue = NULL;

static GHashTable *files_awaiting_annotation = NULL;

static void md5_transform (guint32 buf[4], const guint32 in[16]);

static int _ie = 0x44332211;
static union _endian { int i; char b[4]; } *_endian = (union _endian *)&_ie;
#define	IS_BIG_ENDIAN()		(_endian->b[0] == '\x44')
#define	IS_LITTLE_ENDIAN()	(_endian->b[0] == '\x11')

static void got_file_digest (NautilusFile *file, const char *file_digest);
static void process_digest_requests (void);

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
	close_count += 1;
	g_message ("opened %d, closed %d", open_count, close_count);
}

/* Close the file and then tell the caller we succeeded, handing off
 * the buffer to the caller.
 */
static void
digest_file_completed (NautilusDigestFileHandle *digest_handle)
{
	guchar digest_result[16];
	char digest_string [33];
	char*  hex_string = "0123456789abcdef";
	int index, result_index;
	int current_value;

	if (digest_handle->opened) {
	
		gnome_vfs_async_close (digest_handle->handle,
				       digest_file_close_callback,
				       NULL);
	}
	
	digests_in_progress	 -= 1;
	
	/* Invoke the callback to continue processing the annotation */
	md5_final (&digest_handle->digest_context, digest_result);
	
	/* make a hex string for the digest result */
	digest_string[32] = '\0';
	for (index = 0; index < 32; index++) {
		current_value = digest_result[index >> 1];
		if (index & 1)	{
			result_index = current_value & 15;
		} else {
			result_index = (current_value >> 4) & 15;
		}
		
		digest_string[index] = hex_string[result_index];
	}
	
	(* digest_handle->callback) (digest_handle->file, &digest_string[0]);
		
	nautilus_file_unref (digest_handle->file);
	g_free (digest_handle->buffer);
	g_free (digest_handle);

	/* start new digest requests if necessary */
	process_digest_requests ();

}

/* Tell the caller we failed. */
static void
digest_file_failed (NautilusDigestFileHandle *digest_handle, GnomeVFSResult result)
{
	if (digest_handle->opened) {
		gnome_vfs_async_close (digest_handle->handle,
				       digest_file_close_callback,
				       NULL);
	}
	g_free (digest_handle->buffer);

	digests_in_progress	 -= 1;
	
	(* digest_handle->callback) (digest_handle->file, NULL);
		
	nautilus_file_unref (digest_handle->file);	
	g_free (digest_handle);

	/* start new digest requests if necessary */
	process_digest_requests ();
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

	/* Do a few reality checks. */
	g_assert (bytes_requested == READ_CHUNK_SIZE);
	
	digest_handle = callback_data;
	g_assert (digest_handle->handle == handle);
	g_assert (bytes_read <= bytes_requested);

	/* Check for a failure. */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		digest_file_failed (digest_handle, result);
		return;
	}

	/* accumulate the recently read data into the checksum */
	if (bytes_read > 0) {
		md5_update (&digest_handle->digest_context, buffer, bytes_read);
	}
	
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
	char *name;
	
	digest_handle = callback_data;
	g_assert (digest_handle->handle == handle);

	/* Handle the failure case. */
	if (result != GNOME_VFS_OK) {
		name = nautilus_file_get_name (digest_handle->file);
		g_message ("open failed, filename %s, error was %d", name, result);
		g_free (name);
		digest_file_failed (digest_handle, result);
		return;
	}
	
	/* read in the first chunk of the file */
	digest_handle->opened = TRUE;
	open_count += 1;
	gnome_vfs_async_read (digest_handle->handle,
			      digest_handle->buffer,
			      READ_CHUNK_SIZE,
			      calculate_checksum_callback,
			      digest_handle);
}

/* calculate the digest for the passed-in file asynchronously, invoking the passed in
 * callback when the calculation has been completed.
 */
static NautilusDigestFileHandle*
calculate_file_digest (NautilusFile *file, NautilusCalculateDigestCallback callback)
{
	NautilusDigestFileHandle *handle;
	char *uri;


	/* allocate a digest-handle structure to keep our state */

	handle = g_new0 (NautilusDigestFileHandle, 1);
	uri = nautilus_file_get_uri (file);
	
	handle->callback = callback;
	handle->opened = FALSE;
	handle->file = file;
	nautilus_file_ref (file);
	
	/* allocate the buffer */
	handle->buffer = g_malloc (READ_CHUNK_SIZE);
	
	/* initialize the MD5 stuff */
	md5_init (&handle->digest_context);		
	
	/* open the file */
	gnome_vfs_async_open (&handle->handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      read_file_open_callback,
			      handle);
	g_free (uri);
	return handle;
}

/* process the digest request queue, launching as many requests as we can handle */
static void
process_digest_requests (void)
{
	GList *current_entry;
	NautilusFile *file;
	
	while (digests_in_progress < MAX_DIGESTS_IN_PROGRESS && digest_request_queue != NULL)
		{
			/* pull entry off queue */
			current_entry = digest_request_queue;
			digest_request_queue = current_entry->next;
			
			file = NAUTILUS_FILE (current_entry->data);
			
			/* initiate request */
			calculate_file_digest (file, (NautilusCalculateDigestCallback) got_file_digest);
			
			/* dispose of queue entry */
			nautilus_file_unref (file);
				
			g_list_free_1 (current_entry);		
			digests_in_progress += 1;
		}
}

/* queue the digest request, and start processing it if we haven't exceeded the limit of requests
 * in progress
 */
static void
queue_file_digest_request (NautilusFile *file)
{
	/* if annotation lookup is disabled, don't bother to do all this work */
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_LOOKUP_ANNOTATIONS)) {
		return;
	}
	nautilus_file_ref (file);
	digest_request_queue = g_list_append (digest_request_queue, file);
	process_digest_requests ();
}

/* given a digest, retrieve an associated file object from the hash table */
static NautilusFile *
get_file_from_digest (const char *digest)
{
	if (files_awaiting_annotation == NULL) {
		return NULL;
	}
	
	return g_hash_table_lookup (files_awaiting_annotation, digest);
}

/* given a digest value, return the path to it in the local cache */
static char *
get_annotation_path (const char *digest)
{
	char *user_directory, *annotation_directory;
	char *annotation_path, *directory_uri;
	
	user_directory = nautilus_get_user_directory ();
	annotation_directory = nautilus_make_path (user_directory, "annotations");
	annotation_path = nautilus_make_path (annotation_directory, digest);
	
	/* create the annotation directory if it doesn't exist */
	if (!g_file_exists (annotation_directory)) {
		directory_uri = gnome_vfs_get_uri_from_local_path (annotation_directory);
		gnome_vfs_make_directory (directory_uri,
						 GNOME_VFS_PERM_USER_ALL
						 | GNOME_VFS_PERM_GROUP_ALL
						 | GNOME_VFS_PERM_OTHER_READ);
		g_free (directory_uri);
	}
	
	/* free up the intermediate strings and return the complete path */
	g_free (user_directory);
	g_free (annotation_directory);
	
	return annotation_path;
}

/* look up the passed-in digest in the local annotation cache */
static char *
look_up_local_annotation (NautilusFile *file, const char *digest)
{
	GnomeVFSResult result;
	int  file_size;
	char *uri, *path, *file_data, *buffer;
	
	path = get_annotation_path (digest);
	if (g_file_exists (path)) {
		/* load the file and return it */
		uri = gnome_vfs_get_uri_from_local_path (path);
		result = nautilus_read_entire_file (uri, &file_size, &file_data);
		g_free (uri);
		g_free (path);
		if (result == GNOME_VFS_OK) {
			/* add a null at the end, so it's a valid string */
			buffer = g_realloc (file_data, file_size + 1);
			buffer[file_size] = '\0';					
			return buffer;
		} else {
			return NULL;
		}
	}
	g_free (path);
	return NULL;
}

static gboolean
has_local_annotation (const char *digest)
{
	gboolean has_annotation;
	char *path;
	
	path = get_annotation_path (digest);
	has_annotation = g_file_exists (path);
	
	g_free (path);
	return has_annotation;
}

/* utility routine to add the passed-in xml node to the file associated with the passed-in
 * digest.  If there isn't a file, create one
 */
static void
add_annotations_to_file (xmlNodePtr node_ptr, const char *digest)
{
	char *digest_path;
	xmlDocPtr document;
	
	digest_path = get_annotation_path (digest);

	/* save the subtree as a new document, by making a new document and adding the new node */
	document = xmlNewDoc ("1.0");
	xmlDocSetRootElement (document, node_ptr);
			
	/* save the xml tree as a file in the cache area */
	xmlSaveFile (digest_path, document);
	
	xmlFreeDoc (document);
	g_free (digest_path);
}

/* remember the file object by adding it to a hash table */
static void
remember_file (NautilusFile *file, const char *digest)
{
	nautilus_file_ref (file);
	
	if (files_awaiting_annotation == NULL) {
		files_awaiting_annotation = g_hash_table_new (g_str_hash, g_str_equal);
		/* g_atexit (annotations_file_table_free); */
	}

	g_hash_table_insert (files_awaiting_annotation, g_strdup (digest), file);
}

/* forget a file when we're done with it by removing it from the table */
static void
forget_file (const char *digest)
{
	NautilusFile *file;
	if (files_awaiting_annotation == NULL) {
		return;
	}
	
	file = g_hash_table_lookup (files_awaiting_annotation, digest);	
	if (file != NULL) {
		nautilus_file_unref (file);
		g_hash_table_remove (files_awaiting_annotation, digest);
	}
}

/* completion routine invoked when we've loaded the an annotation file from the service.
 * We must parse it, and walk through it to save the annotations in the local cache.
 */
static void
got_annotations_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	NautilusFile *file;
	xmlDocPtr annotations;
	xmlNodePtr next_annotation, item;
	xmlNodePtr saved_annotation;
	int annotation_count;
	char *buffer, *digest, *info_str;
	time_t date_stamp;
	
	/* exit if there was an error */
	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		return;
	}
	
	/* inexplicably, the gnome-xml parser requires a zero-terminated array, so add the null at the end. */
	buffer = g_realloc (file_contents, file_size + 1);
	buffer[file_size] = '\0';
	annotations = xmlParseMemory (buffer, file_size);
	g_free (buffer);
	
	/* iterate through the xml document, handling each annotation entry */	
	if (annotations != NULL) {
		next_annotation = xmlDocGetRootElement (annotations)->childs;
		while (next_annotation != NULL) {
			if (nautilus_strcmp (next_annotation->name, "annotations") == 0) {
				/* get the digest associated with the annotations */
				digest = xmlGetProp (next_annotation, "digest");
				if (digest != NULL) {
					/* count the number of annotations contained in the node */
					annotation_count = 0;
					item = next_annotation->childs;
					while (item != NULL) {
						if (nautilus_strcmp (item->name, "annotation") == 0) {
							annotation_count += 1;
						}
						item = item->next;
					}
					
					/* write the annotation out to our cache area, if necessary */
					if (annotation_count > 0) {						
						saved_annotation = xmlCopyNode (next_annotation, TRUE);
						add_annotations_to_file (saved_annotation, digest);
					}
					
					/* retrieve the file object, and update it's count and time stamp */
					
					file = get_file_from_digest (digest);	
					time (&date_stamp);
					info_str = g_strdup_printf ("%lu:%d", date_stamp, annotation_count);
					
					nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_NOTES_INFO, NULL, info_str);
					g_free (info_str);
					
					/* issue the changed signal */
					nautilus_file_emit_changed (file);
				
					/* remove the file from the hash table and unref it */
					forget_file (digest);
					xmlFree (digest);
				}
			}
			next_annotation = next_annotation->next;
		}
				
	
		/* free the xml document */
		xmlFreeDoc (annotations);
	}
}

/* format the request, and send it to the server */
/* the first cut implementation simply sends the digests as a cgi parameter,
 * but soon we'll want use SOAP or XML-RPC
 */
static void
fetch_annotations_from_server (void)
{
	GString *temp_string;
	GList *current_entry, *save_entry;
	char *uri;
	
	/* check to see if there are enough requests, or a long enough delay since the last one */
	
	current_entry = annotation_request_queue;
	save_entry = current_entry;
	annotation_request_queue = NULL;
	
	/* simple cgi-based request format passed the digests as part of the uri, so
	 * gather the variable parts
	 */
	temp_string = g_string_new ("");
	while (current_entry != NULL) {
		g_string_append (temp_string, (char*) current_entry->data);
		if (current_entry->next != NULL) {
			g_string_append (temp_string, ",");
		}
		current_entry = current_entry->next;	
	}
	

	uri = g_strdup_printf (SERVER_URI_TEMPLATE, temp_string->str);		
	g_string_free (temp_string, TRUE);
	nautilus_g_list_free_deep (save_entry);
	
	/* read the result from the server asynchronously */
	nautilus_read_entire_file_async (uri, got_annotations_callback, NULL);
	g_free (uri);
}


/* ask the server for an annotation asynchronously  */
static void
get_annotation_from_server (NautilusFile *file, const char *file_digest)
{
	/* see if there's a request for this one already pending - if so, we can return */
	if (get_file_from_digest (file_digest) != NULL) {
		return;
	}	

	/* only do this if lookups are enabled */
	/* if annotation lookup is disabled, don't bother to do all this work */
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_LOOKUP_ANNOTATIONS)) {
		return;
	}
	
	/* add the request to the queue, and kick it off it there's enough of them */
	annotation_request_queue = g_list_prepend (annotation_request_queue, g_strdup (file_digest));
	
	remember_file (file, file_digest);
	fetch_annotations_from_server ();
}

/* callback that's invokes when we've finished calculating the file's digest.  Remember
 * it in the metadata, and look up the associated annotation
 */
static void
got_file_digest (NautilusFile *file, const char *file_digest)
{
	
	if (file_digest == NULL) {
		return;
	}

	/* save the digest in the file metadata */
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_FILE_DIGEST, NULL, file_digest);

	/* lookup the annotations associated with the file. If there is one, flag the change and we're done */
	if (has_local_annotation (file_digest)) {
		nautilus_file_emit_changed (file);
		return;
	}

	/* there isn't a local annotation, so ask the server for one */
	get_annotation_from_server (file, file_digest);
	return;	
}

/* return the annotation associated with a file. If we haven't inspected this file yet,
 * return NULL but queue a request for an annotation lookup, which will be processed
 * asynchronously and issue a "file_changed" signal if any is found.
 */
char	*nautilus_annotation_get_annotation (NautilusFile *file)
{
	char *digest;
	char *annotations;
	char *digest_info;
	
	/* if it's a directory, return NULL, at least until we figure out how to handle directory
	 * annotations
	 */
	if (nautilus_file_is_directory (file)) {
		return NULL;
	}
	 
	/* see if there's a digest available in metadata */
	digest = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_FILE_DIGEST, NULL);
	
	/* there isn't a digest, so start a request for one going, and return NULL */
	if (digest == NULL) {
		queue_file_digest_request (file);
		return NULL;
	}
	
	/* there's a digest, so we if we have the annotations for the file cached locally */
	annotations = look_up_local_annotation (file, digest);
	if (annotations != NULL) {
		g_free (digest);
		return annotations;
	}
		
	/* we don't have a local annotation, so queue a request from the server, if we haven't already tried */
	/* soon, we'll inspect the time stamp, and look it up anyway if it's too old */
	
	digest_info = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_NOTES_INFO, NULL);
	if (digest_info == NULL) {
		get_annotation_from_server (file, digest);
	} else {
		g_free (digest_info);
	}
	
	g_free (digest);
	return NULL;	
}

/* utility routine to map raw annotation text into text to be displayed 
 * for now this is pretty naive and only handles free-form text, just returning
 * the first suitable annotation it can find.
 */
char *
nautilus_annotation_get_display_text (const char *note_text)
{
	char *display_text, *temp_text;
	xmlChar *xml_text;
	xmlDocPtr annotations;
	xmlNodePtr next_annotation;	
	
	/* if its an xml file, parse it to extract the display text */
	if (nautilus_istr_has_prefix (note_text, "<?xml")) {
		display_text = NULL;
		annotations = xmlParseMemory ((char*) note_text, strlen (note_text));
		if (annotations != NULL) {
			next_annotation = xmlDocGetRootElement (annotations)->childs;
			while (next_annotation != NULL) {
				if (nautilus_strcmp (next_annotation->name, "annotation") == 0) {
					xml_text = xmlNodeGetContent (next_annotation);
					temp_text = (char*) xml_text;
					while (*temp_text && *temp_text < ' ') temp_text++;
					display_text = g_strdup (temp_text);
					xmlFree (xml_text);
					break;
				}
				next_annotation = next_annotation->next;
			}
			xmlFreeDoc (annotations);
		}
	} else {
		display_text = g_strdup (note_text);
	}	
	return display_text;
}

/* convenience routine to return the display text of an annotation associated
 * with a file
 */
char *
nautilus_annotation_get_annotation_for_display (NautilusFile *file)
{
	char *raw_text, *display_text;
	
	raw_text = nautilus_annotation_get_annotation (file);
	if (raw_text != NULL) {
		display_text = nautilus_annotation_get_display_text (raw_text);
		g_free (raw_text);
		return display_text;
	}
	return NULL;
}

/* return the number of annotations associated with the passed in file.  If we don't know,
 * return 0, but queue a request like above
 */
int	nautilus_annotation_has_annotation (NautilusFile *file)
{
	char *digest_info, *digits, *temp_str;
	int count;
	
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_DISPLAY_ANNOTATIONS)) {
		return 0;
	}
	
	digest_info = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_NOTES_INFO, NULL);
	
	if (digest_info != NULL) {
		digits = strrchr (digest_info, ':');
		count = atoi (digits + 1);
		g_free (digest_info);
		return count;
	} else {
		/* initiate fetching the annotations from the server */
		temp_str = nautilus_annotation_get_annotation (file);
		g_free (temp_str);	
	}
	g_free (digest_info);
	return 0;
}

/* add an annotation to a file */
void	nautilus_annotation_add_annotation (NautilusFile *file,
					    const char *annotation_type,
					    const char *annotation_text,
					    const char *access)
{
	char *digest;
	char *annotations;
	xmlDocPtr xml_document;
	
	/* we can't handle directories yet, so just return.  */
	if (nautilus_file_is_directory (file)) {
		return;
	}

	/* fetch the local annotation, if one exists */
	digest = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_FILE_DIGEST, NULL);
	
	/* there isn't a digest, so start a request for one going, and return */
	/* this shouldn't happen in practice, since the annotation window will have
	 * already created a digest
	 */
	if (digest == NULL) {
		queue_file_digest_request (file);
		return;
	}
		
	/* there's a digest, so we if we have the annotations for the file cached locally */
	annotations = look_up_local_annotation (file, digest);
	
	/* no annotation exists, so create the initial xml document from scratch */
	if (annotations == NULL || strlen (annotations) == 0) {
		xml_document = xmlNewDoc ("1.0");
		/* create the header node, with the digest attribute */
	} else {
		/* open the existing annotation and load it */
		xml_document = xmlParseMemory (annotations, strlen (annotations));
	}
	
	/* add the new entry.  For now, we only support one entry per file, so we replace the old
	 * one, if it exists, but this will change soon as we support multiple notes per file
	 */
	 
	/* save the modified xml document back to the local repository */
	
	/* update the metadata date and count */
	
	/* issue file changed symbol to update the emblem */
	
	/* if the access is global, send it to the server */

	/* clean up and we're done */
	xmlFreeDoc (xml_document);
	g_free (digest);
	g_free (annotations);	 
}

/* remove an annotation from a file */
void	nautilus_annotation_remove_annotation (NautilusFile *file, int which_annotation)
{
}

