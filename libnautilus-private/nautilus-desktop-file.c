/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus desktop file handling
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Author: Havoc Pennington <hp@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "nautilus-desktop-file.h"
#include "nautilus-program-choosing.h"
#include <libgnomevfs/gnome-vfs.h>
#include <libdesktopfile/desktop-file-loader.h>
#include <libgnome/gnome-util.h>

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

static DesktopFile*
desktop_file_from_uri (const char *uri)
{
        char *contents;
        DesktopFile *df;
        
        contents = slurp_uri_contents (uri);

        if (contents == NULL)
                return NULL;

        df = desktop_file_from_string (contents);

        g_free (contents);

        return df;        
}

void
nautilus_desktop_file_launch (const char *uri)
{
        DesktopFile *df;

        df = desktop_file_from_uri (uri);

        if (df == NULL)
                return;
        
        desktop_file_launch (df);

        desktop_file_free (df);
}

/* Note: these two imply that we load and parse the desktop file twice,
 * once in the icon code and once in the naming code.
 */

char*
nautilus_desktop_file_get_icon (const char *uri)
{
        DesktopFile *df;
        char *icon_name;
        char *absolute;
        char *icon_uri;
	
        df = desktop_file_from_uri (uri);

        if (df == NULL)
                return NULL;
        
        if (!desktop_file_get_string (df, NULL, "Icon", &icon_name))
                icon_name = NULL;
        
        desktop_file_free (df);

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

char*
nautilus_desktop_file_get_name (const char *uri)
{
        DesktopFile *df;
        char *name;
        
        df = desktop_file_from_uri (uri);

        if (df == NULL)
                return NULL;
        
        if (!desktop_file_get_string (df, NULL, "Name", &name))
               name = NULL;
        
        desktop_file_free (df);
        
        return name;
}
