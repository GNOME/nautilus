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
#include "nautilus-volume-monitor.h"

#include "nautilus-cdrom-extensions.h"
#include "nautilus-directory-notify.h"
#include "nautilus-file-utilities.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-iso9660.h"
#include "nautilus-volume-monitor.h"
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <glib.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <mntent.h>
#include <parser.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xmlmemory.h>

struct NautilusVolumeMonitorDetails
{
	GHashTable *volumes_by_fsname;
	GList *volumes;
	guint mount_volume_timer_id;
};

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
static void     get_iso9660_volume_name                              	(NautilusVolume             	*volume);
static void     get_ext2_volume_name                               	(NautilusVolume             	*volume);
static void     get_floppy_volume_name                           	(NautilusVolume             	*volume);
static void     mount_volume_mount                               	(NautilusVolumeMonitor      	*view,
									 NautilusVolume             	*volume);
static gboolean mount_volume_is_mounted                       		(NautilusVolume             	*volume);
static void	mount_volume_activate 					(NautilusVolumeMonitor 	  	*view, 
									 NautilusVolume 		*volume);
static void     mount_volume_deactivate                               	(NautilusVolumeMonitor      	*monitor,
									 NautilusVolume             	*volume);
static void     mount_volume_activate_floppy                          	(NautilusVolumeMonitor      	*view,
									 NautilusVolume             	*volume);
static gboolean	mntent_is_removable_fs					(struct mntent 	  		*ent);
static void	free_volume_info             				(NautilusVolume             	*volume,
						 	 	 	 NautilusVolumeMonitor      	*monitor);
static void	find_volumes 						(NautilusVolumeMonitor 		*monitor);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusVolumeMonitor,
				   nautilus_volume_monitor,
				   GTK_TYPE_OBJECT)

static void
nautilus_volume_monitor_initialize (NautilusVolumeMonitor *monitor)
{
	/* Set up details */
	monitor->details = g_new0 (NautilusVolumeMonitorDetails, 1);	
	monitor->details->volumes_by_fsname = g_hash_table_new (g_str_hash, g_str_equal);
	monitor->details->volumes = NULL;

	find_volumes (monitor);
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
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[VOLUME_UNMOUNTED] 
		= gtk_signal_new ("volume_unmounted",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusVolumeMonitorClass, 
						     volume_unmounted),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);				  
}

static void
nautilus_volume_monitor_destroy (GtkObject *object)
{
	NautilusVolumeMonitor *monitor;
	
	monitor = NAUTILUS_VOLUME_MONITOR (object);

	/* Remove timer function */
	gtk_timeout_remove (monitor->details->mount_volume_timer_id);
		
	/* Clean up other volume info */
	g_list_foreach (monitor->details->volumes, (GFunc)free_volume_info, monitor);
	
	/* Clean up details */	 
	g_hash_table_destroy (monitor->details->volumes_by_fsname);
	g_list_free (monitor->details->volumes);	
	g_free (monitor->details);

	global_volume_monitor = NULL;

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

#define FLOPPY_MOUNT_PATH "/mnt/fd"
#define FLOPPY_DEVICE_PATH "/dev/fd"

static int
floppy_sort (const char *name_1, const char *name_2) 
{
	/* If both are floppies, we don't care yet */
	if ((strncmp (name_1, FLOPPY_MOUNT_PATH, sizeof (FLOPPY_MOUNT_PATH) - 1) == 0) &&
	    (strncmp (name_2, FLOPPY_MOUNT_PATH, sizeof (FLOPPY_MOUNT_PATH) - 1) == 0)) {
		return 0; 
	}
	
	if (strncmp (name_1, FLOPPY_MOUNT_PATH, sizeof (FLOPPY_MOUNT_PATH) - 1) != 0) {
		return 1; 
	}

	return -1;
}

GList *
nautilus_volume_monitor_get_removable_volume_names (void)
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
mount_volume_is_mounted (NautilusVolume *volume)
{
	return volume->is_mounted;
}

static void
mount_volume_cdrom_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	if (volume->volume_fd < 0) {
		volume->volume_fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);
	}

	if (ioctl (volume->volume_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK) {
		int disctype;

		disctype = ioctl (volume->volume_fd, CDROM_DISC_STATUS, CDSL_CURRENT);
		switch (disctype) {
		case CDS_AUDIO:
			/* It's pretty hard to know whether it is actually in use */
			volume->state = STATE_INACTIVE;
			break;
			
		case CDS_DATA_1:
		case CDS_DATA_2:
		case CDS_XA_2_1:
		case CDS_XA_2_2:
		case CDS_MIXED:
			/* Check if it is mounted */
			if (mount_volume_is_mounted (volume)) {
				volume->state = STATE_ACTIVE;
			} else {
				volume->state = STATE_EMPTY;
			}
			break;
			
		default:
			volume->state = STATE_EMPTY;
			break;
		}
	} else {
		volume->state = STATE_EMPTY;
	}
	
	if(volume->volume_fd >= 0) {
		close (volume->volume_fd);
		volume->volume_fd = -1;
	}
}


static void
mount_volume_floppy_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	/* If the floppy is not in mtab, then we set it to empty */
	if (nautilus_volume_monitor_volume_is_mounted (volume->mount_path)) {
		volume->state = STATE_ACTIVE;
	} else {
		volume->state = STATE_EMPTY;
	}
}

static void
mount_volume_ext2_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	volume->state = STATE_ACTIVE;
}

static void
mount_volume_set_state (NautilusVolume *volume, NautilusVolumeMonitor *monitor)
{
	switch (volume->type) {
	case VOLUME_CDROM:
		mount_volume_cdrom_set_state (monitor, volume);
		break;
		
	case VOLUME_FLOPPY:
		mount_volume_floppy_set_state (monitor, volume);
		break;
		
	case VOLUME_EXT2:
		mount_volume_ext2_set_state (monitor, volume);
		break;
		
	default:
		break;
	}
}

static void
volume_set_state_empty (NautilusVolume *volume, NautilusVolumeMonitor *monitor)
{
	volume->state = STATE_EMPTY;
}

static void
mount_volume_mount (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	char *target_uri, *desktop_path;	
	int index;

	desktop_path = nautilus_get_desktop_directory ();
	target_uri = nautilus_get_uri_from_local_path (volume->mount_path);

	/* Make user readable volume name "nice" */

	/* Strip whitespace from the end of the name. */
	for (index = strlen (volume->volume_name) - 1; index > 0; index--) {
		if (volume->volume_name [index] != ' ') {
			break;
		}
		volume->volume_name [index] = '\0';
	}

	/* The volume name may have '/' characters. We need to convert
	 * them to something legal.
	 */
	for (index = 0; ; index++) {
		if (volume->volume_name [index] == '\0') {
			break;
		}
		if (volume->volume_name [index] == '/') {
			volume->volume_name [index] = '-';
		}
	}
	
	g_free (desktop_path);
	g_free (target_uri);
	
	volume->did_mount = TRUE;
}

static void
mount_volume_activate_cdrom (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int disctype;

	if (volume->volume_fd < 0) {
		volume->volume_fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);
	}

	disctype = ioctl (volume->volume_fd, CDROM_DISC_STATUS, CDSL_CURRENT);
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
		get_iso9660_volume_name (volume);
		mount_volume_mount (monitor, volume);
		break;
		
	default:			
		break;
  	}
	
	if (volume->volume_fd >= 0) {
		close (volume->volume_fd);
		volume->volume_fd = -1;
	}
}

static void
mount_volume_activate_floppy (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	/* Get volume name */
	get_floppy_volume_name (volume);

	mount_volume_mount (view, volume);
}

static void
mount_volume_activate_ext2 (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	/* Get volume name */
	get_ext2_volume_name (volume);

	mount_volume_mount (view, volume);
}

typedef void (* ChangeNautilusVolumeFunction) (NautilusVolumeMonitor *view, NautilusVolume *volume);

static void
mount_volume_activate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	switch (volume->type) {
	case VOLUME_CDROM:
		mount_volume_activate_cdrom (monitor, volume);
		break;
		
	case VOLUME_FLOPPY:
		mount_volume_activate_floppy (monitor, volume);
		break;
		
	case VOLUME_EXT2:
		mount_volume_activate_ext2 (monitor, volume);
		break;
		
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_signal_emit (GTK_OBJECT (monitor),
			 signals[VOLUME_MOUNTED],
			 volume);
}


static void 
eject_cdrom (NautilusVolume *volume)
{
#if 0
	if (volume->volume_fd < 0) {
		volume->volume_fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);
	}

	if(volume->volume_fd < 0) {
		return;
	}

	ioctl (volume->volume_fd, CDROMEJECT, 0);

	close (volume->volume_fd);
	volume->volume_fd = -1;
#endif
}

static void
mount_volume_deactivate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	switch (volume->type) {
	case VOLUME_CDROM:
		eject_cdrom (volume);
		break;
		
	default:
		break;
	}

	/* Tell anybody who cares */
	gtk_signal_emit (GTK_OBJECT (monitor),
			 signals[VOLUME_UNMOUNTED],
			 volume);

}

static void
mount_volume_do_nothing (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
}

static void
mount_volume_check_change (gpointer data, gpointer callback_data)
{
	/* What functions to run for particular state transitions */
	static const ChangeNautilusVolumeFunction state_transitions[STATE_LAST][STATE_LAST] = {
		/************  from: ACTIVE                   INACTIVE                 EMPTY */
		/* to */
		/* ACTIVE */   {     mount_volume_do_nothing, mount_volume_activate,   mount_volume_activate   },
		/* INACTIVE */ {     mount_volume_deactivate, mount_volume_do_nothing, mount_volume_do_nothing },
		/* EMPTY */    {     mount_volume_deactivate, mount_volume_deactivate, mount_volume_do_nothing }
	};	

	NautilusVolume *volume;
	NautilusVolumeMonitor *monitor;
	NautilusVolumeState old_state;

	g_assert (data != NULL);

	volume = data;
	monitor = NAUTILUS_VOLUME_MONITOR (callback_data);

  	old_state = volume->state;

  	mount_volume_set_state (volume, monitor);

  	if (old_state != volume->state) {
		(* state_transitions[volume->state][old_state]) (monitor, volume);
  	}
}

static void
mount_volumes_update_is_mounted (NautilusVolumeMonitor *monitor)
{
	FILE *fh;
	char line[PATH_MAX * 3], mntpoint[PATH_MAX], devname[PATH_MAX];
	GList *element;
	NautilusVolume *volume;

	/* Toggle mount state to off and then recheck in mtab. */
	for (element = monitor->details->volumes; element != NULL; element = element->next) {
		volume = element->data;
		volume->is_mounted = FALSE;
	}

	/* Open mtab */
	fh = fopen (_PATH_MOUNTED, "r");
	if (!fh) {		
    		return;
    	}

	while (fgets (line, sizeof(line), fh)) {
		sscanf (line, "%s %s", devname, mntpoint);
    		volume = g_hash_table_lookup (monitor->details->volumes_by_fsname, devname);

    		if(volume) {
      			volume->is_mounted = TRUE;
      		}
  	}

	fclose (fh);
}

static gint
mount_volumes_check_status (NautilusVolumeMonitor *monitor)
{
	mount_volumes_update_is_mounted (monitor);

	g_list_foreach (monitor->details->volumes,
			mount_volume_check_change,
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
	int euid = geteuid ();
	int egid = getegid ();

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
mount_volume_floppy_add (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	volume->mount_type 	= g_strdup ("floppy");
	volume->type 		= VOLUME_FLOPPY;
	return TRUE;
#if 0
	if (check_permissions (volume->fsname, R_OK)) {
		return FALSE;
	}
	
	return TRUE;
#endif
}

static gboolean
mount_volume_ext2_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->fsname, R_OK)) {		
		return FALSE;
	}

	/* Only mount root partition for now */
	if (strcmp (volume->mount_path, "/") != 0) {
		return FALSE;
	}

	volume->mount_type = g_strdup ("blockdevice");
	volume->type = VOLUME_EXT2;
		
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
mount_volume_iso9660_add (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{		
	volume->volume_fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);
	if (volume->volume_fd < 0) {
		return FALSE;
	}
	
	volume->type = VOLUME_CDROM;

	/* It's probably not a CD-ROM drive */
	if (ioctl (volume->volume_fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}

	cdrom_ioctl_frenzy (volume->volume_fd);
	close (volume->volume_fd); volume->volume_fd = -1;

	volume->mount_type = g_strdup ("cdrom");

	return TRUE;
}


/* This is here because mtab lists volumes by their symlink-followed names rather than what is listed in fstab. *sigh* */
static void
mount_volume_add_aliases (NautilusVolumeMonitor *monitor, const char *alias, NautilusVolume *volume)
{
	char buf[PATH_MAX];
	int buflen;

	g_hash_table_insert (monitor->details->volumes_by_fsname, (gpointer)alias, volume);

	buflen = readlink (alias, buf, sizeof(buf));
	if (buflen < 1) {
    		return;
    	}

	buf[buflen] = '\0';

	if (buf[0] != '/') {
		char buf2[PATH_MAX];
		char *dn;

    		dn = g_dirname(alias);
    		sprintf (buf2, "%s/%s", dn, buf);
    		g_free (dn);
    		strcpy (buf, buf2);
	}

	mount_volume_add_aliases (monitor, g_strdup(buf), volume);
}


static void
add_mount_volume (NautilusVolumeMonitor *monitor, struct mntent *ent)
{
	NautilusVolume *newdev = NULL;
	gboolean mounted;

	newdev = g_new0 (NautilusVolume, 1);
	g_assert (newdev);
	newdev->volume_fd   = -1;
	newdev->fsname 	    = g_strdup (ent->mnt_fsname);
	newdev->mount_path  = g_strdup (ent->mnt_dir);
	newdev->volume_name = NULL;
	newdev->state 	    = STATE_EMPTY;

	mounted = FALSE;
	
	if (strcmp (ent->mnt_type, NAUTILUS_MOUNT_TYPE_ISO9660) == 0) {		
    		mounted = mount_volume_iso9660_add (monitor, newdev); 
	} else if (strncmp (ent->mnt_fsname, FLOPPY_DEVICE_PATH, sizeof (FLOPPY_DEVICE_PATH) - 1) == 0) {		
		mounted = mount_volume_floppy_add (monitor, newdev);
	} else if (strcmp (ent->mnt_type, NAUTILUS_MOUNT_TYPE_EXT2) == 0) {		
		mounted = mount_volume_ext2_add (newdev);
	}
	
	if (mounted) {
		newdev->is_read_only = strstr (ent->mnt_opts, MNTOPT_RO) != NULL;
		monitor->details->volumes = g_list_append (monitor->details->volumes, newdev);
		mount_volume_add_aliases (monitor, newdev->fsname, newdev);
	} else {
		close (newdev->volume_fd);
		g_free (newdev->fsname);
		g_free (newdev->mount_path);
		g_free (newdev);		
	}
}

static gboolean
mntent_is_removable_fs (struct mntent *ent)
{
	/* FIXME: this does not detect removable volumes that are not
           CDs or floppies (e.g. zip drives, DVD-ROMs, those weird 20M
           super floppies, etc) */

	/* FIXME: it's incorrect to assume that all ISO9660 volumes
           are removable; you could create one as a "filesystem in a
           file" for testing purposes. */

	if (strcmp (ent->mnt_type, NAUTILUS_MOUNT_TYPE_ISO9660) == 0) {
		return TRUE;
	}
	
	if (strncmp (ent->mnt_fsname, FLOPPY_DEVICE_PATH, sizeof (FLOPPY_DEVICE_PATH) - 1) == 0) {
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

static void
find_volumes (NautilusVolumeMonitor *monitor)
{
	FILE *mef;
	struct mntent *ent;

	mef = setmntent (_PATH_MNTTAB, "r");
	g_return_if_fail (mef);

	while ((ent = getmntent (mef))) {
#if 0
		/* Think some more about these checks */
		/* Check for removable volume */
		if (!mntent_is_removable_fs (ent)) {
			continue;
		}

		if (!mntent_has_option (ent->mnt_opts, MOUNT_OPTIONS_USER)
			&& !mntent_has_option (ent->mnt_opts, MOUNT_OPTIONS_OWNER)) {
			continue;
		}
#endif
		/* Add it to our list of mount points */
		add_mount_volume (monitor, ent);
	}


  	endmntent (mef);

	g_list_foreach (monitor->details->volumes, (GFunc) mount_volume_set_state, monitor);

	/* Manually set state of all volumes to empty so we update */
	g_list_foreach (monitor->details->volumes, (GFunc) volume_set_state_empty, monitor);

	/* make sure the mount states of disks are set up */
	mount_volumes_check_status (monitor);

	/* Add a timer function to check for status change in mounted volumes periodically */
	monitor->details->mount_volume_timer_id = 
		gtk_timeout_add (NAUTILUS_CHECK_INTERVAL, (GtkFunction) mount_volumes_check_status, monitor);
}

void
nautilus_volume_monitor_each_volume (NautilusVolumeMonitor *monitor, 
				     NautilusEachVolumeFunction function,
				     gpointer context)
{
	GList *p;

	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		if ((* function) ((NautilusVolume *) p->data, context)) {
			break;
		}
	}
}

void
nautilus_volume_monitor_each_mounted_volume (NautilusVolumeMonitor *monitor, 
					     NautilusEachVolumeFunction function,
					     gpointer context)
{
	GList *p;
	NautilusVolume *volume;

	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		volume = (NautilusVolume *) p->data;
		if (volume->is_mounted && (* function) (volume, context)) {
			break;
		}
	}
}

gboolean
nautilus_volume_monitor_mount_unmount_removable (NautilusVolumeMonitor *monitor, const char *mount_point)
{
	gboolean is_mounted, found_volume;
	char *argv[3];
	GList *p;
	NautilusVolume *volume;
	int exec_err;
	
	is_mounted = FALSE;
	found_volume = FALSE;
	volume = NULL;
	
	/* Locate NautilusVolume for mount point */
	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		volume = p->data;
		if (strcmp (mount_point, volume->mount_path) == 0) {
			found_volume = TRUE;
			break;
		}
	}
			
	/* Get mount state and then decide to mount/unmount the volume */
	if (found_volume) {
		is_mounted = nautilus_volume_monitor_volume_is_mounted (mount_point);
		argv[1] = (char *)mount_point;
		argv[2] = NULL;

		if (is_mounted) {
			/* Unmount */
			argv[0] = "/bin/umount";
		} else {
			/* Mount */
			argv[0] = "/bin/mount";
		}
		exec_err = gnome_execute_async (g_get_home_dir(), 2, argv);
		is_mounted = !is_mounted;
	}

	return is_mounted;
}

static void
free_volume_info (NautilusVolume *volume, NautilusVolumeMonitor *monitor)
{
	if (volume->volume_fd != -1) {
		close (volume->volume_fd);
		volume->volume_fd = -1;
	}
	
	g_free (volume->fsname);
	volume->fsname = NULL;

	g_free (volume->mount_path);
	volume->mount_path = NULL;

	g_free (volume->mount_type);
	volume->mount_type = NULL;

	g_free (volume->volume_name);
	volume->volume_name = NULL;
}

static void
get_iso9660_volume_name (NautilusVolume *volume)
{
	struct iso_primary_descriptor iso_buffer;

	lseek (volume->volume_fd, (off_t) 2048*16, SEEK_SET);
	read (volume->volume_fd, &iso_buffer, 2048);
	
	g_free (volume->volume_name);
	
	volume->volume_name = g_strdup (iso_buffer.volume_id);
	if (volume->volume_name == NULL) {
		volume->volume_name = g_strdup (volume->mount_type);
	}
}

static void
get_ext2_volume_name (NautilusVolume *volume)
{
	volume->volume_name = g_strdup ("Ext2 Volume");
}

static void
get_floppy_volume_name (NautilusVolume *volume)
{
	volume->volume_name = g_strdup ("Floppy");
}
