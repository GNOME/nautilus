/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-mounting.h - interface for desktop mounting functions.

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

#ifndef FM_DESKSTOP_MOUTING
#define FM_DESKSTOP_MOUTING

#include "fm-desktop-icon-view.h"

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

GList 		*fm_desktop_get_removable_list 		(void);
void		fm_desktop_remove_mount_links		(DeviceInfo             *device,
						 	 FMDesktopIconView      *icon_view);
void		fm_desktop_place_home_directory		(FMDesktopIconView      *icon_view);
void		fm_desktop_rescan_floppy 		(GtkMenuItem 		*item, 
						 	 FMDirectoryView 	*view);
void		fm_desktop_free_device_info             (DeviceInfo             *device,
						 	 FMDesktopIconView      *icon_view);
void		fm_desktop_find_mount_devices        	(FMDesktopIconView      *icon_view,
						 	 const char             *fstab_path);
void		fm_desktop_mount_unmount_removable 	(GtkMenuItem 		*item);
gboolean	fm_desktop_volume_is_mounted 		(const char 		*mount_point);

#endif