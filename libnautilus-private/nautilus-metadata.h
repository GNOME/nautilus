/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_METADATA_H
#define NAUTILUS_METADATA_H

/* Keys for getting/setting Nautilus metadata. All metadata used in Nautilus
 * should define its key here, so we can keep track of the whole set easily.
 */

#define NAUTILUS_CUSTOM_ICON_METADATA_KEY               "CUSTOM_ICON"
#define NAUTILUS_NOTES_METADATA_KEY                     "NOTES"
#define NAUTILUS_ANNOTATION_METADATA_KEY                "ANNOTATION"
#define NAUTILUS_CONTENT_VIEWS_METADATA_KEY		"CONTENT_VIEWS"

#define ICON_VIEW_BACKGROUND_COLOR_METADATA_KEY		"ICON_VIEW_BACKGROUND_COLOR"
#define ICON_VIEW_ZOOM_LEVEL_METADATA_KEY		"ICON_VIEW_ZOOM_LEVEL"
#define ICON_VIEW_ICON_POSITION_METADATA_KEY		"ICON_POSITION"
#define ICON_VIEW_ICON_SCALE_METADATA_KEY               "ICON_SCALE"

#define LIST_VIEW_BACKGROUND_COLOR_METADATA_KEY		"LIST_VIEW_BACKGROUND_COLOR"
#define LIST_VIEW_ZOOM_LEVEL_METADATA_KEY		"LIST_VIEW_ZOOM_LEVEL"
#define LIST_VIEW_SORT_COLUMN_METADATA_KEY		"LIST_VIEW_SORT_COLUMN"
#define LIST_VIEW_SORT_REVERSED_METADATA_KEY		"LIST_VIEW_SORT_REVERSED"

#define INDEX_PANEL_BACKGROUND_COLOR_METADATA_KEY	"INDEX_PANEL_BACKGROUND_COLOR"

#endif /* NAUTILUS_METADATA_H */
