/*
   nautilus-search-directory.h: Subclass of NautilusDirectory to implement
   a virtual directory consisting of the search directory and the search
   icons
 
   Copyright (C) 2005 Novell, Inc
  
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
*/

#pragma once

#include "nautilus-directory.h"

#include "nautilus-types.h"

#define NAUTILUS_SEARCH_DIRECTORY_PROVIDER_NAME "search-directory-provider"
#define NAUTILUS_TYPE_SEARCH_DIRECTORY (nautilus_search_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchDirectory, nautilus_search_directory,
                      NAUTILUS, SEARCH_DIRECTORY, NautilusDirectory)

char   *nautilus_search_directory_generate_new_uri     (void);

NautilusQuery *nautilus_search_directory_get_query       (NautilusSearchDirectory *self);
void           nautilus_search_directory_set_query       (NautilusSearchDirectory *self,
							  NautilusQuery           *query);
