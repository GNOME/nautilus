/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* eel-desktop-file-loader.h

   Copyright (C) 2001 Red Hat, Inc.

   Developers: Havoc Pennington <hp@redhat.com>
               Alexander Larsson <alexl@redhat.com>
	       
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef NAUTILUS_DESKTOP_FILE_LOADER_H
#define NAUTILUS_DESKTOP_FILE_LOADER_H

#include <glib.h>
#include <libgnomevfs/gnome-vfs-result.h>

typedef struct NautilusDesktopFile NautilusDesktopFile;

/* This is a quick-hack to read and modify .desktop files.
 * It has severe limitations, but does what nautilus
 * needs right now. You cannot create new sections or add non-existing keys.
 *
 * The right way to solve this is to write a good desktop file parser
 * and put it in another library for use by the panel, nautilus etc.
 */

GnomeVFSResult       nautilus_desktop_file_load            (const char                      *uri,
							    NautilusDesktopFile            **desktop_file);
NautilusDesktopFile *nautilus_desktop_file_from_string     (const char                      *data);
GnomeVFSResult       nautilus_desktop_file_save            (NautilusDesktopFile             *df,
							    const char                      *uri);
void                 nautilus_desktop_file_free            (NautilusDesktopFile             *df);

/* This is crap, it just ignores the %f etc. in the exec string,
 *  and has no error handling.
 */
void            nautilus_desktop_file_launch          (NautilusDesktopFile            *df);


gboolean nautilus_desktop_file_get_boolean       (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  gboolean             *val);
gboolean nautilus_desktop_file_get_number        (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  double               *val);
gboolean nautilus_desktop_file_get_string        (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  char                **val);
gboolean nautilus_desktop_file_get_locale_string (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  char                **val);
gboolean nautilus_desktop_file_get_regexp        (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  char                **val);
gboolean nautilus_desktop_file_set_string        (NautilusDesktopFile  *df,
						  const char           *section,
						  const char           *keyname,
						  const char           *value);


/* Some getters and setters are missing, they should be added as needed */


#endif /* NAUTILUS_DESKTOP_FILE_LOADER_H */
