/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-volume-monitor.h - interface for desktop mounting functions.

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

#ifndef NAUTILUS_VOLUME_MONITOR_H
#define NAUTILUS_VOLUME_MONITOR_H

#include <gtk/gtkobject.h>

typedef struct NautilusVolumeMonitor NautilusVolumeMonitor;
typedef struct NautilusVolumeMonitorClass NautilusVolumeMonitorClass;
typedef struct NautilusVolumeMonitorDetails NautilusVolumeMonitorDetails;

#define NAUTILUS_TYPE_VOLUME_MONITOR		(nautilus_volume_monitor_get_type())
#define NAUTILUS_VOLUME_MONITOR(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VOLUME_MONITOR, NautilusVolumeMonitor))
#define NAUTILUS_VOLUME_MONITOR_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VOLUME_MONITOR, NautilusVolumeMonitorClass))
#define IS_NAUTILUS_VOLUME_MONITOR(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VOLUME_MONITOR))

struct NautilusVolumeMonitor {
	GtkObject parent;
	NautilusVolumeMonitorDetails *details;
};

typedef struct NautilusVolume NautilusVolume;

struct NautilusVolumeMonitorClass {
	GtkObjectClass parent_class;

	/* Signals */
	void (* volume_mounted)	  	(NautilusVolumeMonitor	*monitor,
				   	 const NautilusVolume	*volume);
	void (* volume_unmount_started) (NautilusVolumeMonitor	*monitor,
				   	 const NautilusVolume	*volume);
	void (* volume_unmount_failed)	(NautilusVolumeMonitor	*monitor,
				   	 const NautilusVolume	*volume);
	void (* volume_unmounted) 	(NautilusVolumeMonitor	*monitor,
				   	 const NautilusVolume	*volume);
};

typedef enum {
	NAUTILUS_VOLUME_AFFS,
	NAUTILUS_VOLUME_AUTO,
	NAUTILUS_VOLUME_CDDA, 	
	NAUTILUS_VOLUME_CDROM, 	
	NAUTILUS_VOLUME_EXT2,
	NAUTILUS_VOLUME_EXT3,
	NAUTILUS_VOLUME_FAT,
	NAUTILUS_VOLUME_HPFS,
	NAUTILUS_VOLUME_HSFS,
	NAUTILUS_VOLUME_MINIX,
	NAUTILUS_VOLUME_MSDOS,
	NAUTILUS_VOLUME_NFS,
	NAUTILUS_VOLUME_PROC,
	NAUTILUS_VOLUME_REISER,
	NAUTILUS_VOLUME_SMB,
	NAUTILUS_VOLUME_UDF,
	NAUTILUS_VOLUME_UFS,
	NAUTILUS_VOLUME_UNSDOS,
	NAUTILUS_VOLUME_VFAT,
	NAUTILUS_VOLUME_XENIX,
	NAUTILUS_VOLUME_XFS,
	NAUTILUS_VOLUME_XIAFS,
	NAUTILUS_VOLUME_UNKNOWN
} NautilusVolumeType;

typedef enum {
	NAUTILUS_DEVICE_AUDIO_CD,
	NAUTILUS_DEVICE_CAMERA,
	NAUTILUS_DEVICE_CD_ROM_DRIVE,
	NAUTILUS_DEVICE_FLOPPY_DRIVE,
	NAUTILUS_DEVICE_JAZ_DRIVE,
	NAUTILUS_DEVICE_MEMORY_STICK,
	NAUTILUS_DEVICE_NFS,
	NAUTILUS_DEVICE_ZIP_DRIVE,
	NAUTILUS_DEVICE_UNKNOWN
} NautilusDeviceType;

struct NautilusVolume {
	NautilusVolumeType volume_type;
	NautilusDeviceType device_type;
		
	char *device_path;
	char *mount_path;
	char *volume_name;
	char *filesystem;
	
	gboolean is_removable;	
	gboolean is_read_only;
};

typedef gboolean (* NautilusEachVolumeFunction) (const NautilusVolume *, gpointer);

GtkType                	nautilus_volume_monitor_get_type                   	(void);
NautilusVolumeMonitor  	*nautilus_volume_monitor_get                        	(void);
char 			*nautilus_volume_monitor_get_volume_name 		(const NautilusVolume 		*volume);

void               	nautilus_volume_monitor_mount_unmount_removable    	(NautilusVolumeMonitor 		*monitor,
									   	 const char            		*mount_point,
									   	 gboolean			 should_mount);
gboolean		nautilus_volume_monitor_volume_is_mounted 		(NautilusVolumeMonitor 		*monitor,
					   					const NautilusVolume 		*mount_point);
gboolean		nautilus_volume_monitor_volume_is_removable		(const NautilusVolume 		*volume);
gboolean               	nautilus_volume_monitor_is_volume_link             	(const char            		*path);

gboolean		nautilus_volume_monitor_should_integrate_trash		(const NautilusVolume 		*volume);
const char		*nautilus_volume_monitor_get_volume_mount_uri 		(const NautilusVolume 		*volume);
void                   	nautilus_volume_monitor_each_mounted_volume        	(NautilusVolumeMonitor 		*monitor,
									  	 NautilusEachVolumeFunction   	function,
									   	 gpointer               	context);
const GList		*nautilus_volume_monitor_get_removable_volumes 		(NautilusVolumeMonitor 		*monitor);
void			nautilus_volume_monitor_free_volume             	(NautilusVolume             	*volume);
char 			*nautilus_volume_monitor_get_target_uri 		(const NautilusVolume 		*volume);
void			nautilus_volume_monitor_set_volume_name 		(NautilusVolumeMonitor 		*monitor,
										 const NautilusVolume 		*volume,
										 const char 			*volume_name);
char 			*nautilus_volume_monitor_get_mount_name_for_display 	(NautilusVolumeMonitor 		*monitor,
										 NautilusVolume 		*volume);
#endif /* NAUTILUS_VOLUME_MONITOR_H */
