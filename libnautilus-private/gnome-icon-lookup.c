/*
 * Copyright (C) 2002 Alexander Larsson <alexl@redhat.com>.
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gnome-icon-lookup.h"
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs.h>

#include <string.h>

#define ICON_NAME_BLOCK_DEVICE          "file-blockdev"
#define ICON_NAME_BROKEN_SYMBOLIC_LINK  "file-symlink"
#define ICON_NAME_CHARACTER_DEVICE      "file-chardev"
#define ICON_NAME_DIRECTORY             "file-directory"
#define ICON_NAME_EXECUTABLE            "file-executable"
#define ICON_NAME_FIFO                  "file-fifo"
#define ICON_NAME_REGULAR               "file-regular"
#define ICON_NAME_SEARCH_RESULTS        "file-search"
#define ICON_NAME_SOCKET                "file-sock"
#define ICON_NAME_THUMBNAIL_LOADING     "loading"
#define ICON_NAME_TRASH_EMPTY           "trash-empty"
#define ICON_NAME_TRASH_NOT_EMPTY       "trash-full"

/* Returns NULL for regular */
static char *
get_icon_name (const char       *file_uri,
	       GnomeVFSFileInfo *file_info,
	       const char       *mime_type)
{
  /* FIXME: Special case trash here */

  if (file_info &&
      (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE))
    {
      switch (file_info->type)
	{
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
	  if (mime_type && g_ascii_strcasecmp (mime_type, "x-directory/search") == 0)
	    return g_strdup (ICON_NAME_SEARCH_RESULTS);
	  else
	    return g_strdup (ICON_NAME_DIRECTORY);
        case GNOME_VFS_FILE_TYPE_FIFO:
	  return g_strdup (ICON_NAME_FIFO);
        case GNOME_VFS_FILE_TYPE_SOCKET:
	  return g_strdup (ICON_NAME_SOCKET);
        case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
	  return g_strdup (ICON_NAME_CHARACTER_DEVICE);
        case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
	  return g_strdup (ICON_NAME_BLOCK_DEVICE);
        case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
	  /* Non-broken symbolic links return the target's type. */
	  return g_strdup (ICON_NAME_BROKEN_SYMBOLIC_LINK);
        case GNOME_VFS_FILE_TYPE_REGULAR:
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        default:
	  break;
	}
    }
  
  /* Regular or unknown: */

  /* don't use the executable icon for text files, since it's more useful to display
   * embedded text
   */
  if (file_info &&
      (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS) &&
      (file_info->permissions	& (GNOME_VFS_PERM_USER_EXEC
				   | GNOME_VFS_PERM_GROUP_EXEC
				   | GNOME_VFS_PERM_OTHER_EXEC)) &&
      (mime_type == NULL || g_ascii_strcasecmp (mime_type, "text/plain") != 0))
    return g_strdup (ICON_NAME_EXECUTABLE);

  return NULL;
}

static char *
get_vfs_mime_name (const char *mime_type)
{
  const char *vfs_mime_name;
  char *p;

  vfs_mime_name = gnome_vfs_mime_get_icon (mime_type);

  if (vfs_mime_name)
    {
      p = strrchr(vfs_mime_name, '.');

      if (p)
	return g_strndup (vfs_mime_name, p - vfs_mime_name);
      else
	return g_strdup (vfs_mime_name);
    }
  return NULL;
}

static char *
make_mime_name (const char *mime_type)
{
  char *mime_type_without_slashes, *icon_name;
  char *p;
  
  if (mime_type == NULL) {
    return NULL;
  }

  mime_type_without_slashes = g_strdup (mime_type);
  
  while ((p = strchr(mime_type_without_slashes, '/')) != NULL)
    *p = '-';
  
  icon_name = g_strconcat ("mime-", mime_type_without_slashes, NULL);
  g_free (mime_type_without_slashes);
  
  return icon_name;
}

char *
gnome_icon_lookup (GnomeIconLoader         *icon_loader,
		   const char              *file_uri,
		   const char              *custom_icon,
		   GnomeVFSFileInfo        *file_info,
		   const char              *mime_type,
		   GnomeIconLookupFlags     flags,
		   GnomeIconLookupOutFlags *out_flags)
{
  char *icon_name;
  char *mime_name;
  
  /* Look for availibility of custom icon */
  if (custom_icon)
    {
      /* WARNING: Does I/O for abs custom icons! */
      if ((custom_icon[0] == '/' && g_file_test (custom_icon, G_FILE_TEST_IS_REGULAR)) ||
	  gnome_icon_loader_has_icon (icon_loader, custom_icon))
	return g_strdup (custom_icon);
    }

  if (flags & GNOME_ICON_LOOKUP_FLAGS_USE_THUMBNAILS)
    {
      /* TODO: look for thumbnails, set out_flags for thumbnail creation */
      /* TODO: Also look for view as itself, even for svgs if supported */
    }

  icon_name = get_icon_name (file_uri, file_info, mime_type);

  if (icon_name && gnome_icon_loader_has_icon (icon_loader, icon_name))
    return icon_name;
  g_free (icon_name);

  if (mime_type)
    {
      mime_name = get_vfs_mime_name (mime_type);
      
      if (mime_name && gnome_icon_loader_has_icon (icon_loader, mime_name))
	return mime_name;
      g_free (mime_name);
      
      mime_name = make_mime_name (mime_type);
      
      if (mime_name && gnome_icon_loader_has_icon (icon_loader, mime_name))
	return mime_name;
      g_free (mime_name);
    }
      

  return g_strdup (ICON_NAME_REGULAR);
}

char *
gnome_icon_lookup_sync (GnomeIconLoader         *icon_loader,
			const char              *file_uri,
			const char              *custom_icon,
			GnomeIconLookupFlags     flags,
			GnomeIconLookupOutFlags *out_flags)
{
  const char *mime_type;
  char *res;
  GnomeVFSFileInfo *file_info;

  file_info = gnome_vfs_file_info_new ();
  gnome_vfs_get_file_info (file_uri,
			   file_info,
			   GNOME_VFS_FILE_INFO_FOLLOW_LINKS|
			   GNOME_VFS_FILE_INFO_GET_MIME_TYPE);

  mime_type = NULL;
  if (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
    mime_type = file_info->mime_type;


  res = gnome_icon_lookup (icon_loader,
			   file_uri,
			   custom_icon,
			   file_info,
			   mime_type,
			   flags,
			   out_flags);
  
  gnome_vfs_file_info_unref (file_info);

  return res;
}



