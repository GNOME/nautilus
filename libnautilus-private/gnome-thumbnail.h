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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef GNOME_THUMBNAIL_H
#define GNOME_THUMBNAIL_H

#include <glib.h>
#include <glib-object.h>
#include <time.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef enum {
  GNOME_THUMBNAIL_SIZE_NORMAL,
  GNOME_THUMBNAIL_SIZE_LARGE,
} GnomeThumbnailSize;

#define GNOME_TYPE_THUMBNAIL_FACTORY	(gnome_thumbnail_factory_get_type ())
#define GNOME_THUMBNAIL_FACTORY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_THUMBNAIL_FACTORY, GnomeThumbnailFactory))
#define GNOME_THUMBNAIL_FACTORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_THUMBNAIL_FACTORY, GnomeThumbnailFactoryClass))
#define GNOME_IS_THUMBNAIL_FACTORY(obj)		(G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_TYPE_THUMBNAIL_FACTORY))
#define GNOME_IS_THUMBNAIL_FACTORY_CLASS(klass)	(G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_THUMBNAIL_FACTORY))

typedef struct _GnomeThumbnailFactory        GnomeThumbnailFactory;
typedef struct _GnomeThumbnailFactoryClass   GnomeThumbnailFactoryClass;
typedef struct _GnomeThumbnailFactoryPrivate GnomeThumbnailFactoryPrivate;

struct _GnomeThumbnailFactory {
	GObject parent;
	
	GnomeThumbnailFactoryPrivate *priv;
};

struct _GnomeThumbnailFactoryClass {
	GObjectClass parent;
};

GType                  gnome_thumbnail_factory_get_type (void);
GnomeThumbnailFactory *gnome_thumbnail_factory_new      (GnomeThumbnailSize     size);

char *                 gnome_thumbnail_factory_lookup   (GnomeThumbnailFactory *factory,
							 const char            *uri,
							 time_t                 mtime);

gboolean               gnome_thumbnail_factory_has_valid_failed_thumbnail (GnomeThumbnailFactory *factory,
									   const char            *uri,
									   time_t                 mtime);
gboolean               gnome_thumbnail_factory_can_thumbnail (GnomeThumbnailFactory *factory,
							      const char            *uri,
							      const char            *mime_type,
							      time_t                 mtime);
GdkPixbuf *            gnome_thumbnail_factory_generate_thumbnail (GnomeThumbnailFactory *factory,
								   const char            *uri,
								   const char            *mime_type);
void                   gnome_thumbnail_factory_save_thumbnail (GnomeThumbnailFactory *factory,
							       GdkPixbuf             *thumbnail,
							       const char            *uri,
							       time_t                 original_mtime);
void                   gnome_thumbnail_factory_create_failed_thumbnail (GnomeThumbnailFactory *factory,
									const char            *uri,
									time_t                 mtime);


/* Thumbnailing utils: */
gboolean   gnome_thumbnail_has_uri           (GdkPixbuf          *pixbuf,
					      const char         *uri);
gboolean   gnome_thumbnail_is_valid          (GdkPixbuf          *pixbuf,
					      const char         *uri,
					      time_t              mtime);
char *     gnome_thumbnail_md5               (const char         *uri);
char *     gnome_thumbnail_path_for_uri      (const char         *uri,
					      GnomeThumbnailSize  size);


/* Pixbuf utils */

GdkPixbuf *gnome_thumbnail_scale_down_pixbuf (GdkPixbuf          *pixbuf,
					      int                 dest_width,
					      int                 dest_height);

GdkPixbuf *gnome_thumbnail_load_pixbuf       (const char         *uri);

G_END_DECLS

#endif /* GNOME_THUMBNAIL_H */
