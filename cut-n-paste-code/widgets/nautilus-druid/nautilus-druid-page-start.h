/* nautilus-druid-page-start.h
 * Copyright (C) 1999  Red Hat, Inc.
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
#ifndef __NAUTILUS_DRUID_PAGE_START_H__
#define __NAUTILUS_DRUID_PAGE_START_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID_PAGE_START            (nautilus_druid_page_start_get_type ())
#define NAUTILUS_DRUID_PAGE_START(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID_PAGE_START, NautilusDruidPageStart))
#define NAUTILUS_DRUID_PAGE_START_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID_PAGE_START, NautilusDruidPageStartClass))
#define NAUTILUS_IS_DRUID_PAGE_START(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID_PAGE_START))
#define NAUTILUS_IS_DRUID_PAGE_START_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID_PAGE_START))


typedef struct _NautilusDruidPageStart        NautilusDruidPageStart;
typedef struct _NautilusDruidPageStartPrivate NautilusDruidPageStartPrivate;
typedef struct _NautilusDruidPageStartClass   NautilusDruidPageStartClass;

struct _NautilusDruidPageStart
{
	NautilusDruidPage parent;

	GdkColor background_color;
	GdkColor textbox_color;
	GdkColor logo_background_color;
	GdkColor title_color;
	GdkColor text_color;

	gchar *title;
	gchar *text;
	GdkPixbuf *logo_image;
	GdkPixbuf *watermark_image;

	/*< private >*/
	NautilusDruidPageStartPrivate *_priv;
};
struct _NautilusDruidPageStartClass
{
  NautilusDruidPageClass parent_class;
};


GtkType    nautilus_druid_page_start_get_type    (void);
GtkWidget *nautilus_druid_page_start_new         (void);
GtkWidget *nautilus_druid_page_start_new_with_vals(const gchar *title,
					       const gchar* text,
					       GdkPixbuf *logo,
					       GdkPixbuf *watermark);
void nautilus_druid_page_start_set_bg_color      (NautilusDruidPageStart *druid_page_start,
					       GdkColor *color);
void nautilus_druid_page_start_set_textbox_color (NautilusDruidPageStart *druid_page_start,
					       GdkColor *color);
void nautilus_druid_page_start_set_logo_bg_color (NautilusDruidPageStart *druid_page_start,
					       GdkColor *color);
void nautilus_druid_page_start_set_title_color   (NautilusDruidPageStart *druid_page_start,
					       GdkColor *color);
void nautilus_druid_page_start_set_text_color    (NautilusDruidPageStart *druid_page_start,
					       GdkColor *color);
void nautilus_druid_page_start_set_text          (NautilusDruidPageStart *druid_page_start,
					       const gchar *text);
void nautilus_druid_page_start_set_title         (NautilusDruidPageStart *druid_page_start,
					       const gchar *title);
void nautilus_druid_page_start_set_logo          (NautilusDruidPageStart *druid_page_start,
					       GdkPixbuf *logo_image);
void nautilus_druid_page_start_set_watermark     (NautilusDruidPageStart *druid_page_start,
					       GdkPixbuf *watermark);

END_GNOME_DECLS

#endif /* __NAUTILUS_DRUID_PAGE_START_H__ */
