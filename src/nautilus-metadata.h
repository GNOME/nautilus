/*
   nautilus-metadata.h: #defines and other metadata-related info
 
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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#pragma once

/* Keys for getting/setting Nautilus metadata. All metadata used in Nautilus
 * should define its key here, so we can keep track of the whole set easily.
 * Any updates here needs to be added in nautilus-metadata.c too.
 */

#include <glib.h>

/* Per-file */

#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY          	"nautilus-icon-view-sort-by"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"nautilus-icon-view-sort-reversed"

#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"nautilus-list-view-sort-column"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"nautilus-list-view-sort-reversed"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS    	"nautilus-list-view-visible-columns"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER    	"nautilus-list-view-column-order"

#define NAUTILUS_METADATA_KEY_CUSTOM_ICON                	"custom-icon"
#define NAUTILUS_METADATA_KEY_CUSTOM_ICON_NAME                	"custom-icon-name"
#define NAUTILUS_METADATA_KEY_EMBLEMS				"emblems"

guint nautilus_metadata_get_id (const char *metadata);
