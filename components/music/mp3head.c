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
int get_bitrate (unsigned char *buf,int bufsize)
{
	int i=0;
	int ver,srindex,brindex,xbytes,xframes;
	float br;
	
	while(!is_mphead(buf+i)) {
		i++;
		if (i>bufsize-4) return(-1);  /* no valid header, give up */
	}	

	ver = (buf[i+1] & 0x08) >> 3;
	brindex = (buf[i+2] & 0xf0) >> 4;
	srindex = (buf[i+2] & 0x0c) >> 2;

	/* but if there is a Xing header we'll use that instead... */	
	i=0;
	while(!is_xhead(buf+i)) {
		i++;
		if (i>bufsize-16) return(bitrates[ver][brindex]);
	}

	xframes = extractI4(buf+i+8);
	xbytes = extractI4(buf+i+12);

	br = (float)samprates[ver][srindex]*xbytes/(576+ver*576)/xframes/125;
	return(br);
}

/* return sample rate from MPEG header */
int get_samprate(unsigned char *buf, int bufsize)
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


static int mp_br_table[2][16] =
	{{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
 	 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0}};
 
static int mp_sr20_table[2][4] = 
	{{441, 480, 320, -999}, {882, 960, 640, -999}};

/* mpeg2 */
static int mp_br_tableL1[2][16] =
	{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},  
	 {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}};

/* mpeg 2 */
static int mp_br_tableL3[2][16] =
	{{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},      
	 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}};


static int 
compare(unsigned char *buf, unsigned char *buf2)
{
	if (buf[0] != buf2[0]) {
      		return 0;
      	}
      	
	if (buf[1] != buf2[1]) {
      		return 0;
      	}
      	
	return 1;
}



/*------------------------------------------------------*/
/*---- scan for next sync, assume start is valid -------*/
/*---- return number bytes to next sync ----------------*/
static int 
sync_scan (unsigned char *buf, int n, int i0)
{
	int i;

	for (i = i0; i < n; i++) {
		if (compare (buf, buf + i)) {
	 		return i;
	 	}
	 }

   	return 0;
}

/*------------------------------------------------------*/
/*- test consecutative syncs, input isync without pad --*/
static int 
sync_test (unsigned char *buf, int n, int isync, int padbytes)
{
	int i, nmatch, pad;

	nmatch = 0;
	for (i = 0;;) {
		pad = padbytes * ((buf[i + 2] & 0x02) >> 1);
		i += (pad + isync);

		if (i > n) {
	 		break;
	 	}
	 	
      		if (!compare(buf, buf + i)) {
	 		return -nmatch;
	 	}
	 	
      		nmatch++;
	}
	
	return nmatch;
}


static int 
find_sync (unsigned char *buf, int n)
{
	int i0, isync, nmatch, pad;
	int padbytes, option;

	/* mod 4/12/95 i0 change from 72, allows as low as 8kbits for mpeg1 */
	i0 = 24;
	padbytes = 1;
	option = (buf[1] & 0x06) >> 1;
	if (option == 3) {
		padbytes = 4;
		i0 = 24;	/* for shorter layer I frames */
   	}

	pad = (buf[2] & 0x02) >> 1;

	n -= 3;			/*  need 3 bytes of header  */

	while (i0 < 2000) {
		isync = sync_scan (buf, n, i0);
		i0 = isync + 1;
		isync -= pad;
		if (isync <= 0) {
	 		return 0;
	 	}
	 	
		nmatch = sync_test (buf, n, isync, padbytes);
		if (nmatch > 0) {
	 		return isync;
	 	}
	}
	
	return 0;
}



int 
get_header_info (unsigned char *buf, unsigned int n, MPEGHeader *h)
{
	int framebytes;
	int mpeg25_flag;
  
	if (n > 10000) {
		/* limit scan for free format */
      		n = 10000;
      	}

	h->sync = 0;
	
   	if ((buf[0] == 0xFF) && ((buf[0+1] & 0xF0) == 0xF0)) {
		/* mpeg 1 & 2 */
		mpeg25_flag = 0;		

   	} else if ((buf[0] == 0xFF) && ((buf[0+1] & 0xF0) == 0xE0)) {
		/* mpeg 2.5 */
		mpeg25_flag = 1;
	} else {
		/* sync fail */
      		return 0;
      	}

	h->sync = 1;
	if (mpeg25_flag) {
		/* low bit clear signals mpeg25 (as in 0xFFE) */
		h->sync = 2;
	}

	h->id = (buf[0+1] & 0x08) >> 3;
	h->option = (buf[0+1] & 0x06) >> 1;
	h->prot = (buf[0+1] & 0x01);

	h->br_index = (buf[0+2] & 0xf0) >> 4;
	h->sr_index = (buf[0+2] & 0x0c) >> 2;
	h->pad = (buf[0+2] & 0x02) >> 1;
	h->private_bit = (buf[0+2] & 0x01);
	h->mode = (buf[0+3] & 0xc0) >> 6;
	h->mode_ext = (buf[0+3] & 0x30) >> 4;
	h->cr = (buf[0+3] & 0x08) >> 3;
	h->original = (buf[0+3] & 0x04) >> 2;
	h->emphasis = (buf[0+3] & 0x03);

	/* compute framebytes for Layer I, II, III */
	if (h->option < 1) {
		return 0;
	}
	
	if (h->option > 3) {
		return 0;
	}

  	framebytes = 0;

	if (h->br_index > 0) {
		if (h->option == 3) {
			/* layer I */
	 		framebytes = 240 * mp_br_tableL1[h->id][h->br_index] / mp_sr20_table[h->id][h->sr_index];
			framebytes = 4 * framebytes;
		} else if (h->option == 2) {
			/* layer II */
			framebytes = 2880 * mp_br_table[h->id][h->br_index] / mp_sr20_table[h->id][h->sr_index];
		} else if (h->option == 1) {
			/* layer III */
			if (h->id) {
				// mpeg1
				framebytes = 2880 * mp_br_tableL3[h->id][h->br_index] / mp_sr20_table[h->id][h->sr_index];
			} else {			
				/* mpeg2 */
				if (mpeg25_flag) {
					/* mpeg2.2 */
	       				framebytes = 2880 * mp_br_tableL3[h->id][h->br_index] / mp_sr20_table[h->id][h->sr_index];
	    			} else {
					framebytes = 1440 * mp_br_tableL3[h->id][h->br_index] / mp_sr20_table[h->id][h->sr_index];
	    			}
			}
		} else {
			/* free format */
			framebytes = find_sync (buf, n);
		}
	}
	
	return framebytes;
}

int 
get_header_info_extended (unsigned char *buf, unsigned int n, MPEGHeader *h, int *bitrate)
{
	int framebytes;

	/*---  return bitrate (in bits/sec) in addition to frame bytes ---*/
	*bitrate = 0;

	/*-- assume fail --*/
   	framebytes = get_header_info (buf, n, h);

	if (framebytes == 0) {
      		return 0; 
	}

	/* layer III */
   	if (h->option == 1) {				
		if (h->br_index > 0) {
			*bitrate = 1000 * mp_br_tableL3[h->id][h->br_index];
		} else {			
			if (h->id) {
				/* mpeg1 */
	    			*bitrate = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (144 * 20);
	 		} else {
				/* mpeg2 */
	    			if ((h->sync & 1) == 0)	{
	    				/* flags mpeg25 */
	       				*bitrate = 500 * framebytes * mp_sr20_table[h->id][h->sr_index] / (72 * 20);
				} else {
	       				*bitrate = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (72 * 20);
	 			}
      			}
   		}
   	}
   	
	/* layer II */
	if (h->option == 2) {	
      		if (h->br_index > 0) {
	 		*bitrate = 1000 * mp_br_table[h->id][h->br_index];
		} else {
	 		*bitrate = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (144 * 20);
   		}
   	}
   	
	/* layer I */
   	if (h->option == 3) {				
      		if (h->br_index > 0) {
	 		*bitrate = 1000 * mp_br_tableL1[h->id][h->br_index];
      		} else {
	 		*bitrate = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (48 * 20);
		}
   	}

	return framebytes;
}

