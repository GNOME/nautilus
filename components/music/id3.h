/*********************************************************************
 * 
 *    Copyright (C) 1998, 1999,  Espen Skoglund
 *    Department of Computer Science, University of Tromsø
 * 
 * Filename:      id3.h
 * Description:   Include file for accessing the ID3 library.
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
#ifndef ID3_H
#define ID3_H

#include <glib/gtypes.h>

/*
 * Option flags to id3_open_*().
 */
#define ID3_OPENF_NONE		0x0000
#define ID3_OPENF_NOCHK		0x0001
#define ID3_OPENF_CREATE	0x0002


/*
 * The size of the read buffer used by file operations.
 */
#define ID3_FD_BUFSIZE	8192


typedef struct id3_t		id3_t;
typedef struct id3_frame_t	id3_frame_t;
typedef struct id3_framedesc_t	id3_framedesc_t;


/*
 * Structure describing the ID3 tag.
 */
struct id3_t {
    int		id3_type;		/* Memory or file desriptor */
    int		id3_oflags;		/* Flags from open call */
    int		id3_flags;		/* Flags from tag header */
    int		id3_altered;		/* Set when tag has been altered */
    int		id3_newtag;		/* Set if this is a new tag */

    int		id3_version;		/* Major ID3 version number */
    int		id3_revision;		/* ID3 revision number */

    int		id3_tagsize;		/* Total size of ID3 tag */
    int		id3_pos;		/* Current position within tag */
    
    char	*id3_error_msg;		/* Last error message */

    char	id3_buffer[256];	/* Used for various strings */

    union {
	/*
	 * Memory specific fields.
	 */
	struct {
	    void	*id3_ptr;
	} me;

	/*
	 * File desc. specific fields.
	 */
	struct {
	    int		id3_fd;
	    void	*id3_buf;
	} fd;

	/*
	 * File ptr. specific fields.
	 */
	struct {
	    FILE	*id3_fp;
	    void	*id3_buf;
	} fp;
    } s;

    /*
     * Functions for doing operations within ID3 tag.
     */
    int (*id3_seek)(id3_t *, int);
    void *(*id3_read)(id3_t *, void *, int);

    /*
     * Linked list of ID3 frames.
     */
    int		id3_numframes;
    id3_frame_t	*id3_frame;
    id3_frame_t	*id3_tail;
};

#define ID3_TYPE_NONE	0
#define ID3_TYPE_MEM	1
#define ID3_TYPE_FD	2
#define ID3_TYPE_FP	3


/*
 * Structure describing an ID3 frame.
 */
struct id3_frame_t {
    id3_t		*fr_owner;
    id3_framedesc_t	*fr_desc;
    int			fr_flags;
    unsigned char	fr_encryption;
    unsigned char	fr_grouping;
    unsigned char	fr_altered;

    void		*fr_data;	/* The uncompressed frame */
    int			fr_size;	/* Size of uncompressed frame */

    void		*fr_data_z;	/* The compressed frame */
    int			fr_size_z;	/* Size of compressed frame */
 
    id3_frame_t		*fr_next;
};


/*
 * Structure describing an ID3 frame type.
 */
struct id3_framedesc_t {
    guint32	fd_id;
    char	fd_idstr[4];
    char	*fd_description;
};


/*
 * Text encodings.
 */
#define ID3_ENCODING_ISO_8859_1	0x00
#define ID3_ENCODING_UNICODE	0x01



/*
 * ID3 frame id numbers.
 */
#define ID3_FRAME_ID(a,b,c,d)   ((a << 24) | (b << 16) | (c << 8) | d)

#define ID3_AENC	ID3_FRAME_ID('A','E','N','C')
#define ID3_APIC	ID3_FRAME_ID('A','P','I','C')
#define ID3_COMM	ID3_FRAME_ID('C','O','M','M')
#define ID3_COMR	ID3_FRAME_ID('C','O','M','R')
#define ID3_ENCR	ID3_FRAME_ID('E','N','C','R')
#define ID3_EQUA	ID3_FRAME_ID('E','Q','U','A')
#define ID3_ETCO	ID3_FRAME_ID('E','T','C','O')
#define ID3_GEOB	ID3_FRAME_ID('G','E','O','B')
#define ID3_GRID	ID3_FRAME_ID('G','R','I','D')
#define ID3_IPLS	ID3_FRAME_ID('I','P','L','S')
#define ID3_LINK	ID3_FRAME_ID('L','I','N','K')
#define ID3_MCDI	ID3_FRAME_ID('M','C','D','I')
#define ID3_MLLT	ID3_FRAME_ID('M','L','L','T')
#define ID3_OWNE	ID3_FRAME_ID('O','W','N','E')
#define ID3_PRIV	ID3_FRAME_ID('P','R','I','V')
#define ID3_PCNT	ID3_FRAME_ID('P','C','N','T')
#define ID3_POPM	ID3_FRAME_ID('P','O','P','M')
#define ID3_POSS	ID3_FRAME_ID('P','O','S','S')
#define ID3_RBUF	ID3_FRAME_ID('R','B','U','F')
#define ID3_RVAD	ID3_FRAME_ID('R','V','A','D')
#define ID3_RVRB	ID3_FRAME_ID('R','V','R','B')
#define ID3_SYLT	ID3_FRAME_ID('S','Y','L','T')
#define ID3_SYTC	ID3_FRAME_ID('S','Y','T','C')
#define ID3_TALB	ID3_FRAME_ID('T','A','L','B')
#define ID3_TBPM	ID3_FRAME_ID('T','B','P','M')
#define ID3_TCOM	ID3_FRAME_ID('T','C','O','M')
#define ID3_TCON	ID3_FRAME_ID('T','C','O','N')
#define ID3_TCOP	ID3_FRAME_ID('T','C','O','P')
#define ID3_TDAT	ID3_FRAME_ID('T','D','A','T')
#define ID3_TDLY	ID3_FRAME_ID('T','D','L','Y')
#define ID3_TENC	ID3_FRAME_ID('T','E','N','C')
#define ID3_TEXT	ID3_FRAME_ID('T','E','X','T')
#define ID3_TFLT	ID3_FRAME_ID('T','F','L','T')
#define ID3_TIME	ID3_FRAME_ID('T','I','M','E')
#define ID3_TIT1	ID3_FRAME_ID('T','I','T','1')
#define ID3_TIT2	ID3_FRAME_ID('T','I','T','2')
#define ID3_TIT3	ID3_FRAME_ID('T','I','T','3')
#define ID3_TKEY	ID3_FRAME_ID('T','K','E','Y')
#define ID3_TLAN	ID3_FRAME_ID('T','L','A','N')
#define ID3_TLEN	ID3_FRAME_ID('T','L','E','N')
#define ID3_TMED	ID3_FRAME_ID('T','M','E','D')
#define ID3_TOAL	ID3_FRAME_ID('T','O','A','L')
#define ID3_TOFN	ID3_FRAME_ID('T','O','F','N')
#define ID3_TOLY	ID3_FRAME_ID('T','O','L','Y')
#define ID3_TOPE	ID3_FRAME_ID('T','O','P','E')
#define ID3_TORY	ID3_FRAME_ID('T','O','R','Y')
#define ID3_TOWN	ID3_FRAME_ID('T','O','W','N')
#define ID3_TPE1	ID3_FRAME_ID('T','P','E','1')
#define ID3_TPE2	ID3_FRAME_ID('T','P','E','2')
#define ID3_TPE3	ID3_FRAME_ID('T','P','E','3')
#define ID3_TPE4	ID3_FRAME_ID('T','P','E','4')
#define ID3_TPOS	ID3_FRAME_ID('T','P','O','S')
#define ID3_TPUB	ID3_FRAME_ID('T','P','U','B')
#define ID3_TRCK	ID3_FRAME_ID('T','R','C','K')
#define ID3_TRDA	ID3_FRAME_ID('T','R','D','A')
#define ID3_TRSN	ID3_FRAME_ID('T','R','S','N')
#define ID3_TRSO	ID3_FRAME_ID('T','R','S','O')
#define ID3_TSIZ	ID3_FRAME_ID('T','S','I','Z')
#define ID3_TSRC	ID3_FRAME_ID('T','S','R','C')
#define ID3_TSSE	ID3_FRAME_ID('T','S','S','E')
#define ID3_TYER	ID3_FRAME_ID('T','Y','E','R')
#define ID3_TXXX	ID3_FRAME_ID('T','X','X','X')
#define ID3_UFID	ID3_FRAME_ID('U','F','I','D')
#define ID3_USER	ID3_FRAME_ID('U','S','E','R')
#define ID3_USLT	ID3_FRAME_ID('U','S','L','T')
#define ID3_WCOM	ID3_FRAME_ID('W','C','O','M')
#define ID3_WCOP	ID3_FRAME_ID('W','C','O','P')
#define ID3_WOAF	ID3_FRAME_ID('W','O','A','F')
#define ID3_WOAR	ID3_FRAME_ID('W','O','A','R')
#define ID3_WOAS	ID3_FRAME_ID('W','O','A','S')
#define ID3_WORS	ID3_FRAME_ID('W','O','R','S')
#define ID3_WPAY	ID3_FRAME_ID('W','P','A','Y')
#define ID3_WPUB	ID3_FRAME_ID('W','P','U','B')
#define ID3_WXXX	ID3_FRAME_ID('W','X','X','X')



/*
 * Prototypes.
 */

/* From id3.c */
id3_t *id3_open_mem(void *, int);
id3_t *id3_open_fd(int, int);
id3_t *id3_open_fp(FILE *, int);
int id3_set_output(id3_t *, char *);
int id3_close(id3_t *);
int id3_tell(id3_t *);
int id3_alter_file(id3_t *);
int id3_write_tag(id3_t *, int);

/* From id3_frame.c */
int id3_read_frame(id3_t *id3);
id3_frame_t *id3_get_frame(id3_t *, guint32, int);
int id3_delete_frame(id3_frame_t *frame);
id3_frame_t *id3_add_frame(id3_t *, guint32);
int id3_decompress_frame(id3_frame_t *);

/* From id3_frame_text.c */
gint8 id3_get_encoding(id3_frame_t *);
int id3_set_encoding(id3_frame_t *, gint8);
char *id3_get_text(id3_frame_t *);
char *id3_get_text_desc(id3_frame_t *);
int id3_get_text_number(id3_frame_t *);
int id3_set_text(id3_frame_t *, char *);
int id3_set_text_number(id3_frame_t *, int);

/* From id3_frame_content.c */
char *id3_get_content(id3_frame_t *);

/* From id3_frame_url.c */
char *id3_get_url(id3_frame_t *);
char *id3_get_url_desc(id3_frame_t *);

/* From id3_tag.c */
void id3_init_tag(id3_t *id3);
int id3_read_tag(id3_t *id3);

#endif /* ID3_H */
