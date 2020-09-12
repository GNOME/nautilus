/*
   nautilus-directory-notify.h: Nautilus directory notify calls.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#pragma once

#include <gio/gio.h>

#include "nautilus-types.h"

typedef struct {
	GFile *from;
	GFile *to;
} GFilePair;

/* Almost-public change notification calls */
void nautilus_directory_notify_files_added   (GList *files);
void nautilus_directory_notify_files_moved   (GList *file_pairs);
void nautilus_directory_notify_files_changed (GList *files);
void nautilus_directory_notify_files_removed (GList *files);

/* Change notification hack.
 * This is called when code modifies the file and it needs to trigger
 * a notification. Eventually this should become private, but for now
 * it needs to be used for code like the thumbnail generation.
 */
void nautilus_file_changed                       (NautilusFile *file);
