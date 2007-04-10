/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-cdrom-extension.c - CDROM handliong constants copied from <linux/cdrom.h>.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Gene Z. Ragan <gzr@eazel.com>
*/

#ifndef FM_CDROM_EXTENSIONS_H
#define FM_CDROM_EXTENSIONS_H

#include <sys/types.h>

#ifdef HAVE_SYS_CDIO_H

#include <sys/cdio.h>

#else

/*******************************************************
 * The CD-ROM IOCTL commands  -- these should be supported by 
 * all the various cdrom drivers.  For the CD-ROM ioctls, we 
 * will commandeer byte 0x53, or 'S'.
 *******************************************************/
#define CDROMPAUSE		0x5301 /* Pause Audio Operation */ 
#define CDROMRESUME		0x5302 /* Resume paused Audio Operation */
#define CDROMPLAYMSF		0x5303 /* Play Audio MSF (struct cdrom_msf) */
#define CDROMPLAYTRKIND		0x5304 /* Play Audio Track/index 
                                           (struct cdrom_ti) */
#define CDROMREADTOCHDR		0x5305 /* Read TOC header 
                                           (struct cdrom_tochdr) */
#define CDROMREADTOCENTRY	0x5306 /* Read TOC entry 
                                           (struct cdrom_tocentry) */
#define CDROMSTOP		0x5307 /* Stop the cdrom drive */
#define CDROMSTART		0x5308 /* Start the cdrom drive */
#define CDROMEJECT		0x5309 /* Ejects the cdrom media */
#define CDROMVOLCTRL		0x530a /* Control output volume 
                                           (struct cdrom_volctrl) */
#define CDROMSUBCHNL		0x530b /* Read subchannel data 
                                           (struct cdrom_subchnl) */
#define CDROMREADMODE2		0x530c /* Read CDROM mode 2 data (2336 Bytes) 
                                           (struct cdrom_read) */
#define CDROMREADMODE1		0x530d /* Read CDROM mode 1 data (2048 Bytes)
                                           (struct cdrom_read) */
#define CDROMREADAUDIO		0x530e /* (struct cdrom_read_audio) */
#define CDROMEJECT_SW		0x530f /* enable(1)/disable(0) auto-ejecting */
#define CDROMMULTISESSION	0x5310 /* Obtain the start-of-last-session 
                                           address of multi session disks 
                                           (struct cdrom_multisession) */
#define CDROM_GET_MCN		0x5311 /* Obtain the "Universal Product Code" 
                                           if available (struct cdrom_mcn) */
#define CDROM_GET_UPC		CDROM_GET_MCN  /* This one is depricated, 
                                          but here anyway for compatability */
#define CDROMRESET		0x5312 /* hard-reset the drive */
#define CDROMVOLREAD		0x5313 /* Get the drive's volume setting 
                                          (struct cdrom_volctrl) */
#define CDROMREADRAW		0x5314	/* read data in raw mode (2352 Bytes)
                                           (struct cdrom_read) */
/* 
 * These ioctls are used only used in aztcd.c and optcd.c
 */
#define CDROMREADCOOKED		0x5315	/* read data in cooked mode */
#define CDROMSEEK		0x5316  /* seek msf address */
  
/*
 * This ioctl is only used by the scsi-cd driver.  
   It is for playing audio in logical block addressing mode.
 */
#define CDROMPLAYBLK		0x5317	/* (struct cdrom_blk) */

/* 
 * These ioctls are only used in optcd.c
 */
#define CDROMREADALL		0x5318	/* read all 2646 bytes */



#endif /* HAVE_SYS_CDIO_H */


/* 
 * These ioctls are (now) only in ide-cd.c for controlling 
 * drive spindown time.  They should be implemented in the
 * Uniform driver, via generic packet commands, GPCMD_MODE_SELECT_10,
 * GPCMD_MODE_SENSE_10 and the GPMODE_POWER_PAGE...
 *  -Erik
 */
#define CDROMGETSPINDOWN        0x531d
#define CDROMSETSPINDOWN        0x531e

/* 
 * These ioctls are implemented through the uniform CD-ROM driver
 * They _will_ be adopted by all CD-ROM drivers, when all the CD-ROM
 * drivers are eventually ported to the uniform CD-ROM driver interface.
 */
#define CDROMCLOSETRAY		0x5319	/* pendant of CDROMEJECT */
#define CDROM_SET_OPTIONS	0x5320  /* Set behavior options */
#define CDROM_CLEAR_OPTIONS	0x5321  /* Clear behavior options */
#define CDROM_SELECT_SPEED	0x5322  /* Set the CD-ROM speed */
#define CDROM_SELECT_DISC	0x5323  /* Select disc (for juke-boxes) */
#define CDROM_MEDIA_CHANGED	0x5325  /* Check is media changed  */
#define CDROM_DRIVE_STATUS	0x5326  /* Get tray position, etc. */
#define CDROM_DISC_STATUS	0x5327  /* Get disc type, etc. */
#define CDROM_CHANGER_NSLOTS    0x5328  /* Get number of slots */
#define CDROM_LOCKDOOR		0x5329  /* lock or unlock door */
#define CDROM_DEBUG		0x5330	/* Turn debug messages on/off */
#define CDROM_GET_CAPABILITY	0x5331	/* get capabilities */

/* This ioctl is only used by sbpcd at the moment */
#define CDROMAUDIOBUFSIZ        0x5382	/* set the audio buffer size */

/* DVD-ROM Specific ioctls */
#define DVD_READ_STRUCT		0x5390  /* Read structure */
#define DVD_WRITE_STRUCT	0x5391  /* Write structure */
#define DVD_AUTH		0x5392  /* Authentication */

#define CDROM_SEND_PACKET	0x5393	/* send a packet to the drive */
#define CDROM_NEXT_WRITABLE	0x5394	/* get next writable block */
#define CDROM_LAST_WRITTEN	0x5395	/* get last block written on disc */

/* Some generally useful CD-ROM information -- mostly based on the above */
#define CD_MINS              74 /* max. minutes per CD, not really a limit */
#define CD_SECS              60 /* seconds per minute */
#define CD_FRAMES            75 /* frames per second */
#define CD_SYNC_SIZE         12 /* 12 sync bytes per raw data frame */
#define CD_MSF_OFFSET       150 /* MSF numbering offset of first frame */
#define CD_CHUNK_SIZE        24 /* lowest-level "data bytes piece" */
#define CD_NUM_OF_CHUNKS     98 /* chunks per frame */
#define CD_FRAMESIZE_SUB     96 /* subchannel data "frame" size */
#define CD_HEAD_SIZE          4 /* header (address) bytes per raw data frame */
#define CD_SUBHEAD_SIZE       8 /* subheader bytes per raw XA data frame */
#define CD_EDC_SIZE           4 /* bytes EDC per most raw data frame types */
#define CD_ZERO_SIZE          8 /* bytes zero per yellow book mode 1 frame */
#define CD_ECC_SIZE         276 /* bytes ECC per most raw data frame types */
#define CD_FRAMESIZE       2048 /* bytes per frame, "cooked" mode */
#define CD_FRAMESIZE_RAW   2352 /* bytes per frame, "raw" mode */
#define CD_FRAMESIZE_RAWER 2646 /* The maximum possible returned bytes */ 
/* most drives don't deliver everything: */
#define CD_FRAMESIZE_RAW1 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE) /*2340*/
#define CD_FRAMESIZE_RAW0 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE-CD_HEAD_SIZE) /*2336*/

#define CD_XA_HEAD        (CD_HEAD_SIZE+CD_SUBHEAD_SIZE) /* "before data" part of raw XA frame */
#define CD_XA_TAIL        (CD_EDC_SIZE+CD_ECC_SIZE) /* "after data" part of raw XA frame */
#define CD_XA_SYNC_HEAD   (CD_SYNC_SIZE+CD_XA_HEAD) /* sync bytes + header of XA frame */

/* CD-ROM address types (cdrom_tocentry.cdte_format) */
#define	CDROM_LBA 0x01 /* "logical block": first frame is #0 */
#define	CDROM_MSF 0x02 /* "minute-second-frame": binary, not bcd here! */

/* bit to tell whether track is data or audio (cdrom_tocentry.cdte_ctrl) */
#define	CDROM_DATA_TRACK	0x04

/* The leadout track is always 0xAA, regardless of # of tracks on disc */
#define	CDROM_LEADOUT		0xAA

/* audio states (from SCSI-2, but seen with other drives, too) */
#define	CDROM_AUDIO_INVALID	0x00	/* audio status not supported */
#define	CDROM_AUDIO_PLAY	0x11	/* audio play operation in progress */
#define	CDROM_AUDIO_PAUSED	0x12	/* audio play operation paused */
#define	CDROM_AUDIO_COMPLETED	0x13	/* audio play successfully completed */
#define	CDROM_AUDIO_ERROR	0x14	/* audio play stopped due to error */
#define	CDROM_AUDIO_NO_STATUS	0x15	/* no current audio status to return */

/* capability flags used with the uniform CD-ROM driver */ 
#define CDC_CLOSE_TRAY		0x1     /* caddy systems _can't_ close */
#define CDC_OPEN_TRAY		0x2     /* but _can_ eject.  */
#define CDC_LOCK		0x4     /* disable manual eject */
#define CDC_SELECT_SPEED 	0x8     /* programmable speed */
#define CDC_SELECT_DISC		0x10    /* select disc from juke-box */
#define CDC_MULTI_SESSION 	0x20    /* read sessions>1 */
#define CDC_MCN			0x40    /* Medium Catalog Number */
#define CDC_MEDIA_CHANGED 	0x80    /* media changed */
#define CDC_PLAY_AUDIO		0x100   /* audio functions */
#define CDC_RESET               0x200   /* hard reset device */
#define CDC_IOCTLS              0x400   /* driver has non-standard ioctls */
#define CDC_DRIVE_STATUS        0x800   /* driver implements drive status */
#define CDC_GENERIC_PACKET	0x1000	/* driver implements generic packets */
#define CDC_CD_R		0x2000	/* drive is a CD-R */
#define CDC_CD_RW		0x4000	/* drive is a CD-RW */
#define CDC_DVD			0x8000	/* drive is a DVD */
#define CDC_DVD_R		0x10000	/* drive can write DVD-R */
#define CDC_DVD_RAM		0x20000	/* drive can write DVD-RAM */

/* drive status possibilities returned by CDROM_DRIVE_STATUS ioctl */
#define CDS_NO_INFO		0	/* if not implemented */
#define CDS_NO_DISC		1
#define CDS_TRAY_OPEN		2
#define CDS_DRIVE_NOT_READY	3
#define CDS_DISC_OK		4

/* return values for the CDROM_DISC_STATUS ioctl */
/* can also return CDS_NO_[INFO|DISC], from above */
#define CDS_AUDIO		100
#define CDS_DATA_1		101
#define CDS_DATA_2		102
#define CDS_XA_2_1		103
#define CDS_XA_2_2		104
#define CDS_MIXED		105

/* User-configurable behavior options for the uniform CD-ROM driver */
#define CDO_AUTO_CLOSE		0x1     /* close tray on first open() */
#define CDO_AUTO_EJECT		0x2     /* open tray on last release() */
#define CDO_USE_FFLAGS		0x4     /* use O_NONBLOCK information on open */
#define CDO_LOCK		0x8     /* lock tray on open files */
#define CDO_CHECK_TYPE		0x10    /* check type on open for data */

/* Special codes used when specifying changer slots. */
#define CDSL_NONE       	((int) (~0U>>1)-1)
#define CDSL_CURRENT    	((int) (~0U>>1))

/* For partition based multisession access. IDE can handle 64 partitions
 * per drive - SCSI CD-ROM's use minors to differentiate between the
 * various drives, so we can't do multisessions the same way there.
 * Use the -o session=x option to mount on them.
 */
#define CD_PART_MAX		64
#define CD_PART_MASK		(CD_PART_MAX - 1)

#endif  /* FM_CDROM_EXTENSIONS_H */
