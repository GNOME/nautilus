/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-volume-monitor.c - Desktop volume mounting routines.

   Copyright (C) 2000, 2001 Eazel, Inc.

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
#include "nautilus-iso9660.h"
#include "nautilus-volume-monitor.h"
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <errno.h>
#include <fcntl.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#ifdef HAVE_SYS_VFSTAB_H
#include <sys/vfstab.h>
#else
#include <fstab.h>
#endif

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#define MOUNT_TABLE_PATH _PATH_MNTTAB
#elif defined (HAVE_SYS_MNTTAB_H)
#define SOLARIS_MNT 1
#include <sys/mnttab.h>
#define MOUNT_TABLE_PATH "/etc/mnttab"
#else
/* FIXME: How does this help anything? */
#define MOUNT_TABLE_PATH ""
#endif

#ifdef SOLARIS_MNT
#define USE_VOLRMMOUNT 1
#endif

#ifdef HAVE_SYS_PARAM_H
#ifdef MIN
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifndef MNTOPT_RO
#define MNTOPT_RO "ro"
#endif

#ifndef HAVE_SETMNTENT
#define setmntent(f,m) fopen(f,m)
#endif

#ifdef HAVE_CDDA_INTERFACE_H
#ifdef HAVE_CDDA_PARANOIA_H
/* Take this out for now */
/*#define HAVE_CDDA 1*/
#endif
#endif

#ifdef HAVE_CDDA

#define size16 short
#define size32 int

#include <cdda_interface.h>
#include <cdda_paranoia.h>

#define CD_AUDIO_PATH "/dev/cdrom"
#define CDDA_SCHEME "cdda"

/* This is here to work around a broken header file. cdda_interface.h
 * has a statically defined array of chars that is unused. This will
 * break our build due to our strict error checking.
 */
char **broken_cdda_interface_h_workaround = strerror_tr;

#endif

#define CHECK_STATUS_INTERVAL 2000

#define FLOPPY_MOUNT_PATH_PREFIX "/mnt/fd"

#ifdef HAVE_SYS_MNTTAB_H
typedef struct mnttab MountTableEntry;
#elif defined (HAVE_GETMNTINFO)
typedef struct statfs MountTableEntry;
#else
typedef struct mntent MountTableEntry;
#endif

struct NautilusVolumeMonitorDetails
{
	GList *mounts;
	GList *removable_volumes;
	guint mount_volume_timer_id;
	GHashTable *readable_mount_point_names;
};

static NautilusVolumeMonitor *global_volume_monitor = NULL;
static const char *floppy_device_path_prefix;
static const char *noauto_string;
static gboolean mnttab_exists;


/* The NautilusVolumeMonitor signals.  */
enum {
	VOLUME_MOUNTED,
	VOLUME_UNMOUNT_STARTED,
	VOLUME_UNMOUNT_FAILED,
	VOLUME_UNMOUNTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static void            nautilus_volume_monitor_initialize       (NautilusVolumeMonitor      *desktop_mounter);
static void            nautilus_volume_monitor_initialize_class (NautilusVolumeMonitorClass *klass);
static void            nautilus_volume_monitor_destroy          (GtkObject                  *object);
static void            get_iso9660_volume_name                  (NautilusVolume             *volume,
								 int                         volume_fd);
static void            get_ext2_volume_name                     (NautilusVolume             *volume);
static void            get_msdos_volume_name                    (NautilusVolume             *volume);
static void            get_nfs_volume_name                      (NautilusVolume             *volume);
static void            get_reiser_volume_name                   (NautilusVolume             *volume);
static void            get_ufs_volume_name                      (NautilusVolume             *volume);
static void            get_generic_volume_name                  (NautilusVolume             *volume);
static void            mount_volume_get_name                    (NautilusVolume             *volume);
static void            mount_volume_activate                    (NautilusVolumeMonitor      *view,
								 NautilusVolume             *volume);
static void            mount_volume_deactivate                  (NautilusVolumeMonitor      *monitor,
								 NautilusVolume             *volume);
static void            load_additional_mount_list_info          (GList                      *volume_list);
static GList *         mount_volume_prepend_filesystem          (GList                      *volume_list,
								 NautilusVolume             *volume);
static NautilusVolume *create_volume                            (const char                 *device_path,
								 const char                 *mount_path,
								 const char                 *filesystem);
static NautilusVolume *copy_volume                              (NautilusVolume             *volume);
static void            find_volumes                             (NautilusVolumeMonitor      *monitor);
static void            free_mount_list                          (GList                      *mount_list);
static GList *         get_removable_volumes                    (void);
static GHashTable *    create_readable_mount_point_name_table   (void);
static int             get_cdrom_type                           (const char                 *vol_dev_path,
								 int                        *fd);

#ifdef HAVE_CDDA
static gboolean        locate_audio_cd                          (void);
#endif

EEL_DEFINE_CLASS_BOILERPLATE (NautilusVolumeMonitor,
				   nautilus_volume_monitor,
				   GTK_TYPE_OBJECT)

static void
nautilus_volume_monitor_initialize (NautilusVolumeMonitor *monitor)
{
	/* Set up details */
	monitor->details = g_new0 (NautilusVolumeMonitorDetails, 1);	
	monitor->details->mounts = NULL;
	monitor->details->removable_volumes = NULL;
	monitor->details->readable_mount_point_names = create_readable_mount_point_name_table ();

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

	signals[VOLUME_UNMOUNT_STARTED] 
		= gtk_signal_new ("volume_unmount_started",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusVolumeMonitorClass, 
						     volume_unmount_started),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[VOLUME_UNMOUNT_FAILED] 
		= gtk_signal_new ("volume_unmoun_failed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusVolumeMonitorClass, 
						     volume_unmount_failed),
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

	/* Check environment a bit. */
	if (g_file_exists ("/vol/dev")) {
		floppy_device_path_prefix = "/vol/dev/diskette/";
	} else {
		floppy_device_path_prefix = "/dev/fd";
	}
	if (g_file_exists ("/vol")) {
		noauto_string = "/vol/";
	} else {
		noauto_string = "/dev/fd";
	}
	mnttab_exists = g_file_exists ("/etc/mnttab");
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

	/* Clean up readable names table */	
	g_hash_table_destroy (monitor->details->readable_mount_point_names);

	/* Clean up details */	 
	g_free (monitor->details);

	global_volume_monitor = NULL;

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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

	is_floppy_1 = volume1->device_type == NAUTILUS_DEVICE_FLOPPY_DRIVE;
	is_floppy_2 = volume2->device_type == NAUTILUS_DEVICE_FLOPPY_DRIVE;

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


#if defined (HAVE_GETMNTINFO) || defined (HAVE_MNTENT_H) || defined (SOLARIS_MNT)

static gboolean
has_removable_mntent_options (MountTableEntry *ent)
{
#ifdef HAVE_HASMNTOPT
	/* Use "owner" or "user" or "users" as our way of determining a removable volume */
	if (hasmntopt (ent, "user") != NULL
	    || hasmntopt (ent, "users") != NULL
	    || hasmntopt (ent, "owner") != NULL) {
		return TRUE;
	}
#endif

#ifdef SOLARIS_MNT
	if (eel_str_has_prefix (ent->mnt_special, "/vol/")) {
		return TRUE;
	}
#endif
	
	return FALSE;
}

#endif

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
	MountTableEntry *ent;
	NautilusVolume *volume;
#ifdef HAVE_SYS_MNTTAB_H
        MountTableEntry ent_storage;
#endif
	ent = NULL;
	volume = NULL;
	volumes = NULL;

#ifdef HAVE_GETMNTINFO
	int count, index;
	
	count = getmntinfo (&ent, MNT_WAIT);
	/* getmentinfo returns a pointer to static data. Do not free. */
	for (index = 0; index < count; index++) {
		if (has_removable_mntent_options (ent + 1)) {
			volume = g_new (NautilusVolume, 1);
			volume->device_path = g_strdup (ent[index].f_mntfromname);
			volume->mount_path = g_strdup (ent[index].f_mntoname);
			volume->filesystem = g_strdup (ent[index].f_fstyename);
			volume->is_removable = TRUE;
			volume->is_read_only = ((ent[index].f_flags & MNT_RDONLY) != 0);
			volumes = mount_volume_prepend_filesystem (volumes, volume);				
		}
	}
#endif
	
	file = setmntent (MOUNT_TABLE_PATH, "r");
	if (file == NULL) {
		return NULL;
	}
	
#ifdef HAVE_SYS_MNTTAB_H
        ent = &ent_storage;
	while (! getmntent (file, ent)) {
		/* On Solaris look for /vol/ for determining a removable volume */
		if (eel_str_has_prefix (ent->mnt_special, noauto_string)) {
			volume = create_volume (ent->mnt_special, ent->mnt_mountp, ent->mnt_fstype);
			volume->is_removable = TRUE;
			volume->is_read_only = hasmntopt (ent, MNTOPT_RO) != NULL;
			volumes = mount_volume_prepend_filesystem (volumes, volume);
		}
	}
#elif defined (HAVE_MNTENT_H)
	while ((ent = getmntent (file)) != NULL) {
		if (has_removable_mntent_options (ent)) {
			volume = create_volume (ent->mnt_fsname, ent->mnt_dir, ent->mnt_type);
			volumes = mount_volume_prepend_filesystem (volumes, volume);
		}
	}
#endif
			
	fclose (file);
	
#ifdef HAVE_CDDA
	volume = create_volume (CD_AUDIO_PATH, CD_AUDIO_PATH, CDDA_SCHEME);
	volumes = mount_volume_prepend_filesystem (volumes, volume);
#endif

	load_additional_mount_list_info (volumes);
	
	/* Move all floppy mounts to top of list */
	return g_list_sort (g_list_reverse (volumes), (GCompareFunc) floppy_sort);
}


static gboolean
volume_is_removable (const NautilusVolume *volume)
{
	FILE *file;
     	MountTableEntry *ent;
#ifdef HAVE_SYS_MNTTAB_H
	MountTableEntry ent_storage;
#endif

	ent = NULL;

	file = setmntent (MOUNT_TABLE_PATH, "r");
	if (file == NULL) {
		return FALSE;
	}
	
	/* Search for our device in the fstab */
#ifdef HAVE_SYS_MNTTAB_H
	ent = &ent_storage;
	while (!getmntent (file, ent)) {
		if (strcmp (volume->device_path, ent->mnt_special) == 0) {
  			/* On Solaris look for /vol/ for determining
			a removable volume */
   		 	if (eel_str_has_prefix (ent->mnt_special, noauto_string)) {
				fclose (file);
				return TRUE;
			}
		}	
	}
#elif defined (HAVE_MNTENT_H)
	while ((ent = getmntent (file)) != NULL) {
		if (strcmp (volume->device_path, ent->mnt_fsname) == 0
		    && has_removable_mntent_options (ent)) {
			fclose (file);
			return TRUE;
		}	
	}
#endif
	
	fclose (file);
	return FALSE;
}

static gboolean
volume_is_read_only (const NautilusVolume *volume)
{
	FILE *file;
	MountTableEntry *ent;

#ifdef HAVE_SYS_MNTTAB_H
 	MountTableEntry ent_storage;

	file = setmntent (MOUNT_TABLE_PATH, "r");
 	if (file == NULL) {
 		return FALSE;
 	}
 
 	 /* Search for our device in the fstab */
 	ent = &ent_storage;
	while (!getmntent (file, ent)) {
 		if (strcmp (volume->device_path, ent->mnt_special) == 0) {
 			if (strstr (ent->mnt_mntopts, MNTOPT_RO) != NULL) {
 				fclose (file);
 				return TRUE;
 			}
 		}
 	}
#elif defined (HAVE_MNTENT_H)
	file = setmntent (MOUNT_TABLE_PATH, "r");
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
#else
	ent = NULL;	
	file = NULL;
#endif
				
	fclose (file);
	return FALSE;
}


char *
nautilus_volume_monitor_get_volume_name (const NautilusVolume *volume)
{
	if (volume->volume_name == NULL) {
		return g_strdup (_("Unknown"));
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
		volume->volume_name = g_strdup (_("Unknown"));
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
	volume->volume_name = name;
}

/* nautilus_volume_monitor_get_target_uri
 *	
 * Returns the activation uri of the volume
 */
 
char *
nautilus_volume_monitor_get_target_uri (const NautilusVolume *volume)
{
	char *uri;

	switch (volume->volume_type) {
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
	return volume->volume_type == NAUTILUS_VOLUME_EXT2
		|| volume->volume_type == NAUTILUS_VOLUME_FAT
		|| volume->volume_type == NAUTILUS_VOLUME_NFS
		|| volume->volume_type == NAUTILUS_VOLUME_VFAT
		|| volume->volume_type == NAUTILUS_VOLUME_REISER
		|| volume->volume_type == NAUTILUS_VOLUME_UFS
		|| volume->volume_type == NAUTILUS_VOLUME_SMB;
}

const char *
nautilus_volume_monitor_get_volume_mount_uri (const NautilusVolume *volume)
{
	return volume->mount_path;
}


/* create_readable_mount_point_name_table
 *
 * Create a table with mapping between the mount point names that are found
 * in /etc/fstab and names that are clear and easy to understand.  
 */
static GHashTable *
create_readable_mount_point_name_table (void)
{
	GHashTable *table;
	
	table = g_hash_table_new (g_str_hash, g_str_equal);
	
	/* Populate table with items we know localized names for. */
	g_hash_table_insert (table, "floppy", _("Floppy"));
	g_hash_table_insert (table, "cdrom", _("CD-ROM"));
	g_hash_table_insert (table, "zip", _("Zip Drive"));
	
	return table;	
}

static void
mount_volume_get_cdrom_name (NautilusVolume *volume)
{
	int fd, disctype;

	disctype = get_cdrom_type (volume->device_path, &fd);

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

	disctype = get_cdrom_type (volume->device_path, &fd);

	switch (disctype) {
	case CDS_AUDIO:
		break;
	default:			
		break;
  	}

	close(fd);	
}

static void
mount_volume_activate_cdrom (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	int fd, disctype;

	disctype = get_cdrom_type (volume->device_path, &fd);

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
	switch (volume->volume_type) {
	case NAUTILUS_VOLUME_CDDA:
		mount_volume_get_cdda_name (volume);
		break;

	case NAUTILUS_VOLUME_CDROM:
	case NAUTILUS_VOLUME_HSFS:
		mount_volume_get_cdrom_name (volume);
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
		
	case NAUTILUS_VOLUME_REISER:
		get_reiser_volume_name (volume);
		break;
	case NAUTILUS_VOLUME_UFS:
		get_ufs_volume_name (volume);
		break;

	case NAUTILUS_VOLUME_AFFS:
	case NAUTILUS_VOLUME_AUTO:
	case NAUTILUS_VOLUME_HPFS:
	case NAUTILUS_VOLUME_MINIX:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UDF:
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
	switch (volume->volume_type) {
	case NAUTILUS_VOLUME_CDDA:
		mount_volume_activate_cdda (monitor, volume);
		break;

	case NAUTILUS_VOLUME_CDROM:
	case NAUTILUS_VOLUME_HSFS:
		mount_volume_activate_cdrom (monitor, volume);
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
	case NAUTILUS_VOLUME_REISER:
	case NAUTILUS_VOLUME_UFS:
	case NAUTILUS_VOLUME_SMB:
	case NAUTILUS_VOLUME_UDF:
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
		eel_gnome_shell_execute (command);
		g_free (command);
		g_free (path);
	}
	
	pthread_exit (NULL);

	/* compilation on Solaris warns of no return 
	   value on non-void function...so....*/
	return (void *) 0;
}

static void
mount_volume_deactivate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
	pthread_t eject_thread;

	switch (volume->volume_type) {
	case NAUTILUS_VOLUME_CDROM:
	case NAUTILUS_VOLUME_HSFS:
		pthread_create (&eject_thread, NULL, eject_device, g_strdup (volume->device_path));
		break;
		
	default:
		break;
	}

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
	gboolean found_match;
	
	new_list = NULL;
		
	for (ptrOne = list_one; ptrOne != NULL; ptrOne = ptrOne->next) {
	
		found_match = FALSE;
		volOne = (NautilusVolume *) ptrOne->data;
							
		for (ptrTwo = list_two; ptrTwo != NULL; ptrTwo = ptrTwo->next) {
			
			volTwo = (NautilusVolume *) ptrTwo->data;

			/* Check and see if mount point from list one is in list two */
			if (strcmp (volOne->mount_path, volTwo->mount_path) == 0) {
				found_match = TRUE;
				break;			
			}			
		}
		
		if (!found_match) {
			/* No match. Add it to the list to be returned; */
			new_volume = copy_volume (volOne);
			new_list = g_list_prepend (new_list, new_volume);
		}
	}
	
	return new_list;
}



#ifdef SOLARIS_MNT

static GList *
get_mount_list (void) 
{
        FILE *fh;
        GList *volumes;
        MountTableEntry ent;
        NautilusVolume *volume;

	volumes = NULL;
        
	fh = setmntent (MOUNT_TABLE_PATH, "r");
	if (fh == NULL) {
		return NULL;
	}

        while (! getmntent(fh, &ent)) {
                volume = create_volume (ent.mnt_special, ent.mnt_mountp, ent.mnt_fstype);
                volume->is_removable = has_removable_mntent_options (&ent);
                volume->is_read_only = hasmntopt (&ent, MNTOPT_RO) != NULL;
                volumes = mount_volume_prepend_filesystem (volumes, volume);
        }

	fclose (fh);

        return volumes;
}

#else /* !SOLARIS_MNT */

static GList *
get_mount_list (void) 
{
        GList *volumes;
        NautilusVolume *volume;
        static FILE *fh = NULL;
        const char *file_name;
	const char *separator;
	char line[PATH_MAX * 3];
	char device_name[sizeof (line)];
	EelStringList *list;
	char *device_path, *mount_path, *filesystem;

	volumes = NULL;
        
	if (mnttab_exists) { 
		file_name = "/etc/mnttab";
		separator = "\t";
	} else {
		file_name = "/proc/mounts";
		separator = " ";
	}
	
	if (fh == NULL) {
                fh = fopen (file_name, "r");
                if (fh == NULL) {
                        g_warning ("Unable to open %s: %s", file_name, strerror (errno));
                        return NULL;
                }
        } else {
                rewind (fh);
        }

	while (fgets (line, sizeof(line), fh)) {
                if (sscanf (line, "%s", device_name) != 1) {
                        continue;
		}

                list = eel_string_list_new_from_tokens (line, separator, FALSE);
                if (list == NULL) {
                        continue;
		}
                
                /* The string list needs to have at least 3 items per line.
                 * We need to find at least device path, mount path and file system type.
                 */
                if (eel_string_list_get_length (list) >= 3) {
                        device_path = eel_string_list_nth (list, 0);
                        mount_path = eel_string_list_nth (list, 1);
                        filesystem = eel_string_list_nth (list, 2);
                        volume = create_volume (device_path, mount_path, filesystem);
                        g_free (device_path);
                        g_free (mount_path);
                        g_free (filesystem);
                        volumes = mount_volume_prepend_filesystem (volumes, volume);
                }

		eel_string_list_free (list);
  	}
        
        return volumes;
}

#endif /* !SOLARIS_MNT */


static GList *
get_current_mount_list (void)
{
	GList *volumes;
#ifdef HAVE_CDDA
        NautilusVolume *volume;
#endif

	volumes = get_mount_list ();

#ifdef HAVE_CDDA
	/* CD Audio tricks */
	if (locate_audio_cd ()) {
		volume = create_volume (CD_AUDIO_PATH, CD_AUDIO_PATH, CDDA_SCHEME);
		mount_volume_get_name (volume);
		volumes = mount_volume_prepend_filesystem (volumes, volume);
	}
#endif

	return g_list_reverse (volumes);
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
	GList *saved_mount_list, *p;
	
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
		
	/* Create list of new and old mounts */
	new_mounts = build_volume_list_delta (current_mounts, monitor->details->mounts);
	old_mounts = build_volume_list_delta (monitor->details->mounts, current_mounts);  		
		
	/* Stash a copy of the current mount list for updating mount names later. */
	saved_mount_list = monitor->details->mounts;
		
	/* Free previous mount list and replace with new */
	monitor->details->mounts = current_mounts;

	/* Process list results to check for a properties that require opening files on disk. */
	load_additional_mount_list_info (new_mounts);
	
	if (old_mounts != NULL) {
		load_additional_mount_list_info (old_mounts);
		load_additional_mount_list_info (saved_mount_list);
	}
	
	/* Check and see if we have new mounts to add */
	for (p = new_mounts; p != NULL; p = p->next) {
		mount_volume_activate (monitor, (NautilusVolume *)p->data);
	}				
	
	/* Check and see if we have old mounts to remove */
	for (p = old_mounts; p != NULL; p = p->next) {
		/* First we need to update the volume names in this list with modified names in the old list. Names in
		 * the old list may have been modifed due to icon name conflicts.  The list of old mounts is unable
		 * take this into account when it is created
		 */			
		update_modifed_volume_name (saved_mount_list, (NautilusVolume *) p->data);
		
		/* Deactivate the volume. */
		mount_volume_deactivate (monitor, (NautilusVolume *) p->data);
	}
	
	free_mount_list (old_mounts);
	free_mount_list (new_mounts);
	
	free_mount_list (saved_mount_list);
}

static int
mount_volumes_check_status (NautilusVolumeMonitor *monitor)
{
	verify_current_mount_state (monitor);
			
	return TRUE;
}

static gboolean
mount_volume_ext2_add (NautilusVolume *volume)
{		
	volume->volume_type = NAUTILUS_VOLUME_EXT2;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	
		
	return TRUE;
}

static gboolean
mount_volume_udf_add (NautilusVolume *volume)
{		
	volume->volume_type = NAUTILUS_VOLUME_UDF;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	
	
	return TRUE;
}

static gboolean
mount_volume_vfat_add (NautilusVolume *volume)
{		
	volume->volume_type = NAUTILUS_VOLUME_VFAT;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_msdos_add (NautilusVolume *volume)
{		
	volume->volume_type = NAUTILUS_VOLUME_MSDOS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	
	
	return TRUE;
}

static int
get_cdrom_type (const char *vol_dev_path, int* fd)
{
#ifndef SOLARIS_MNT
	*fd = open (vol_dev_path, O_RDONLY|O_NONBLOCK);
	return ioctl (*fd, CDROM_DISC_STATUS, CDSL_CURRENT);
#else
	GString *new_dev_path;
	struct cdrom_tocentry entry;
	struct cdrom_tochdr header;
	int type;

	/* For ioctl call to work dev_path has to be /vol/dev/rdsk.
	 * There has to be a nicer way to do this.
	 */
	new_dev_path = g_string_new (vol_dev_path);
	new_dev_path = g_string_insert_c (new_dev_path, 9, 'r');
	*fd = open (new_dev_path->str, O_RDONLY | O_NONBLOCK);
	g_string_free (new_dev_path, TRUE);

	if (*fd < 0) {
		return CDS_DATA_1;
	}

	if (ioctl (*fd, CDROMREADTOCHDR, &header) == 0) {
		return CDS_DATA_1;
	}

	type = CDS_DATA_1;
	
	for (entry.cdte_track = 1;
	     entry.cdte_track <= header.cdth_trk1;
	     entry.cdte_track++) {
		entry.cdte_format = CDROM_LBA;
		if (ioctl (*fd, CDROMREADTOCENTRY, &entry) == 0) {
			if (entry.cdte_ctrl & CDROM_DATA_TRACK) {
				type = CDS_AUDIO;
				break;
			}
		}
	}

	return type;
#endif
}

static gboolean
mount_volume_iso9660_add (NautilusVolume *volume)
{
#ifndef SOLARIS_MNT
	int fd;
	
	fd = open (volume->device_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return FALSE;
	}
	
	/* If this fails it's probably not a CD-ROM drive */
	if (ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
    		return FALSE;
    	}
	close (fd);
#endif

	volume->volume_type = NAUTILUS_VOLUME_CDROM;
	volume->device_type = NAUTILUS_DEVICE_CD_ROM_DRIVE;
	
	return TRUE;
}

static gboolean
mount_volume_affs_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_AFFS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

/* This is intended mainly for adding removable volumes from /etc/fstab.
 * The auto type will not show up in /proc/mounts. The NAUTILUS_VOLUME_AUTO
 * type is just a practical placeholder.
 */
static gboolean
mount_volume_auto_add (NautilusVolume *volume)
{
	if (eel_str_has_prefix (volume->device_path, floppy_device_path_prefix)) {	
		volume->volume_type = NAUTILUS_VOLUME_AUTO;
		volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;	
	} else {
		volume->volume_type = NAUTILUS_VOLUME_AUTO;
		volume->device_type = NAUTILUS_DEVICE_UNKNOWN;
	}
	return TRUE;
}

static gboolean
mount_volume_cdda_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_CDDA;
	volume->device_type = NAUTILUS_DEVICE_CD_ROM_DRIVE;	

	return TRUE;
}

static gboolean
mount_volume_fat_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_FAT;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_hpfs_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_HPFS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_hsfs_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_HSFS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_minix_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_MINIX;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

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
		
	volume->volume_type = NAUTILUS_VOLUME_NFS;
	volume->device_type = NAUTILUS_DEVICE_NFS;	

	return TRUE;
}

static gboolean
mount_volume_proc_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_EXT2;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}
static gboolean
mount_volume_reiserfs_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_REISER;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_ufs_add (NautilusVolume *volume)
{
        volume->volume_type = NAUTILUS_VOLUME_UFS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

        return TRUE;
}

static gboolean
mount_volume_smb_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_SMB;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_unsdos_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_UNSDOS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_xenix_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_XENIX;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static gboolean
mount_volume_xiafs_add (NautilusVolume *volume)
{
	volume->volume_type = NAUTILUS_VOLUME_XIAFS;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;	

	return TRUE;
}

static void
find_volumes (NautilusVolumeMonitor *monitor)
{
	/* make sure the mount states of disks are set up */
	mount_volumes_check_status (monitor);
	load_additional_mount_list_info (monitor->details->mounts);

	/* Add a timer function to check for status change in mounted volumes periodically */
	monitor->details->mount_volume_timer_id = 
		gtk_timeout_add (CHECK_STATUS_INTERVAL,
				 (GtkFunction) mount_volumes_check_status,
				 monitor);
}

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


#ifdef USE_VOLRMMOUNT

static const char *volrmmount_locations [] = {
       "/usr/bin/volrmmount",
       NULL
};

#define MOUNT_COMMAND volrmmount_locations
#define MOUNT_SEPARATOR " -i "
#define UMOUNT_COMMAND volrmmount_locations
#define UMOUNT_SEPARATOR " -e "

#else

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

#define MOUNT_COMMAND mount_known_locations
#define MOUNT_SEPARATOR " "
#define UMOUNT_COMMAND umount_known_locations
#define UMOUNT_SEPARATOR " "

#endif /* USE_VOLRMMOUNT */

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
	char *mount_point;
	gboolean mount;
} MountStatusInfo;


static gboolean
display_mount_error (gpointer callback_data)
{
	MountStatusInfo *info;
	const char *title;
	NautilusVolumeMonitor *monitor;
	NautilusVolume *volume;
	GList *p;
	
	info = callback_data;
	
	title = info->mount ? _("Mount Error") :  _("Unmount Error");

	eel_show_error_dialog_with_details 
		(info->message, title, info->detailed_message, NULL);
	
	if (!info->mount) {
		/* Locate volume in current list */
		monitor = nautilus_volume_monitor_get ();
		for (p = monitor->details->mounts; p != NULL; p = p->next) {
			volume = (NautilusVolume *)p->data;
			if (strcmp (volume->mount_path, info->mount_point) == 0) {
				gtk_signal_emit (GTK_OBJECT (monitor), signals[VOLUME_UNMOUNT_FAILED], volume);
				break;
			}
		}
	}
		
	g_free (info->mount_point);
	g_free (info->message);
	g_free (info->detailed_message);
	g_free (info);
	
	return FALSE;
}

static void
close_error_pipe (gboolean mount, const char *mount_path)
{
	char *message;
	char detailed_msg[MAX_PIPE_SIZE];
	int length;
	gboolean is_floppy;
	MountStatusInfo *info;

	if (old_error < 0) {
		return;
	}
	
	close (2);
	dup (old_error);
	close (old_error);
	
	/* FIXME: This keeps reading into the same buffer over and
	 * over again and makes no attempt to save bytes from any
	 * calls other than the last read call.
	 */
	do {
		length = read (error_pipe[0], detailed_msg, MAX_PIPE_SIZE);							
	} while (length < 0);
	
	if (length >= 0) {
		detailed_msg[length] = 0;
	}
	
	close (error_pipe[0]);
	
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
		/* FIXME: Should we parse this message and report something more meaningful? */
		message = g_strdup (_("Nautilus was unable to unmount the selected volume."));
	}
	
	/* Set up info and pass it to callback to display dialog.  We do this because this
	   routine may be called from a thread */
	info = g_new0 (MountStatusInfo, 1);
	info->message = message;	
	info->detailed_message = g_strdup (detailed_msg);
	info->mount_point = g_strdup (mount_path);
	info->mount = mount;
	gtk_idle_add (display_mount_error, info);	
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
	
	return NULL;
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
	const char *name;

	volume = NULL;
	
	/* Check and see if volume exists in mounts already */
	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		volume = (NautilusVolume *)p->data;
		if (strcmp (volume->mount_path, mount_point) == 0) {
			if (should_mount) {
				return;
			} else  {
				break;
			}
		}
	}
	
#ifdef USE_VOLRMMOUNT
       name = strrchr (mount_point, '/');
       if (name != NULL) {
               name = name + 1;
       } else {
	       name = mount_point;
       }
#else
       name = mount_point;
#endif
       
       if (should_mount) {
               command = find_command (MOUNT_COMMAND);
               command_string = g_strconcat (command, MOUNT_SEPARATOR, name, NULL);
       } else {
               command = find_command (UMOUNT_COMMAND);
               command_string = g_strconcat (command, UMOUNT_SEPARATOR, name, NULL);
               if (volume != NULL) {
			gtk_signal_emit (GTK_OBJECT (monitor), signals[VOLUME_UNMOUNT_STARTED], volume);
		}
       }

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
create_volume (const char *device_path, const char *mount_path, const char *filesystem)
{
	NautilusVolume *volume;

	volume = g_new0 (NautilusVolume, 1);
	volume->device_path = g_strdup (device_path);
	volume->mount_path = g_strdup (mount_path);
	volume->filesystem = g_strdup (filesystem);

	volume->volume_type = NAUTILUS_VOLUME_UNKNOWN;
	volume->device_type = NAUTILUS_DEVICE_UNKNOWN;
	
	return volume;
}

static NautilusVolume *
copy_volume (NautilusVolume *volume)
{
	NautilusVolume *new_volume;	
	new_volume = create_volume (volume->device_path, volume->mount_path, volume->filesystem);
		
	new_volume->volume_type = volume->volume_type;	
	new_volume->device_type = volume->device_type;	
	new_volume->volume_name = g_strdup (volume->volume_name);
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
get_reiser_volume_name (NautilusVolume *volume)
{
	char *name;

	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("ReiserFS Volume"));
	}
}


static void
get_ufs_volume_name (NautilusVolume *volume)
{
	char *name;

	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name++;
		volume->volume_name = g_strdup (name);
		modify_volume_name_for_display (volume);
	} else {
		volume->volume_name = g_strdup (_("UFS Volume"));
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


static void
load_additional_mount_list_info (GList *volume_list)
{		
	GList *element;
	NautilusVolume *volume;
	
	for (element = volume_list; element != NULL; element = element->next) {
		volume = element->data;
		
#ifndef SOLARIS_MNT
		/* These are set up by get_current_mount_list for Solaris. */
		volume->is_removable = volume_is_removable (volume);
		volume->is_read_only = volume_is_read_only (volume);
#endif

		mount_volume_get_name (volume);
	}
}


static GList *
mount_volume_prepend_filesystem (GList *volume_list, NautilusVolume *volume)
{
	gboolean added;
	char *device_name;
	
	added = FALSE;
	
	if (strcmp (volume->filesystem, "affs") == 0) {		
		added = mount_volume_affs_add (volume);
	} else if (strcmp (volume->filesystem, "auto") == 0) {		
		added = mount_volume_auto_add (volume);
	} else if (strcmp (volume->filesystem, "cdda") == 0) {		
		added = mount_volume_cdda_add (volume);
	} else if (strcmp (volume->filesystem, "hsfs") == 0) {	
		added = mount_volume_hsfs_add (volume);
	} else if (strcmp (volume->filesystem, "ext2") == 0) {		
		added = mount_volume_ext2_add (volume);
	} else if (strcmp (volume->filesystem, "fat") == 0) {		
		added = mount_volume_fat_add (volume);
	} else if (strcmp (volume->filesystem, "hpfs") == 0) {		
		added = mount_volume_hpfs_add (volume);
	} else if (strcmp (volume->filesystem, "iso9660") == 0) {		    		
		added = mount_volume_iso9660_add (volume);
	} else if (strcmp (volume->filesystem, "minix") == 0) {		    		
		added = mount_volume_minix_add (volume);
	} else if (strcmp (volume->filesystem, "msdos") == 0) {		
		added = mount_volume_msdos_add (volume);
	} else if (strcmp (volume->filesystem, "nfs") == 0) {		
		added = mount_volume_nfs_add (volume);
	} else if (strcmp (volume->filesystem, "proc") == 0) {		
		added = mount_volume_proc_add (volume);
	} else if (strcmp (volume->filesystem, "reiserfs") == 0) {
		added = mount_volume_reiserfs_add (volume);
	} else if (strcmp (volume->filesystem, "ufs") == 0) {
		added = mount_volume_ufs_add (volume);
	} else if (strcmp (volume->filesystem, "smb") == 0) {		
		added = mount_volume_smb_add (volume);
	} else if (strcmp (volume->filesystem, "udf") == 0) {		
		added = mount_volume_udf_add (volume);
	} else if (strcmp (volume->filesystem, "ufs") == 0) {		
		added = mount_volume_udf_add (volume);
	} else if (strcmp (volume->filesystem, "unsdos") == 0) {		
		added = mount_volume_unsdos_add (volume);
	} else if (strcmp (volume->filesystem, "vfat") == 0) {		
		added = mount_volume_vfat_add (volume);
	} else if (strcmp (volume->filesystem, "xenix") == 0) {		
		added = mount_volume_xenix_add (volume);
	} else if (strcmp (volume->filesystem, "xiafs") == 0) {
		added = mount_volume_xiafs_add (volume);
	}

	if (added) {
		volume_list = g_list_prepend (volume_list, volume);
		
		/* Identify device type */
		if (eel_str_has_prefix (volume->mount_path, "/mnt/")) {		
			device_name = g_strdup (volume->mount_path + 5);
											
			if (eel_str_has_prefix (device_name, "cdrom")) {
				volume->device_type = NAUTILUS_DEVICE_CD_ROM_DRIVE;
				volume->is_removable = TRUE;
			} else if (eel_str_has_prefix (device_name, "floppy")) {
				volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;
				volume->is_removable = TRUE;
			} else if (eel_str_has_prefix (volume->device_path, floppy_device_path_prefix)) {		
				volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;
				volume->is_removable = TRUE;
			} else if (eel_str_has_prefix (device_name, "zip")) {
				volume->device_type = NAUTILUS_DEVICE_ZIP_DRIVE;
				volume->is_removable = TRUE;
			} else if (eel_str_has_prefix (device_name, "jaz")) {
				volume->device_type = NAUTILUS_DEVICE_JAZ_DRIVE;
				volume->is_removable = TRUE;
			} else if (eel_str_has_prefix (device_name, "camera")) {
				volume->device_type = NAUTILUS_DEVICE_CAMERA;
				volume->is_removable = TRUE;					
			} else if (eel_str_has_prefix (device_name, "memstick")) {
				volume->device_type = NAUTILUS_DEVICE_MEMORY_STICK;
				volume->is_removable = TRUE;
			} else {
				volume->device_type = NAUTILUS_DEVICE_UNKNOWN;
				volume->is_removable = FALSE;
			}
			
			g_free (device_name);
		}
	} else {
		nautilus_volume_monitor_free_volume (volume);	
	}
				
	return volume_list;
}

char *
nautilus_volume_monitor_get_mount_name_for_display (NautilusVolumeMonitor *monitor,
						    NautilusVolume *volume)
{
	const char *name, *found_name;
	
	if (monitor == NULL || volume == NULL) {
		return NULL;
	}
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name = name + 1;
	} else {
		name = volume->mount_path;
	}
	
	/* Look for a match in out localized mount name list */
	found_name = g_hash_table_lookup (monitor->details->readable_mount_point_names, name);
	if (found_name != NULL) {
		return g_strdup (found_name);
	} else {
		return g_strdup (name);
	}
}

#ifdef HAVE_CDDA

static gboolean
locate_audio_cd (void)
{
	cdrom_drive *drive;
	gboolean opened;
	
	drive = cdda_identify (CD_AUDIO_PATH, FALSE, NULL);
	if (drive == NULL) {
		return FALSE;
	}
	
	/* Turn off verbosity */
	cdda_verbose_set (drive, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	/* Open drive */
	switch (cdda_open (drive)) {
	case -2:
	case -3:
	case -4:
	case -5:
    		/*g_message ("Unable to open disc.  Is there an audio CD in the drive?");*/
    		opened = FALSE;
		break;
		
	case -6:
    		/*g_message ("CDDA method could not find a way to read audio from this drive.");*/
		opened = FALSE;
		break;
		
	case 0:
		opened = TRUE;
    		break;
		
	default:
    		/*g_message ("Unable to open disc.");*/
		opened = FALSE;
		break;
	}
	cdda_close (drive);
	
	return opened;
}

#endif /* HAVE_CDDA */
