/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

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

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
*/

#include <config.h>

#include "fm-desktop-icon-view.h"

#include "fm-cdrom-extensions.h"
#include "fm-icon-view.h"

#include "iso9660.h"
#include "src/nautilus-application.h"

#include <errno.h>
#include <fcntl.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-directory-private.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <mntent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



#define MOUNT_TYPE_ISO9660 	"iso9660"
#define MOUNT_TYPE_EXT2 	"ext2"
#define MOUNT_OPTIONS_USER 	"user"
#define MOUNT_OPTIONS_OWNER 	"owner"

#define CHECK_INTERVAL 		2000

const char * const state_names[] = { 
	"ACTIVE", 
	"INACTIVE", 
	"EMPTY" 
};

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

const char * const type_names[] = { 
	"CDROM", 
	"FLOPPY",
	"LOCAL_DISK",
	"OTHER" 
};

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

	NautilusFile *file;	
} DeviceInfo;

struct FMDesktopIconViewDetails
{
	GHashTable *devices_by_fsname;
	GList *devices;
};

static void fm_desktop_icon_view_initialize		(FMDesktopIconView        *desktop_icon_view);
static void fm_desktop_icon_view_initialize_class	(FMDesktopIconViewClass   *klass);

static void fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu);

static char *   fm_desktop_icon_view_get_directory_sort_by       (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_sort_by       (FMIconView *icon_view, NautilusDirectory *directory, const char* sort_by);
static gboolean fm_desktop_icon_view_get_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory, gboolean sort_reversed);
static gboolean fm_desktop_icon_view_get_directory_auto_layout   (FMIconView *icon_view, NautilusDirectory *directory);
static void     fm_desktop_icon_view_set_directory_auto_layout   (FMIconView *icon_view, NautilusDirectory *directory, gboolean auto_layout);

static void		find_mount_devices 			 (FMDesktopIconView 	*icon_view, 
								  const char 		*fstab_path);
static void		remove_mount_link 			 (DeviceInfo 		*device);								  
static void		get_iso9660_volume_name 		 (DeviceInfo 		*device);
static void		get_ext2_volume_name 			 (DeviceInfo 		*device);
static void		remove_mount_symlinks 			 (DeviceInfo 		*device, 
								  FMDesktopIconView 	*icon_view);
static void		free_device_info 			 (DeviceInfo 		*device, 
								  FMDesktopIconView 	*icon_view);
static void		place_home_directory 			 (FMDesktopIconView 	*icon_view);
static GnomeVFSResult	create_desktop_link 			 (const char 		*directory_path, 
								  const char 		*name, 
								  const char 		*image, 
								  const char 		*uri);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView, fm_desktop_icon_view, FM_TYPE_ICON_VIEW);

static NautilusIconContainer *
get_icon_container (FMDesktopIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_DESKSTOP_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
fm_desktop_icon_view_destroy (GtkObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Remove symlink mount files */
	g_list_foreach (icon_view->details->devices, (GFunc)remove_mount_symlinks, icon_view);

	/* Clean up other device info */
	g_list_foreach (icon_view->details->devices, (GFunc)free_device_info, icon_view);
	
	/* Clean up details */	 
	g_hash_table_destroy (icon_view->details->devices_by_fsname);
	g_list_free (icon_view->details->devices);
	g_free (icon_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


static void
fm_desktop_icon_view_initialize_class (FMDesktopIconViewClass *klass)
{
	GtkObjectClass		*object_class;
	FMDirectoryViewClass	*fm_directory_view_class;
	FMIconViewClass		*fm_icon_view_class;

	object_class		= GTK_OBJECT_CLASS (klass);
	fm_directory_view_class	= FM_DIRECTORY_VIEW_CLASS (klass);
	fm_icon_view_class	= FM_ICON_VIEW_CLASS (klass);

	object_class->destroy = fm_desktop_icon_view_destroy;

        fm_directory_view_class->create_background_context_menu_items = fm_desktop_icon_view_create_background_context_menu_items;

        fm_icon_view_class->get_directory_sort_by       = fm_desktop_icon_view_get_directory_sort_by;
        fm_icon_view_class->set_directory_sort_by       = fm_desktop_icon_view_set_directory_sort_by;
        fm_icon_view_class->get_directory_sort_reversed = fm_desktop_icon_view_get_directory_sort_reversed;
        fm_icon_view_class->set_directory_sort_reversed = fm_desktop_icon_view_set_directory_sort_reversed;
        fm_icon_view_class->get_directory_auto_layout   = fm_desktop_icon_view_get_directory_auto_layout;
        fm_icon_view_class->set_directory_auto_layout   = fm_desktop_icon_view_set_directory_auto_layout;
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
	NautilusIconContainer *icon_container;

	icon_container = get_icon_container (desktop_icon_view);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	
	desktop_icon_view->details->devices_by_fsname = g_hash_table_new (g_str_hash, g_str_equal);
	desktop_icon_view->details->devices = NULL;

	/* Setup home directory link */
	place_home_directory (desktop_icon_view);
	
	/* Check for mountable devices */
	find_mount_devices (desktop_icon_view, _PATH_MNTTAB);
}

static void
fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu)
{
	GtkWidget *menu_item;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_background_context_menu_items, 
		 (view, menu));

        menu_item = gtk_menu_item_new_with_label (_("Close Nautilus Desktop"));
	gtk_signal_connect (GTK_OBJECT (menu_item),
			    "activate",
			    GTK_SIGNAL_FUNC (nautilus_application_close_desktop),
			    NULL);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

static char *
fm_desktop_icon_view_get_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory)
{
	return g_strdup("name");
}

static void
fm_desktop_icon_view_set_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory, const char* sort_by)
{
	/* do nothing - the desktop always uses the same sort_by */
}

static gboolean
fm_desktop_icon_view_get_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory, gboolean sort_reversed)
{
	/* do nothing - the desktop always uses sort_reversed == FALSE */
}

static gboolean
fm_desktop_icon_view_get_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory, gboolean auto_layout)
{
	/* do nothing - the desktop always uses auto_layout == FALSE */
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
	switch(device->type) {
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

typedef gboolean (*ChangeFunc)(FMDesktopIconView *view, DeviceInfo *device);

static void
mount_device_mount (FMDesktopIconView *view, DeviceInfo *device)
{
	char *target_uri, *desktop_uri, *icon_name;
	NautilusIconContainer *container;
	GnomeVFSResult result;
	int index;
	GList *new_files_list;

	new_files_list = NULL;
	container = get_icon_container (view);

	desktop_uri = nautilus_get_desktop_directory ();
	target_uri = g_strdup_printf ("file://%s/", device->mount_path);

	/* Make volume name link "nice" */

	/* Strip whitespace from the end of the name. */
	for (index = strlen (device->volume_name) - 1; index > 0; index--) {
		if (device->volume_name [index] != ' ') {
			break;
		}
		device->volume_name [index] = '\0';
	}

	/* The volume name may have '/' characters. We need to convert them to 
	 * something legal.
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
	 * we double check to be sure. */
	remove_mount_link (device);

	/* Get icon type */
	if (strcmp (device->mount_type, "cdrom") == 0) {
		icon_name = g_strdup ("i-cdrom.png");
	} else if (strcmp (device->mount_type, "floppy") == 0) {
		icon_name = g_strdup ("i-floppy.png");
	} else {
		icon_name = g_strdup ("i-blockdev.png");
	}
	
	/* Create link */
	device->link_uri = g_strdup_printf ("%s/%s", desktop_uri, device->volume_name);

	result = create_desktop_link (desktop_uri, device->volume_name, icon_name, target_uri);
	if (result == GNOME_VFS_OK) {
		new_files_list = g_list_prepend (new_files_list, device->link_uri);
		nautilus_directory_notify_files_added (new_files_list);
		g_list_free (new_files_list);
	} else {
		g_message ("Unable to create mount link: %s", gnome_vfs_result_to_string (result));
		g_free (device->link_uri);
		device->link_uri = NULL;
	}
	
	g_free (desktop_uri);
	g_free (target_uri);
	g_free (icon_name);
	
	device->did_mount = TRUE;
}


static void
mount_device_activate_cdrom (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	int disctype;

	if(device->device_fd < 0) {
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
	mount_device_mount (view, device);
}

static void
mount_device_activate_ext2 (FMDesktopIconView *view, DeviceInfo *device)
{
	/* Get volume name */
	get_ext2_volume_name (device);

	mount_device_mount (view, device);
}

static gboolean
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
			g_message ("Unknown mount type");
			break;
	}

	return FALSE;
}

static gboolean
mount_device_deactivate (FMDesktopIconView *icon_view, DeviceInfo *device)
{
	NautilusIconContainer *container;
	
	/* Clean up old link */
	remove_mount_link (device);

	/* Remove mounted device icon from desktop */
	if (device->file != NULL) {
		container = get_icon_container (icon_view);
		nautilus_icon_container_remove (container, NAUTILUS_ICON_CONTAINER_ICON_DATA (device->file));
		nautilus_file_unref (device->file);
		device->file = NULL;
	}

	device->did_mount = FALSE;

	return TRUE;
}

static void
mount_device_check_change (DeviceInfo *device, FMDesktopIconView *icon_view)
{
	/* What functions to run for particular state transitions */
	static ChangeFunc state_transitions[STATE_LAST][STATE_LAST] = {
		/************  from: ACTIVE                 	INACTIVE                 EMPTY */
		/* to */
		/* ACTIVE */   {     NULL,                  	mount_device_activate,	 mount_device_activate  },
		/* INACTIVE */ {     mount_device_deactivate, 	NULL,   	 	 mount_device_activate 	},
		/* EMPTY */    {     mount_device_deactivate, 	mount_device_deactivate, NULL               	}
	};	

	DeviceState old_state;

  	old_state = device->state;

  	mount_device_set_state (device, icon_view);

  	if(old_state != device->state) {
    		ChangeFunc func;

    		func = state_transitions[device->state][old_state];

    		g_message ("State on %s changed from %s to %s, running %p",
			   device->fsname, state_names[old_state], state_names[device->state], func);
			
    		if (func != NULL) {
      			func (icon_view, device);
      		}
  	}
}

static void
mount_devices_update_is_mounted (FMDesktopIconView *icon_view)
{
	FILE *fh;
	char line[PATH_MAX * 3], mntpoint[PATH_MAX], devname[PATH_MAX];
	GList *ltmp;
	DeviceInfo *device;

	for (ltmp = icon_view->details->devices; ltmp; ltmp = g_list_next (ltmp)) {
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

	g_list_foreach (icon_view->details->devices, (GFunc)mount_device_check_change, icon_view);
	
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
	newdev->file 	    = NULL;
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
		g_message ("Unknown file system: %s", ent->mnt_type);
	}
	
	if (mounted) {
		icon_view->details->devices = g_list_append (icon_view->details->devices, newdev);
		mount_device_add_aliases (icon_view, newdev->fsname, newdev);
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

static void
find_mount_devices (FMDesktopIconView *icon_view, const char *fstab_path)
{
	FILE *mef;
	struct mntent *ent;

	mef = setmntent (fstab_path, "r");
	g_return_if_fail (mef);

	while ((ent = getmntent (mef))) {
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
	gtk_timeout_add (CHECK_INTERVAL, (GtkFunction) mount_devices_check_status, icon_view);
}


static void
remove_mount_link (DeviceInfo *device)
{
	GnomeVFSResult result;
	
	if (device->link_uri != NULL) {
		result = gnome_vfs_unlink (device->link_uri);
		if (result != GNOME_VFS_OK) {
			g_message ("Unable to remove mount link: %s", gnome_vfs_result_to_string (result));
		}
		g_free (device->link_uri);
		device->link_uri = NULL;
	}
}


static void
remove_mount_symlinks (DeviceInfo *device, FMDesktopIconView *icon_view)
{
	remove_mount_link (device);
}


static void
free_device_info (DeviceInfo *device, FMDesktopIconView *icon_view)
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

	if (device->file != NULL) {
		nautilus_file_unref (device->file);
		device->file = NULL;
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

/* place_home_directory
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
static void
place_home_directory (FMDesktopIconView *icon_view)
{
	char *user_path, *desktop_uri, *user_homelink_uri, *user_icon_name;
	GnomeVFSResult result;
	GnomeVFSFileInfo info;
	GList *new_files_list;
	
	user_path = g_strdup_printf ("%s/", g_get_home_dir ());
	desktop_uri = nautilus_get_desktop_directory ();
	user_icon_name = g_strdup_printf("%s's Home", g_get_user_name ());
	user_homelink_uri = g_strdup_printf ("%s/%s", desktop_uri, user_icon_name);
	
	result = gnome_vfs_get_file_info (user_homelink_uri, &info, 0);
	if (result != GNOME_VFS_OK) {
		/* There was no link file.  Create it and add it to the desktop view */		
		result = create_desktop_link (desktop_uri, user_icon_name, "temp-home.png", user_path);
		if (result == GNOME_VFS_OK) {
			new_files_list = NULL;
			new_files_list = g_list_prepend (new_files_list, user_homelink_uri);
			nautilus_directory_notify_files_added (new_files_list);
			g_list_free (new_files_list);
		} else {
			g_message ("Unable to create home link: %s", gnome_vfs_result_to_string (result));
		}		
	}
	
	g_free (desktop_uri);
	g_free (user_path);
	g_free (user_homelink_uri);
	g_free (user_icon_name);
}


static GnomeVFSResult
create_desktop_link (const char *directory_path, const char *name, const char *image, const char *uri)
{
	xmlDocPtr output_document;
	xmlNodePtr root_node;
	char *file_name;
	int result;
	
	/* create a new xml document */
	output_document = xmlNewDoc ("1.0");
	
	/* add the root node to the output document */
	root_node = xmlNewDocNode (output_document, NULL, "NAUTILUS_OBJECT", NULL);
	xmlDocSetRootElement (output_document, root_node);

	/* Add mime magic string so that the mime sniffer can recognize us.
	 * Note: The value of the tag has no meaning.  */
	xmlSetProp (root_node, "NAUTILUS_LINK", "Nautilus Link");

	/* Add link and custom icon tags */
	xmlSetProp (root_node, "CUSTOM_ICON", image);
	xmlSetProp (root_node, "LINK", uri);
	
	/* all done, so save the xml document as a link file */
	file_name = g_strdup_printf ("%s/%s", directory_path, name);
	result = xmlSaveFile (file_name, output_document);
	g_free (file_name);
	
	xmlFreeDoc (output_document);

	return GNOME_VFS_OK;
}
