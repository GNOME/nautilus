/*
 * MPEG Layer3 header info extraction routines
 *
 * Author: Erik Gustavsson, <cyrano@algonet.se>
 *
 * This software is released under the GNU General Public License.
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at you own risk!
 */

#ifndef MP3HEAD_H
#define MP3HEAD_H

/* mpeg audio header   */
typedef struct
{
	int sync;			/* 1 if valid sync */
	int id;
	int option;
	int prot;
	int br_index;
	int sr_index;
	int pad;
	int private_bit;
	int mode;
	int mode_ext;
	int cr;
	int original;
	int emphasis;
} MPEGHeader;


/* ID3v2 Tag Header */
typedef struct {
	char id_str[3];
	unsigned char version[2];
	unsigned char flags[1];
	unsigned char size[4];
} ID3V2Header;


int get_bitrate(unsigned char *buf,int bufsize);
int get_samprate(unsigned char *buf,int bufsize);
int get_mpgver(unsigned char *buf,int bufsize);
int get_stereo(unsigned char *buf,int bufsize);

int get_header_info (unsigned char *buf, unsigned int n, MPEGHeader *h);
int get_header_info_extended (unsigned char *buf, unsigned int n, MPEGHeader *h, int *bitrate);

#endif

