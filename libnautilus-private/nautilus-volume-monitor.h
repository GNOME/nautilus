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

#include <glib.h>
#include <gtk/gtk.h>
#include "nautilus-icon-container.h"

#define NAUTILUS_MOUNT_LINK_KEY	"NAUTILUS_MOUNT_LINK"

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

struct NautilusVolumeMonitorClass {
	GtkObjectClass parent_class;

	/* Signals */
	char *	     (* volume_mounted)	  	(NautilusVolumeMonitor *monitor);
	char *	     (* volume_unmounted)	(NautilusVolumeMonitor *monitor);
};

struct NautilusVolumeMonitorDetails
{
	GHashTable *devices_by_fsname;
	GList *devices;
	guint mount_device_timer_id;
};


#define MOUNT_TYPE_ISO9660 	"iso9660"
#define MOUNT_TYPE_EXT2 	"ext2"
#define MOUNT_OPTIONS_USER 	"user"
#define MOUNT_OPTIONS_OWNER 	"owner"

#define CHECK_INTERVAL 		2000

typedef enum { 
	STATE_ACTIVE = 0, 
	STATE_INACTIVE, 
	STATE_EMPTY, 
	STATE_LAST,
} DeviceState;

typedef enum { 
	DEVICE_CDROM, 
	DEVICE_FLOPPY,
	DEVICE_EXT2,
	DEVICE_OTHER 
} DeviceType;

typedef struct {
	DeviceType type;
	DeviceState state;
	int device_fd;
	
	char *fsname;
	char *mount_path;
	char *mount_type;
	char *volume_name;
	char *link_uri;
	
	gboolean is_mounted;
	gboolean did_mount;
} DeviceInfo;

GtkType               	nautilus_volume_monitor_get_type 		(void);
NautilusVolumeMonitor 	*nautilus_volume_monitor_get 			(void);
GList 			*fm_desktop_get_removable_volume_list 		(void);
gboolean		nautilus_volume_monitor_volume_is_mounted 	(const char		*mount_point);
void			nautilus_volume_monitor_find_mount_devices 	(NautilusVolumeMonitor 	*icon_view);
gboolean		nautilus_volume_monitor_mount_unmount_removable (NautilusVolumeMonitor 	*monitor, 
									 const char 		*mount_point);
gboolean		nautilus_volume_monitor_is_volume_link 		(const char 		*path);

#endif