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

/* Per-file */

#define NAUTILUS_METADATA_KEY_CONTENT_VIEWS              	"content_views"

#define NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE	 	"default_action_type"
#define NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION	 	"default_application"
#define NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT		 	"default_component"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD 	"short_list_application_add"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE	"short_list_application_remove"
#define NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION	  	"application"
#define NAUTILUS_METADATA_SUBKEY_APPLICATION_ID		  	"id"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD   	"short_list_component_add"
#define NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE 	"short_list_component_remove"
#define NAUTILUS_METADATA_KEY_EXPLICIT_COMPONENT	  	"explicit_content_view"
#define NAUTILUS_METADATA_SUBKEY_COMPONENT_IID		  	"iid"

#define NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR 	"background_color"
#define NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE 	"background_tile_image"

#define NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL       	"icon_view_zoom_level"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT      	"icon_view_auto_layout"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT      	"icon_view_tighter_layout"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY          	"icon_view_sort_by"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"icon_view_sort_reversed"

#define NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL       	"list_view_zoom_level"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"list_view_sort_column"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"list_view_sort_reversed"

#define NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY			"window_geometry"

#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR   	"sidebar_background_color"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE   	"sidebar_background_tile_image"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BUTTONS			"sidebar_buttons"
#define NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR		 	"sidebar_tab_color"
#define NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR	 	"sidebar_title_tab_color"

#define NAUTILUS_METADATA_KEY_NOTES                      	"notes"
#define NAUTILUS_METADATA_KEY_ANNOTATION                 	"annotation"
#define NAUTILUS_METADATA_KEY_ICON_POSITION              	"icon_position"
#define NAUTILUS_METADATA_KEY_ICON_SCALE                 	"icon_scale"
#define NAUTILUS_METADATA_KEY_CUSTOM_ICON                	"custom_icon"

/* per link file */

#define NAUTILUS_METADATA_KEY_EXTRA_TEXT		 	"extra_text"

#endif /* NAUTILUS_METADATA_H */
