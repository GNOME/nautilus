/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-uri.c -- tools for creating
   and parsing search uris 

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Rebecca Schulman <rebecka@eazel.com>
*/

#include <libnautilus-extensions/nautilus-global-preferences.h>

char *                      nautilus_search_uri_to_simple_search_criteria      (const char *location);
char *                      nautilus_simple_search_criteria_to_search_uri      (const char *search_criteria);


NautilusSearchBarMode       nautilus_search_uri_to_search_bar_mode             (const char *uri);
gboolean                    nautilus_search_uri_is_displayable_by_mode         (const char *uri,
										NautilusSearchBarMode mode);
