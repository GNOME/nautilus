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

/*#define MOUNT_AUDIO_CD 1*/

#include <config.h>
#include "nautilus-volume-monitor.h"

#include "nautilus-cdrom-extensions.h"
#include "nautilus-directory-notify.h"
#include "nautilus-file-utilities.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-iso9660.h"
#include "nautilus-stock-dialogs.h"
#include "nautilus-string.h"
#include "nautilus-string-list.h"
#include "nautilus-volume-monitor.h"
#include <errno.h>
#include <fcntl.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <pthread.h>
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

#include <mntent.h>

#ifdef MOUNT_AUDIO_CD

#define size16 short
#define size32 int

#define CD_AUDIO_PATH "/dev/cdrom"
#define CD_AUDIO_URI "cdda:///dev/cdrom"

#include <cdda_interface.h>
#include <cdda_paranoia.h>
#endif

#define CHECK_STATUS_INTERVAL 2000

#define PATH_PROC_MOUNTS "/proc/mounts"

#define FLOPPY_MOUNT_PATH_PREFIX "/mnt/fd"
#define FLOPPY_DEVICE_PATH_PREFIX "/dev/fd"

struct NautilusVolumeMonitorDetails
{
	GList *mounts;
	GList *removable_volumes;
	guint mount_volume_timer_id;
};

static NautilusVolumeMonitor *global_volume_monitor = NULL;


/* The NautilusVolumeMonitor signals.  */
enum {
	VOLUME_MOUNTED,
	VOLUME_UNMOUNTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static void		nautilus_volume_monitor_initialize 		(NautilusVolumeMonitor 		*desktop_mounter);
static void		nautilus_volume_monitor_initialize_class 	(NautilusVolumeMonitorClass 	*klass);
static void		nautilus_volume_monitor_destroy 		(GtkObject 			*object);
static void     	get_iso9660_volume_name                         (NautilusVolume             	*volume,
									 int                             volume_fd);
static void     	get_ext2_volume_name                         	(NautilusVolume             	*volume);
static void     	get_msdos_volume_name                         	(NautilusVolume             	*volume);
static void     	get_nfs_volume_name                         	(NautilusVolume             	*volume);
static void     	get_floppy_volume_name                         	(NautilusVolume             	*volume);
static void     	get_generic_volume_name                         (NautilusVolume             	*volume);
static void		mount_volume_get_name 				(NautilusVolume 		*volume);
static void		mount_volume_activate 				(NautilusVolumeMonitor 	  	*view, 
									 NautilusVolume 		*volume);
static void     	mount_volume_deactivate                          (NautilusVolumeMonitor      	*monitor,
									 NautilusVolume             	*volume);
static void     	mount_volume_activate_floppy                    (NautilusVolumeMonitor      	*view,
									 NautilusVolume             	*volume);
static gboolean 	mount_volume_add_filesystem 			(NautilusVolume 		*volume);
static NautilusVolume	*copy_volume					(NautilusVolume             	*volume);									 
static void		find_volumes 					(NautilusVolumeMonitor 		*monitor);
static void		free_mount_list 				(GList 				*mount_list);
static GList 		*get_removable_volumes 				(void);

#ifdef MOUNT_AUDIO_CD
static cdrom_drive 	*open_cdda_device 				(GnomeVFSURI 			*uri);
static gboolean 	locate_audio_cd 				(void);
#endif

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusVolumeMonitor,
				   nautilus_volume_monitor,
				   GTK_TYPE_OBJECT)

static void
nautilus_volume_monitor_initialize (NautilusVolumeMonitor *monitor)
{
	/* Set up details */
	monitor->details = g_new0 (NautilusVolumeMonitorDetails, 1);	
	monitor->details->mounts = NULL;
	monitor->details->removable_volumes = NULL;
	
	monitor->details->removable_volumes = get_removable_volumes ();
	
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
		
	/* Clean up mount info */
	free_mount_list (monitor->details->mounts);
	free_mount_list (monitor->details->removable_volumes);

	/* Clean up details */	 
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
floppy_sort (const NautilusVolume *volume1, const NautilusVolume *volume2) 
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
nautilus_volume_monitor_volume_is_removable (const NautilusVolume *volume)
{
	return volume->is_removable;
}


/* nautilus_volume_monitor_get_removable_volumes
 *
 * Accessor. List and internal data is not to be freed.
 */
 
const GList *
nautilus_volume_monitor_get_removable_volumes (NautilusVolumeMonitor *monitor)
{
	return monitor->details->removable_volumes;
}


/* get_removable_volumes
 *	
 * Returns a list a device paths.
 * Caller needs to free these as well as the list.
 */

static GList *
get_removable_volumes (void)
{
	FILE *file;
	GList *volumes;
	struct mntent *ent;
	NautilusVolume *volume;
	
	volumes = NULL;
	
	file = setmntent (_PATH_MNTTAB, "r");
	if (file == NULL) {
		return NULL;
	}
	
	while ((ent = getmntent (file)) != NULL) {
		/* Use noauto as our way of determining a removable volume */
		if (strstr (ent->mnt_opts, MNTOPT_NOAUTO) != NULL) {
			volume = g_new0 (NautilusVolume, 1);
			volume->device_path = g_strdup (ent->mnt_fsname);
			volume->mount_path = g_strdup (ent->mnt_dir);
			volume->filesystem = g_strdup (ent->mnt_type);
			if (mount_volume_add_filesystem (volume)) {				
				volumes = g_list_append (volumes, volume);
			} else {
				nautilus_volume_monitor_free_volume (volume);
			}
		}	
	}
			
	fclose (file);
	
#ifdef MOUNT_AUDIO_CD
	volume = g_new0 (NautilusVolume, 1);
	volume->device_path = g_strdup ("/dev/cdrom");
	volume->mount_path = g_strdup ("/dev/cdrom");
	volume->filesystem = g_strdup ("cdda");
	volumes = g_list_append (volumes, volume);
#endif

	/* Move all floppy mounts to top of list */
	return g_list_sort (g_list_reverse (volumes), (GCompareFunc) floppy_sort);
}


static gboolean
volume_is_removable (const NautilusVolume *volume)
{
	FILE *file;
	struct mntent *ent;
	
	file = setmntent (_PATH_MNTTAB, "r");
	if (file == NULL) {
		return FALSE;
	}
	
	/* Search for our device in the fstab */
	while ((ent = getmntent (file)) != NULL) {
		if (strcmp (volume->device_path, ent->mnt_fsname) == 0) {
			/* Use noauto as our way of determining a removable volume */
			if (strstr (ent->mnt_opts, MNTOPT_NOAUTO) != NULL) {
				fclose (file);
				return TRUE;
			}
		}	
	}
				
	fclose (file);
	return FALSE;
}

static gboolean
volume_is_read_only (const NautilusVolume *volume)
{
	FILE *file;
	struct mntent *ent;
	
	file = setmntent (_PATH_MNTTAB, "r");
	if (file == NULL) {
		return FALSE;
	}
		
	/* Search for our device in the fstab */
	while ((ent = getmntent (file)) != NULL) {
		if (strcmp (volume->device_path, ent->mnt_fsname) == 0) {
			if (strstr (ent->mnt_opts, MNTOPT_RO) != NULL) {
				fclose (file);
				return TRUE;
			}
		}	
	}
				
	fclose (file);
	return FALSE;
}


char *
nautilus_volume_monitor_get_volume_name (const NautilusVolume *volume)
{
	if (volume->volume_name == NULL) {
		return g_strdup ("Unknown");
	}
	
	return g_strdup (volume->volume_name);
}


/* modify_volume_name_for_display
 *	
 * Modify volume to be in human readable form
 */
 
static void
modify_volume_name_for_display (NautilusVolume *volume)
{
	int index;
	char *name;

	if (volume->volume_name == NULL) {
		volume->volume_name = g_strdup ("Unknown");
		return;
	}
	
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
	
	/* Save pretty name back into volume info */
	g_free (volume->volume_name);
	volume->volume_name = g_strdup (name);
}

/* nautilus_volume_monitor_get_target_uri
 *	
 * Returns the activation uri of the volume
 */
 
char *
nautilus_volume_monitor_get_target_uri (const NautilusVolume *volume)
{
	char *uri;

	switch (volume->type) {
	case NAUTILUS_VOLUME_CDDA:
		uri = g_strdup_printf ("cdda://%s", volume->mount_path);
		break;
		
	default:
		uri = gnome_vfs_get_uri_from_local_path (volume->mount_path);
		break;
	}
	
	return uri;
}

gboolean 
nautilus_volume_monitor_should_integrate_trash (const NautilusVolume *volume)
{
	/* Hand-pick a bunch of file system types that we know we can support
	 * trash on. It would probably be harder to keep a list of the ones
	 * we can't try to support trash on because the list would have to be
	 * more definitive.
	 */
	return volume->type == NAUTILUS_VOLUME_EXT2
		|| volume->type == NAUTILUS_VOLUME_FAT
		|| volume->type == NAUTILUS_VOLUME_NFS
		|| volume->type == NAUTILUS_VOLUME_VFAT
		|| volume->type == NAUTILUS_VOLUME_FLOPPY
		|| volume->type == NAUTILUS_VOLUME_SMB;
}

const char *
nautilus_volume_monitor_get_volume_mount_uri (const NautilusVolume *volume)
{
	return volume->mount_path;
}

static void
mount_volume_get_cdrom_name (NautilusVolume *volume)
{
	int fd, disctype;
	
	fd = open (volume->device_path, O_RDONLY|O_NONBLOCK);

	disctype = ioctl (fd, CDROM_DISC_STATUS, CDSL_CURRENT);
	switch (disctype) {
	case CDS_AUDIO:
		volume->volume_name = g_strdup (_("Audio CD"));
		break;
		
	case CDS_DATA_1:
	case CDS_DATA_2:
	case CDS_XA_2_1:
	case CDS_XA_2_2:
	case CDS_MIXED:
		/* Get volume name */
		get_iso9660_volume_name (volume, fd);
		break;
		
	default:			
		break;
  	}
	
	close (fd);
}

static void
mount_volume_get_cdda_name (NautilusVolume *volume)
{
	volume->volume_name = g_strdup (_("Audio CD"));
}


static void
mount_volume_activate_cdda (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int fd, disctype;

	fd = open (volume->device_path, O_RDONLY | O_NONBLOCK);

	disctype = ioctl (fd, CDROM_DISC_STATUS, CDSL_CURRENT);
	switch (disctype) {
	case CDS_AUDIO:
		break;
	default:			
		break;
  	}
	
	close (fd);
}


static void
mount_volume_activate_cdrom (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int fd, disctype;

	fd = open (volume->device_path, O_RDONLY | O_NONBLOCK);

	disctype = ioctl (fd, CDROM_DISC_STATUS, CDSL_CURRENT);
	switch (disctype) {
	case CDS_AUDIO:
		break;
		
	case CDS_DATA_1:
	case CDS_DATA_2:
	case CDS_XA_2_1:
	case CDS_XA_2_2:
	case CDS_MIXED:
		/* Get volume name */
		get_iso9660_volume_name (volume, fd);
		break;
		
	default:			
		break;
  	}
	
	close (fd);
}

static void
mount_volume_activate_floppy (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
}

static void
mount_volume_activate_ext2 (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
}

static void
mount_volume_activate_msdos (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
}

static void
mount_volume_activate_nfs (NautilusVolumeMonitor *view, NautilusVolume *volume)
{
}

static void
mount_volume_activate_generic (NautilusVolumeMonitor *view, NautilusVolume *volume)
{	
}


typedef void (* ChangeNautilusVolumeFunction) (NautilusVolumeMonitor *view, NautilusVolume *volume);


static void
mount_volume_get_name (NautilusVolume *volume)
{
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDDA:
		mount_volume_get_cdda_name (volume);
		break;

	case NAUTILUS_VOLUME_CDROM:
		mount_volume_get_cdrom_name (volume);
		break;
		
	case NAUTILUS_VOLUME_FLOPPY:
		get_floppy_volume_name (volume);
		break;
		
	case NAUTILUS_VOLUME_EXT2:
		get_ext2_volume_name (volume);
		break;

	case NAUTILUS_VOLUME_FAT:
	case NAUTILUS_VOLUME_VFAT:
	case NAUTILUS_VOLUME_MSDOS:
		get_msdos_volume_name (volume);
		break;
	
	case NAUTILUS_VOLUME_NFS:
		get_nfs_volume_name (volume);
		break;
		
	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UDF:
	case NAUTILUS_VOLUME_UFS:
	case NAUTILUS_VOLUME_UNSDOS:
	case NAUTILUS_VOLUME_XENIX:
	case NAUTILUS_VOLUME_XIAFS:
		get_generic_volume_name (volume);
		break;
		
	default:
		g_assert_not_reached ();
		break;
	}
}


static void
mount_volume_activate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	switch (volume->type) {
	case NAUTILUS_VOLUME_CDDA:
		mount_volume_activate_cdda (monitor, volume);
		break;

	case NAUTILUS_VOLUME_CDROM:
		mount_volume_activate_cdrom (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_FLOPPY:
		mount_volume_activate_floppy (monitor, volume);
		break;
		
	case NAUTILUS_VOLUME_EXT2:
		mount_volume_activate_ext2 (monitor, volume);
		break;

	case NAUTILUS_VOLUME_MSDOS:
		mount_volume_activate_msdos (monitor, volume);		
		break;
		
	case NAUTILUS_VOLUME_NFS:
		mount_volume_activate_nfs (monitor, volume);		
		break;
		
	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_FAT:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
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


static void *
eject_device (void *arg)
{
	char *command, *path;	
	
	path = arg;

	if (path != NULL) {		
		command = g_strdup_printf ("eject %s", path);	
		nautilus_gnome_shell_execute (command);
		g_free (command);
		g_free (path);
	}
	
	pthread_exit (NULL);
}

static void
mount_volume_deactivate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	pthread_t eject_thread;

	switch (volume->type) {
	case NAUTILUS_VOLUME_CDROM:
		pthread_create (&eject_thread, NULL, eject_device, g_strdup (volume->device_path));
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
free_mount_list (GList *mount_list)
{
	if (mount_list == NULL) {
		return;
	}
	
	g_list_foreach (mount_list, (GFunc) nautilus_volume_monitor_free_volume, NULL);
	g_list_free (mount_list);
	mount_list = NULL;
}


/* List returned, but not the data it contains, must be freed by caller */
static GList *
build_volume_list_delta (GList *list_one, GList *list_two)
{
	GList *ptrOne, *ptrTwo;
	GList *new_list;
	NautilusVolume *volOne, *volTwo, *new_volume;
	int index;
	gboolean found_match;
	
	new_list = NULL;
		
	for (ptrOne = list_one, index = 0; ptrOne != NULL; ptrOne = ptrOne->next, index++) {
	
		found_match = FALSE;
		volOne = (NautilusVolume *)ptrOne->data;
							
		for (ptrTwo = list_two; ptrTwo != NULL; ptrTwo = ptrTwo->next) {
			
			volTwo = (NautilusVolume *)ptrTwo->data;			

			/* Check and see if mount point from list one is in list two */
			if (strcmp (volOne->mount_path, volTwo->mount_path) == 0) {
				found_match = TRUE;
				break;			
			}			
		}
		
		if (!found_match) {
			/* No match. Add it to the list to be returned; */
			new_volume = copy_volume (volOne);
			new_list = g_list_append (new_list, new_volume);
		}
	}
		
	return new_list;
}


static GList *
get_current_mount_list (void)
{
	GList *current_mounts = NULL;
	NautilusVolume *volume = NULL;
	FILE *fh;
	char line[PATH_MAX * 3];
	char device_name[sizeof (line)];
	NautilusStringList *list;

	/* Open /proc/mounts */
	fh = fopen (PATH_PROC_MOUNTS, "r");
	g_return_val_if_fail (fh != NULL, NULL);

	while (fgets (line, sizeof(line), fh)) {
		if (sscanf (line, "%s", device_name) == 1) {
				list = nautilus_string_list_new_from_tokens (line, " ", FALSE);
			if (list != NULL) {
				/* The string list needs to have at least 3 items per line.
				 * We need to find at least device path, mount path and file system type. */
				if (nautilus_string_list_get_length (list) >= 3) {
					volume = g_new0 (NautilusVolume, 1);
					volume->device_path = nautilus_string_list_nth (list, 0);
					volume->mount_path = nautilus_string_list_nth (list, 1);
					volume->filesystem = nautilus_string_list_nth (list, 2);

					if (mount_volume_add_filesystem (volume)) {
						current_mounts = g_list_append (current_mounts, volume);
					} else {
						nautilus_volume_monitor_free_volume (volume);
					}					
				}				
				nautilus_string_list_free (list);
			}			
		}
  	}

#ifdef MOUNT_AUDIO_CD
	/* CD Audio tricks */
	if (locate_audio_cd ()) {
		volume = g_new0 (NautilusVolume, 1);
		volume->device_path = g_strdup ("/dev/cdrom");
		volume->mount_path = g_strdup ("/dev/cdrom");
		volume->filesystem = g_strdup ("cdda");
		if (mount_volume_get_name (volume)) {
			current_mounts = g_list_append (current_mounts, volume);
		} else {
			nautilus_volume_monitor_free_volume (volume);
		}
	}
#endif

	fclose (fh);
	
	return current_mounts;

}


static void
update_modifed_volume_name (GList *mount_list, NautilusVolume *volume)
{
	GList *element;
	NautilusVolume *found_volume;
	
	for (element = mount_list; element != NULL; element = element->next) {
		found_volume = element->data;
		if (strcmp (volume->device_path, found_volume->device_path) == 0) {
			g_free (volume->volume_name);
			volume->volume_name = g_strdup (found_volume->volume_name);
		}
	}
}


static gboolean
mount_lists_are_identical (GList *list_a, GList *list_b)
{
	GList *p, *q;
	NautilusVolume *volumeOne, *volumeTwo;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		
		volumeOne = p->data;
		volumeTwo = q->data;
		
		if (strcmp (volumeOne->device_path, volumeTwo->device_path) != 0) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

static void
verify_current_mount_state (NautilusVolumeMonitor *monitor)
{
	GList *new_mounts, *old_mounts, *current_mounts;
	GList *saved_mount_list;
	gboolean free_new_mounts;
	
	new_mounts = old_mounts = current_mounts = saved_mount_list = NULL;

	/* Free the new mount list only if it is not a copy of the current mount list. */
	if (monitor->details->mounts != NULL) {	
		free_new_mounts = TRUE;
	} else {
		free_new_mounts = FALSE;
	}
	
	/* Get all current mounts */
	current_mounts = get_current_mount_list ();
	if (current_mounts == NULL) {
		return;
	}
  	
  	/* If the new list is the same of the current list, bail. */
	if (mount_lists_are_identical (current_mounts, monitor->details->mounts)) {
		free_mount_list (current_mounts);
		return;
	}
		
  	/* If saved mounts is NULL, this is the first time we have been here. New mounts
  	 * are the same as current mounts
  	 */
  	if (monitor->details->mounts == NULL) {
		new_mounts = current_mounts;
		old_mounts = NULL;
  	} else {	
		/* Create list of new and old mounts */
  		new_mounts = build_volume_list_delta (current_mounts, monitor->details->mounts);
  		old_mounts = build_volume_list_delta (monitor->details->mounts, current_mounts);  		
	}
		
	/* Stash a copy of the current mount list for updating mount names later. */
	saved_mount_list = monitor->details->mounts;
		
	/* Free previous mount list and replace with new */
	monitor->details->mounts = current_mounts;

	/* Check and see if we have new mounts to add */
	if (new_mounts != NULL) {
		GList *p;
		for (p = new_mounts; p != NULL; p = p->next) {
			mount_volume_activate (monitor, (NautilusVolume *)p->data);
		}				
	}
	
	/* Check and see if we have old mounts to remove */
	if (old_mounts != NULL) {		
		GList *p;
				
		for (p = old_mounts; p != NULL; p = p->next) {
			/* First we need to update the volume names in this list with modified names in the old list. Names in
			 * the old list may have been modifed due to icon name conflicts.  The list of old mounts is unable
		 	 * take this into account when it is created
		 	 */			
			update_modifed_volume_name (saved_mount_list, (NautilusVolume *)p->data);
			
			/* Deactivate the volume. */
			mount_volume_deactivate (monitor, (NautilusVolume *)p->data);
		}
		free_mount_list (old_mounts);		
	}

	/* Free the new mount list only if it is not a copy of the current mount list.
	 * It will be a copy if there are no previous mounts.
	 */
	if (free_new_mounts) {
		free_mount_list (new_mounts);
	}
	
	free_mount_list (saved_mount_list);
}


static int
mount_volumes_check_status (NautilusVolumeMonitor *monitor)
{
	verify_current_mount_state (monitor);
			
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
	guint euid, egid;
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
mount_volume_floppy_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_FLOPPY;
	return TRUE;
}

static gboolean
mount_volume_ext2_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->device_path, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_EXT2;
		
	return TRUE;
}

static gboolean
mount_volume_udf_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->device_path, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_UDF;
		
	return TRUE;
}

static gboolean
mount_volume_vfat_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->device_path, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_VFAT;
		
	return TRUE;
}

static gboolean
mount_volume_msdos_add (NautilusVolume *volume)
{		
	if (check_permissions (volume->device_path, R_OK)) {		
		return FALSE;
	}

	volume->type = NAUTILUS_VOLUME_MSDOS;
		
	return TRUE;
}

static void
cdrom_ioctl_get_info (int fd)
{
	ioctl (fd, CDROM_CLEAR_OPTIONS, CDO_LOCK | CDO_AUTO_CLOSE | CDO_AUTO_EJECT);
	ioctl (fd, CDROM_SET_OPTIONS, CDO_USE_FFLAGS | CDO_CHECK_TYPE);
	ioctl (fd, CDROM_LOCKDOOR, 0);
}

static gboolean
mount_volume_iso9660_add (NautilusVolume *volume)
{
	int fd;

	fd = open (volume->device_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return FALSE;
	}
	
	/* If this fails it's probably not a CD-ROM drive */
	if (ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}

	cdrom_ioctl_get_info (fd);
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
mount_volume_cdda_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_CDDA;
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
	/* We need to filter out autofs magic NFS directories.  These entries will have the text
	 * "(pid" in the first element of its entry in /proc/mounts. An example would be eazel:(pid1234)
	 * or eazel(pid1234).  If we signal that the volume monitor has added this type of file system
	 * the trash monitor will become confused and recurse indefinitely.
	 */
	
	if (strstr (volume->device_path, "(pid") != NULL) {
		return FALSE;
	}
		
	volume->type = NAUTILUS_VOLUME_NFS;
	return TRUE;
}

static gboolean
mount_volume_proc_add (NautilusVolume *volume)
{
	volume->type = NAUTILUS_VOLUME_EXT2;
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

static void
find_volumes (NautilusVolumeMonitor *monitor)
{
	/* make sure the mount states of disks are set up */
	mount_volumes_check_status (monitor);

	/* Add a timer function to check for status change in mounted volumes periodically */
	monitor->details->mount_volume_timer_id = 
		gtk_timeout_add (CHECK_STATUS_INTERVAL,
				 (GtkFunction) mount_volumes_check_status,
				 monitor);
}

#if 0
void
nautilus_volume_monitor_each_volume (NautilusVolumeMonitor *monitor, 
				     NautilusEachVolumeFunction function,
				     gpointer context)
{
	GList *p;

	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		if ((* function) ((NautilusVolume *) p->data, context)) {
			break;
		}
	}
}
#endif

void
nautilus_volume_monitor_each_mounted_volume (NautilusVolumeMonitor *monitor, 
					     NautilusEachVolumeFunction function,
					     gpointer context)
{
	GList *p;
	NautilusVolume *volume;

	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		volume = (NautilusVolume *) p->data;
		(* function) (volume, context);
	}
}


gboolean
nautilus_volume_monitor_volume_is_mounted (NautilusVolumeMonitor *monitor,
					   const NautilusVolume *volume)
{
	GList *p;
	NautilusVolume *new_volume;
	
	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		new_volume = (NautilusVolume *)p->data;
		
		if (strcmp (new_volume->mount_path, volume->mount_path) == 0) {
			return TRUE;
		}
	}
		
	return FALSE;					   
}						 

static const char *mount_known_locations [] = {
	"/sbin/mount", "/bin/mount",
	"/usr/sbin/mount", "/usr/bin/mount",
	NULL
};

static const char *umount_known_locations [] = {
	"/sbin/umount", "/bin/umount",
	"/usr/sbin/umount", "/usr/bin/umount",
	NULL
};


/* Returns the full path to the queried command */
static const char *
find_command (const char **known_locations)
{
	int i;

	for (i = 0; known_locations [i]; i++){
		if (g_file_exists (known_locations [i]))
			return known_locations [i];
	}
	return NULL;
}

/* Pipes are guaranteed to be able to hold at least 4096 bytes */
/* More than that would be unportable */
#define MAX_PIPE_SIZE 4096

static int error_pipe[2];	/* File descriptors of error pipe */
static int old_error;		/* File descriptor of old standard error */

/* Creates a pipe to hold standard error for a later analysis. */
static void
open_error_pipe (void)
{
	pipe (error_pipe);
    
	old_error = dup (2);
	if (old_error < 0 || close(2) || dup (error_pipe[1]) != 2) {
		close (error_pipe[0]);
		close (error_pipe[1]);
	}
    
	close (error_pipe[1]);
}

typedef struct {
	char *command;
	char *mount_point;
	gboolean should_mount;
} MountThreadInfo;

typedef struct {
	char *message;
	char *detailed_message;
	char *title;
} MountStatusInfo;


static gboolean
display_mount_status (gpointer callback_data)
{
	MountStatusInfo *info = callback_data;
		
	nautilus_show_error_dialog_with_details 
		(info->message, info->title, info->detailed_message, NULL);

	g_free (info->message);
	g_free (info->detailed_message);
	g_free (info->title);
	g_free (info);
	
	return FALSE;
}

static void
close_error_pipe (gboolean mount, const char *mount_path)
{
	char *message;
	const char *title;
	char detailed_msg[MAX_PIPE_SIZE];
	int length = 0;
	gboolean is_floppy;
	MountStatusInfo *info;
	
	if (mount) {
		title = _("Mount Error");
	} else {
		title = _("Unmount Error");
	}
		
	if (old_error >= 0) {
		close (2);
		dup (old_error);
		close (old_error);
		
		do {
			length = read (error_pipe[0], detailed_msg, MAX_PIPE_SIZE);							
		} while (length < 0);
		
		if (length >= 0) {
			detailed_msg[length] = 0;
		}
		
		close (error_pipe[0]);
	}
	
	/* No output to show */
	if (length == 0) {
		return;
	}
	
	is_floppy = strstr (mount_path, "floppy") != NULL;
		
	/* Determine a user readable message from the obscure pipe error */
	/* Attention localizers: The strings being examined by strstr need to be identical
	   to the errors returned to the terminal by mount. */
	if (mount) {
		if (strstr (detailed_msg, _("is write-protected, mounting read-only")) != NULL) {
			/* This is not an error. Just an informative message from mount. */
			return;
		} else if ((strstr (detailed_msg, _("is not a valid block device")) != NULL) ||
			   (strstr (detailed_msg, _("No medium found")) != NULL)) {
			/* No media in drive */
			if (is_floppy) {
				/* Handle floppy case */
				message = g_strdup_printf (_("Nautilus was unable to mount the floppy drive. "
				     			     "There is probably no floppy in the drive."));
			} else {
				/* All others */
				message = g_strdup_printf (_("Nautilus was unable to mount the volume. "
							     "There is probably no media in the device."));
			}
		} else if (strstr (detailed_msg, _("wrong fs type, bad option, bad superblock on")) != NULL) {
			/* Unknown filesystem */
			if (is_floppy) {
				message = g_strdup_printf (_("Nautilus was unable to mount the floppy drive. "
				     			     "The floppy is probably in a format that cannot be mounted."));
			} else {
				message = g_strdup_printf (_("Nautilus was unable to mount the selected volume. "
				     			     "The volume is probably in a format that cannot be mounted."));
			}
		} else {
			if (is_floppy) {
				message = g_strdup (_("Nautilus was unable to mount the selected floppy drive."));
			} else {
				message = g_strdup (_("Nautilus was unable to mount the selected volume."));
			}
		}
	} else {
		message = g_strdup (_("Nautilus was unable to unmount the selected volume."));
	}
	
	/* Set up info and pass it to callback to display dialog.  We do this because this
	   routine may be called from a thread */
	info = g_new0 (MountStatusInfo, 1);
	info->message = g_strdup (message);	
	info->detailed_message = g_strdup (detailed_msg);
	info->title = g_strdup (title);

	gtk_idle_add (display_mount_status, info);
	
	g_free (message);	
}


static void *
mount_unmount_callback (void *arg)
{
	FILE *file;
	MountThreadInfo *info;
	info = arg;
	
	if (info != NULL) {	
		open_error_pipe ();
		file = popen (info->command, "r");
		close_error_pipe (info->should_mount, info->mount_point);
		pclose (file);
		
		g_free (info->command);
		g_free (info->mount_point);
		g_free (info);
	}
	
	pthread_exit (NULL); 	
}


void
nautilus_volume_monitor_mount_unmount_removable (NautilusVolumeMonitor *monitor,
						 const char *mount_point,
						 gboolean should_mount)
{
	const char *command;
	GList *p;
	NautilusVolume *volume;
	char *command_string;
	MountThreadInfo *mount_info;
	pthread_t mount_thread;

	volume = NULL;
	
	/* Check and see if volume exists in mounts already */
	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		volume = (NautilusVolume *)p->data;
		if (should_mount && strcmp (volume->mount_path, mount_point) == 0) {
			return;
		}		
	}
		
	if (should_mount) {
		command = find_command (mount_known_locations);
	} else {
		command = find_command (umount_known_locations);
	}

	command_string = g_strconcat (command, " ", mount_point, NULL);

	mount_info = g_new0 (MountThreadInfo, 1);
	mount_info->command = g_strdup (command_string);	
	mount_info->mount_point = g_strdup (mount_point);
	mount_info->should_mount = should_mount;

	pthread_create (&mount_thread, NULL, mount_unmount_callback, mount_info);
	
	g_free (command_string);
}


void
nautilus_volume_monitor_set_volume_name (NautilusVolumeMonitor *monitor,
					 const NautilusVolume *volume, const char *volume_name)
{
	GList *element;
	NautilusVolume *found_volume;
	
	/* Find volume and set new name */
	for (element = monitor->details->mounts; element != NULL; element = element->next) {
		found_volume = element->data;
		if (strcmp (found_volume->device_path, volume->device_path) == 0) {
			g_free (found_volume->volume_name);
			found_volume->volume_name = g_strdup (volume_name);
			return;
		}
	}
}

static NautilusVolume *
copy_volume (NautilusVolume *volume)
{
	NautilusVolume *new_volume;
	
	new_volume = g_new0 (NautilusVolume, 1);
	
	new_volume->type = volume->type;
	new_volume->device_path = g_strdup (volume->device_path);
	new_volume->mount_path = g_strdup (volume->mount_path);
	new_volume->volume_name = g_strdup (volume->volume_name);
	new_volume->filesystem = g_strdup (volume->filesystem);
	new_volume->is_removable = volume->is_removable;
	new_volume->is_read_only = volume->is_read_only;
	
	return new_volume;
}

void
nautilus_volume_monitor_free_volume (NautilusVolume *volume)
{
	g_free (volume->device_path);
	g_free (volume->mount_path);
	g_free (volume->volume_name);
	g_free (volume->filesystem);
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
	} else {
		modify_volume_name_for_display (volume);
	}
}

static void
get_ext2_volume_name (NautilusVolume *volume)
{
	char *name;
		
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		/* Handle special case for "/" */
		if (strlen (name) == 1 && strcmp (name, "/") == 0) {
			volume->volume_name = g_strdup (_("Root"));
		} else {		
			name++;
			volume->volume_name = g_strdup (name);
			modify_volume_name_for_display (volume);
		}
	} else {
		volume->volume_name = g_strdup (_("Ext2 Volume"));
	}
}

static void
get_msdos_volume_name (NautilusVolume *volume)
{
	char *name;
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("MSDOS Volume"));
	}
}

static void
get_nfs_volume_name (NautilusVolume *volume)
{
	char *name;
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("NFS Volume"));
	}
}

static void
get_floppy_volume_name (NautilusVolume *volume)
{
	char *name;
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("Floppy"));
	}
}

static void
get_generic_volume_name (NautilusVolume *volume)
{
	char *name;
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("Unknown Volume"));
	}
}

static gboolean
mount_volume_add_filesystem (NautilusVolume *volume)
{
	gboolean mounted = FALSE;
			
	if (nautilus_str_has_prefix (volume->device_path, FLOPPY_DEVICE_PATH_PREFIX)) {		
		mounted = mount_volume_floppy_add (volume);
	} else if (strcmp (volume->filesystem, "affs") == 0) {		
		mounted = mount_volume_affs_add (volume);
	} else if (strcmp (volume->filesystem, "cdda") == 0) {		
		mounted = mount_volume_cdda_add (volume);
	} else if (strcmp (volume->filesystem, "ext2") == 0) {		
		mounted = mount_volume_ext2_add (volume);
	} else if (strcmp (volume->filesystem, "fat") == 0) {		
		mounted = mount_volume_fat_add (volume);
	} else if (strcmp (volume->filesystem, "hpfs") == 0) {		
		mounted = mount_volume_hpfs_add (volume);
	} else if (strcmp (volume->filesystem, "iso9660") == 0) {		    		
		mounted = mount_volume_iso9660_add (volume);
	} else if (strcmp (volume->filesystem, "minix") == 0) {		    		
		mounted = mount_volume_minix_add (volume);
	} else if (strcmp (volume->filesystem, "msdos") == 0) {		
		mounted = mount_volume_msdos_add (volume);
	} else if (strcmp (volume->filesystem, "nfs") == 0) {		
		mounted = mount_volume_nfs_add (volume);
	} else if (strcmp (volume->filesystem, "proc") == 0) {		
		mounted = mount_volume_proc_add (volume);
	} else if (strcmp (volume->filesystem, "smb") == 0) {		
		mounted = mount_volume_smb_add (volume);
	} else if (strcmp (volume->filesystem, "udf") == 0) {		
		mounted = mount_volume_udf_add (volume);
	} else if (strcmp (volume->filesystem, "ufs") == 0) {		
		mounted = mount_volume_udf_add (volume);
	} else if (strcmp (volume->filesystem, "unsdos") == 0) {		
		mounted = mount_volume_unsdos_add (volume);
	} else if (strcmp (volume->filesystem, "vfat") == 0) {		
		mounted = mount_volume_vfat_add (volume);
	} else if (strcmp (volume->filesystem, "xenix") == 0) {		
		mounted = mount_volume_xenix_add (volume);
	} else if (strcmp (volume->filesystem, "xiafs") == 0) {
		mounted = mount_volume_xiafs_add (volume);
	}
	
	if (mounted) {
		volume->is_removable = volume_is_removable (volume);
		volume->is_read_only = volume_is_read_only (volume);
		mount_volume_get_name (volume);
	}
			
	return mounted;
}


#ifdef MOUNT_AUDIO_CD

static cdrom_drive *
open_cdda_device (GnomeVFSURI *uri)
{
	const char *device_name;
	cdrom_drive *drive;
	
	device_name = gnome_vfs_uri_get_path (uri);

	drive = cdda_identify (device_name, FALSE, NULL);
	if (drive == NULL) {
		return NULL;
	}
	
	/* Turn off verbosity */
	cdda_verbose_set (drive, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	/* Open drive */
	switch (cdda_open (drive)) {
  		case -2:
  		case -3:
  		case -4:
  		case -5:
    		//g_message ("Unable to open disc.  Is there an audio CD in the drive?");
    		return NULL;

  		case -6:
    		//g_message ("CDDA method could not find a way to read audio from this drive.");
    		return NULL;
    			
  		case 0:
    		break;

  		default:
    		//g_message ("Unable to open disc.");
    		return NULL;
	}
	
	return drive;
}

static gboolean
locate_audio_cd (void) {
	cdrom_drive *drive;
	GnomeVFSURI *uri;
	gboolean found_one;
	
	found_one = FALSE;
		
	uri = gnome_vfs_uri_new (CD_AUDIO_URI);
	if (uri == NULL) {
		return found_one;
	}
		
	drive = open_cdda_device (uri);
	gnome_vfs_uri_unref (uri);
	
	if (drive != NULL) {
		found_one = TRUE;				
		cdda_close (drive);
	}
		
	return found_one;
}

#endif
