/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Code to generate human-readable strings from search uris.

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

   Author: Mathieu Lacage <mathieu@eazel.com>
*/

#ifndef NAUTILUS_SEARCH_URI_H
#define NAUTILUS_SEARCH_URI_H

#include <glib.h>

/* These strings are used programatically; they must not be translated */
#define NAUTILUS_SEARCH_URI_TEXT_NAME		"file_name"
#define NAUTILUS_SEARCH_URI_TEXT_CONTENT	"content"
#define NAUTILUS_SEARCH_URI_TEXT_TYPE		"file_type"
#define NAUTILUS_SEARCH_URI_TEXT_SIZE		"size"
#define NAUTILUS_SEARCH_URI_TEXT_EMBLEMS	"keywords"
#define NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED	"modified"
#define NAUTILUS_SEARCH_URI_TEXT_OWNER		"owner"

gboolean nautilus_is_search_uri       			 (const char *uri);
char *   nautilus_search_uri_to_human 			 (const char *search_uri);
char *   nautilus_get_target_uri_from_search_result_name (const char *search_result_name);

#endif /* NAUTILUS_SEARCH_URI_H */
