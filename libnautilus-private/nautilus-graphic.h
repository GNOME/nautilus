/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-graphic.h - A widget to display a composited pixbuf.

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

#ifndef NAUTILUS_GRAPHIC_H
#define NAUTILUS_GRAPHIC_H

#include <gtk/gtkwidget.h>
#include <libgnome/gnome-defs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* NautilusGraphic is GtkWidget that can display a GdkPixbuf.  This pixbuf
 * will be composited with full alpha support on wither a solid background
 * or a background pixbuf.
 */

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_GRAPHIC            (nautilus_graphic_get_type ())
#define NAUTILUS_GRAPHIC(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_GRAPHIC, NautilusGraphic))
#define NAUTILUS_GRAPHIC_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GRAPHIC, NautilusGraphicClass))
#define NAUTILUS_IS_GRAPHIC(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_GRAPHIC))
#define NAUTILUS_IS_GRAPHIC_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GRAPHIC))

typedef struct _NautilusGraphic		  NautilusGraphic;
typedef struct _NautilusGraphicClass	  NautilusGraphicClass;
typedef struct _NautilusGraphicDetail	  NautilusGraphicDetail;

struct _NautilusGraphic
{
	/* Superclass */
	GtkWidget		widget;

	/* Private things */
	NautilusGraphicDetail	*detail;
};

struct _NautilusGraphicClass
{
	GtkWidgetClass parent_class;
};

typedef enum
{
	NAUTILUS_GRAPHIC_PLACEMENT_TILE,
	NAUTILUS_GRAPHIC_PLACEMENT_CENTER
} NautilusGraphicPlacementType;

typedef enum
{
	NAUTILUS_GRAPHIC_BACKGROUND_PIXBUF,
	NAUTILUS_GRAPHIC_BACKGROUND_SOLID,
} NautilusGraphicBackgroundType;

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

GtkType                       nautilus_graphic_get_type              (void);
GtkWidget *                   nautilus_graphic_new                   (void);
void                          nautilus_graphic_set_background_pixbuf (NautilusGraphic               *graphic,
								      GdkPixbuf                     *background);
GdkPixbuf*                    nautilus_graphic_get_background_pixbuf (const NautilusGraphic         *graphic);
void                          nautilus_graphic_set_background_type   (NautilusGraphic               *graphic,
								      NautilusGraphicBackgroundType  background_type);
NautilusGraphicBackgroundType nautilus_graphic_get_background_type   (const NautilusGraphic         *graphic);
void                          nautilus_graphic_set_placement_type    (NautilusGraphic               *graphic,
								      NautilusGraphicPlacementType   placement);
NautilusGraphicPlacementType  nautilus_graphic_get_placement_type    (const NautilusGraphic         *graphic);
void                          nautilus_graphic_set_background_color  (NautilusGraphic               *graphic,
								      guint32                        color);
guint32                       nautilus_graphic_get_background_color  (const NautilusGraphic         *graphic);
void                          nautilus_graphic_set_pixbuf            (NautilusGraphic               *graphic,
								      GdkPixbuf                     *pixbuf);
GdkPixbuf*                    nautilus_graphic_get_pixbuf            (const NautilusGraphic         *graphic);
void                          nautilus_graphic_set_overall_alpha     (NautilusGraphic               *graphic,
								      guchar                         pixbuf_alpha);
void                          nautilus_graphic_set_label_text        (NautilusGraphic               *graphic,
								      const gchar                   *text);
gchar*                        nautilus_graphic_get_label_text        (NautilusGraphic               *graphic);
void                          nautilus_graphic_set_label_font        (NautilusGraphic               *graphic,
								      GdkFont                       *font);
GdkFont*                      nautilus_graphic_get_label_font        (NautilusGraphic               *graphic);
void                          nautilus_graphic_set_left_offset       (NautilusGraphic               *graphic,
								      guint                          left_offset);
void                          nautilus_graphic_set_right_offset      (NautilusGraphic               *graphic,
								      guint                          right_offset);
void                          nautilus_graphic_set_top_offset        (NautilusGraphic               *graphic,
								      guint                          top_offset);
void                          nautilus_graphic_set_bottom_offset     (NautilusGraphic               *graphic,
								      guint                          bottom_offset);
void                          nautilus_graphic_set_extra_width       (NautilusGraphic               *graphic,
								      guint                          extra_width);
void                          nautilus_graphic_set_extra_height      (NautilusGraphic               *graphic,
								      guint                          extra_width);
END_GNOME_DECLS

#endif /* NAUTILUS_GRAPHIC_H */


