/*********************************************************************
 * 
 *    Copyright (C) 1999,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3_frame_content.c
 * Description:   Code for handling ID3 content frames.
 * Author:        Espen Skoglund <espensk@stud.cs.uit.no>
 * Created at:    Mon Feb  8 17:13:46 1999
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
#include "mpg123.h"

#include "id3.h"


/*
 * Function id3_get_content (frame)
 *
 *    Expand content type string of frame and return it.  Return NULL
 *    upon error.
 *
 */
char *id3_get_content(id3_frame_t *frame)
{
    char *text, *ptr;
    char *buffer = frame->fr_owner->id3_buffer;
    int spc = sizeof( frame->fr_owner->id3_buffer ) - 1;

    /* Type check */
    if ( frame->fr_desc->fd_id != ID3_TCON )
	return NULL;

    /* Check if frame is compressed */
    if ( frame->fr_data_z && !frame->fr_data )
	if ( id3_decompress_frame( frame ) == -1 )
	    return NULL;
    
    text = (char *) frame->fr_data + 1;

    /*
     * If content is just plain text, return it.
     */
    if ( text[0] != '(' )
	return text;

    /*
     * Expand ID3v1 genre numbers.
     */
    ptr = buffer;
    while ( text[0] == '(' && text[1] != '(' && spc > 0 ) {
	char *genre;
	int num = 0;

	if ( text[1] == 'R' && text[2] == 'X' ) {
	    text += 4;
	    genre = ptr != buffer ? " (Remix)" : "(Remix)";

	} else if ( text[1] == 'C' && text[2] == 'R' ) {
	    text += 4;
	    genre = ptr != buffer ? " (Cover)" : "(Cover)";

	} else {
	    /* Get ID3v1 genre number */
	    text++;
	    while ( *text != ')' ) {
		num *= 10;
		num += *text++ - '0';
	    }
	    text++;

	    /* Boundary check */
	    if ( num >= (int)(sizeof(mpg123_id3_genres) / sizeof(char *)) )
		continue;

	    genre = (char *)mpg123_id3_genres[num];

	    if ( ptr != buffer && spc-- > 0 )
		*ptr++ = '/';
	}
	
	/* Expand string into buffer */
	while ( *genre != '\0' && spc > 0 ) {
	    *ptr++ = *genre++;
	    spc--;
	}
    }

    /*
     * Add plaintext refinement.
     */
    if ( *text == '(' )
	text++;
    if ( *text != '\0' && ptr != buffer && spc-- > 0 )
	*ptr++ = ' ';
    while ( *text != '\0' && spc > 0 ) {
	*ptr++ = *text++;
	spc--;
    }
    *ptr = '\0';


    /*
     * Return the expanded content string.
     */
    return buffer;
}
