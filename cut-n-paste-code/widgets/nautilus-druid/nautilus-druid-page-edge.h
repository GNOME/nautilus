/* gnome-druid-page-edge.h
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
#ifndef __GNOME_DRUID_PAGE_EDGE_H__
#define __GNOME_DRUID_PAGE_EDGE_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
BEGIN_GNOME_DECLS

#define GNOME_TYPE_DRUID_PAGE_EDGE            (gnome_druid_page_edge_get_type ())
#define GNOME_DRUID_PAGE_EDGE(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DRUID_PAGE_EDGE, GnomeDruidPageEdge))
#define GNOME_DRUID_PAGE_EDGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DRUID_PAGE_EDGE, GnomeDruidPageEdgeClass))
#define GNOME_IS_DRUID_PAGE_EDGE(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DRUID_PAGE_EDGE))
#define GNOME_IS_DRUID_PAGE_EDGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DRUID_PAGE_EDGE))

typedef enum {
  /* update structure when adding enums */
	GNOME_EDGE_START,
	GNOME_EDGE_FINISH,
	GNOME_EDGE_OTHER
} GnomeEdgePosition;


typedef struct _GnomeDruidPageEdge        GnomeDruidPageEdge;
typedef struct _GnomeDruidPageEdgePrivate GnomeDruidPageEdgePrivate;
typedef struct _GnomeDruidPageEdgeClass   GnomeDruidPageEdgeClass;

struct _GnomeDruidPageEdge
{
	GnomeDruidPage parent;
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
	GnomeDruidPageEdgePrivate *_priv;
};

struct _GnomeDruidPageEdgeClass
{
	GnomeDruidPageClass parent_class;
};

GtkType    gnome_druid_page_edge_get_type          (void);
GtkWidget *gnome_druid_page_edge_new               (GnomeEdgePosition   position);
GtkWidget *gnome_druid_page_edge_new_with_vals     (GnomeEdgePosition   position,
						    const gchar        *title,
						    const gchar        *text,
						    GdkPixbuf          *logo,
						    GdkPixbuf          *watermark);
void       gnome_druid_page_edge_set_bg_color      (GnomeDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       gnome_druid_page_edge_set_textbox_color (GnomeDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       gnome_druid_page_edge_set_logo_bg_color (GnomeDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       gnome_druid_page_edge_set_title_color   (GnomeDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       gnome_druid_page_edge_set_text_color    (GnomeDruidPageEdge *druid_page_edge,
						    GdkColor           *color);
void       gnome_druid_page_edge_set_text          (GnomeDruidPageEdge *druid_page_edge,
						    const gchar        *text);
void       gnome_druid_page_edge_set_title         (GnomeDruidPageEdge *druid_page_edge,
						    const gchar        *title);
void       gnome_druid_page_edge_set_logo          (GnomeDruidPageEdge *druid_page_edge,
						    GdkPixbuf          *logo_image);
void       gnome_druid_page_edge_set_watermark     (GnomeDruidPageEdge *druid_page_edge,
						    GdkPixbuf          *watermark);

END_GNOME_DECLS

#endif /* __GNOME_DRUID_PAGE_EDGE_H__ */
