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
	void (* volume_mounted)	  (NautilusVolumeMonitor 	 *monitor,
				   const NautilusVolume      	 *volume);
	void (* volume_unmounted) (NautilusVolumeMonitor 	 *monitor,
				   const NautilusVolume      	 *volume);
};

#define NAUTILUS_MOUNT_TYPE_ISO9660 	"iso9660"
#define NAUTILUS_MOUNT_TYPE_EXT2 	"ext2"
#define NAUTILUS_MOUNT_OPTIONS_USER 	"user"
#define NAUTILUS_MOUNT_OPTIONS_OWNER 	"owner"

#define NAUTILUS_CHECK_INTERVAL 	2000

typedef enum { 
	STATE_ACTIVE = 0, 
	STATE_INACTIVE, 
	STATE_EMPTY, 
	STATE_LAST,
} NautilusVolumeState;

typedef enum { 
	VOLUME_CDROM, 
	VOLUME_FLOPPY,
	VOLUME_EXT2,
	VOLUME_OTHER 
} NautilusVolumeType;

struct NautilusVolume {
	NautilusVolumeType type;
	NautilusVolumeState state;
	int volume_fd;
	
	char *fsname;
	char *mount_path;
	char *mount_type;
	char *volume_name;
	
	gboolean is_mounted;
	gboolean did_mount;
	
	gboolean is_read_only;
};

typedef gboolean (* NautilusEachVolumeFunction) (const NautilusVolume *, gpointer);

GtkType                nautilus_volume_monitor_get_type                   (void);
NautilusVolumeMonitor *nautilus_volume_monitor_get                        (void);
gboolean               nautilus_volume_monitor_volume_is_mounted          (const char            	*mount_point);
void                   nautilus_volume_monitor_find_volumes         	  (NautilusVolumeMonitor 	*monitor);
gboolean               nautilus_volume_monitor_mount_unmount_removable    (NautilusVolumeMonitor 	*monitor,
									   const char            	*mount_point);
gboolean               nautilus_volume_monitor_is_volume_link             (const char            	*path);
void                   nautilus_volume_monitor_each_volume                (NautilusVolumeMonitor 	*monitor,
									   NautilusEachVolumeFunction  	function,
									   gpointer               	context);
void                   nautilus_volume_monitor_each_mounted_volume        (NautilusVolumeMonitor 	*monitor,
									   NautilusEachVolumeFunction   function,
									   gpointer               	context);
GList *                nautilus_volume_monitor_get_removable_volume_names (void);

#endif /* NAUTILUS_VOLUME_MONITOR_H */
