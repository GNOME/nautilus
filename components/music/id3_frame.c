/*********************************************************************
 * 
 *    Copyright (C) 1999-2000,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3_frame.c
 * Description:   Code for handling ID3 frames.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Fri Feb  5 23:47:08 1999
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
#if HAVE_LIBZ
#include <zlib.h>
#endif
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "id3.h"
#include "id3_header.h"

/*
 * Description of all valid ID3v2 frames.
 */
static id3_framedesc_t Framedesc[] = {
    { ID3_AENC, "AENC", "Audio encryption" },
    { ID3_APIC, "APIC", "Attached picture" },

    { ID3_COMM, "COMM", "Comments" },
    { ID3_COMR, "COMR", "Commercial frame" },

    { ID3_ENCR, "ENCR", "Encryption method registration" },
    { ID3_EQUA, "EQUA", "Equalization" },
    { ID3_ETCO, "ETCO", "Event timing codes" },

    { ID3_GEOB, "GEOB", "General encapsulated object" },
    { ID3_GRID, "GRID", "Group identification registration" },

    { ID3_IPLS, "IPLS", "Involved people list" },

    { ID3_LINK, "LINK", "Linked information" },

    { ID3_MCDI, "MCDI", "Music CD identifier" },
    { ID3_MLLT, "MLLT", "MPEG location lookup table" },

    { ID3_OWNE, "OWNE", "Ownership frame" },

    { ID3_PRIV, "PRIV", "Private frame" },
    { ID3_PCNT, "PCNT", "Play counter" },
    { ID3_POPM, "POPM", "Popularimeter" },
    { ID3_POSS, "POSS", "Position synchronisation frame" },

    { ID3_RBUF, "RBUF", "Recommended buffer size" },
    { ID3_RVAD, "RVAD", "Relative volume adjustment" },
    { ID3_RVRB, "RVRB", "Reverb" },

    { ID3_SYLT, "SYLT", "Synchronized lyric/text" },
    { ID3_SYTC, "SYTC", "Synchronized tempo codes" },

    { ID3_TALB, "TALB", "Album/Movie/Show title" },
    { ID3_TBPM, "TBPM", "BPM (beats per minute)" },
    { ID3_TCOM, "TCOM", "Composer" },
    { ID3_TCON, "TCON", "Content type" },
    { ID3_TCOP, "TCOP", "Copyright message" },
    { ID3_TDAT, "TDAT", "Date" },
    { ID3_TDLY, "TDLY", "Playlist delay" },
    { ID3_TENC, "TENC", "Encoded by" },
    { ID3_TEXT, "TEXT", "Lyricist/Text writer" },
    { ID3_TFLT, "TFLT", "File type" },
    { ID3_TIME, "TIME", "Time" },
    { ID3_TIT1, "TIT1", "Content group description" },
    { ID3_TIT2, "TIT2", "Title/songname/content description" },
    { ID3_TIT3, "TIT3", "Subtitle/Description refinement" },
    { ID3_TKEY, "TKEY", "Initial key" },
    { ID3_TLAN, "TLAN", "Language(s)" },
    { ID3_TLEN, "TLEN", "Length" },
    { ID3_TMED, "TMED", "Media type" },
    { ID3_TOAL, "TOAL", "Original album/movie/show title" },
    { ID3_TOFN, "TOFN", "Original filename" },
    { ID3_TOLY, "TOLY", "Original lyricist(s)/text writer(s)" },
    { ID3_TOPE, "TOPE", "Original artist(s)/performer(s)" },
    { ID3_TORY, "TORY", "Original release year" },
    { ID3_TOWN, "TOWN", "File owner/licensee" },
    { ID3_TPE1, "TPE1", "Lead performer(s)/Soloist(s)" },
    { ID3_TPE2, "TPE2", "Band/orchestra/accompaniment" },
    { ID3_TPE3, "TPE3", "Conductor/performer refinement" },
    { ID3_TPE4, "TPE4", "Interpreted, remixed, or otherwise modified by" },
    { ID3_TPOS, "TPOS", "Part of a set" },
    { ID3_TPUB, "TPUB", "Publisher" },
    { ID3_TRCK, "TRCK", "Track number/Position in set" },
    { ID3_TRDA, "TRDA", "Recording dates" },
    { ID3_TRSN, "TRSN", "Internet radio station name" },
    { ID3_TRSO, "TRSO", "Internet radio station owner" },
    { ID3_TSIZ, "TSIZ", "Size" },
    { ID3_TSRC, "TSRC", "ISRC (international standard recording code)" },
    { ID3_TSSE, "TSSE", "Software/Hardware and settings used for encoding" },
    { ID3_TYER, "TYER", "Year" },
    { ID3_TXXX, "TXXX", "User defined text information frame" },

    { ID3_UFID, "UFID", "Unique file identifier" },
    { ID3_USER, "USER", "Terms of use" },
    { ID3_USLT, "USLT", "Unsychronized lyric/text transcription" },

    { ID3_WCOM, "WCOM", "Commercial information" },
    { ID3_WCOP, "WCOP", "Copyright/Legal information" },
    { ID3_WOAF, "WOAF", "Official audio file webpage" },
    { ID3_WOAR, "WOAR", "Official artist/performer webpage" },
    { ID3_WOAS, "WOAS", "Official audio source webpage" },
    { ID3_WORS, "WORS", "Official internet radio station homepage" },
    { ID3_WPAY, "WPAY", "Payment" },
    { ID3_WPUB, "WPUB", "Publishers official webpage" },
    { ID3_WXXX, "WXXX", "User defined URL link frame" },
};


/*
 * Function id3_read_frame (id3)
 *
 *    Read next frame from the indicated ID3 tag.  Return 0 upon
 *    success, or -1 if an error occured.
 *
 */
int id3_read_frame(id3_t *id3)
{
    id3_framehdr_t *framehdr;
    id3_frame_t *frame;
    guint32 id;
    void *p;
    int i;

    /*
     * Read frame header.
     */
    framehdr = id3->id3_read( id3, NULL, sizeof(*framehdr) );
    if ( framehdr == NULL )
	return -1;

    /*
     * If we encounter an invalid frame id, we assume that there is
     * some padding in the header.  We just skip the rest of the ID3
     * tag.
     */
    i = *((guint8 *) &framehdr->fh_id);
    if ( !((i >= '0' && i <= '9') || (i >= 'A' && i <= 'Z')) ) {
	id3->id3_seek( id3, id3->id3_tagsize - id3->id3_pos );
	return 0;
    }
    id = g_ntohl( framehdr->fh_id );

    /*
     * Allocate frame.
     */
    frame = malloc( sizeof(*frame) );
    if ( frame == NULL )
	return -1;

    frame->fr_owner = id3;
    frame->fr_size = g_ntohl(framehdr->fh_size);
    frame->fr_flags = g_ntohs(framehdr->fh_flags);
    frame->fr_encryption = 0;
    frame->fr_grouping = 0;
    frame->fr_desc = NULL;
    frame->fr_altered = 0;
    frame->fr_data = NULL;

    /*
     * Determine the type of the frame.
     */
    for ( i = 0;
	  i < (int)(sizeof(Framedesc) / sizeof(id3_framedesc_t));
	  i++ ) {

	if ( Framedesc[i].fd_id == id ) {
	    /*
	     * Insert frame into linked list.
	     */
	    frame->fr_next = NULL;
	    if ( id3->id3_frame ) {
		id3->id3_tail->fr_next = frame;
	    } else {
		id3->id3_frame = frame;
	    }
	    id3->id3_tail = frame;
	    id3->id3_numframes++;

	    /*
	     * Initialize frame.
	     */
	    frame->fr_desc = Framedesc + i;

	    /*
	     * When allocating memory to hold a text frame, we
	     * allocate 2 extra byte.  This simplifies retrieval of
	     * text strings.
	     */
	    frame->fr_data = malloc( frame->fr_size +
				     (frame->fr_desc->fd_idstr[0] == 'T' ||
				      frame->fr_desc->fd_idstr[0] == 'W' ?
				      2 : 0) );
	    if ( frame->fr_data == NULL ) {
		free( frame );
		return -1;
	    }
	    p = id3->id3_read(id3, frame->fr_data, frame->fr_size );
	    if ( p == NULL ) {
		free( frame->fr_data );
		free( frame );
		return -1;
	    }

	    /*
	     * Null-terminate text frames.
	     */
	    if ( frame->fr_desc->fd_idstr[0] == 'T' ||
		 frame->fr_desc->fd_idstr[0] == 'W' ) {
		((char *) frame->fr_data)[frame->fr_size]   = 0;
		((char *) frame->fr_data)[frame->fr_size+1] = 0;
	    }

	    break;
	}
    }

    /*
     * Check if frame had a valid id.
     */
    if ( frame->fr_desc == NULL ) {
	/*
	 * No. Ignore the frame.
	 */
	return 0;
    }


    if ( frame->fr_flags & ID3_FHFLAG_COMPRESS ) {
	/*
	 * Frame is compressed using zlib.  Fetch the size of the
	 * decompressed data.
	 */
	frame->fr_size_z = frame->fr_size;
	frame->fr_size   = g_ntohl( *((guint32 *) frame->fr_data) );
	frame->fr_data_z = GINT_TO_POINTER ((GPOINTER_TO_INT (frame->fr_data) + 4));
	frame->fr_data   = NULL;
    } else {
	/*
	 * Frame is not compressed.
	 */
	frame->fr_size_z = 0;
	frame->fr_data_z = NULL;
    }

    /*
     * If frame is encrypted, we have one extra byte in the header.
     */
    if ( frame->fr_flags & ID3_FHFLAG_ENCRYPT ) {
	if ( frame->fr_data_z )
	    frame->fr_data_z = (char *) frame->fr_data_z + 1;
	else
	    frame->fr_data = (char *) frame->fr_data + 1;
    }

    /*
     * If frame has grouping identity, we have one extra byte in the
     * header.
     */
    if ( frame->fr_flags & ID3_FHFLAG_GROUP ) {
	if ( frame->fr_data_z )
	    frame->fr_data_z = (char *) frame->fr_data + 1;
	else
	    frame->fr_data = (char *) frame->fr_data + 1;
    }

    return 0;
}


/*
 * Function id3_get_frame (id3, type, num)
 *
 *    Search in the list of frames for the ID3-tag, and return a frame
 *    of the indicated type.  If tag contains several frames of the
 *    indicated type, the third argument tells which of the frames to
 *    return.
 *
 */
id3_frame_t *id3_get_frame(id3_t *id3, guint32 type, int num)
{
    id3_frame_t	*fr = id3->id3_frame;

    while ( fr != NULL ) {
	if (fr->fr_desc && fr->fr_desc->fd_id ==  type )
	{
	    if ( --num <= 0 )
		break;
        }
	fr = fr->fr_next;
    } 
    return fr;
}


/*
 * Function id3_decompress_frame (frame)
 *
 *    Uncompress the indicated frame.  Return 0 upon success, or -1 if
 *    an error occured.
 *
 */
int id3_decompress_frame(id3_frame_t *frame)
{
#ifdef HAVE_LIBZ
    z_stream z;
    int r;

    /*
     * Allocate memory to hold uncompressed frame.
     */
    frame->fr_data = malloc( frame->fr_size + 
			     (frame->fr_desc->fd_idstr[0] == 'T' ||
			      frame->fr_desc->fd_idstr[0] == 'W' ? 2 : 0) );
    if ( frame->fr_data == NULL ) {
	id3_error( frame->fr_owner, "malloc - no memory" );
	return -1;
    }

    /*
     * Initialize zlib.
     */
    z.next_in   = (Bytef *) frame->fr_data_z;
    z.avail_in  = frame->fr_size_z;
    z.zalloc    = NULL;
    z.zfree     = NULL;
    z.opaque    = NULL;

    r = inflateInit( &z );
    switch ( r ) {
    case Z_OK:
	break;
    case Z_MEM_ERROR:
	id3_error( frame->fr_owner, "zlib - no memory" );
	goto Error_init;
    case Z_VERSION_ERROR:
	id3_error( frame->fr_owner, "zlib - invalid version" );
	goto Error_init;
    default:
	id3_error( frame->fr_owner, "zlib - unknown error" );
	goto Error_init;
    }

    /*
     * Decompress frame.
     */
    z.next_out   = (Bytef *) frame->fr_data;
    z.avail_out  = frame->fr_size;
    r = inflate( &z, Z_SYNC_FLUSH );
    switch ( r ) {
    case Z_STREAM_END:
	break;
    case Z_OK:
	if ( z.avail_in == 0 )
	    /*
	     * This should not be possible with a correct stream.
	     * We will be nice however, and try to go on.
	     */
	    break;
	id3_error( frame->fr_owner, "zlib - buffer exhausted" );
	goto Error_inflate;
    default:
	id3_error( frame->fr_owner, "zlib - unknown error" );
	goto Error_inflate;
    }

    r = inflateEnd( &z );
    if ( r != Z_OK )
	id3_error( frame->fr_owner, "zlib - inflateEnd error" );

    /*
     * Null-terminate text frames.
     */
    if ( frame->fr_desc->fd_idstr[0] == 'T' ||
	 frame->fr_desc->fd_idstr[0] == 'W' ) {
	((char *) frame->fr_data)[frame->fr_size]   = 0;
	((char *) frame->fr_data)[frame->fr_size+1] = 0;
    }

    return 0;

    /*
     * Cleanup code.
     */
 Error_inflate:
    r = inflateEnd( &z );
 Error_init:
    free( frame->fr_data );
    frame->fr_data = NULL;
#endif
    return -1;
}


/*
 * Function id3_delete_frame (frame)
 *
 *    Remove frame from ID3 tag and release memory ocupied by it.
 *
 */
int id3_delete_frame(id3_frame_t *frame)
{
    id3_frame_t *fr = frame->fr_owner->id3_frame;
    id3_frame_t *prev = NULL;
    int ret;

    /*
     * Search for frame in list.
     */
    while ( fr != frame && fr != NULL ) {
	prev = fr;
	fr = fr->fr_next;
    }

    if ( fr == NULL ) {
	/*
	 * Frame does not exist in frame list.
	 */
	ret = -1;

    } else {
	/*
	 * Remove frame from frame list.
	 */
	if ( prev == NULL ) {
	    frame->fr_owner->id3_frame = frame->fr_next;
	} else {
	    prev->fr_next = frame->fr_next;
	}
	if ( frame->fr_owner->id3_tail == frame )
	    frame->fr_owner->id3_tail = prev;
	
	frame->fr_owner->id3_numframes--;
	frame->fr_owner->id3_altered = 1;
	ret = 0;
    }

    /*
     * Release memory occupied by frame.
     */
    if ( frame->fr_data )
	 free( frame->fr_data );
    if ( frame->fr_data_z )
	 free( frame->fr_data_z );
    free( frame );

    return ret;
}


/*
 * Function id3_add_frame (id3, type)
 *
 *    Add a new frame to the ID3 tag.  Return a pointer to the new
 *    frame, or NULL if an error occured.
 *
 */
id3_frame_t *id3_add_frame(id3_t *id3, guint32 type)
{
    id3_frame_t *frame;
    int i;

    /*
     * Allocate frame.
     */
    frame = malloc( sizeof(*frame) );
    if ( frame == NULL )
	return NULL;

    /*
     * Initialize frame
     */
    frame->fr_owner		= id3;
    frame->fr_desc		= NULL;
    frame->fr_flags		= 0;
    frame->fr_encryption	= 0;
    frame->fr_grouping		= 0;
    frame->fr_altered		= 0;
    frame->fr_data = frame->fr_data_z = NULL;
    frame->fr_size = frame->fr_size_z = 0;

    /*
     * Try finding the correct frame descriptor.
     */
    for ( i = 0;
	  i < (int)(sizeof(Framedesc) / sizeof(id3_framedesc_t));
	  i++ ) {
	if ( Framedesc[i].fd_id == type ) {
	    frame->fr_desc = &Framedesc[i];
	    break;
	}
    }

    /*
     * Insert frame into linked list.
     */
    frame->fr_next = NULL;
    if ( id3->id3_frame ) {
	id3->id3_tail->fr_next = frame;
    } else {
	id3->id3_frame = frame;
    }
    id3->id3_tail = frame;
    id3->id3_numframes++;
    id3->id3_altered = 1;

    return frame;
}


