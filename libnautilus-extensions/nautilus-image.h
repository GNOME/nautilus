/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.h - A widget to display a composited pixbuf.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_IMAGE_H
#define NAUTILUS_IMAGE_H

#include <gtk/gtkmisc.h>
#include <libgnome/gnome-defs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* NautilusImage is GtkWidget that can display a GdkPixbuf.  This pixbuf
 * will be composited with full alpha support on wither a solid background
 * or a background pixbuf.
 */

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_IMAGE            (nautilus_image_get_type ())
#define NAUTILUS_IMAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_IMAGE, NautilusImage))
#define NAUTILUS_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_IMAGE, NautilusImageClass))
#define NAUTILUS_IS_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_IMAGE))
#define NAUTILUS_IS_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_IMAGE))

typedef struct _NautilusImage		  NautilusImage;
typedef struct _NautilusImageClass	  NautilusImageClass;
typedef struct _NautilusImageDetail	  NautilusImageDetail;

struct _NautilusImage
{
	/* Superclass */
	GtkMisc			misc;

	/* Private things */
	NautilusImageDetail	*detail;
};

struct _NautilusImageClass
{
	GtkMiscClass		parent_class;
};

typedef enum
{
	NAUTILUS_IMAGE_PLACEMENT_TILE,
	NAUTILUS_IMAGE_PLACEMENT_CENTER
} NautilusImagePlacementType;

typedef enum
{
	NAUTILUS_IMAGE_BACKGROUND_PIXBUF,
	NAUTILUS_IMAGE_BACKGROUND_SOLID,
} NautilusImageBackgroundType;

/* Pack RGBA components */
#define NAUTILUS_RGBA_COLOR_PACK(_r, _g, _b, _a)	\
( ((_a) << 24) |					\
  ((_r) << 16) |					\
  ((_g) <<  8) |					\
  ((_b) <<  0) )

/* Access RGBA components */
#define NAUTILUS_RGBA_COLOR_GET_R(_color) (((_color) >> 16) & 0xff)
#define NAUTILUS_RGBA_COLOR_GET_G(_color) (((_color) >> 8) & 0xff)
#define NAUTILUS_RGBA_COLOR_GET_B(_color) (((_color) >> 0) & 0xff)
#define NAUTILUS_RGBA_COLOR_GET_A(_color) (((_color) >> 24) & 0xff)

GtkType                     nautilus_image_get_type              (void);
GtkWidget *                 nautilus_image_new                   (void);
void                        nautilus_image_set_background_pixbuf (NautilusImage               *image,
								  GdkPixbuf                   *background);
GdkPixbuf*                  nautilus_image_get_background_pixbuf (const NautilusImage         *image);
void                        nautilus_image_set_background_type   (NautilusImage               *image,
								  NautilusImageBackgroundType  background_type);
NautilusImageBackgroundType nautilus_image_get_background_type   (const NautilusImage         *image);
void                        nautilus_image_set_placement_type    (NautilusImage               *image,
								  NautilusImagePlacementType   placement);
NautilusImagePlacementType  nautilus_image_get_placement_type    (const NautilusImage         *image);
void                        nautilus_image_set_background_color  (NautilusImage               *image,
								  guint32                      color);
guint32                     nautilus_image_get_background_color  (const NautilusImage         *image);
void                        nautilus_image_set_pixbuf            (NautilusImage               *image,
								  GdkPixbuf                   *pixbuf);
GdkPixbuf*                  nautilus_image_get_pixbuf            (const NautilusImage         *image);
void                        nautilus_image_set_overall_alpha     (NautilusImage               *image,
								  guchar                       pixbuf_alpha);
void                        nautilus_image_set_label_text        (NautilusImage               *image,
								  const gchar                 *text);
gchar*                      nautilus_image_get_label_text        (NautilusImage               *image);
void                        nautilus_image_set_label_font        (NautilusImage               *image,
								  GdkFont                     *font);
GdkFont*                    nautilus_image_get_label_font        (NautilusImage               *image);
void                        nautilus_image_set_left_offset       (NautilusImage               *image,
								  guint                        left_offset);
void                        nautilus_image_set_right_offset      (NautilusImage               *image,
								  guint                        right_offset);
void                        nautilus_image_set_top_offset        (NautilusImage               *image,
								  guint                        top_offset);
void                        nautilus_image_set_bottom_offset     (NautilusImage               *image,
								  guint                        bottom_offset);
void                        nautilus_image_set_extra_width       (NautilusImage               *image,
								  guint                        extra_width);
void                        nautilus_image_set_extra_height      (NautilusImage               *image,
								  guint                        extra_width);


END_GNOME_DECLS

#endif /* NAUTILUS_IMAGE_H */


