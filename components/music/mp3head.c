/*
 * MPEG Layer3 header info extraction routines
 *
 * Author: Erik Gustavsson, <cyrano@algonet.se>
 *
 * This software is released under the GNU General Public License.
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at you own risk!
 */

#include "mp3head.h"

int bitrates[2][16] = {
{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},          /* MPEG2 */
{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,}}; /* MPEG1 */

long samprates[2][3] = {
{ 22050, 24000, 16000 },  /* MPEG2 */
{ 44100, 48000, 32000 }}; /* MPEG1 */




static int extractI4(unsigned char *buf)
{
	int x;
	/* big endian extract */

	x = buf[0];
	x <<= 8;
	x |= buf[1];
	x <<= 8;
	x |= buf[2];
	x <<= 8;
	x |= buf[3];

	return(x);
}

/* check for valid MPEG header */
static int is_mphead(unsigned char *buf)
{
	if (buf[0] != 0xff) return(0);		
	if ((buf[1] & 0xf0) != 0xf0) return(0);	 /* 12 bits framesync */
	
	return(1); 
}

/* check for valid "Xing" VBR header */
static int is_xhead(unsigned char *buf)
{
	if (buf[0] != 'X') return(0);
	if (buf[1] != 'i') return(0);
	if (buf[2] != 'n') return(0);
	if (buf[3] != 'g') return(0);
	
	return(1);
}

/* return bitrate from MPEG header, average rate for VBR files or -1 on error */
int get_bitrate(unsigned char *buf,int bufsize)
{
	int i=0;
	int ver,srindex,brindex,xbytes,xframes;
	float br;
	
	while(!is_mphead(buf+i))
	{
		i++;
		if (i>bufsize-4) return(-1);  /* no valid header, give up */
	}	

	ver = (buf[i+1] & 0x08) >> 3;
	brindex = (buf[i+2] & 0xf0) >> 4;
	srindex = (buf[i+2] & 0x0c) >> 2;

	/* but if there is a Xing header we'll use that instead... */
	
	i=0;
	while(!is_xhead(buf+i))
	{
		i++;
		if (i>bufsize-16) return(bitrates[ver][brindex]);
	}

	xframes = extractI4(buf+i+8);
	xbytes = extractI4(buf+i+12);

	br = (float)samprates[ver][srindex]*xbytes/(576+ver*576)/xframes/125;
	return(br);
}

/* return sample rate from MPEG header */
int get_samprate(unsigned char *buf,int bufsize)
{
	int i=0;
	int ver,srindex;
	
	while(!is_mphead(buf+i))
	{
		i++;
		if (i>bufsize-4) return(-1);  /* no valid header, give up */
	}	

	ver = (buf[i+1] & 0x08) >> 3;
	srindex = (buf[i+2] & 0x0c) >> 2;

	return(samprates[ver][srindex]);
}


/* return 1 for MPEG1, 0 for MPEG2, -1 for error */
int get_mpgver(unsigned char *buf,int bufsize)
{
	int i=0;
	int ver;
	
	while(!is_mphead(buf+i))
	{
		i++;
		if (i>bufsize-4) return(-1);  /* no valid header, give up */
	}	

	ver = (buf[i+1] & 0x08) >> 3;

	return(ver);
}

/* return 0=mono, 1=dual ch, 2=joint stereo, 3=stereo */
int get_stereo(unsigned char *buf,int bufsize)
{
	int i=0;
	int st;
	
	while(!is_mphead(buf+i))
	{
		i++;
		if (i>bufsize-4) return(-1);  /* no valid header, give up */
	}	

	st = (buf[i+3] & 0xC0) >> 6;

	return(3-st);
}

