/*
 * gnome-thumbnail.h: Utilities for handling thumbnails
 *
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef GNOME_DESKTOP_THUMBNAIL_H
#define GNOME_DESKTOP_THUMBNAIL_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeDesktopThumbnail is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnome-desktop-thumbnail.h
#endif

#include <glib.h>
#include <glib-object.h>
#include <time.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef enum {
  GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL,
  GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE
} GnomeDesktopThumbnailSize;

#define GNOME_DESKTOP_TYPE_THUMBNAIL_FACTORY		(gnome_desktop_thumbnail_factory_get_type ())

G_DECLARE_DERIVABLE_TYPE (GnomeDesktopThumbnailFactory,
                          gnome_desktop_thumbnail_factory,
                          GNOME_DESKTOP,
                          THUMBNAIL_FACTORY,
                          GObject)

struct _GnomeDesktopThumbnailFactoryClass {
	GObjectClass parent;
};

GType                  gnome_desktop_thumbnail_factory_get_type (void);
GnomeDesktopThumbnailFactory *gnome_desktop_thumbnail_factory_new      (GnomeDesktopThumbnailSize     size);

char *                 gnome_desktop_thumbnail_factory_lookup   (GnomeDesktopThumbnailFactory *factory,
								 const char            *uri,
								 time_t                 mtime);

gboolean               gnome_desktop_thumbnail_factory_has_valid_failed_thumbnail (GnomeDesktopThumbnailFactory *factory,
										   const char            *uri,
										   time_t                 mtime);
gboolean               gnome_desktop_thumbnail_factory_can_thumbnail (GnomeDesktopThumbnailFactory *factory,
								      const char            *uri,
								      const char            *mime_type,
								      time_t                 mtime);
GdkPixbuf *            gnome_desktop_thumbnail_factory_generate_thumbnail (GnomeDesktopThumbnailFactory *factory,
									   const char            *uri,
									   const char            *mime_type);
void                   gnome_desktop_thumbnail_factory_save_thumbnail (GnomeDesktopThumbnailFactory *factory,
								       GdkPixbuf             *thumbnail,
								       const char            *uri,
								       time_t                 original_mtime);
void                   gnome_desktop_thumbnail_factory_create_failed_thumbnail (GnomeDesktopThumbnailFactory *factory,
										const char            *uri,
										time_t                 mtime);


/* Thumbnailing utils: */
gboolean   gnome_desktop_thumbnail_is_valid          (GdkPixbuf          *pixbuf,
						      const char         *uri,
						      time_t              mtime);
char *     gnome_desktop_thumbnail_path_for_uri      (const char         *uri,
						      GnomeDesktopThumbnailSize  size);

G_END_DECLS

#endif /* GNOME_DESKTOP_THUMBNAIL_H */
