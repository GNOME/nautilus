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

#include <eel/eel-string-list.h>
#include <gdk/gdktypes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkobject.h>
#include <libnautilus-private/nautilus-file.h>
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnomeui/gnome-thumbnail.h>

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

#define NAUTILUS_ICON_SIZE_THUMBNAIL   96

/* Maximum size of an icon that the icon factory will ever produce */
#define NAUTILUS_ICON_MAXIMUM_SIZE     320

/* Icon size to use for menus. NAUTILUS_ICON_SIZE_SMALLEST
 * is a little too small and NAUTILUS_ICON_SIZE_SMALLER is
 * a little too big.
 */
#define NAUTILUS_ICON_SIZE_FOR_MENUS	20

/* here's a structure to hold the emblem attach points */

#define MAX_ATTACH_POINTS 12

typedef struct {
	int num_points;
	GdkPoint points[MAX_ATTACH_POINTS];
} NautilusEmblemAttachPoints;

/* Instead of a class declaration here, I will just document
 * the signals.
 *
 *	"icons_changed", no parameters
 */

/* There's a single NautilusIconFactory object.
 * The only thing you need it for is to connect to its signals.
 */
GObject *             nautilus_icon_factory_get                          (void);

/* Relationship between zoom levels and icons sizes. */
guint                 nautilus_get_icon_size_for_zoom_level              (NautilusZoomLevel            zoom_level);

/* Choose the appropriate icon, but don't render it yet. */
char *                nautilus_icon_factory_get_icon_for_file            (NautilusFile                *file);
gboolean              nautilus_icon_factory_is_icon_ready_for_file       (NautilusFile                *file);
GList *               nautilus_icon_factory_get_required_file_attributes (void);

/* The calls below do not account for top-left text, allowing it to be loaded progressively. */
gboolean              nautilus_icon_factory_is_basic_icon_ready_for_file (NautilusFile                *file);
GList *               nautilus_icon_factory_get_basic_file_attributes    (void);

GList *               nautilus_icon_factory_get_emblem_icons_for_file    (NautilusFile                *file,
									  EelStringList               *exclude);
char *                nautilus_icon_factory_get_emblem_icon_by_name      (const char                  *emblem_name);

/* Render an icon to a particular size.
 * Ownership of a ref. count in this pixbuf comes with the deal.
 * This allows scaling in both dimensions. All other calls assume
 * that X and Y scaling are the same. Optionally, we also pass
 * back an array of emblem attach points, if the pointer is non-null
 * If the wants_default boolean is set, return a default icon instead
 * of NULL if we can't find anything
 */
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_for_icon          (const char                  *icon,
									  const char                  *modifier,
									  const char                  *embedded_text,
									  guint                        nominal_size,
									  NautilusEmblemAttachPoints  *attach_points,
									  gboolean                     wants_default);
									  
/* Convenience functions for the common case where you want to choose
 * and render the icon into a pixbuf all at once.
 */
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_for_file          (NautilusFile                *file,
									  const char                  *modifer,
									  guint                        size_in_pixels);

/* Convenience routine for getting a pixbuf from an icon name
 */
GdkPixbuf *           nautilus_icon_factory_get_pixbuf_from_name         (const char                  *icon_name,
									  const char                  *modifer,
									  guint                        size_in_pixels);

GnomeIconTheme *       nautilus_icon_factory_get_icon_theme               (void);
GnomeThumbnailFactory *nautilus_icon_factory_get_thumbnail_factory        (void);

#endif /* NAUTILUS_ICON_FACTORY_H */

