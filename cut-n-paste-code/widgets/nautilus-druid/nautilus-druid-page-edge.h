/* nautilus-druid-page-edge.h
 * Copyright (C) 1999  Red Hat, Inc.
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
*/
#ifndef __NAUTILUS_DRUID_PAGE_EDGE_H__
#define __NAUTILUS_DRUID_PAGE_EDGE_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID_PAGE_EDGE            (nautilus_druid_page_edge_get_type ())
#define NAUTILUS_DRUID_PAGE_EDGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID_PAGE_EDGE, NautilusDruidPageEdge))
#define NAUTILUS_DRUID_PAGE_EDGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID_PAGE_EDGE, NautilusDruidPageEdgeClass))
#define NAUTILUS_IS_DRUID_PAGE_EDGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID_PAGE_EDGE))
#define NAUTILUS_IS_DRUID_PAGE_EDGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID_PAGE_EDGE))

typedef enum {
  /* update structure when adding enums */
	GNOME_EDGE_START,
	GNOME_EDGE_FINISH,
	GNOME_EDGE_OTHER
} GnomeEdgePosition;


typedef struct _NautilusDruidPageEdge        NautilusDruidPageEdge;
typedef struct _NautilusDruidPageEdgePrivate NautilusDruidPageEdgePrivate;
typedef struct _NautilusDruidPageEdgeClass   NautilusDruidPageEdgeClass;

struct _NautilusDruidPageEdge
{
	NautilusDruidPage parent;
	gchar *title;
	gchar *text;
	GdkPixbuf *logo_image;
	GdkPixbuf *watermark_image;

	GdkColor background_color;
	GdkColor textbox_color;
	GdkColor logo_background_color;
	GdkColor title_color;
	GdkColor text_color;

	GnomeEdgePosition position : 2;

	/*< private >*/
	NautilusDruidPageEdgePrivate *_priv;
};

struct _NautilusDruidPageEdgeClass
{
	NautilusDruidPageClass parent_class;
};

GtkType    nautilus_druid_page_edge_get_type          (void);
GtkWidget *nautilus_druid_page_edge_new               (GnomeEdgePosition   position);
GtkWidget *nautilus_druid_page_edge_new_with_vals     (GnomeEdgePosition   position,
						    const gchar        *title,
						    const gchar        *text,
						    GdkPixbuf          *logo,
						    GdkPixbuf          *watermark);
void       nautilus_druid_page_edge_set_bg_color      (NautilusDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       nautilus_druid_page_edge_set_textbox_color (NautilusDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       nautilus_druid_page_edge_set_logo_bg_color (NautilusDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       nautilus_druid_page_edge_set_title_color   (NautilusDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       nautilus_druid_page_edge_set_text_color    (NautilusDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       nautilus_druid_page_edge_set_text          (NautilusDruidPageEdge *druid_page_edge,
						    const gchar        *text);
void       nautilus_druid_page_edge_set_title         (NautilusDruidPageEdge *druid_page_edge,
						    const gchar        *title);
void       nautilus_druid_page_edge_set_logo          (NautilusDruidPageEdge *druid_page_edge,
						    GdkPixbuf          *logo_image);
void       nautilus_druid_page_edge_set_watermark     (NautilusDruidPageEdge *druid_page_edge,
						    GdkPixbuf          *watermark);

END_GNOME_DECLS

#endif /* __NAUTILUS_DRUID_PAGE_EDGE_H__ */
