/*********************************************************************
 * 
 *    Copyright (C) 1999,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3_frame_text.c
 * Description:   Code for handling ID3 text frames.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Fri Feb  5 23:50:33 1999
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "id3.h"
#include "id3_header.h"


/*
 * Function id3_get_encoding (frame)
 *
 *    Return text encoding for frame, or -1 if frame does not have any
 *    text encoding.
 *
 */
gint8 id3_get_encoding(id3_frame_t *frame)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' &&
	 frame->fr_desc->fd_id != ID3_WXXX &&
	 frame->fr_desc->fd_id != ID3_IPLS &&
	 frame->fr_desc->fd_id != ID3_USLT &&
	 frame->fr_desc->fd_id != ID3_SYLT &&
	 frame->fr_desc->fd_id != ID3_COMM &&
	 frame->fr_desc->fd_id != ID3_APIC &&
	 frame->fr_desc->fd_id != ID3_GEOB &&
	 frame->fr_desc->fd_id != ID3_USER &&
	 frame->fr_desc->fd_id != ID3_OWNE &&
	 frame->fr_desc->fd_id != ID3_COMR )
	return -1;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return -1;

    return *(gint8 *) frame->fr_data;
}


/*
 * Function id3_set_encoding (frame, encoding)
 *
 *    Set text encoding for frame.  Return 0 upon success, or -1 if an
 *    error occured. 
 *
 */
int id3_set_encoding(id3_frame_t *frame, gint8 encoding)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' &&
	 frame->fr_desc->fd_id != ID3_WXXX &&
	 frame->fr_desc->fd_id != ID3_IPLS &&
	 frame->fr_desc->fd_id != ID3_USLT &&
	 frame->fr_desc->fd_id != ID3_SYLT &&
	 frame->fr_desc->fd_id != ID3_COMM &&
	 frame->fr_desc->fd_id != ID3_APIC &&
	 frame->fr_desc->fd_id != ID3_GEOB &&
	 frame->fr_desc->fd_id != ID3_USER &&
	 frame->fr_desc->fd_id != ID3_OWNE &&
	 frame->fr_desc->fd_id != ID3_COMR )
	return -1;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return -1;

    /* Changing the encoding of frames is not supported yet */
    if ( *(gint8 *) frame->fr_data != encoding )
	return -1;

    /* Set encoding */
    *(gint8 *) frame->fr_data = encoding;
    return 0;
}


/*
 * Function id3_get_text (frame)
 *
 *    Return string contents of frame.
 *
 */
char *id3_get_text(id3_frame_t *frame)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' )
	return NULL;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return NULL;

    if ( frame->fr_desc->fd_id == ID3_TXXX ) {
	/*
	 * This is a user defined text frame.  Skip the description.
	 */
	switch ( *(guint8 *) frame->fr_data ) {
	case ID3_ENCODING_ISO_8859_1:
	{
	    char *text = (char *) frame->fr_data + 1;

	    while ( *text != 0 )
		text++;

	    return ++text;
	}
	case ID3_ENCODING_UNICODE:
	{
	    gint16 *text16 = GINT_TO_POINTER ((GPOINTER_TO_INT (frame->fr_data) + 1));

	    while ( *text16 != 0 )
		text16++;

	    return (char *) ++text16;
	}
	default:
	    return NULL;
	}
    }

    return (char *) frame->fr_data + 1;
}


/*
 * Function id3_get_text_desc (frame)
 *
 *    Get description part of a text frame.
 *
 */
char *id3_get_text_desc(id3_frame_t *frame)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' )
	return NULL;

    /* If predefined text frame, return description. */
    if ( frame->fr_desc->fd_id != ID3_TXXX )
	return frame->fr_desc->fd_description;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return NULL;

    return (char *) frame->fr_data + 1;
}


/*
 * Function id3_get_text_number (frame)
 *
 *    Return string contents of frame translated to a positive
 *    integer, or -1 if an error occured.
 *
 */
int id3_get_text_number(id3_frame_t *frame)
{
    int number = 0;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return -1;

    /*
     * Generate integer according to encoding.
     */
    switch ( *(guint8 *) frame->fr_data ) {
    case ID3_ENCODING_ISO_8859_1:
    {
	char *text = ((char *) frame->fr_data) + 1;

	while ( *text >= '0' && *text <= '9' ) {
	    number *= 10;
	    number += *text - '0';
	    text++;
	}

	return number;
    }
    case ID3_ENCODING_UNICODE:
    {
	gint16 *text = ((gint16 *) frame->fr_data) + 1;

	while ( *text >= '0' && *text <= '9' ) {
	    number *= 10;
	    number += *text - '0';
	    text++;
	}

	return number;
    }

    default:
	return -1;
    }
}


/*
 * Function id3_set_text (frame, text)
 *
 *    Set text for the indicated frame (only ISO-8859-1 is currently
 *    supported).  Return 0 upon success, or -1 if an error occured.
 *
 */
int id3_set_text(id3_frame_t *frame, char *text)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' )
	return -1;

    /*
     * Release memory occupied by previous data.
     */
    if ( frame->fr_data )
	free( frame->fr_data );
    if ( frame->fr_data_z )
	free( frame->fr_data_z );
    frame->fr_data_z = NULL;
    frame->fr_size_z = 0;

    /*
     * Allocate memory for new data.
     */
    frame->fr_size = strlen(text) + 1;
    frame->fr_data = malloc( frame->fr_size + 1 );
    if ( frame->fr_data == NULL )
	return -1;

    /*
     * Copy contents.
     */
    *(gint8 *) frame->fr_data = ID3_ENCODING_ISO_8859_1;
    memcpy( (char *) frame->fr_data + 1, text, frame->fr_size );

    frame->fr_altered = 1;
    frame->fr_owner->id3_altered = 1;

    return 0;
}


/*
 * Function id3_set_text_number (frame, number)
 *
 *    Set number for the indicated frame (only ISO-8859-1 is currently
 *    supported).  Return 0 upon success, or -1 if an error occured.
 *
 */
int id3_set_text_number(id3_frame_t *frame, int number)
{
    char buf[64];
    int pos;
    char *text;

    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'T' )
	return -1;

    /*
     * Release memory occupied by previous data.
     */
    if ( frame->fr_data )
	free( frame->fr_data );
    if ( frame->fr_data_z )
	free( frame->fr_data_z );
    frame->fr_data_z = NULL;
    frame->fr_size_z = 0;

    /*
     * Create a string with a reversed number.
     */
    pos = 0;
    while ( number > 0 && pos < 64 ) {
	buf[pos++] = (number % 10) + '0';
	number /= 10;
    }
    if ( pos == 64 )
	return -1;
    if ( pos == 0 )
	buf[pos++] = '0';

    /*
     * Allocate memory for new data.
     */
    frame->fr_size = pos + 1;
    frame->fr_data = malloc( frame->fr_size + 1 );
    if ( frame->fr_data == NULL )
	return -1;

    /*
     * Insert contents.
     */
    *(gint8 *) frame->fr_data = ID3_ENCODING_ISO_8859_1;
    text = (char *) frame->fr_data + 1;
    while ( --pos >= 0 ) 
	*text++ = buf[pos];
    *text = '\0';

    frame->fr_altered = 1;
    frame->fr_owner->id3_altered = 1;

    return 0;
}
