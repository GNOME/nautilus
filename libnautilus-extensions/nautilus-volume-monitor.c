/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-volume-monitor.c - Desktop volume mounting routines.

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

#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <glib.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <mntent.h>
#include <libnautilus-extensions/nautilus-cdrom-extensions.h>
#include <libnautilus-extensions/nautilus-directory-private.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-iso9660.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-volume-monitor.h>

#include <parser.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xmlmemory.h>


/* FIXME: Remove messages when this code is done. */
#define MESSAGE g_message

NautilusVolumeMonitor *global_volume_monitor = NULL;

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

/* The NautilusVolumeMonitor signals.  */
enum {
	VOLUME_MOUNTED,
	VOLUME_UNMOUNTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void	nautilus_volume_monitor_initialize 			(NautilusVolumeMonitor 		*desktop_mounter);
static void	nautilus_volume_monitor_initialize_class 		(NautilusVolumeMonitorClass 	*klass);
static void	nautilus_volume_monitor_destroy 			(GtkObject 			*object);
static void     remove_mount_link                                     	(DeviceInfo             	*device);								  
static void     get_iso9660_volume_name                              	(DeviceInfo             	*device);
static void     get_ext2_volume_name                               	(DeviceInfo             	*device);
static void     get_floppy_volume_name                           	(DeviceInfo             	*device);
static void     mount_device_mount                               	(NautilusVolumeMonitor      	*view,
									 DeviceInfo             	*device);
static gboolean mount_device_is_mounted                       		(DeviceInfo             	*device);
static void	mount_device_activate 					(NautilusVolumeMonitor 	  	*view, 
									 DeviceInfo 		  	*device);
static void     mount_device_deactivate                               	(NautilusVolumeMonitor      	*monitor,
									 DeviceInfo             	*device);
static void     mount_device_activate_floppy                          	(NautilusVolumeMonitor      	*view,
									 DeviceInfo             	*device);
static gboolean	mntent_is_removable_fs					(struct mntent 	  		*ent);
static void	free_device_info             				(DeviceInfo             	*device,
						 	 	 	 NautilusVolumeMonitor      	*monitor);
static gboolean	add_mount_link_property 				(const char 			*path);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusVolumeMonitor, nautilus_volume_monitor, GTK_TYPE_OBJECT)

static void
nautilus_volume_monitor_initialize (NautilusVolumeMonitor *monitor)
{
	/* Set up details */
	monitor->details = g_new0 (NautilusVolumeMonitorDetails, 1);	
	monitor->details->devices_by_fsname = g_hash_table_new (g_str_hash, g_str_equal);
	monitor->details->devices = NULL;
}

static void
nautilus_volume_monitor_initialize_class (NautilusVolumeMonitorClass *klass)
{
	GtkObjectClass		*object_class;

	object_class		= GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_volume_monitor_destroy;

	signals[VOLUME_MOUNTED] 
		= gtk_signal_new ("volume_mounted",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusVolumeMonitorClass, 
						     volume_mounted),
				  nautilus_gtk_marshal_STRING__NONE,
				  GTK_TYPE_STRING, 0);

	signals[VOLUME_UNMOUNTED] 
		= gtk_signal_new ("volume_unmounted",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusVolumeMonitorClass, 
						     volume_unmounted),
				  nautilus_gtk_marshal_STRING__NONE,
				  GTK_TYPE_STRING, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);				  
}

static void
nautilus_volume_monitor_destroy (GtkObject *object)
{
	NautilusVolumeMonitor *monitor;
	
	monitor = NAUTILUS_VOLUME_MONITOR (object);

	/* Remove timer function */
	gtk_timeout_remove (monitor->details->mount_device_timer_id);
		
	/* Clean up other device info */
	g_list_foreach (monitor->details->devices, (GFunc)free_device_info, monitor);

	/* Remove timer function */
	gtk_timeout_remove (monitor->details->mount_device_timer_id);
	
	/* Clean up details */	 
	g_hash_table_destroy (monitor->details->devices_by_fsname);
	g_list_free (monitor->details->devices);
	g_free (monitor->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Return the global instance of the NautilusVolumeMonitor.  Create one
 * if we have not done so already
 */
NautilusVolumeMonitor *
nautilus_volume_monitor_get (void)
{
	if (global_volume_monitor == NULL) {
		global_volume_monitor = NAUTILUS_VOLUME_MONITOR
					(gtk_object_new (nautilus_volume_monitor_get_type(),
					 NULL));
	}

	return global_volume_monitor;
}

static int
floppy_sort (const char *name_1, const char *name_2) 
{
	/* If both are floppies, we don't care yet */
	if ((strncmp (name_1, "/mnt/fd", strlen("/mnt/fd")) == 0) &&
	   (strncmp (name_2, "/mnt/fd", strlen("/mnt/fd")) == 0)) {
		return 0; 
	}

	if (strncmp (name_1, "/mnt/fd", strlen("/mnt/fd")) != 0) {
		return 1; 
	}

	return -1;
}

GList *
fm_desktop_get_removable_volume_list (void)
{
	GList *list;
	FILE *mef;
	struct mntent *ent;

	list = NULL;

	mef = setmntent (_PATH_FSTAB, "r");
	g_return_val_if_fail (mef, NULL);

	while ((ent = getmntent (mef))) {
		if (mntent_is_removable_fs (ent)) {
			list = g_list_append (list, g_strdup (ent->mnt_dir));
			continue;
		}
	}
  	endmntent (mef);

	/* Move all floppy mounts to top of list */
	list = g_list_sort (list, (GCompareFunc) floppy_sort);
	
  	return list;	
}

gboolean
nautilus_volume_monitor_volume_is_mounted (const char *mount_point)
{
	FILE *fh;
	char line[PATH_MAX * 3];
	char mntpoint[PATH_MAX], devname[PATH_MAX];
	
	/* Open mtab */
	fh = fopen (_PATH_MOUNTED, "r");
	if (!fh) {		
    		return FALSE;
    	}
    	
	while (fgets (line, sizeof(line), fh)) {
		sscanf(line, "%s %s", devname, mntpoint);
		if (strcmp (mntpoint, mount_point) == 0) {
			fclose (fh);	
			return TRUE;
		}
	}
	
	fclose (fh);
	return FALSE;
}

static gboolean
mount_device_is_mounted (DeviceInfo *device)
{
	return device->is_mounted;
}

static void
mount_device_cdrom_set_state (NautilusVolumeMonitor *monitor, DeviceInfo *device)
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
					device->state = STATE_EMPTY;
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
mount_device_floppy_set_state (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{
	/* If the floppy is not in mtab, then we set it to empty */
	if (nautilus_volume_monitor_volume_is_mounted (device->mount_path)) {
		device->state = STATE_ACTIVE;
	} else {
		device->state = STATE_EMPTY;
	}
}

static void
mount_device_ext2_set_state (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{
	device->state = STATE_ACTIVE;
}

static void
mount_device_set_state (DeviceInfo *device, NautilusVolumeMonitor *monitor)
{
	switch (device->type) {
		case DEVICE_CDROM:
			mount_device_cdrom_set_state (monitor, device);
			break;

		case DEVICE_FLOPPY:
			mount_device_floppy_set_state (monitor, device);
			break;

		case DEVICE_EXT2:
			mount_device_ext2_set_state (monitor, device);
			break;
	
		default:
			break;
	}
}

static void
device_set_state_empty (DeviceInfo *device, NautilusVolumeMonitor *monitor)
{
	device->state = STATE_EMPTY;
}

static void
mount_device_mount (NautilusVolumeMonitor *view, DeviceInfo *device)
{
	char *target_uri, *desktop_path;
	const char *icon_name;
	gboolean result;
	int index;

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

		/* Add some special magic so we are able to identify this as a mount link */
		add_mount_link_property (device->link_uri);
		
	} else {
		MESSAGE ("Unable to create mount link");
	}
	
	g_free (desktop_path);
	g_free (target_uri);
	
	device->did_mount = TRUE;
}

static void
mount_device_activate_cdrom (NautilusVolumeMonitor *monitor, DeviceInfo *device)
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
			mount_device_mount (monitor, device);
			break;

		default:
			MESSAGE ("Unknown CDROM type");
    			break;
  	}

	if (device->device_fd >= 0) {
		close (device->device_fd);
		device->device_fd = -1;
	}
}

static void
mount_device_activate_floppy (NautilusVolumeMonitor *view, DeviceInfo *device)
{
	/* Get volume name */
	get_floppy_volume_name (device);

	mount_device_mount (view, device);
}

static void
mount_device_activate_ext2 (NautilusVolumeMonitor *view, DeviceInfo *device)
{
	/* Get volume name */
	get_ext2_volume_name (device);

	mount_device_mount (view, device);
}

typedef void (* ChangeDeviceInfoFunction) (NautilusVolumeMonitor *view, DeviceInfo *device);

static void
mount_device_activate (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{
	switch (device->type) {
		case DEVICE_CDROM:
			mount_device_activate_cdrom (monitor, device);
			break;
			
		case DEVICE_FLOPPY:
			mount_device_activate_floppy (monitor, device);
			break;
			
		case DEVICE_EXT2:
			mount_device_activate_ext2 (monitor, device);
			break;
			
		default:
			g_assert_not_reached ();
			break;
	}

	gtk_signal_emit (GTK_OBJECT (monitor),
			 signals[VOLUME_MOUNTED],
			 &device->mount_path);
}


static void 
eject_cdrom (DeviceInfo *device)
{
#if 0
	if (device->device_fd < 0) {
		device->device_fd = open (device->fsname, O_RDONLY|O_NONBLOCK);
	}

	if(device->device_fd < 0) {
		return;
	}

	g_message ("Trying to eject");
	ioctl (device->device_fd, CDROMEJECT, 0);

	close (device->device_fd);
	device->device_fd = -1;
#endif
}

static void
mount_device_deactivate (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{
	GList dummy_list;
	
	/* Remove mounted device icon from desktop */
	dummy_list.data = device->link_uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_removed (&dummy_list);

	/* Clean up old link */
	remove_mount_link (device);

	switch (device->type) {
		case DEVICE_CDROM:
			eject_cdrom (device);
			break;
			
		default:
			break;
	}

	/* Tell anybody who cares */
	gtk_signal_emit (GTK_OBJECT (monitor),
			 signals[VOLUME_UNMOUNTED],
			 &device->mount_path);

}

static void
mount_device_do_nothing (NautilusVolumeMonitor *monitor, DeviceInfo *device)
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
		/* INACTIVE */ {     mount_device_deactivate, mount_device_do_nothing, mount_device_do_nothing },
		/* EMPTY */    {     mount_device_deactivate, mount_device_deactivate, mount_device_do_nothing }
	};	

	DeviceInfo *device;
	NautilusVolumeMonitor *monitor;
	DeviceState old_state;
	ChangeDeviceInfoFunction f;

	g_assert (data != NULL);

	device = data;
	monitor = NAUTILUS_VOLUME_MONITOR (callback_data);

  	old_state = device->state;

  	mount_device_set_state (device, monitor);

  	if (old_state != device->state) {
    		f = state_transitions[device->state][old_state];

    		MESSAGE ("State on %s changed from %s to %s, running %p",
			   device->fsname, state_names[old_state], state_names[device->state], f);
			
		(* f) (monitor, device);
  	}
}

static void
mount_devices_update_is_mounted (NautilusVolumeMonitor *monitor)
{
	FILE *fh;
	char line[PATH_MAX * 3], mntpoint[PATH_MAX], devname[PATH_MAX];
	GList *element;
	DeviceInfo *device;

	/* Toggle mount state to off and then recheck in mtab. */
	for (element = monitor->details->devices; element != NULL; element = element->next) {
		device = element->data;
		device->is_mounted = FALSE;
	}

	/* Open mtab */
	fh = fopen (_PATH_MOUNTED, "r");
	if (!fh) {		
    		return;
    	}

	while (fgets (line, sizeof(line), fh)) {
		sscanf(line, "%s %s", devname, mntpoint);
    		device = g_hash_table_lookup (monitor->details->devices_by_fsname, devname);

    		if(device) {
      			device->is_mounted = TRUE;
      		}
  	}

	fclose (fh);
}

static gint
mount_devices_check_status (NautilusVolumeMonitor *monitor)
{
	mount_devices_update_is_mounted (monitor);

	g_list_foreach (monitor->details->devices,
			mount_device_check_change,
			monitor);
	
	return TRUE;
}


/* Like access, but a bit more accurate - access will let
 * root do anything. Does not get read-only or no-exec
 * filesystems right.
 */
static gboolean
check_permissions (gchar *filename, int mode)
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
mount_device_floppy_add (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{
	device->mount_type 	= g_strdup ("floppy");
	device->type 		= DEVICE_FLOPPY;
	return TRUE;
#if 0
	if (check_permissions (device->fsname, R_OK)) {
		return FALSE;
	}
	
	return TRUE;
#endif
}

static gboolean
mount_device_ext2_add (DeviceInfo *device)
{		
	if (check_permissions (device->fsname, R_OK)) {		
		return FALSE;
	}

	/* Only mount root partition for now */
	if ( strcmp (device->mount_path, "/") != 0) {
		return FALSE;
	}

	device->mount_type = g_strdup ("blockdevice");
	device->type = DEVICE_EXT2;
		
	return TRUE;
}


static void
cdrom_ioctl_frenzy (int fd)
{
	ioctl (fd, CDROM_CLEAR_OPTIONS, CDO_LOCK|CDO_AUTO_CLOSE | CDO_AUTO_EJECT);
	ioctl (fd, CDROM_SET_OPTIONS, CDO_USE_FFLAGS | CDO_CHECK_TYPE);
	ioctl (fd, CDROM_LOCKDOOR, 0);
}


static gboolean
mount_device_iso9660_add (NautilusVolumeMonitor *monitor, DeviceInfo *device)
{		
	device->device_fd = open (device->fsname, O_RDONLY|O_NONBLOCK);
	if(device->device_fd < 0) {
		return FALSE;
	}
	
	device->type = DEVICE_CDROM;

	/* It's probably not a CD-ROM drive */
	if (ioctl (device->device_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}

	cdrom_ioctl_frenzy (device->device_fd);
	close (device->device_fd); device->device_fd = -1;

	device->mount_type = g_strdup ("cdrom");

	return TRUE;
}


/* This is here because mtab lists devices by their symlink-followed names rather than what is listed in fstab. *sigh* */
static void
mount_device_add_aliases (NautilusVolumeMonitor *monitor, const char *alias, DeviceInfo *device)
{
	char buf[PATH_MAX];
	int buflen;

	g_hash_table_insert (monitor->details->devices_by_fsname, (gpointer)alias, device);

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

	mount_device_add_aliases (monitor, g_strdup(buf), device);
}


static void
add_mount_device (NautilusVolumeMonitor *monitor, struct mntent *ent)
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
    		mounted = mount_device_iso9660_add (monitor, newdev); 
	} else if (strncmp (ent->mnt_fsname, "/dev/fd", strlen("/dev/fd")) == 0) {		
		mounted = mount_device_floppy_add (monitor, newdev);
	} else if (strcmp (ent->mnt_type, MOUNT_TYPE_EXT2) == 0) {		
		mounted = mount_device_ext2_add (newdev);
	} else {
		/* FIXME: Is this a reasonable way to report this error? */
		MESSAGE ("Unknown file system: %s", ent->mnt_type);
	}
	
	if (mounted) {
		monitor->details->devices = g_list_append (monitor->details->devices, newdev);
		mount_device_add_aliases (monitor, newdev->fsname, newdev);		
		MESSAGE ("Device %s came through (type %s)", newdev->fsname, type_names[newdev->type]);
	} else {
		close (newdev->device_fd);
		g_free (newdev->fsname);
		g_free (newdev->mount_path);
		g_free (newdev);		
	}
}

static gboolean
mntent_is_removable_fs (struct mntent *ent)
{
	if (strcmp (ent->mnt_type, MOUNT_TYPE_ISO9660) == 0) {
		return TRUE;
	}
	
	if (strncmp (ent->mnt_fsname, "/dev/fd", strlen("/dev/fd")) == 0) {
		return TRUE;
	}
	
	return FALSE;
}

#if 0
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
nautilus_volume_monitor_find_mount_devices (NautilusVolumeMonitor *monitor)
{
	FILE *mef;
	struct mntent *ent;

	mef = setmntent (_PATH_MNTTAB, "r");
	g_return_if_fail (mef);

	while ((ent = getmntent (mef))) {
		MESSAGE ("Checking device %s", ent->mnt_fsname);

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
		/* Add it to our list of mount points */
		add_mount_device (monitor, ent);
	}


  	endmntent (mef);

	g_list_foreach (monitor->details->devices, (GFunc) mount_device_set_state, monitor);

	/* Manually set state of all volumes to empty so we update */
	g_list_foreach (monitor->details->devices, (GFunc) device_set_state_empty, monitor);

	/* Add a timer function to check for status change in mounted devices */
	monitor->details->mount_device_timer_id = 
		gtk_timeout_add (CHECK_INTERVAL, (GtkFunction) mount_devices_check_status, monitor);
}


gboolean
nautilus_volume_monitor_mount_unmount_removable (NautilusVolumeMonitor *monitor, const char *mount_point)
{
	gboolean is_mounted, found_device;
	char *argv[3];
	GList *element;
	DeviceInfo *device;
	int exec_err;
	
	is_mounted = FALSE;
	found_device = FALSE;
	device = NULL;
	
	/* Locate DeviceInfo for mount point */
	for (element = monitor->details->devices; element != NULL; element = element->next) {
		device = element->data;
		if (strcmp (mount_point, device->mount_path) == 0) {
			found_device = TRUE;
			break;
		}
	}
			
	/* Get mount state and then decide to mount/unmount the volume */
	if (found_device) {
		is_mounted = nautilus_volume_monitor_volume_is_mounted (mount_point);
		argv[1] = (char *)mount_point;
		argv[2] = NULL;

		if (is_mounted) {
			/* Unount */
			argv[0] = "/bin/umount";
			exec_err = gnome_execute_async (g_get_home_dir(), 2, argv);
			is_mounted = FALSE;
		} else {
			/* Mount */
			argv[0] = "/bin/mount";
			exec_err = gnome_execute_async (g_get_home_dir(), 2, argv);
			is_mounted = TRUE;
		}
	}

	return is_mounted;
}


static void
remove_mount_link (DeviceInfo *device)
{
	GnomeVFSResult result;
	
	if (device->link_uri != NULL) {
		result = gnome_vfs_unlink (device->link_uri);
		if (result != GNOME_VFS_OK) {
			/* FIXME: Is a message to the console acceptable here? */
			MESSAGE ("Unable to remove mount link: %s", gnome_vfs_result_to_string (result));
		}
		g_free (device->link_uri);
		device->link_uri = NULL;
	}
}

static void
free_device_info (DeviceInfo *device, NautilusVolumeMonitor *monitor)
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


/* Add a special XML tag that identifies this link as a mount link.
 */
static gboolean
add_mount_link_property (const char *path)
{
	xmlDocPtr document;

	document = xmlParseFile (path);
	if (document == NULL) {
		return FALSE;
	}

	xmlSetProp (xmlDocGetRootElement (document),
		    NAUTILUS_MOUNT_LINK_KEY,
		    "Nautilus Mount Link");
	xmlSaveFile (path, document);
	xmlFreeDoc (document);
				
	return TRUE;
}

gboolean
nautilus_volume_monitor_is_volume_link (const char *path)
{
	xmlDocPtr document;
	char *property;
	gboolean retval;

	retval = FALSE;
	
	document = xmlParseFile (path);
	if (document == NULL) {
		return FALSE;
	}

	property = xmlGetProp (xmlDocGetRootElement (document), NAUTILUS_MOUNT_LINK_KEY);
	if (property != NULL) {
		retval = TRUE;
		xmlFree (property);
	}
	
	xmlFreeDoc (document);
			
	return retval;
}
