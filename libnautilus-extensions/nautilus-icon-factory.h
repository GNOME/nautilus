/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory.h: Class for obtaining icons for files and other objects.
 
   Copyright (C) 1999, 2000 Red Hat Inc.
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
#define NAUTILUS_ICON_FACTORY_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnautilus/nautilus-file.h>
#include <gtk/gtkobject.h>
#include <libart_lgpl/art_rect.h>

/* NautilusIconFactory is a class that knows how to hand out icons to be
 * used for representing files and some other objects. It was designed
 * specifically to be useful for the Nautilus file browser, but could be
 * used by any program that wants to display the standard icon for a
 * file.
 * 
 * The most common usage is to get a NautilusIconFactory object with
 * nautilus_get_current_icon_factory, then ask for an icon for a specific
 * file with nautilus_icon_factory_get_icon_for_file. The caller can ask
 * for any size icon, but normally will use one of the defined 
 * NAUTILUS_ICON_SIZE macros.
 */

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
#define NAUTILUS_ICON_SIZE_SMALLEST	12
#define NAUTILUS_ICON_SIZE_SMALLER	24
#define NAUTILUS_ICON_SIZE_SMALL	36
#define NAUTILUS_ICON_SIZE_STANDARD	48
#define NAUTILUS_ICON_SIZE_LARGE	72
#define NAUTILUS_ICON_SIZE_LARGER	96
#define NAUTILUS_ICON_SIZE_LARGEST     192

typedef struct NautilusScalableIcon NautilusScalableIcon;

/* Instead of a class declaration here, I will just document
 * the signals.
 *
 *	"theme_changed", no parameters
 */

/* There's a single NautilusIconFactory object.
 * The only thing you need it for is to connect to its signals.
 */
GtkObject *           nautilus_icon_factory_get                          (void);

/* Relationship between zoom levels and icons sizes. */
guint                 nautilus_get_icon_size_for_zoom_level              (NautilusZoomLevel      zoom_level);

/* Switch themes. */
char *                nautilus_icon_factory_get_theme                    (void);
void                  nautilus_icon_factory_set_theme                    (const char            *theme_name);

/* Choose the appropriate icon, but don't render it yet. */
NautilusScalableIcon *nautilus_icon_factory_get_icon_for_file            (NautilusFile          *file);
									  
NautilusScalableIcon *nautilus_icon_factory_get_icon_by_name             (const char            *icon_name);
GList *               nautilus_icon_factory_get_emblem_icons_for_file    (NautilusFile          *file);

/* Render an icon to a particular size.
 * Ownership of a ref. count in this pixbuf comes with the deal.
 * This allows scaling in both dimensions. All other calls assume
 * that X and Y scaling are the same. The text rectangle tells where
 * this icon can accomodate text. If the icon can't accomodate any
 * text, then the rectangle is (0, 0, 0, 0).
 */
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_for_icon          (NautilusScalableIcon  *scalable_icon,
									  guint                  size_in_pixels_x,
									  guint                  size_in_pixels_y,
									  ArtIRect              *embedded_text_rectangle);

/* Convenience functions for the common case where you want to choose
 * and render the icon into a pixbuf all at once.
 */
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_for_file          (NautilusFile          *file,
									  guint                  size_in_pixels);
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_by_name           (const char            *icon_name,
									  guint                  size_in_pixels);

/* Convenience functions for legacy interfaces that require a pixmap and
 * bitmap. Maybe we can get rid of these one day.
 */
void                  nautilus_icon_factory_get_pixmap_and_mask_for_file (NautilusFile          *file,
									  guint                  size_in_pixels,
									  GdkPixmap            **pixmap,
									  GdkBitmap            **mask);
void                  nautilus_icon_factory_get_pixmap_and_mask_by_name  (NautilusFile          *file,
									  guint                  size_in_pixels,
									  GdkPixmap            **pixmap,
									  GdkBitmap            **mask);
									  
/* Manage a scalable icon.
 * Since the factory always passes out references to the same scalable
 * icon, you can compare two scalable icons to see if they are the same
 * with ==.
 */
void                  nautilus_scalable_icon_ref                         (NautilusScalableIcon  *scalable_icon);
void                  nautilus_scalable_icon_unref                       (NautilusScalableIcon  *scalable_icon);

/* The name of a scalable icon is suitable for storage in metadata.
 * This is a quick way to record the result of getting an icon by name.
 */
char *                nautilus_scalable_icon_get_name                    (NautilusScalableIcon  *scalable_icon);

/* Convenience function for freeing a list of scalable icons.
 * Unrefs all the icons before freeing the list.
 */
void                  nautilus_scalable_icon_list_free                   (GList                 *scalable_icon_list);

#endif /* NAUTILUS_ICON_FACTORY_H */
