/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory.h: Class for obtaining icons for files and other objects.
 
 * Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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

#ifndef NAUTILUS_ICON_FACTORY_H
#define NAUTILUS_ICON_FACTORY_H 1

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnautilus/nautilus-directory.h>

/* Names for Nautilus's different zoom levels, from tiniest items to largest items */
typedef enum {
	NAUTILUS_ZOOM_LEVEL_SMALLEST,
	NAUTILUS_ZOOM_LEVEL_SMALLER,
	NAUTILUS_ZOOM_LEVEL_SMALL,
	NAUTILUS_ZOOM_LEVEL_STANDARD,
	NAUTILUS_ZOOM_LEVEL_LARGE,
	NAUTILUS_ZOOM_LEVEL_LARGER,
	NAUTILUS_ZOOM_LEVEL_LARGEST
} NautilusZoomLevel;

/* Nominal icon sizes for each Nautilus zoom level.
 * This scheme assumes that icons are designed to
 * fit in a square space, though each image needn't
 * be square. Since individual icons can be stretched,
 * each icon is not constrained to this nominal size.
 */
#define NAUTILUS_ICON_SIZE_SMALLEST	16
#define NAUTILUS_ICON_SIZE_SMALLER	24
#define NAUTILUS_ICON_SIZE_SMALL	32
#define NAUTILUS_ICON_SIZE_STANDARD	48
#define NAUTILUS_ICON_SIZE_LARGE	64
#define NAUTILUS_ICON_SIZE_LARGER	96
#define NAUTILUS_ICON_SIZE_LARGEST     144

typedef struct _NautilusIconFactory NautilusIconFactory;

NautilusIconFactory *nautilus_icon_factory_new               (const char *theme_name);
void         	     nautilus_icon_factory_destroy           (NautilusIconFactory  *factory);
void         	     nautilus_icon_factory_set_theme         (NautilusIconFactory  *factory,
                                                		const char *theme_name);
NautilusIconFactory *nautilus_get_current_icon_factory       (void);

guint	     	     nautilus_icon_size_for_zoom_level 	     (NautilusZoomLevel zoom_level);

/* Ownership of a ref. count in this pixbuf comes with the deal */
GdkPixbuf *  	     nautilus_icon_factory_get_icon_for_file (NautilusIconFactory *factory,
                                                	      NautilusFile *file,
                                                	      guint size_in_pixels);

#endif /* NAUTILUS_ICON_FACTORY_H */

