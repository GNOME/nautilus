/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-vfs-utils.h - Utility gnome-vfs methods. Will use gnome-vfs
                       HEAD in time.

   Copyright (C) 1999 Free Software Foundation
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

   Authors: Ettore Perazzoli <ettore@comm2000.it>
   	    John Sullivan <sullivan@eazel.com> 
*/

#ifndef EGG_RECENT_VFS_UTILS_H
#define EGG_RECENT_VFS_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

char     *egg_recent_vfs_format_uri_for_display (const char *uri);
char     *egg_recent_vfs_make_uri_from_input    (const char *uri);
gboolean  egg_recent_vfs_uris_match             (const char *uri_1,
						 const char *uri_2);
char     *egg_recent_vfs_get_uri_scheme         (const char *uri);

G_END_DECLS

#endif /* GNOME_VFS_UTILS_H */
