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
            Seth Nickell  <snickell@stanford.edu>
*/

#include <config.h>
#include "nautilus-volume-monitor.h"

#include "nautilus-cdrom-extensions.h"
#include "nautilus-directory-notify.h"
#include "nautilus-file-utilities.h"
#include "nautilus-iso9660.h"
#include "nautilus-volume-monitor.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <errno.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
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

typedef struct {
	char *name;
	char *default_volume_name;
	gboolean can_handle_trash;
} NautilusFileSystemType;

struct NautilusVolume {
	NautilusDeviceType device_type;
	NautilusFileSystemType *file_system_type;
	
	char *device_path;
	char *mount_path;
	char *volume_name;
	dev_t device;
	
	gboolean is_read_only;
	gboolean is_removable;
	gboolean is_audio_cd;
};

struct NautilusVolumeMonitorDetails
{
	GList *mounts;
	GList *removable_volumes;
	guint mount_volume_timer_id;
	GHashTable *readable_mount_point_names;
	GHashTable *file_system_table;
};

typedef void (* ChangeNautilusVolumeFunction) (NautilusVolumeMonitor *view,
					       NautilusVolume        *volume);

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
static char *          get_iso9660_volume_name                  (NautilusVolume             *volume,
								 int                         volume_fd);
static GHashTable *    load_file_system_table                   (void);
static void            mount_volume_activate                    (NautilusVolumeMonitor      *view,
								 NautilusVolume             *volume);
static void            mount_volume_deactivate                  (NautilusVolumeMonitor      *monitor,
								 NautilusVolume             *volume);
static void            load_additional_mount_list_info          (GList                      *volume_list);
static NautilusVolume *create_volume                            (const char                 *device_path,
								 const char                 *mount_path);
static GList *         finish_creating_volume_and_prepend       (NautilusVolumeMonitor      *monitor,
								 NautilusVolume             *volume,
								 const char                 *file_system_type_name,
								 GList                      *volume_list);
static NautilusVolume *copy_volume                              (const NautilusVolume       *volume);
static void            find_volumes                             (NautilusVolumeMonitor      *monitor);
static void            free_mount_list                          (GList                      *mount_list);
static GList *         copy_mount_list                          (GList                      *mount_list);
static GList *         get_removable_volumes                    (NautilusVolumeMonitor      *monitor);
static GHashTable *    create_readable_mount_point_name_table   (void);
static int             get_cdrom_type                           (const char                 *vol_dev_path,
								 int                        *fd);
static void            nautilus_volume_free                     (NautilusVolume             *volume);

#ifdef HAVE_CDDA
static gboolean        locate_audio_cd                          (void);
#endif

EEL_DEFINE_CLASS_BOILERPLATE (NautilusVolumeMonitor,
			      nautilus_volume_monitor,
			      GTK_TYPE_OBJECT)

static GHashTable *
load_file_system_table (void)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	GHashTable *table;
	char *file_system_attributes_file;
	xmlChar *name, *default_volume_name, *trash;
	NautilusFileSystemType *type;

	table = g_hash_table_new (g_str_hash, g_str_equal);
	
	file_system_attributes_file = nautilus_get_data_file_path ("filesystem-attributes.xml");
	if (file_system_attributes_file == NULL) {
		return table;
	}
	doc = xmlParseFile (file_system_attributes_file); 
	g_free (file_system_attributes_file);
	if (doc == NULL) {
		return table;
	}

	for (node = doc->xmlRootNode->xmlChildrenNode; node != NULL; node = node->next) {
		name = xmlGetProp (node, "name");

		if (name != NULL) {
			default_volume_name = eel_xml_get_property_translated (node, "default_volume_name");
			trash = xmlGetProp (node, "trash");

			if (g_hash_table_lookup (table, name) != NULL) {
				g_message ("duplicate entry for file system type %s", name);
			}
			type = g_new (NautilusFileSystemType, 1);
			type->name = g_strdup (name);
			type->default_volume_name = g_strdup (default_volume_name);
			type->can_handle_trash = eel_str_is_equal (trash, "yes");
			g_hash_table_insert (table, type->name, type);

			xmlFree (default_volume_name);
			xmlFree (trash);
		}

		xmlFree (name);
	}

	xmlFreeDoc (doc);

	return table;
}

static void
nautilus_volume_monitor_initialize (NautilusVolumeMonitor *monitor)
{
	/* Set up details */
	monitor->details = g_new0 (NautilusVolumeMonitorDetails, 1);	
	monitor->details->readable_mount_point_names = create_readable_mount_point_name_table ();
	monitor->details->file_system_table = load_file_system_table ();
	monitor->details->removable_volumes = get_removable_volumes (monitor);
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
		= gtk_signal_new ("volume_unmount_failed",
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
nautilus_volume_is_removable (const NautilusVolume *volume)
{
	return volume->is_removable;
}

gboolean
nautilus_volume_is_read_only (const NautilusVolume  *volume)
{
	return volume->is_read_only;
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

/**
 * nautilus_volume_monitor_get_volume_for_path:
 * @path: a local filesystem path
 * 
 * Find the volume in which @path resides.
 * 
 * Return value: a NautilusVolume for @path, or %NULL if the operation
 *   fails, probably because stat() fails on @path.
 *    
 **/
NautilusVolume *
nautilus_volume_monitor_get_volume_for_path (NautilusVolumeMonitor *monitor,
					     const char            *path)
{
	struct stat statbuf;
	dev_t device;
	GList *p;
	NautilusVolume *volume;

	if (stat (path, &statbuf) != 0)
		return NULL;

	device = statbuf.st_dev;

	for (p = monitor->details->mounts; p != NULL; p = p->next) {
		volume = (NautilusVolume *) p->data;

		if (volume->device == device) {
			return volume;
		}
	}

	return NULL;
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
get_removable_volumes (NautilusVolumeMonitor *monitor)
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
			volume = create_volume (ent[index].f_mntfromname,
						ent[index].f_mntoname);
			volume->is_removable = TRUE;
			volumes = finish_creating_volume_and_prepend
				(monitor, volume, ent[index].f_fstyename, volumes);
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
		if (eel_str_has_prefix (ent->mnt_special, noauto_string)) {
			volume = create_volume (ent->mnt_special, ent->mnt_mountp);
			volume->is_removable = TRUE;
			volumes = finish_creating_volume_and_prepend
				(monitor, volume, ent->mnt_fstype, volumes);
		}
	}
#elif defined (HAVE_MNTENT_H)
	while ((ent = getmntent (file)) != NULL) {
		if (has_removable_mntent_options (ent)) {
			volume = create_volume (ent->mnt_fsname, ent->mnt_dir);
			volumes = finish_creating_volume_and_prepend
				(monitor, volume, ent->mnt_type, volumes);
		}
	}
#endif
			
	fclose (file);
	
#ifdef HAVE_CDDA
	volume = create_volume (CD_AUDIO_PATH, CD_AUDIO_PATH);
	volumes = finish_creating_volume_and_prepend (monitor, volume, "cdda", volumes);
#endif

	load_additional_mount_list_info (volumes);
	
	/* Move all floppy mounts to top of list */
	return g_list_sort (g_list_reverse (volumes), (GCompareFunc) floppy_sort);
}

#ifndef SOLARIS_MNT

static gboolean
volume_is_removable (const NautilusVolume *volume)
{
	gboolean removable;
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

	removable = FALSE;
	
	/* Search for our device in the fstab */
#ifdef HAVE_SYS_MNTTAB_H
	ent = &ent_storage;
	while (!getmntent (file, ent)) {
		if (strcmp (volume->device_path, ent->mnt_special) == 0) {
   		 	if (eel_str_has_prefix (ent->mnt_special, noauto_string)) {
				removable = TRUE;
				break;
			}
		}	
	}
#elif defined (HAVE_MNTENT_H)
	while ((ent = getmntent (file)) != NULL) {
		if (strcmp (volume->device_path, ent->mnt_fsname) == 0
		    && has_removable_mntent_options (ent)) {
			removable = TRUE;
			break;
		}	
	}
#endif
	
	fclose (file);
	return removable;
}

#endif /* !SOLARIS_MNT */

char *
nautilus_volume_get_name (const NautilusVolume *volume)
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
 
static char *
modify_volume_name_for_display (const char *unmodified_name)
{
	int index;
	char *name;

	if (unmodified_name == NULL) {
		return NULL;
	}
	
	name = g_strdup (unmodified_name);

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
	return name;
}

/* nautilus_volume_monitor_get_target_uri
 *	
 * Returns the activation uri of the volume
 */
 
char *
nautilus_volume_get_target_uri (const NautilusVolume *volume)
{
	char *uri, *escaped_path;

	if (volume->is_audio_cd) {
		escaped_path = gnome_vfs_escape_path_string (volume->mount_path);
		uri = g_strconcat ("cdda://", escaped_path, NULL);
		g_free (escaped_path);
		return uri;
	} else {
		return gnome_vfs_get_uri_from_local_path (volume->mount_path);
	}
}

gboolean 
nautilus_volume_should_integrate_trash (const NautilusVolume *volume)
{
	g_return_val_if_fail (volume != NULL, FALSE);
	return volume->file_system_type != NULL
		&& volume->file_system_type->can_handle_trash;
}

const char *
nautilus_volume_get_mount_path (const NautilusVolume *volume)
{
	g_return_val_if_fail (volume != NULL, NULL);
	return volume->mount_path;
}

const NautilusDeviceType
nautilus_volume_get_device_type (const NautilusVolume *volume)
{
	g_return_val_if_fail (volume != NULL, NAUTILUS_DEVICE_UNKNOWN);
	return volume->device_type;
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

static char *
mount_volume_make_cdrom_name (NautilusVolume *volume)
{
	char *name;
	int fd, disctype;

	disctype = get_cdrom_type (volume->device_path, &fd);

	switch (disctype) {
	case CDS_AUDIO:
		name = g_strdup (_("Audio CD"));
		break;
		
	case CDS_DATA_1:
	case CDS_DATA_2:
	case CDS_XA_2_1:
	case CDS_XA_2_2:
	case CDS_MIXED:
		/* Get volume name */
		name = get_iso9660_volume_name (volume, fd);
		break;

	default:
		name = NULL;
  	}
	
	close (fd);

	return name;
}

static char *
make_volume_name_from_path (NautilusVolume *volume)
{
	const char *name;
	
	name = strrchr (volume->mount_path, '/');
	if (name == NULL) {
		if (volume->file_system_type == NULL) {
			return NULL;
		} else {
			return g_strdup (volume->file_system_type->default_volume_name);
		}
	}
	if (name[0] == '/' && name[1] == '\0') {
		return g_strdup (_("Root Volume"));
	}
	return modify_volume_name_for_display (name + 1);
}

static char *
mount_volume_make_name (NautilusVolume *volume)
{
	if (volume->is_audio_cd) {
		return g_strdup (_("Audio CD"));
	} else if (volume->device_type == NAUTILUS_DEVICE_CDROM_DRIVE) {
		return mount_volume_make_cdrom_name (volume);
	} else {
		return make_volume_name_from_path (volume);
	}
}


static void
mount_volume_activate (NautilusVolumeMonitor *monitor, NautilusVolume *volume)
{
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

	switch (volume->device_type) {
	case NAUTILUS_DEVICE_CDROM_DRIVE:
		pthread_create (&eject_thread, NULL, eject_device, g_strdup (volume->device_path));
		break;
	default:
	}

	gtk_signal_emit (GTK_OBJECT (monitor),
			 signals[VOLUME_UNMOUNTED],
			 volume);
}

static void
free_mount_list (GList *mount_list)
{
	g_list_foreach (mount_list, (GFunc) nautilus_volume_free, NULL);
	g_list_free (mount_list);
}

static GList *
copy_mount_list (GList *mount_list)
{
	GList *new_list = NULL;
	GList *list =  mount_list;
	NautilusVolume *volume;
		
	while (list) {
		volume = list->data;

		new_list = g_list_prepend (new_list, copy_volume (volume));
				
		list = list->next;
	}
	
	return g_list_reverse (new_list);
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
get_mount_list (NautilusVolumeMonitor *monitor) 
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
                volume = create_volume (ent.mnt_special, ent.mnt_mountp);
                volume->is_removable = has_removable_mntent_options (&ent);
                volumes = finish_creating_volume_and_prepend
			(monitor, volume, ent.mnt_fstype, volumes);
        }

	fclose (fh);

        return volumes;
}

#else /* !SOLARIS_MNT */

static gboolean
option_list_has_option (const char *optlist,
			const char *option)
{
        gboolean retval = FALSE;
        char **options;
        int i;
	
        options = g_strsplit (optlist, ",", -1);
	
        for (i = 0; options[i]; i++) {
                if (!strcmp (options[i], option)) {
                        retval = TRUE;
                        break;
                }
        }
	
        g_strfreev (options);
	
	return retval;
}

static GList *
get_mount_list (NautilusVolumeMonitor *monitor) 
{
        GList *volumes;
        NautilusVolume *volume;
	static time_t last_mtime = 0;
        static FILE *fh = NULL;
        static GList *saved_list = NULL;
        const char *file_name;
	const char *separator;
	char line[PATH_MAX * 3];
	char device_name[sizeof (line)];
	EelStringList *list;
	char *device_path, *mount_path, *file_system_type_name;
	struct stat sb;

	volumes = NULL;
        
	if (mnttab_exists) { 
		file_name = "/etc/mnttab";
		separator = "\t";
	} else {
		file_name = "/proc/mounts";
		separator = " ";
	}

	/* /proc/mounts mtime never changes, so stat /etc/mtab.
	 * Isn't this lame?
	 */
	if (stat ("/etc/mtab", &sb) < 0) {
		g_warning ("Unable to stat %s: %s", file_name,
			   g_strerror (errno));
		return NULL;
	}
	
	if (sb.st_mtime == last_mtime) {
		return copy_mount_list (saved_list);
	}

	last_mtime = sb.st_mtime;
	
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
                        file_system_type_name = eel_string_list_nth (list, 2);
                        volume = create_volume (device_path, mount_path);
			if (eel_string_list_get_length (list) >= 4 &&
			    option_list_has_option (eel_string_list_nth (list, 3), MNTOPT_RO))
				volume->is_read_only = TRUE;
                        volumes = finish_creating_volume_and_prepend
				(monitor, volume, file_system_type_name, volumes);
 			
                        g_free (device_path);
                        g_free (mount_path);
                        g_free (file_system_type_name);
                }

		eel_string_list_free (list);
  	}

	free_mount_list (saved_list);
        saved_list = volumes;
	
        return copy_mount_list (volumes);
}

#endif /* !SOLARIS_MNT */


static GList *
get_current_mount_list (NautilusVolumeMonitor *monitor)
{
	GList *volumes;
#ifdef HAVE_CDDA
        NautilusVolume *volume;
#endif

	volumes = get_mount_list (monitor);

#ifdef HAVE_CDDA
	/* CD Audio tricks */
	if (locate_audio_cd ()) {
		volume = create_volume (CD_AUDIO_PATH, CD_AUDIO_PATH);
		volume->volume_name = mount_volume_make_name (volume);
		volumes = finish_creating_volume_and_prepend (monitor, volume, "cdda", volumes);
	}
#endif

	return g_list_reverse (volumes);
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
	GList *current_mounts, *new_mounts, *old_mounts, *node;
	
	/* Get all current mounts */
	current_mounts = get_current_mount_list (monitor);
	if (current_mounts == NULL) {
		return;
	}
  	
  	/* If the new list is the same of the current list, bail. */
	if (mount_lists_are_identical (current_mounts, monitor->details->mounts)) {
		free_mount_list (current_mounts);
		return;
	}
		
	/* Process list results to check for a properties that require opening files on disk. */
	load_additional_mount_list_info (current_mounts);
	
	/* Create list of new and old mounts */
	new_mounts = build_volume_list_delta (current_mounts, monitor->details->mounts);
	old_mounts = build_volume_list_delta (monitor->details->mounts, current_mounts);  		
		
	/* Free previous mount list and replace with new */
	free_mount_list (monitor->details->mounts);
	monitor->details->mounts = current_mounts;

	/* Check and see if we have new mounts to add */
	for (node = new_mounts; node != NULL; node = node->next) {
		mount_volume_activate (monitor, node->data);
	}				
	
	/* Check and see if we have old mounts to remove */
	for (node = old_mounts; node != NULL; node = node->next) {
		mount_volume_deactivate (monitor, node->data);
	}
	
	free_mount_list (old_mounts);
	free_mount_list (new_mounts);
}

static int
mount_volumes_check_status (NautilusVolumeMonitor *monitor)
{
	verify_current_mount_state (monitor);
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
	volume->device_type = NAUTILUS_DEVICE_CDROM_DRIVE;
	
	return TRUE;
}

/* This is intended mainly for adding removable volumes from /etc/fstab.
 * The auto type will not show up in /proc/mounts.
 */
static gboolean
mount_volume_auto_add (NautilusVolume *volume)
{
	if (eel_str_has_prefix (volume->device_path, floppy_device_path_prefix)) {	
		volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;	
	}
	/* FIXME: add cdroms to this too */
	return TRUE;
}

static gboolean
mount_volume_cdda_add (NautilusVolume *volume)
{
	volume->device_type = NAUTILUS_DEVICE_CDROM_DRIVE;	
	volume->is_audio_cd = TRUE;
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
		
	volume->device_type = NAUTILUS_DEVICE_NFS;	

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
					     NautilusEachVolumeCallback function,
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
	if (mount) {
		if (strstr (detailed_msg, "is write-protected, mounting read-only") != NULL) {
			/* This is not an error. Just an informative message from mount. */
			return;
		} else if ((strstr (detailed_msg, "is not a valid block device") != NULL) ||
			   (strstr (detailed_msg, "No medium found") != NULL)) {
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
		} else if (strstr (detailed_msg, "wrong fs type, bad option, bad superblock on") != NULL) {
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
	gchar *old_locale;

	info = arg;
	
	if (info != NULL) {	
		old_locale = g_getenv ("LC_ALL");
		eel_setenv ("LC_ALL", "C", TRUE);

		open_error_pipe ();
		file = popen (info->command, "r");
		close_error_pipe (info->should_mount, info->mount_point);
		pclose (file);
		
		if (old_locale != NULL) {
			eel_setenv ("LC_ALL", old_locale, TRUE);
		} else {
			eel_unsetenv("LC_ALL");
		}

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
	GList *node;
	NautilusVolume *found_volume;
	
	/* Find volume and set new name */
	for (node = monitor->details->mounts; node != NULL; node = node->next) {
		found_volume = node->data;
		if (strcmp (found_volume->device_path, volume->device_path) == 0) {
			g_free (found_volume->volume_name);
			found_volume->volume_name = g_strdup (volume_name);
			return;
		}
	}
}

static NautilusVolume *
create_volume (const char *device_path, const char *mount_path)
{
	NautilusVolume *volume;

	volume = g_new0 (NautilusVolume, 1);

	volume->device_path = g_strdup (device_path);
	volume->mount_path = g_strdup (mount_path);

	return volume;
}

static NautilusVolume *
copy_volume (const NautilusVolume *volume)
{
	NautilusVolume *new_volume;

	new_volume = g_new (NautilusVolume, 1);

	new_volume->device_type = volume->device_type;
	new_volume->file_system_type = volume->file_system_type;

	new_volume->device_path = g_strdup (volume->device_path);
	new_volume->mount_path = g_strdup (volume->mount_path);
	new_volume->volume_name = g_strdup (volume->volume_name);
	new_volume->device = volume->device;
	
	new_volume->is_read_only = volume->is_read_only;
	new_volume->is_removable = volume->is_removable;
	new_volume->is_audio_cd = volume->is_audio_cd;

	return new_volume;
}

void
nautilus_volume_free (NautilusVolume *volume)
{
	g_free (volume->device_path);
	g_free (volume->mount_path);
	g_free (volume->volume_name);
	g_free (volume);
}

static char *
get_iso9660_volume_name (NautilusVolume *volume, int fd)
{
	struct iso_primary_descriptor iso_buffer;

	lseek (fd, (off_t) 2048*16, SEEK_SET);
	read (fd, &iso_buffer, 2048);
	
	if (iso_buffer.volume_id == NULL) {
		return g_strdup (_("ISO 9660 Volume"));
	}

	return modify_volume_name_for_display (iso_buffer.volume_id);
}

static void
load_additional_mount_list_info (GList *volume_list)
{		
	GList *node;
	NautilusVolume *volume;
	
	for (node = volume_list; node != NULL; node = node->next) {
		volume = node->data;
		
#ifndef SOLARIS_MNT
		/* These are set up by get_current_mount_list for Solaris. */
		volume->is_removable = volume_is_removable (volume);
#endif

		volume->volume_name = mount_volume_make_name (volume);
	}
}

static gboolean
finish_creating_volume (NautilusVolumeMonitor *monitor, NautilusVolume *volume,
			const char *file_system_type_name)
{
	gboolean ok;
	const char *name;
	struct stat statbuf;
	
	volume->file_system_type = g_hash_table_lookup
		(monitor->details->file_system_table, file_system_type_name);

	if (strcmp (file_system_type_name, "auto") == 0) {		
		ok = mount_volume_auto_add (volume);
	} else if (strcmp (file_system_type_name, "cdda") == 0) {		
		ok = mount_volume_cdda_add (volume);
	} else if (strcmp (file_system_type_name, "iso9660") == 0) {		    		
		ok = mount_volume_iso9660_add (volume);
	} else if (strcmp (file_system_type_name, "nfs") == 0) {		
		ok = mount_volume_nfs_add (volume);
	} else {
		ok = TRUE;
	}
	
	if (!ok) {
		return FALSE;
	}

	if (stat (volume->mount_path, &statbuf) == 0) {
		volume->device = statbuf.st_dev;
	}

	/* Identify device type */
	if (eel_str_has_prefix (volume->mount_path, "/mnt/")) {		
		name = volume->mount_path + strlen ("/mnt/");
		
		if (eel_str_has_prefix (name, "cdrom")) {
			volume->device_type = NAUTILUS_DEVICE_CDROM_DRIVE;
			volume->is_removable = TRUE;
		} else if (eel_str_has_prefix (name, "floppy")) {
			volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;
				volume->is_removable = TRUE;
		} else if (eel_str_has_prefix (volume->device_path, floppy_device_path_prefix)) {		
			volume->device_type = NAUTILUS_DEVICE_FLOPPY_DRIVE;
			volume->is_removable = TRUE;
		} else if (eel_str_has_prefix (name, "zip")) {
			volume->device_type = NAUTILUS_DEVICE_ZIP_DRIVE;
			volume->is_removable = TRUE;
		} else if (eel_str_has_prefix (name, "jaz")) {
			volume->device_type = NAUTILUS_DEVICE_JAZ_DRIVE;
			volume->is_removable = TRUE;
		} else if (eel_str_has_prefix (name, "camera")) {
			volume->device_type = NAUTILUS_DEVICE_CAMERA;
			volume->is_removable = TRUE;					
		} else if (eel_str_has_prefix (name, "memstick")) {
			volume->device_type = NAUTILUS_DEVICE_MEMORY_STICK;
			volume->is_removable = TRUE;
		} else {
			volume->is_removable = FALSE;
		}
	}
	
	return TRUE;
}

static GList *
finish_creating_volume_and_prepend (NautilusVolumeMonitor *monitor,
				    NautilusVolume *volume,
				    const char *file_system_type_name,
				    GList *list)
{
	if (finish_creating_volume (monitor, volume, file_system_type_name)) {
		list = g_list_prepend (list, volume);
	} else {
		nautilus_volume_free (volume);
	}
	return list;
}

char *
nautilus_volume_monitor_get_mount_name_for_display (NautilusVolumeMonitor *monitor,
						    const NautilusVolume *volume)
{
	const char *name, *found_name;

	g_return_val_if_fail (monitor != NULL, NULL);
	g_return_val_if_fail (volume != NULL, NULL);
	
	name = strrchr (volume->mount_path, '/');
	if (name != NULL) {
		name = name + 1;
	} else {
		name = volume->mount_path;
	}
	
	/* Look for a match in our localized mount name list */
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
