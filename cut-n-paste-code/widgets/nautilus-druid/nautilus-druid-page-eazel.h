/* nautilus-druid-page-eazel.h
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
#ifndef NAUTILUS_DRUID_PAGE_EAZEL_H
#define NAUTILUS_DRUID_PAGE_EAZEL_H

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID_PAGE_EAZEL            (nautilus_druid_page_eazel_get_type ())
#define NAUTILUS_DRUID_PAGE_EAZEL(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID_PAGE_EAZEL, NautilusDruidPageEazel))
#define NAUTILUS_DRUID_PAGE_EAZEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID_PAGE_EAZEL, NautilusDruidPageEazelClass))
#define NAUTILUS_IS_DRUID_PAGE_EAZEL(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID_PAGE_EAZEL))
#define NAUTILUS_IS_DRUID_PAGE_EAZEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID_PAGE_EAZEL))

typedef enum {
  /* update structure when adding enums */
	NAUTILUS_DRUID_START,
	NAUTILUS_DRUID_FINISH,
	NAUTILUS_DRUID_OTHER
} NautilusDruidPosition;


typedef struct _NautilusDruidPageEazel        NautilusDruidPageEazel;
typedef struct _NautilusDruidPageEazelPrivate NautilusDruidPageEazelPrivate;
typedef struct _NautilusDruidPageEazelClass   NautilusDruidPageEazelClass;

struct _NautilusDruidPageEazel
{
	NautilusDruidPage parent;

	GtkWidget *canvas;
	char *title;
	char *text;
	GdkPixbuf *title_image;
	GdkPixbuf *sidebar_image;
	GdkPixbuf *background_image;

	GtkWidget *widget;

	NautilusDruidPosition position : 2;

	/*< private >*/
	NautilusDruidPageEazelPrivate *_priv;
};

struct _NautilusDruidPageEazelClass
{
	NautilusDruidPageClass parent_class;
};

GtkType    nautilus_druid_page_eazel_get_type          (void);
GtkWidget *nautilus_druid_page_eazel_new               (NautilusDruidPosition   position);
GtkWidget *nautilus_druid_page_eazel_new_with_vals     (NautilusDruidPosition   position,
							const gchar        *title,
							const gchar        *text,
							GdkPixbuf          *title_image,
							GdkPixbuf          *sidebar_image,
							GdkPixbuf          *background_image);
void       nautilus_druid_page_eazel_put_widget        (NautilusDruidPageEazel *druid_page_eazel,
							GtkWidget          *widget);
void       nautilus_druid_page_eazel_set_text          (NautilusDruidPageEazel *druid_page_eazel,
							const gchar        *text);
void       nautilus_druid_page_eazel_set_title         (NautilusDruidPageEazel *druid_page_eazel,
							const gchar        *title);
void       nautilus_druid_page_eazel_set_title_image   (NautilusDruidPageEazel *druid_page_eazel,
							GdkPixbuf          *title_image);
void       nautilus_druid_page_eazel_set_sidebar_image (NautilusDruidPageEazel *druid_page_eazel,
							GdkPixbuf          *sidebar_image);
void       nautilus_druid_page_eazel_set_background_image(NautilusDruidPageEazel *druid_page_eazel,
							  GdkPixbuf          *background_image);

END_GNOME_DECLS

#endif /* NAUTILUS_DRUID_PAGE_EAZEL_H */
