/*********************************************************************
 * 
 *    Copyright (C) 1998, 1999,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3_header.h
 * Description:   Definitions for various ID3 headers.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Thu Nov  5 15:55:10 1998
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
#ifndef ID3_HEADER_H
#define ID3_HEADER_H

#ifndef __GNUC__
#define __attribute__(A)
#endif

/*
 * Layout for the ID3 tag header.
 */
typedef struct id3_taghdr_t id3_taghdr_t;
struct id3_taghdr_t {
    guint8	th_version;
    guint8	th_revision;
    guint8	th_flags;
    guint32	th_size;
} __attribute__ ((packed));

#define ID3_THFLAG_USYNC	0x80000000
#define ID3_THFLAG_EXT		0x40000000
#define ID3_THFLAG_EXP		0x20000000

#define ID3_SET_SIZE28(size)		\
    ( ((size << 3) & 0x7f000000) |	\
      ((size << 2) & 0x007f0000) |	\
      ((size << 1) & 0x00007f00) |	\
      ((size     ) & 0x0000007f) )

#define ID3_GET_SIZE28(size)		\
    ( ((size & 0x7f000000) >> 3) |	\
      ((size & 0x007f0000) >> 2) |	\
      ((size & 0x00007f00) >> 1) |	\
      ((size & 0x0000007f)     ) )



/*
 * Layout for the extended header.
 */
typedef struct id3_exthdr_t id3_exthdr_t;
struct id3_exthdr_t {
    guint32	eh_size;
    guint16	eh_flags;
    guint32	eh_padsize;
} __attribute__ ((packed));

#define ID3_EHFLAG_CRC		0x80000000



/*
 * Layout for the frame header.
 */
typedef struct id3_framehdr_t id3_framehdr_t;
struct id3_framehdr_t {
    guint32	fh_id;
    guint32	fh_size;
    guint16	fh_flags;
} __attribute__ ((packed));

#define ID3_FHFLAG_TAGALT	0x8000
#define ID3_FHFLAG_FILEALT	0x4000
#define ID3_FHFLAG_RO		0x2000
#define ID3_FHFLAG_COMPRESS	0x0080
#define ID3_FHFLAG_ENCRYPT	0x0040
#define ID3_FHFLAG_GROUP	0x0020


typedef enum {
    ID3_UNI_LATIN	= 0x007f,
    ID3_UNI_LATIN_1	= 0x00ff,

    ID3_UNI_SUPPORTED	= 0x00ff,
    ID3_UNI_UNSUPPORTED	= 0xffff,
} id3_unicode_blocks;


#ifdef DEBUG_ID3
#define id3_error(id3, error)		\
  (void) ( id3->id3_error_msg = error,	\
           printf( "Error %s, line %d: %s\n", __FILE__, __LINE__, error ) )


#else
#define id3_error(id3, error)		\
  (void) ( id3->id3_error_msg = error )

#endif


#endif /* ID3_HEADER_H */
