/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-mounting.c - Desktop volume mounting routines.

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

#include <config.h>
#include "fm-desktop-mounting.h"

#include "fm-cdrom-extensions.h"
#include "fm-desktop-icon-view.h"
#include "fm-icon-view.h"
#include "iso9660.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <mntent.h>
#include <libnautilus-extensions/nautilus-directory-private.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

const char * const state_names[] = { 
	"ACTIVE", 
	"INACTIVE", 
	"EMPTY" 
};

const char * const type_names[] = { 
	"CDROM", 
	"FLOPPY",
	"LOCAL_DISK",
	"OTHER" 
};


static void     remove_mount_link                                         (DeviceInfo             *device);								  
static void     get_iso9660_volume_name                                   (DeviceInfo             *device);
static void     get_ext2_volume_name                                      (DeviceInfo             *device);
static void     get_floppy_volume_name                                    (DeviceInfo             *device);
static void     mount_device_mount                                        (FMDesktopIconView      *view,
									   DeviceInfo             *device);
static gboolean mount_device_is_mounted                                   (DeviceInfo             *device);
static void     mount_device_deactivate                                   (FMDesktopIconView      *icon_view,
									   DeviceInfo             *device);
static void     mount_device_activate_floppy                              (FMDesktopIconView      *view,
									   DeviceInfo             *device);

void 
fm_desktop_rescan_floppy (GtkMenuItem *item, FMDirectoryView *view)
{
	char *argv[3];
	FMDesktopIconView *icon_view;
	GList *element;
	DeviceInfo *device;

	icon_view = FM_DESKTOP_ICON_VIEW (view);
	
	/* Locate floppy in device */
	for (element = icon_view->details->devices; element != NULL; element = element->next) {
		device = element->data;
		if (strncmp ("/dev/fd", device->fsname, strlen("/dev/fd")) == 0) {
			/* FIXME: Remove messages when this code is done. */
			g_message ("Mounting floppy: %s", device->mount_path);

			argv[1] = device->mount_path;
			argv[2] = NULL;

			/* Unmount the device if needed */
			if (mount_device_is_mounted (device)) {
				argv[0] = "/bin/umount";
				gnome_execute_async (g_get_home_dir(), 2, argv);

				mount_device_deactivate (icon_view, device);
			}
			
			/* Mount the device */
			mount_device_activate_floppy (icon_view, device);
		}
	}    	
}

static gboolean
mount_device_is_mounted (DeviceInfo *device)
{
	return device->is_mounted;
}

static void
mount_device_cdrom_set_state (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	if (device->device_fd < 0) {
		device->device_fd = open (device->fsname, O_RDONLY|O_NONBLOCK);
	}

	if (ioctl (device->device_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK) {
		int disctype;

		disctype = ioctl (device->device_fd, CDROM_DISC_STATUS, CDSL_CURRENT);
		switch (disctype) {
			case CDS_AUDIO:
				/* It's pretty hard to know whether it is actually in use */
				device->state = STATE_INACTIVE;
				break;

			case CDS_DATA_1:
			case CDS_DATA_2:
			case CDS_XA_2_1:
			case CDS_XA_2_2:
			case CDS_MIXED:
				/* Check if it is mounted */
				if (mount_device_is_mounted (device)) {
					device->state = STATE_ACTIVE;
				} else {
					device->state = STATE_INACTIVE;
				}
				break;

			default:
				device->state = STATE_EMPTY;
				break;
		}
	} else {
		device->state = STATE_EMPTY;
	}

	if(device->device_fd >= 0) {
		close (device->device_fd);
		device->device_fd = -1;
	}
}


static void
mount_device_floppy_set_state (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	int fd, err;

  	fd = open (device->fsname, O_RDONLY);

  	if(fd < 0) {
    		err = errno;

    		if (err == EBUSY) {
      			device->state = STATE_ACTIVE;
    		} else {
      			device->state = STATE_EMPTY;
      		}
  	} else {
		close (fd);
		device->state = STATE_INACTIVE;
  	}
}

static void
mount_device_ext2_set_state (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	device->state = STATE_ACTIVE;
}

static void
mount_device_set_state (DeviceInfo *device, FMDesktopIconView *icon_view)
{
	switch (device->type) {
		case DEVICE_CDROM:
			mount_device_cdrom_set_state (icon_view, device);
			break;

		case DEVICE_FLOPPY:
			mount_device_floppy_set_state (icon_view, device);
			break;

		case DEVICE_EXT2:
			mount_device_ext2_set_state (icon_view, device);
			break;
	
		default:
			break;
	}
}

static void
device_set_state_empty (DeviceInfo *device, FMDesktopIconView *icon_view)
{
	device->state = STATE_EMPTY;
}

static void
mount_device_mount (FMDesktopIconView *view, DeviceInfo *device)
{
	char *target_uri, *desktop_path;
	const char *icon_name;
	NautilusIconContainer *container;
	gboolean result;
	int index;

	container = NAUTILUS_ICON_CONTAINER (GTK_BIN (view)->child);

	desktop_path = nautilus_get_desktop_directory ();
	target_uri = nautilus_get_uri_from_local_path (device->mount_path);

	/* Make volume name link "nice" */

	/* Strip whitespace from the end of the name. */
	for (index = strlen (device->volume_name) - 1; index > 0; index--) {
		if (device->volume_name [index] != ' ') {
			break;
		}
		device->volume_name [index] = '\0';
	}

	/* The volume name may have '/' characters. We need to convert
	 * them to something legal.
	 */
	for (index = 0; ; index++) {
		if (device->volume_name [index] == '\0') {
			break;
		}
		if (device->volume_name [index] == '/') {
			device->volume_name [index] = '-';
		}
	}

	/* Clean up old link, This should have been done earlier, but
	 * we double check to be sure.
	 */
	remove_mount_link (device);

	/* Get icon type */
	if (strcmp (device->mount_type, "cdrom") == 0) {
		icon_name = "i-cdrom.png";
	} else if (strcmp (device->mount_type, "floppy") == 0) {
		icon_name = "i-floppy.png";
	} else {
		icon_name = "i-blockdev.png";
	}
	
	/* Create link */
	result = nautilus_link_create (desktop_path, device->volume_name, icon_name, target_uri);
	if (result) {
		device->link_uri = nautilus_make_path (desktop_path, device->volume_name);
	} else {
		g_message ("Unable to create mount link");
	}
	
	g_free (desktop_path);
	g_free (target_uri);
	
	device->did_mount = TRUE;
}

static void
mount_device_activate_cdrom (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	int disctype;

	if (device->device_fd < 0) {
		device->device_fd = open (device->fsname, O_RDONLY|O_NONBLOCK);
	}

	disctype = ioctl (device->device_fd, CDROM_DISC_STATUS, CDSL_CURRENT);
	switch (disctype) {
		case CDS_AUDIO:
			/* Should we run a CD player here? */
			break;

		case CDS_DATA_1:
		case CDS_DATA_2:
  		case CDS_XA_2_1:
  		case CDS_XA_2_2:
  		case CDS_MIXED:
			/* Get volume name */
			get_iso9660_volume_name (device);
			mount_device_mount (icon_view, device);
			break;

		default:
			g_message ("Unknown CDROM type");
    			break;
  	}

	if (device->device_fd >= 0) {
		close (device->device_fd);
		device->device_fd = -1;
	}
}

static void
mount_device_activate_floppy (FMDesktopIconView *view, DeviceInfo *device)
{
	char *argv[3];

	/* Get volume name */
	get_floppy_volume_name (device);

	g_message ("Mounting floppy: %s", device->mount_path);
	
	argv[0] = "/bin/mount";
	argv[1] = device->mount_path;
	argv[2] = NULL;

	gnome_execute_async (g_get_home_dir(), 2, argv);

	mount_device_mount (view, device);
}

static void
mount_device_activate_ext2 (FMDesktopIconView *view, DeviceInfo *device)
{
	/* Get volume name */
	get_ext2_volume_name (device);

	mount_device_mount (view, device);
}

typedef void (* ChangeDeviceInfoFunction) (FMDesktopIconView *view, DeviceInfo *device);

static void
mount_device_activate (FMDesktopIconView *view, DeviceInfo *device)
{
	switch (device->type) {
		case DEVICE_CDROM:
			mount_device_activate_cdrom (view, device);
			break;
			
		case DEVICE_FLOPPY:
			mount_device_activate_floppy (view, device);
			break;
			
		case DEVICE_EXT2:
			mount_device_activate_ext2 (view, device);
			break;
			
		default:
			g_assert_not_reached ();
			break;
	}
}

static void
mount_device_deactivate (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	GList dummy_list;
	
	/* Remove mounted device icon from desktop */
	dummy_list.data = device->link_uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_removed (&dummy_list);

	/* Clean up old link */
	remove_mount_link (device);

	device->did_mount = FALSE;
}

static void
mount_device_do_nothing (FMDesktopIconView *icon_view, DeviceInfo *device)
{
}

static void
mount_device_check_change (gpointer data, gpointer callback_data)
{
	/* What functions to run for particular state transitions */
	static const ChangeDeviceInfoFunction state_transitions[STATE_LAST][STATE_LAST] = {
		/************  from: ACTIVE                   INACTIVE                 EMPTY */
		/* to */
		/* ACTIVE */   {     mount_device_do_nothing, mount_device_activate,   mount_device_activate   },
		/* INACTIVE */ {     mount_device_deactivate, mount_device_do_nothing, mount_device_activate   },
		/* EMPTY */    {     mount_device_deactivate, mount_device_deactivate, mount_device_do_nothing }
	};	

	DeviceInfo *device;
	FMDesktopIconView *icon_view;
	DeviceState old_state;
	ChangeDeviceInfoFunction f;

	g_assert (data != NULL);

	device = data;
	icon_view = FM_DESKTOP_ICON_VIEW (callback_data);

  	old_state = device->state;

  	mount_device_set_state (device, icon_view);

  	if (old_state != device->state) {
    		f = state_transitions[device->state][old_state];

		/* FIXME: Remove messages when this code is done. */
    		g_message ("State on %s changed from %s to %s, running %p",
			   device->fsname, state_names[old_state], state_names[device->state], f);
			
		(* f) (icon_view, device);
  	}
}

static void
mount_devices_update_is_mounted (FMDesktopIconView *icon_view)
{
	FILE *fh;
	char line[PATH_MAX * 3], mntpoint[PATH_MAX], devname[PATH_MAX];
	GList *ltmp;
	DeviceInfo *device;

	for (ltmp = icon_view->details->devices; ltmp; ltmp = ltmp->next) {
		device = ltmp->data;

		device->is_mounted = FALSE;
	}

	fh = fopen (_PATH_MOUNTED, "r");
	if (!fh) {
    		return;
    	}

	while (fgets(line, sizeof(line), fh)) {
		sscanf(line, "%s %s", devname, mntpoint);
    		device = g_hash_table_lookup (icon_view->details->devices_by_fsname, devname);

    		if(device) {
      			device->is_mounted = TRUE;
      		}
  	}

	fclose (fh);
}

static gint
mount_devices_check_status (FMDesktopIconView *icon_view)
{
	mount_devices_update_is_mounted (icon_view);

	g_list_foreach (icon_view->details->devices,
			mount_device_check_change,
			icon_view);
	
	return TRUE;
}


/* Like access, but a bit more accurate - access will let
 * root do anything. Does not get read-only or no-exec
 * filesystems right.
 */
static gboolean
my_g_check_permissions (gchar *filename, int mode)
{
	int euid = geteuid();
	int egid = getegid();

	struct stat statbuf;

	if (stat (filename, &statbuf) == 0) {
		if ((mode & R_OK) &&
		    !((statbuf.st_mode & S_IROTH) ||
		      ((statbuf.st_mode & S_IRUSR) && euid == statbuf.st_uid) ||
		      ((statbuf.st_mode & S_IRGRP) && egid == statbuf.st_gid)))
			return FALSE;
		if ((mode & W_OK) &&
		    !((statbuf.st_mode & S_IWOTH) ||
		      ((statbuf.st_mode & S_IWUSR) && euid == statbuf.st_uid) ||
		      ((statbuf.st_mode & S_IWGRP) && egid == statbuf.st_gid)))
			return FALSE;
		if ((mode & X_OK) &&
		    !((statbuf.st_mode & S_IXOTH) ||
		      ((statbuf.st_mode & S_IXUSR) && euid == statbuf.st_uid) ||
		      ((statbuf.st_mode & S_IXGRP) && egid == statbuf.st_gid)))
			return FALSE;

		return TRUE;
	}
	return FALSE;
}

static gboolean
mount_device_floppy_add (DeviceInfo *device)
{
	if (my_g_check_permissions (device->fsname, R_OK)) {
		return FALSE;
	}

	device->type = DEVICE_FLOPPY;

	return TRUE;
}

static gboolean
mount_device_ext2_add (DeviceInfo *device)
{
	if (my_g_check_permissions (device->fsname, R_OK)) {		
		return FALSE;
	}

	/* Only mount root partition for now */
	if ( strcmp (device->mount_path, "/") != 0) {
		return FALSE;
	}

	device->type = DEVICE_EXT2;
		
	return TRUE;
}


static void
cdrom_ioctl_frenzy (int fd)
{
	ioctl(fd, CDROM_CLEAR_OPTIONS, CDO_LOCK|CDO_AUTO_CLOSE | CDO_AUTO_EJECT);
	ioctl(fd, CDROM_SET_OPTIONS, CDO_USE_FFLAGS | CDO_CHECK_TYPE);
#ifdef CDROM_LOCKDOOR
	ioctl(fd, CDROM_LOCKDOOR, 0);
#else
	#warning "Need Linux kernel >= 2.2.4 to work with IDE."
#endif
}


static gboolean
mount_device_iso9660_add (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	device->device_fd = open (device->fsname, O_RDONLY|O_NONBLOCK);
	if(device->device_fd < 0) {
		return FALSE;
	}

	device->type = DEVICE_CDROM;

	/* It's probably not a CD-ROM drive */
	if(ioctl (device->device_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}

	cdrom_ioctl_frenzy (device->device_fd);
	close (device->device_fd); device->device_fd = -1;

	return TRUE;
}


/* This is here because mtab lists devices by their symlink-followed names rather than what is listed in fstab. *sigh* */
static void
mount_device_add_aliases (FMDesktopIconView *icon_view, const char *alias, DeviceInfo *device)
{
	char buf[PATH_MAX];
	int buflen;

	g_hash_table_insert (icon_view->details->devices_by_fsname, (gpointer)alias, device);

	buflen = readlink (alias, buf, sizeof(buf));
	if(buflen < 1) {
    		return;
    	}

	buf[buflen] = '\0';

	if(buf[0] != '/') {
		char buf2[PATH_MAX];
		char *dn;
    		dn = g_dirname(alias);
    		sprintf(buf2, "%s/%s", dn, buf);
    		g_free(dn);
    		strcpy(buf, buf2);
	}

	mount_device_add_aliases (icon_view, g_strdup(buf), device);
}


static void
add_mount_device (FMDesktopIconView *icon_view, struct mntent *ent)
{
	DeviceInfo *newdev = NULL;
	gboolean mounted;

	newdev = g_new0 (DeviceInfo, 1);
	g_assert (newdev);
	newdev->device_fd   = -1;
	newdev->fsname 	    = g_strdup (ent->mnt_fsname);
	newdev->mount_path  = g_strdup (ent->mnt_dir);
	newdev->volume_name = NULL;
	newdev->link_uri    = NULL;
	newdev->state 	    = STATE_EMPTY;

	mounted = FALSE;
	
	if (strcmp (ent->mnt_type, MOUNT_TYPE_ISO9660) == 0) {
		newdev->mount_type = g_strdup ("cdrom");
    		mounted = mount_device_iso9660_add (icon_view, newdev); 
	} else if (strncmp (ent->mnt_fsname, "/dev/fd", strlen("/dev/fd")) == 0) {
		newdev->mount_type = g_strdup ("floppy");
		mounted = mount_device_floppy_add (newdev);
	} else if (strcmp (ent->mnt_type, MOUNT_TYPE_EXT2) == 0) {
		newdev->mount_type = g_strdup ("blockdevice");
		mounted = mount_device_ext2_add (newdev);
	} else {
		/* FIXME: Is this a reasonable way to report this error? */
		g_message ("Unknown file system: %s", ent->mnt_type);
	}
	
	if (mounted) {
		icon_view->details->devices = g_list_append (icon_view->details->devices, newdev);
		mount_device_add_aliases (icon_view, newdev->fsname, newdev);
		/* FIXME: Remove messages when this code is done. */
		g_message ("Device %s came through (type %s)", newdev->fsname, type_names[newdev->type]);
	} else {
		close (newdev->device_fd);
		g_free (newdev->fsname);
		g_free (newdev->mount_path);
		g_free (newdev);		
	}
}

#if 0
static gboolean
mntent_is_removable_fs(struct mntent *ent)
{
	if (strcmp (ent->mnt_type, MOUNT_TYPE_ISO9660) == 0) {
		return TRUE;
	}

#ifdef FLOPPY_SUPPORT
	if (!strncmp (ent->mnt_fsname, "/dev/fd", strlen("/dev/fd"))) {
		return TRUE;
	}
#endif

	return FALSE;
}

static gboolean
mntent_has_option(const char *optlist, const char *option)
{
	gboolean retval = FALSE;
	char **options;
	int i;

	options = g_strsplit (optlist, ",", -1);

	for(i = 0; options[i]; i++) {
		if (strcmp (options[i], option) == 0) {
			retval = TRUE;
			break;
    		}
  	}

  	g_strfreev (options);

  	return retval;
}
#endif

void
fm_desktop_find_mount_devices (FMDesktopIconView *icon_view, const char *fstab_path)
{
	FILE *mef;
	struct mntent *ent;

	mef = setmntent (fstab_path, "r");
	g_return_if_fail (mef);

	while ((ent = getmntent (mef))) {
		/* FIXME: Remove messages when this code is done. */
		g_message ("Checking device %s", ent->mnt_fsname);

#if 0
		/* Think some more about these checks */
		/* Check for removable device */
		if (!mntent_is_removable_fs (ent)) {
			continue;
		}

		if (!mntent_has_option (ent->mnt_opts, MOUNT_OPTIONS_USER)
			&& !mntent_has_option (ent->mnt_opts, MOUNT_OPTIONS_OWNER)) {
			continue;
		}
#endif
		/* Add it to out list of mount points */
		add_mount_device (icon_view, ent);
	}


  	endmntent (mef);

	g_list_foreach (icon_view->details->devices, (GFunc) mount_device_set_state, icon_view);

	/* Manually set state of all volumes to empty so we update */
	g_list_foreach (icon_view->details->devices, (GFunc) device_set_state_empty, icon_view);

	/* Add a timer function to check for status change in mounted devices */
	icon_view->details->mount_device_timer_id = 
		gtk_timeout_add (CHECK_INTERVAL, (GtkFunction) mount_devices_check_status, icon_view);
}


static void
remove_mount_link (DeviceInfo *device)
{
	GnomeVFSResult result;
	
	if (device->link_uri != NULL) {
		result = gnome_vfs_unlink (device->link_uri);
		if (result != GNOME_VFS_OK) {
			/* FIXME: Is a message to the console acceptable here? */
			g_message ("Unable to remove mount link: %s", gnome_vfs_result_to_string (result));
		}
		g_free (device->link_uri);
		device->link_uri = NULL;
	}
}


void
fm_desktop_remove_mount_links (DeviceInfo *device, FMDesktopIconView *icon_view)
{
	remove_mount_link (device);
}


void
fm_desktop_free_device_info (DeviceInfo *device, FMDesktopIconView *icon_view)
{

	if (device->device_fd != -1) {
		close (device->device_fd);
		device->device_fd = -1;
	}
	
	if (device->fsname != NULL) {
		g_free (device->fsname);
		device->fsname = NULL;
	}

	if (device->mount_path != NULL) {
		g_free (device->mount_path);
		device->mount_path = NULL;
	}

	if (device->mount_type != NULL) {
		g_free (device->mount_type);
		device->mount_type = NULL;
	}

	if (device->volume_name != NULL) {
		g_free (device->volume_name);
		device->volume_name = NULL;
	}

	if (device->link_uri != NULL) {
		g_free (device->link_uri);
		device->link_uri = NULL;
	}
}

static void
get_iso9660_volume_name (DeviceInfo *device)
{
	struct iso_primary_descriptor iso_buffer;

	lseek (device->device_fd, (off_t) 2048*16, SEEK_SET);
	read (device->device_fd, &iso_buffer, 2048);
	
	if (device->volume_name != NULL) {
		g_free (device->volume_name);
	}
	
	device->volume_name = g_strdup (iso_buffer.volume_id);
	if (device->volume_name == NULL) {
		device->volume_name = g_strdup (device->mount_type);
	}
}


static void
get_ext2_volume_name (DeviceInfo *device)
{
	device->volume_name = g_strdup ("Ext2 Volume");
}

static void
get_floppy_volume_name (DeviceInfo *device)
{
	device->volume_name = g_strdup ("Floppy");
}

/* fm_dekstop_place_home_directory
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
void
fm_desktop_place_home_directory (FMDesktopIconView *icon_view)
{
	char *desktop_path, *home_link_name, *home_link_path, *home_link_uri, *home_dir_uri;
	GnomeVFSResult result;
	GnomeVFSFileInfo info;
	
	desktop_path = nautilus_get_desktop_directory ();
	home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
	home_link_path = nautilus_make_path (desktop_path, home_link_name);
	home_link_uri = nautilus_get_uri_from_local_path (home_link_path);
	
	result = gnome_vfs_get_file_info (home_link_uri, &info, 0);
	if (result != GNOME_VFS_OK) {
		/* FIXME: Maybe we should only create if the error was "not found". */
		/* There was no link file.  Create it and add it to the desktop view */		
		home_dir_uri = nautilus_get_uri_from_local_path (g_get_home_dir ());
		result = nautilus_link_create (desktop_path, home_link_name, "temp-home.png", home_dir_uri);
		g_free (home_dir_uri);
		if (result != GNOME_VFS_OK) {
			/* FIXME: Is a message to the console acceptable here? */
			g_message ("Unable to create home link: %s", gnome_vfs_result_to_string (result));
		}
	}
	
	g_free (home_link_uri);
	g_free (home_link_path);
	g_free (home_link_name);
	g_free (desktop_path);
}
