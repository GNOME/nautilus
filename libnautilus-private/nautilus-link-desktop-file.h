/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-desktop-file.h: .

   Copyright (C) 2001 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Jonathan Blandford <jrb@redhat.com>
*/

#ifndef NAUTILUS_LINK_DESKTOP_FILE_H
#define NAUTILUS_LINK_DESKTOP_FILE_H

#include "nautilus-file.h"
#include <gdk/gdk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-dentry.h>


gboolean         nautilus_link_desktop_file_local_create                     (const char        *directory_path,
									      const char        *name,
									      const char        *image,
									      const char        *target_uri,
									      const GdkPoint    *point,
									      NautilusLinkType   type);
gboolean         nautilus_link_desktop_file_local_set_icon                   (const char        *path,
									      const char        *icon_name);
char *           nautilus_link_desktop_file_local_get_text                   (const char        *path);
char *           nautilus_link_desktop_file_local_get_additional_text        (const char        *path);
NautilusLinkType nautilus_link_desktop_file_local_get_link_type              (const char        *path);
gboolean         nautilus_link_desktop_file_local_is_volume_link             (const char        *path);
gboolean         nautilus_link_desktop_file_local_is_home_link               (const char        *path);
gboolean         nautilus_link_desktop_file_local_is_trash_link              (const char        *path);
char *           nautilus_link_desktop_file_local_get_link_uri               (const char        *path);
char *           nautilus_link_desktop_file_get_link_uri_given_file_contents (const char        *link_file_contents,
									      int                link_file_size);
char *           nautilus_link_desktop_file_get_link_name_given_file_contents (const char        *link_file_contents,
									       int                link_file_size);
char *           nautilus_link_desktop_file_get_link_icon_given_file_contents (const char        *link_file_contents,
									       int                link_file_size);
void             nautilus_link_desktop_file_local_create_from_gnome_entry    (GnomeDesktopEntry *entry,
									      const char        *dest_path,
									      const GdkPoint    *position);


#endif /* NAUTILUS_LINK_DESKTOP_FILE_H */

