/*********************************************************************
 * 
 *    Copyright (C) 1999,
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3_frame_url.c
 * Description:   Code for handling ID3 URL frames.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Tue Feb  9 21:10:45 1999
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
#include <stdio.h>

#include "id3.h"
#include "id3_header.h"


/*
 * Function id3_get_url (frame)
 *
 *    Return URL of frame.
 *
 */
char *id3_get_url(id3_frame_t *frame)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'W' )
	return NULL;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return NULL;

    if ( frame->fr_desc->fd_id == ID3_WXXX ) {
	/*
	 * This is a user defined link frame.  Skip the description.
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

    return (char *) frame->fr_data;
}


/*
 * Function id3_get_url_desc (frame)
 *
 *    Get description of a URL.
 *
 */
char *id3_get_url_desc(id3_frame_t *frame)
{
    /* Type check */
    if ( frame->fr_desc->fd_idstr[0] != 'W' )
	return NULL;

    /* If predefined link frame, return description. */
    if ( frame->fr_desc->fd_id != ID3_WXXX )
	return frame->fr_desc->fd_description;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return NULL;

    return (char *) frame->fr_data + 1;
}
