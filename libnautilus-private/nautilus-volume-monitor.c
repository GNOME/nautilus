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
#include "nautilus-string.h"
#include "nautilus-volume-monitor.h"
#include <errno.h>
#include <fcntl.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if HAVE_SYS_VFSTAB_H
#include <sys/vfstab.h>
#else
#include <fstab.h>
#endif

#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#else
#include <mntent.h>
#endif

#define CHECK_STATUS_INTERVAL 2000

#ifndef _PATH_MOUNTED
#define _PATH_MOUNTED "/etc/mnttab"
#endif

#define MOUNT_OPTIONS_USER "user"
#define MOUNT_OPTIONS_OWNER "owner"

#define FLOPPY_MOUNT_PATH_PREFIX "/mnt/fd"
#define FLOPPY_DEVICE_PATH_PREFIX "/dev/fd"

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
static void     get_iso9660_volume_name                              	(NautilusVolume             	*volume,
									 int                             volume_fd);
static void     get_ext2_volume_name                               	(NautilusVolume             	*volume);
static void     get_floppy_volume_name                           	(NautilusVolume             	*volume);
static void     mount_volume_mount                               	(NautilusVolumeMonitor      	*view,
									 NautilusVolume             	*volume);
static void	mount_volume_activate 					(NautilusVolumeMonitor 	  	*view, 
									 NautilusVolume 		*volume);
static void     mount_volume_deactivate                               	(NautilusVolumeMonitor      	*monitor,
									 NautilusVolume             	*volume);
static void     mount_volume_activate_floppy                          	(NautilusVolumeMonitor      	*view,
									 NautilusVolume             	*volume);
static void	free_volume             				(NautilusVolume             	*volume);
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

	find_volumes (monitor);
}

static void
nautilus_volume_monitor_initialize_class (NautilusVolumeMonitorClass *klass)
{
	GtkObjectClass		*object_class;

	object_class = GTK_OBJECT_CLASS (klass);

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
	g_list_foreach (monitor->details->volumes, (GFunc) free_volume, NULL);
	
	/* Clean up details */	 
	g_hash_table_destroy (monitor->details->volumes_by_fsname);
	g_list_free (monitor->details->volumes);	
	g_free (monitor->details);

	global_volume_monitor = NULL;

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
unref_global_volume_monitor (void)
{
	gtk_object_unref (GTK_OBJECT (global_volume_monitor));
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
		gtk_object_ref (GTK_OBJECT (global_volume_monitor));
		gtk_object_sink (GTK_OBJECT (global_volume_monitor));
		g_atexit (unref_global_volume_monitor);
	}

	return global_volume_monitor;
}

static int
floppy_sort (NautilusVolume *volume1, NautilusVolume *volume2) 
{
	gboolean is_floppy_1, is_floppy_2;

	is_floppy_1 = volume1->type == NAUTILUS_VOLUME_FLOPPY;
	is_floppy_2 = volume2->type == NAUTILUS_VOLUME_FLOPPY;

	if (is_floppy_1 && !is_floppy_2) {
		return -1;
	}
	if (!is_floppy_1 && is_floppy_2) {
		return +1;
	}
	return 0;
}

gboolean		
nautilus_volume_monitor_volume_is_removable (NautilusVolume *volume)
{
	/* FIXME bugzilla.eazel.com 2450: 
	   this does not detect removable volumes that are not
           CDs or floppies (e.g. zip drives, DVD-ROMs, those weird 20M
           super floppies, etc) */

	switch (volume->type) {
	case NAUTILUS_VOLUME_CDROM:
	case NAUTILUS_VOLUME_UDF:
	case NAUTILUS_VOLUME_FLOPPY:
		return TRUE;
		break;

	case NAUTILUS_VOLUME_EXT2:	
	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_FAT:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
	case NAUTILUS_VOLUME_MSDOS:
	case NAUTILUS_VOLUME_NFS:
	case NAUTILUS_VOLUME_PROC:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UFS:
	case NAUTILUS_VOLUME_UNSDOS:
	case NAUTILUS_VOLUME_VFAT:
	case NAUTILUS_VOLUME_XENIX:
	case NAUTILUS_VOLUME_XIAFS:
		return FALSE;
		break;
				
	default:
		return FALSE;
		break;
	}
}


GList *
nautilus_volume_monitor_get_removable_volumes (NautilusVolumeMonitor *monitor)
{
	GList *list, *p;
	NautilusVolume *volume;

	list = NULL;
	
	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		volume = p->data;
		if (nautilus_volume_monitor_volume_is_removable (volume)) {
			list = g_list_prepend (list, volume);
		}		
	}

	/* Move all floppy mounts to top of list */
	return g_list_sort (g_list_reverse (list), (GCompareFunc) floppy_sort);

	
}

/* nautilus_volume_monitor_get_volume_name
 *	
 * Returns name of volume in human readable form
 */
 
char *
nautilus_volume_monitor_get_volume_name (const NautilusVolume *volume)
{
	int index;
	char *name;

	name = g_strdup (volume->volume_name);

	/* Strip whitespace from the end of the name. */
	g_strchomp (name);

	/* The volume name may have '/' characters. We need to convert
	 * them to something that's suitable for use in the name of a
	 * link on the desktop.
	 */
	for (index = 0; ; index++) {
		if (name [index] == '\0') {
			break;
		}
		if (name [index] == '/') {
			name [index] = '-';
		}
	}
	return name;
}

gboolean
nautilus_volume_monitor_volume_is_mounted (const NautilusVolume *volume)
{
	FILE *fh;
	char line[PATH_MAX * 3];
	char mntpoint[sizeof (line)], devname[sizeof (line)];
	
	/* Open mtab */
	fh = fopen (_PATH_MOUNTED, "r");

	g_return_val_if_fail (fh != NULL, FALSE);
    	
	while (fgets (line, sizeof (line), fh)) {
		/* No chance of overflow on this because both device_name
		 * is as big as line is.
		 */
		if (sscanf (line, "%s %s", devname, mntpoint) == 2) {
			if (strcmp (mntpoint, volume->mount_path) == 0) {
				fclose (fh);	
				return TRUE;
			}
		}
	}
	
	fclose (fh);
	return FALSE;
}

static void
mount_volume_cdrom_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int fd;

	fd = open (volume->fsname, O_RDONLY | O_NONBLOCK);

	if (ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK) {
		int disctype;

		disctype = ioctl (fd, CDROM_DISC_STATUS, CDSL_CURRENT);
		switch (disctype) {
		case CDS_AUDIO:
			/* It's pretty hard to know whether it is actually in use */
			volume->state = NAUTILUS_VOLUME_INACTIVE;
			break;
			
		case CDS_DATA_1:
		case CDS_DATA_2:
		case CDS_XA_2_1:
		case CDS_XA_2_2:
		case CDS_MIXED:
			/* Check if it is mounted */
			if (volume->is_mounted) {
				volume->state = NAUTILUS_VOLUME_ACTIVE;
			} else {
				volume->state = NAUTILUS_VOLUME_EMPTY;
			}
			break;
			
		default:
			volume->state = NAUTILUS_VOLUME_EMPTY;
			break;
		}
	} else {
		volume->state = NAUTILUS_VOLUME_EMPTY;
	}
	
	close (fd);
}


static void
mount_volume_floppy_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	/* If the floppy is not in mtab, then we set it to empty */
	if (nautilus_volume_monitor_volume_is_mounted (volume)) {
		volume->state = NAUTILUS_VOLUME_ACTIVE;
	} else {
		volume->state = NAUTILUS_VOLUME_EMPTY;
	}
}

static void
mount_volume_generic_set_state (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	volume->state = NAUTILUS_VOLUME_ACTIVE;
}

static void
mount_volume_set_state (NautilusVolume *volume, NautilusVolumeMonitor *monitor)
{
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDROM:
		mount_volume_cdrom_set_state (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_FLOPPY:
		mount_volume_floppy_set_state (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_EXT2:
	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_FAT:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
	case NAUTILUS_VOLUME_MSDOS:
	case NAUTILUS_VOLUME_NFS:
	case NAUTILUS_VOLUME_PROC:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UDF:
	case NAUTILUS_VOLUME_UFS:
	case NAUTILUS_VOLUME_UNSDOS:
	case NAUTILUS_VOLUME_VFAT:
	case NAUTILUS_VOLUME_XENIX:
	case NAUTILUS_VOLUME_XIAFS:
		mount_volume_generic_set_state (monitor, volume);
		break;
		
	default:
		break;
	}
}

static void
volume_set_state_empty (NautilusVolume *volume, NautilusVolumeMonitor *monitor)
{
	volume->state = NAUTILUS_VOLUME_EMPTY;
}

static void
mount_volume_mount (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	volume->did_mount = TRUE;	
}

static void
mount_volume_activate_cdrom (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int fd, disctype;

	fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);

	disctype = ioctl (fd, CDROM_DISC_STATUS, CDSL_CURRENT);
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
		get_iso9660_volume_name (volume, fd);
		mount_volume_mount (monitor, volume);
		break;
		
	default:			
		break;
  	}
	
	close (fd);
}

static void
mount_volume_activate_floppy (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	get_floppy_volume_name (volume);
	mount_volume_mount (view, volume);
}

static void
mount_volume_activate_ext2 (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
	get_ext2_volume_name (volume);
	mount_volume_mount (view, volume);
}

static void
mount_volume_activate_generic (NautilusVolumeMonitor *view, NautilusVolume *volume)
{	
	volume->volume_name = g_strdup (_("Unknown Volume"));
	mount_volume_mount (view, volume);
}


typedef void (* ChangeNautilusVolumeFunction) (NautilusVolumeMonitor *view, NautilusVolume *volume);

static void
mount_volume_activate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDROM:
		mount_volume_activate_cdrom (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_FLOPPY:
		mount_volume_activate_floppy (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_EXT2:
		mount_volume_activate_ext2 (monitor, volume);
		break;

	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_FAT:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
	case NAUTILUS_VOLUME_MSDOS:
	case NAUTILUS_VOLUME_NFS:
	case NAUTILUS_VOLUME_PROC:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UDF:
	case NAUTILUS_VOLUME_UFS:
	case NAUTILUS_VOLUME_UNSDOS:
	case NAUTILUS_VOLUME_VFAT:
	case NAUTILUS_VOLUME_XENIX:
	case NAUTILUS_VOLUME_XIAFS:
		mount_volume_activate_generic (monitor, volume);
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
	int fd;

	fd = open (volume->fsname, O_RDONLY|O_NONBLOCK);
	if (fd < 0) {
		return;
	}

	ioctl (fd, CDROMEJECT, 0);

	close (fd);
}

static void
mount_volume_deactivate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDROM:
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
	static const ChangeNautilusVolumeFunction state_transitions
		[NAUTILUS_VOLUME_NUMBER_OF_STATES]
		[NAUTILUS_VOLUME_NUMBER_OF_STATES] = {
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
	char line[PATH_MAX * 3], device_name[sizeof (line)];
	GList *p;
	NautilusVolume *volume;

	/* Open mtab */
	fh = fopen (_PATH_MOUNTED, "r");

	g_return_if_fail (fh != NULL);

	/* Toggle mount state to off and then recheck in mtab. */
	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		volume = p->data;

		volume->is_mounted = FALSE;
	}

	while (fgets (line, sizeof(line), fh)) {
		/* No chance of overflow on this because device_name
		 * is as big as line is.
		 */
		if (sscanf (line, "%s", device_name) == 1) {
			volume = g_hash_table_lookup (monitor->details->volumes_by_fsname,
						      device_name);
			if (volume != NULL) {
				volume->is_mounted = TRUE;
			}
		}
  	}

	fclose (fh);
}

static int
mount_volumes_check_status (NautilusVolumeMonitor *monitor)
{
	mount_volumes_update_is_mounted (monitor);

	g_list_foreach (monitor->details->volumes,
			mount_volume_check_change,
			monitor);
	
	return TRUE;
}


/* Like access, but a bit more accurate in one way - access will let
 * root do anything. Less accurate in other ways: does not get
 * read-only or no-exec filesystems right, and returns FALSE if
 * there's an error.
 */
static gboolean
check_permissions (const char *filename, int mode)
{
	int euid, egid;
	struct stat statbuf;

	euid = geteuid ();
	egid = getegid ();
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
	volume->type = NAUTILUS_VOLUME_FLOPPY;
	return TRUE;
}

static gboolean
mount_volume_ext2_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->fsname, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_EXT2;
		
	return TRUE;
}

static gboolean
mount_volume_udf_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->fsname, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_UDF;
		
	return TRUE;
}

static gboolean
mount_volume_vfat_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->fsname, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_VFAT;
		
	return TRUE;
}

static gboolean
mount_volume_msdos_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->fsname, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_MSDOS;
		
	return TRUE;
}

static void
cdrom_ioctl_frenzy (int fd)
{
	ioctl (fd, CDROM_CLEAR_OPTIONS, CDO_LOCK | CDO_AUTO_CLOSE | CDO_AUTO_EJECT);
	ioctl (fd, CDROM_SET_OPTIONS, CDO_USE_FFLAGS | CDO_CHECK_TYPE);
	ioctl (fd, CDROM_LOCKDOOR, 0);
}

static gboolean
mount_volume_iso9660_add (NautilusVolume *volume)
{
	int fd;

	fd = open (volume->fsname, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return FALSE;
	}
	
	/* If this fails it's probably not a CD-ROM drive */
	if (ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}

	cdrom_ioctl_frenzy (fd);
	close (fd);

	volume->type = NAUTILUS_VOLUME_CDROM;

	return TRUE;
}

static gboolean
mount_volume_affs_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_AFFS;
	return TRUE;
}

static gboolean
mount_volume_fat_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_FAT;
	return TRUE;
}

static gboolean
mount_volume_hpfs_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_HPFS;
	return TRUE;
}

static gboolean
mount_volume_minix_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_MINIX;
	return TRUE;
}

static gboolean
mount_volume_nfs_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_NFS;
	return TRUE;
}

static gboolean
mount_volume_proc_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_PROC;
	return TRUE;
}

static gboolean
mount_volume_smb_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_SMB;
	return TRUE;
}

static gboolean
mount_volume_unsdos_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_UNSDOS;
	return TRUE;
}

static gboolean
mount_volume_xenix_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_XENIX;
	return TRUE;
}

static gboolean
mount_volume_xiafs_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_XIAFS;
	return TRUE;
}


/* This is here because mtab lists volumes by their symlink-followed names rather than what is listed in fstab. */
static void
mount_volume_add_aliases (NautilusVolumeMonitor *monitor, const char *alias, NautilusVolume *volume)
{
	char buf[PATH_MAX + 1];
	int buflen;
	char *directory_path, *path;

	g_hash_table_insert (monitor->details->volumes_by_fsname, (gpointer)alias, volume);

	buflen = readlink (alias, buf, sizeof(buf) - 1);
	if (buflen < 1) {
    		return;
    	}

	buf[buflen] = '\0';

	if (buf[0] == '/') {
		path = g_strdup (buf);
	} else {
		directory_path = g_dirname (alias);
    		path = g_strconcat (directory_path,
				    "/",
				    buf,
				    NULL);
		g_free (directory_path);
	}

	mount_volume_add_aliases (monitor, path, volume);
}

#if HAVE_SYS_MNTTAB_H

static void
mnttab_add_mount_volume (NautilusVolumeMonitor *monitor, struct mnttab *tab) 
{
	NautilusVolume *volume;
	gboolean mounted;

	volume = g_new0 (NautilusVolume, 1);
	volume->fsname = g_strdup (tab->mnt_fstype);
	volume->mount_path = g_strdup (tab->mnt_mountp);

	mounted = FALSE;

	if (strcmp (tab->mnt_fstype, "iso9660") == 0) {
		mounted = mount_volume_iso9660_add (volume);
	} else if (nautilus_str_has_prefix (volume->fsname, FLOPPY_DEVICE_PATH_PREFIX)) {
		mounted = mount_volume_floppy_add (monitor, volume);
	} else if (strcmp (tab->mnt_fstype, "ufs") == 0) {
		mounted = mount_volume_ext2_add (volume);
	} else if (strcmp (tab->mnt_fstype, "udf") == 0) {		
		mounted = mount_volume_udf_add (volume);
	}


	if (mounted) {
		volume->is_read_only = strstr (tab->mnt_mntopts, "r") != NULL;
		monitor->details->volumes = g_list_append (monitor->details->volumes, volume);
		mount_volume_add_aliases (monitor, volume->fsname, volume);
	} else {
		g_free (volume->fsname);
		g_free (volume->mount_path);
		g_free (volume);
	}
}

#else /* !HAVE_SYS_MNTTAB_H */

static void
mntent_add_mount_volume (NautilusVolumeMonitor *monitor, struct mntent *ent)
{
	NautilusVolume *volume;
	gboolean mounted;

	volume = g_new0 (NautilusVolume, 1);
	volume->fsname = g_strdup (ent->mnt_fsname);
	volume->mount_path = g_strdup (ent->mnt_dir);

	mounted = FALSE;
	
	if (nautilus_str_has_prefix (ent->mnt_fsname, FLOPPY_DEVICE_PATH_PREFIX)) {		
		mounted = mount_volume_floppy_add (monitor, volume);
	} else if (strcmp (ent->mnt_type, "affs") == 0) {		
		mounted = mount_volume_affs_add (volume);
	} else if (strcmp (ent->mnt_type, "ext2") == 0) {		
		mounted = mount_volume_ext2_add (volume);
	} else if (strcmp (ent->mnt_type, "fat") == 0) {		
		mounted = mount_volume_fat_add (volume);
	} else if (strcmp (ent->mnt_type, "hpfs") == 0) {		
		mounted = mount_volume_hpfs_add (volume);
	} else if (strcmp (ent->mnt_type, "iso9660") == 0) {		    		
		mounted = mount_volume_iso9660_add (volume);
	} else if (strcmp (ent->mnt_type, "minix") == 0) {		    		
		mounted = mount_volume_minix_add (volume);
	} else if (strcmp (ent->mnt_type, "msdos") == 0) {		
		mounted = mount_volume_msdos_add (volume);
	} else if (strcmp (ent->mnt_type, "nfs") == 0) {		
		mounted = mount_volume_nfs_add (volume);
	} else if (strcmp (ent->mnt_type, "proc") == 0) {		
		mounted = mount_volume_proc_add (volume);
	} else if (strcmp (ent->mnt_type, "smb") == 0) {		
		mounted = mount_volume_smb_add (volume);
	} else if (strcmp (ent->mnt_type, "udf") == 0) {		
		mounted = mount_volume_udf_add (volume);
	} else if (strcmp (ent->mnt_type, "ufs") == 0) {		
		mounted = mount_volume_udf_add (volume);
	} else if (strcmp (ent->mnt_type, "unsdos") == 0) {		
		mounted = mount_volume_unsdos_add (volume);
	} else if (strcmp (ent->mnt_type, "vfat") == 0) {		
		mounted = mount_volume_vfat_add (volume);
	} else if (strcmp (ent->mnt_type, "xenix") == 0) {		
		mounted = mount_volume_xenix_add (volume);
	} else if (strcmp (ent->mnt_type, "xiafs") == 0) {		
		mounted = mount_volume_xiafs_add (volume);
	}

	if (mounted) {
		volume->is_read_only = strstr (ent->mnt_opts, MNTOPT_RO) != NULL;
		monitor->details->volumes = g_list_append (monitor->details->volumes, volume);
		mount_volume_add_aliases (monitor, volume->fsname, volume);
	} else {
		g_free (volume->fsname);
		g_free (volume->mount_path);
		g_free (volume);		
	}
}
#endif /* HAVE_SYS_MNTTAB_H */

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
#if HAVE_SYS_MNTTAB_H
	struct mnttab *tab;
#else
	struct mntent *ent;
#endif

#if HAVE_SETMNTENT
	mef = setmntent (_PATH_MNTTAB, "r");
#else
	mef = fopen ("/etc/mtab", "r");
#endif

	g_return_if_fail (mef != NULL);

#if HAVE_SYS_MNTTAB_H
	while (getmntent (mef, tab) != 0) {
		/* Add it to our list of mount points */
		mnttab_add_mount_volume (monitor, tab);
	}

#else /* !HAVE_SYS_MNTTAB_H */

	while ((ent = getmntent (mef)) != NULL) {
		/* Add it to our list of mount points */
		mntent_add_mount_volume (monitor, ent);
	}
#endif /* HAVE_SYS_MNTTAB_H */

#if HAVE_ENDMNTENT
  	endmntent (mef);
#else
	fclose (mef);
#endif

	g_list_foreach (monitor->details->volumes, (GFunc) mount_volume_set_state, monitor);

	/* Manually set state of all volumes to empty so we update */
	g_list_foreach (monitor->details->volumes, (GFunc) volume_set_state_empty, monitor);

	/* make sure the mount states of disks are set up */
	mount_volumes_check_status (monitor);

	/* Add a timer function to check for status change in mounted volumes periodically */
	monitor->details->mount_volume_timer_id = 
		gtk_timeout_add (CHECK_STATUS_INTERVAL,
				 (GtkFunction) mount_volumes_check_status,
				 monitor);
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
nautilus_volume_monitor_mount_unmount_removable (NautilusVolumeMonitor *monitor,
						 const char *mount_point)
{
	gboolean is_mounted, found_volume;
	char *argv[3];
	GList *p;
	NautilusVolume *volume;
	int exec_err;
	
	is_mounted = FALSE;
	found_volume = FALSE;
	
	/* Locate NautilusVolume for mount point */
	volume = NULL;
	for (p = monitor->details->volumes; p != NULL; p = p->next) {
		volume = p->data;

		if (strcmp (mount_point, volume->mount_path) == 0) {
			found_volume = TRUE;
			break;
		}
	}
			
	/* Get mount state and then decide to mount/unmount the volume */
	if (found_volume) {
		is_mounted = nautilus_volume_monitor_volume_is_mounted (volume);

		argv[0] = is_mounted ? "/bin/umount" : "/bin/mount";
		argv[1] = (char *) mount_point;
		argv[2] = NULL;

		exec_err = gnome_execute_async (g_get_home_dir(), 2, argv);
		if (exec_err == -1) {
			is_mounted = !is_mounted;
		}
	}

	return is_mounted;
}

static void
free_volume (NautilusVolume *volume)
{
	g_free (volume->fsname);
	g_free (volume->mount_path);
	g_free (volume->volume_name);
	g_free (volume);
}

static void
get_iso9660_volume_name (NautilusVolume *volume, int fd)
{
	struct iso_primary_descriptor iso_buffer;

	lseek (fd, (off_t) 2048*16, SEEK_SET);
	read (fd, &iso_buffer, 2048);
	
	g_free (volume->volume_name);
	
	volume->volume_name = g_strdup (iso_buffer.volume_id);
	if (volume->volume_name == NULL) {
		volume->volume_name = g_strdup (_("ISO 9660 Volume"));
	}
}

static void
get_ext2_volume_name (NautilusVolume *volume)
{
	volume->volume_name = g_strdup (_("Ext2 Volume"));
}

static void
get_floppy_volume_name (NautilusVolume *volume)
{
	volume->volume_name = g_strdup (_("Floppy"));
}
