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

/* Per-directory */

#define NAUTILUS_METADATA_KEY_CONTENT_VIEWS              	"CONTENT_VIEWS"

#define NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE	 	"DEFAULT_ACTION_TYPE"
#define NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION	 	"DEFAULT_APPLICATION"
#define NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT		 	"DEFAULT_COMPONENT"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD 	"SHORT_LIST_APPLICATION_ADD"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE	"SHORT_LIST_APPLICATION_REMOVE"
#define NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION	  	"APPLICATION"
#define NAUTILUS_METADATA_SUBKEY_APPLICATION_ID		  	"ID"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD   	"SHORT_LIST_COMPONENT_ADD"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE 	"SHORT_LIST_COMPONENT_REMOVE"
#define NAUTILUS_METADATA_KEY_EXPLICIT_COMPONENT	  	"EXPLICIT_CONTENT_VIEW"
#define NAUTILUS_METADATA_SUBKEY_COMPONENT_IID		  	"IID"

#define NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR 	"BACKGROUND_COLOR"
#define NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE 	"BACKGROUND_TILE_IMAGE"

#define NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL       	"ICONS_ZOOM_LEVEL"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT      	"ICONS_AUTO_LAYOUT"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY          	"ICONS_SORT_BY"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"ICONS_SORT_REVERSED"

#define NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL       	"LIST_ZOOM_LEVEL"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"LIST_SORT_COLUMN"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"LIST_SORT_REVERSED"

#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR   	"SIDEBAR_BACKGROUND_COLOR"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE   	"SIDEBAR_BACKGROUND_TILE_IMAGE"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BUTTONS			"SIDEBAR_BUTTONS"
#define NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR		 	"SIDEBAR_TAB_COLOR"
#define NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR	 	"SIDEBAR_TITLE_TAB_COLOR"

/* Per-file */

#define NAUTILUS_METADATA_KEY_NOTES                      	"NOTES"
#define NAUTILUS_METADATA_KEY_ANNOTATION                 	"ANNOTATION"
#define NAUTILUS_METADATA_KEY_ICON_POSITION              	"ICON_POSITION"
#define NAUTILUS_METADATA_KEY_ICON_SCALE                 	"ICON_SCALE"
#define NAUTILUS_METADATA_KEY_CUSTOM_ICON                	"CUSTOM_ICON"

/* per link file */

#define NAUTILUS_METADATA_KEY_EXTRA_TEXT		 	"EXTRA_TEXT"

#endif /* NAUTILUS_METADATA_H */
