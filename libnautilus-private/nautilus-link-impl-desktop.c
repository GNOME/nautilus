#include <config.h>
#include "nautilus-link.h"
#include "nautilus-link-impl-desktop.h"
#include "desktop-file-loader.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>

#define NAUTILUS_LINK_GENERIC_TAG	"Link"
#define NAUTILUS_LINK_TRASH_TAG 	"X-nautilus-trash"
#define NAUTILUS_LINK_MOUNT_TAG 	"FSDevice"
#define NAUTILUS_LINK_HOME_TAG 		"X-nautilus-home"

#define REMOTE_ICON_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				     | GNOME_VFS_PERM_GROUP_ALL \
				     | GNOME_VFS_PERM_OTHER_ALL)


typedef struct {
	char *link_uri;
	char *file_path;
} NautilusLinkIconNotificationInfo;

typedef void (* NautilusFileFunction) (NautilusFile *file);


static char*
slurp_uri_contents (const char *uri)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	GnomeVFSFileInfo info;
	char *buffer;
	GnomeVFSFileSize bytes_read;
        
	result = gnome_vfs_get_file_info (uri, &info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	buffer = g_malloc (info.size + 1);
	result = gnome_vfs_read (handle, buffer, info.size, &bytes_read);
	if (result != GNOME_VFS_OK) {
                g_free (buffer);
                buffer = NULL;
	}

	gnome_vfs_close (handle);

	return buffer;
}


/* utility to return the local pathname of a cached icon, given the leaf name */
/* if the icons directory hasn't been created yet, create it */
static char *
make_local_path (const char *image_uri)
{
	GnomeVFSResult result;
	
	char *escaped_uri, *local_directory_path, *local_directory_uri, *local_file_path;
	
	escaped_uri = gnome_vfs_escape_slashes (image_uri);		
	
	local_directory_path = g_strconcat
		(g_get_home_dir (),
		 "/.nautilus/remote_icons",
		 NULL);

	/* We must create the directory if it doesn't exist. */
	local_directory_uri = gnome_vfs_get_uri_from_local_path (local_directory_path);
	result = gnome_vfs_make_directory (local_directory_uri, REMOTE_ICON_DIR_PERMISSIONS);
	if (result != GNOME_VFS_OK) {
		g_free (local_directory_uri);
		g_free (escaped_uri);
		g_free (local_directory_path);
		return NULL;
	}
			
	local_file_path = nautilus_make_path (local_directory_path, escaped_uri);
	g_free (local_directory_uri);
	g_free (escaped_uri);
	g_free (local_directory_path);

	return local_file_path;
}

/* utility to free the icon notification info */

static void
free_icon_notification_info (NautilusLinkIconNotificationInfo *info)
{
	g_free (info->link_uri);
	g_free (info->file_path);
	g_free (info);
}

/* callback to handle the asynchronous reading of icons */
static void
icon_read_done_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	int size;
	FILE* outfile;
	NautilusFile *file;
	NautilusLinkIconNotificationInfo *info;
	
	info = (NautilusLinkIconNotificationInfo *) callback_data;

	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		free_icon_notification_info (info);
		return;
	}

	/* write out the file into the cache area */	
	size = file_size;
	outfile = fopen (info->file_path, "wb");	 	
	fwrite (file_contents, size, 1, outfile);
	fclose (outfile);

	g_free (file_contents);

	/* tell the world that the file has changed */
	file = nautilus_file_get (info->link_uri);
	if (file != NULL) {
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
	
	/* free up the notification info */	
	free_icon_notification_info (info);
}

static const char *
get_tag (NautilusLinkType type)
{
	switch (type) {
	default:
		g_assert_not_reached ();
		/* fall through */
	case NAUTILUS_LINK_GENERIC:
		return NAUTILUS_LINK_GENERIC_TAG;
	case NAUTILUS_LINK_TRASH:
		return NAUTILUS_LINK_TRASH_TAG;
	case NAUTILUS_LINK_MOUNT:
		return NAUTILUS_LINK_MOUNT_TAG;
	case NAUTILUS_LINK_HOME:
		return NAUTILUS_LINK_HOME_TAG;
	}
}

static gchar *
slurp_key_string (const char *path,
		  const char *keyname)
{
	DesktopFile *desktop_file;
	gchar *text;
	gboolean set;
	gchar *contents;
	contents = slurp_uri_contents (path);

	if (contents == NULL)
		return NULL;
	desktop_file = desktop_file_from_string (contents);
	g_free (contents);

	if (desktop_file == NULL)
		return NULL;
	set = desktop_file_get_string (desktop_file,
				       "Desktop Entry",
				       keyname,
				       &text);
	desktop_file_free (desktop_file);
	if (set == FALSE)
		return NULL;

	return text;
}

gboolean
nautilus_link_impl_desktop_local_create (const char        *directory_path,
					 const char        *name,
					 const char        *image,
					 const char        *target_uri,
					 const GdkPoint    *point,
					 NautilusLinkType   type)
{
	FILE *desktop_file;
	gchar *path;
	gchar *file_name;
	char *uri;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;

	g_return_val_if_fail (directory_path != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);
	g_return_val_if_fail (target_uri != NULL, FALSE);

	path = nautilus_make_path (directory_path, name);
	file_name = g_strconcat (path, ".desktop", NULL);
	g_free (path);
	desktop_file = fopen (file_name, "w");
	if (desktop_file == NULL) {
		g_free (file_name);
		return FALSE;
	}

	fwrite ("[Desktop Entry]\nEncoding=Legacy-Mixed\nName=", 1, strlen ("[Desktop Entry]\nEncoding=Legacy-Mixed\nName="), desktop_file);
	fwrite (name, 1, strlen (name), desktop_file);
	fwrite ("\nType=", 1, strlen ("\nType="), desktop_file);
	fwrite (get_tag (type), 1, strlen (get_tag (type)), desktop_file);
	fwrite ("\nX-Nautilus-Icon=", 1, strlen ("\nX-Nautilus-Icon="), desktop_file);
	fwrite (image, 1, strlen (image), desktop_file);
	fwrite ("\nURL=", 1, strlen ("\nURL="), desktop_file);
	fwrite (target_uri, 1, strlen (target_uri), desktop_file);
	fwrite ("\n", 1, 1, desktop_file);
	/* ... */
	fclose (desktop_file);

	uri = gnome_vfs_get_uri_from_local_path (file_name);
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	nautilus_directory_schedule_metadata_remove (&dummy_list);

	if (point != NULL) {
		item.uri = uri;
		item.set = TRUE;
		item.point.x = point->x;
		item.point.y = point->y;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
	}

	g_free (uri);
	g_free (file_name);
	return TRUE;
}

gboolean
nautilus_link_impl_desktop_local_set_icon (const char *path,
					   const char *icon_name)
{
	/* We don't want to actually set the name of the file. */
	return FALSE;
}

gboolean
nautilus_link_impl_desktop_local_set_type (const char       *path,
					   NautilusLinkType  type)
{
	return FALSE;
}

gboolean
nautilus_link_impl_desktop_local_set_link_uri (const char *path,
					       const char *uri)
{
	return FALSE;
}

char *
nautilus_link_impl_desktop_local_get_text (const char *path)
{
	return slurp_key_string (path, "Name");
}

char *
nautilus_link_impl_desktop_local_get_additional_text (const char *path)
{
	gchar *type;
	gchar *retval;

	type = slurp_key_string (path, "Type");
	retval = NULL;
	if (type == NULL)
		return NULL;

	if (strcmp (type, "Application") == 0)
		retval = slurp_key_string (path, "Comment");
	g_free (type);

	return retval;
}

char *
nautilus_link_impl_desktop_local_get_image_uri (const char *path)
{
	char *icon_uri;
	char *local_path, *local_uri;
	NautilusLinkIconNotificationInfo *info;
	
	icon_uri = slurp_key_string (path, "X-Nautilus-Icon");
	
	if (icon_uri == NULL) {
		gchar *absolute;
		gchar *icon_name;

		/* Fall back to a standard icon. */
		icon_name = slurp_key_string (path, "Icon");
		if (icon_name == NULL)
			return NULL;

		absolute = gnome_pixmap_file (icon_name);
		if (absolute) {
			g_free (icon_name);
			icon_name = absolute;
		}
		if (icon_name[0] == '/')
			icon_uri = gnome_vfs_get_uri_from_local_path (icon_name);
		else
			icon_uri = NULL;
		g_free (icon_name);

		return icon_uri;
	}

	/* if the image is remote, see if we can find it in our local cache */
	if (eel_is_remote_uri (icon_uri)) {
		local_path = make_local_path (icon_uri);
		if (local_path == NULL) {
			g_free (icon_uri);
			return NULL;
		}
		if (g_file_exists (local_path)) {
			g_free (icon_uri);			
			local_uri = gnome_vfs_get_uri_from_local_path (local_path);
			g_free (local_path);
			return local_uri;	
		}
	 
		/* load it asynchronously through gnome-vfs */
	        info = g_new0 (NautilusLinkIconNotificationInfo, 1);
		info->link_uri = gnome_vfs_get_uri_from_local_path (path);
		info->file_path = g_strdup (local_path);
		eel_read_entire_file_async (icon_uri, icon_read_done_callback, info);
		
		g_free (icon_uri);
  		g_free (local_path);
		return NULL; /* return NULL since the icon is still loading - it will get correctly set by the callback */
	}
	
	return icon_uri;
}

NautilusLinkType
nautilus_link_impl_desktop_local_get_link_type (const char *path)
{
	gchar *type;
	NautilusLinkType retval;

	type = slurp_key_string (path, "Type");

	if (type == NULL)
		return NAUTILUS_LINK_GENERIC;
	if (strcmp (type, NAUTILUS_LINK_HOME_TAG) == 0)
		retval = NAUTILUS_LINK_HOME;
	else if (strcmp (type, NAUTILUS_LINK_MOUNT_TAG) == 0)
		retval = NAUTILUS_LINK_MOUNT;
	else if (strcmp (type, NAUTILUS_LINK_TRASH_TAG) == 0)
		retval = NAUTILUS_LINK_TRASH;
	else
		retval = NAUTILUS_LINK_GENERIC;

	g_free (type);
	return retval;
}

gboolean
nautilus_link_impl_desktop_local_is_volume_link (const char *path)
{
	return (nautilus_link_impl_desktop_local_get_link_type (path) ==  NAUTILUS_LINK_MOUNT);
}

gboolean
nautilus_link_impl_desktop_local_is_home_link (const char *path)
{
	return (nautilus_link_impl_desktop_local_get_link_type (path) ==  NAUTILUS_LINK_HOME);
}

gboolean
nautilus_link_impl_desktop_local_is_trash_link (const char *path)
{
	return (nautilus_link_impl_desktop_local_get_link_type (path) ==  NAUTILUS_LINK_TRASH);
}

static gchar *
nautilus_link_impl_desktop_get_link_uri_from_desktop (DesktopFile *desktop_file)
{
	gchar *type;
	gchar *retval;

	retval = NULL;

	type = NULL;
	if (! desktop_file_get_string (desktop_file,
				       "Desktop Entry",
				       "Type",
				       &type))
		return NULL;

	if (strcmp (type, "Application") == 0) {
		gchar *terminal_command;
		gchar *launch_string;
		gboolean need_term;

		if (! desktop_file_get_string (desktop_file,
					       "Desktop Entry",
					       "Exec",
					       &launch_string))
			return NULL;

		need_term = FALSE;
		desktop_file_get_boolean (desktop_file,
					  "Desktop Entry",
					  "Terminal",
					  &need_term);
		if (need_term) {
			terminal_command = eel_gnome_make_terminal_command (launch_string);
			retval = g_strconcat ("command:", terminal_command, NULL);
			g_free (terminal_command);
		} else {
			retval = g_strconcat ("command:", launch_string, NULL);
		}
		g_free (launch_string);
	} else if ((strcmp (type, NAUTILUS_LINK_GENERIC_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_TRASH_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_HOME_TAG) == 0)) {
		desktop_file_get_string (desktop_file,
					 "Desktop Entry",
					 "URL",
					 &retval);
	}
	return retval;
}


char *
nautilus_link_impl_desktop_local_get_link_uri (const char *path)
{
	DesktopFile *desktop_file;
	gchar *contents;
	gchar *retval;
	contents = slurp_uri_contents (path);
	if (contents == NULL)
		return NULL;
	desktop_file = desktop_file_from_string (contents);
	g_free (contents);

	if (desktop_file == NULL)
		return NULL;
	retval = nautilus_link_impl_desktop_get_link_uri_from_desktop (desktop_file);
	
	desktop_file_free (desktop_file);
	return retval;
}

char *
nautilus_link_impl_desktop_get_link_uri_given_file_contents (const char *link_file_contents,
							     int         link_file_size)
{
	DesktopFile *desktop_file;
	gchar *slurp;
	gchar *retval;

	slurp = g_strndup (link_file_contents, link_file_size);
	desktop_file = desktop_file_from_string (slurp);
	g_free (slurp);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_impl_desktop_get_link_uri_from_desktop (desktop_file);

	desktop_file_free (desktop_file);
	return retval;
	
}

void
nautilus_link_impl_desktop_local_create_from_gnome_entry    (GnomeDesktopEntry *entry,
							     const char        *dest_path,
							     const GdkPoint    *position)
{
	char *uri;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;
	GnomeDesktopEntry *new_entry;
	char *file_name;

	new_entry = gnome_desktop_entry_copy (entry);
	g_free (new_entry->location);
	file_name = g_strdup_printf ("%s.desktop", entry->name);
	new_entry->location = nautilus_make_path (dest_path, file_name);
	g_free (file_name);
	gnome_desktop_entry_save (new_entry);

	uri = gnome_vfs_get_uri_from_local_path (dest_path);
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	nautilus_directory_schedule_metadata_remove (&dummy_list);

	if (position != NULL) {
		item.uri = uri;
		item.set = TRUE;
		item.point.x = position->x;
		item.point.y = position->y;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
	}
	gnome_desktop_entry_free (new_entry);
}


#if 0
void
nautilus_desktop_file_launch (const char *uri)
{
	DesktopFile *desktop_file;
	gchar *contents;

	contents = slurp_uri_contents (uri);

	if (contents == NULL)
		return;

	desktop_file = desktop_file_from_string (contents);
	g_free (contents);

        if (desktop_file == NULL)
                return;
        
        desktop_file_launch (desktop_file);

        desktop_file_free (desktop_file);
}
#endif
