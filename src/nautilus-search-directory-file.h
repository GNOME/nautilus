/*
   nautilus-search-directory-file.h: Subclass of NautilusFile to implement the
   the case of the search directory
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#pragma once

#include "nautilus-file.h"

#define NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE nautilus_search_directory_file_get_type ()
G_DECLARE_FINAL_TYPE (NautilusSearchDirectoryFile, nautilus_search_directory_file,
                      NAUTILUS, SEARCH_DIRECTORY_FILE,
                      NautilusFile)

void nautilus_search_directory_file_update_display_name (NautilusSearchDirectoryFile *search_file);
