/* gnome-druid-page-start.h
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
#ifndef __GNOME_DRUID_PAGE_START_H__
#define __GNOME_DRUID_PAGE_START_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define GNOME_TYPE_DRUID_PAGE_START            (gnome_druid_page_start_get_type ())
#define GNOME_DRUID_PAGE_START(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DRUID_PAGE_START, GnomeDruidPageStart))
#define GNOME_DRUID_PAGE_START_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DRUID_PAGE_START, GnomeDruidPageStartClass))
#define GNOME_IS_DRUID_PAGE_START(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DRUID_PAGE_START))
#define GNOME_IS_DRUID_PAGE_START_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DRUID_PAGE_START))


typedef struct _GnomeDruidPageStart        GnomeDruidPageStart;
typedef struct _GnomeDruidPageStartPrivate GnomeDruidPageStartPrivate;
typedef struct _GnomeDruidPageStartClass   GnomeDruidPageStartClass;

struct _GnomeDruidPageStart
{
	GnomeDruidPage parent;

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
	GnomeDruidPageStartPrivate *_priv;
};
struct _GnomeDruidPageStartClass
{
  GnomeDruidPageClass parent_class;
};


GtkType    gnome_druid_page_start_get_type    (void);
GtkWidget *gnome_druid_page_start_new         (void);
GtkWidget *gnome_druid_page_start_new_with_vals(const gchar *title,
					       const gchar* text,
					       GdkPixbuf *logo,
					       GdkPixbuf *watermark);
void gnome_druid_page_start_set_bg_color      (GnomeDruidPageStart *druid_page_start,
					       GdkColor *color);
void gnome_druid_page_start_set_textbox_color (GnomeDruidPageStart *druid_page_start,
					       GdkColor *color);
void gnome_druid_page_start_set_logo_bg_color (GnomeDruidPageStart *druid_page_start,
					       GdkColor *color);
void gnome_druid_page_start_set_title_color   (GnomeDruidPageStart *druid_page_start,
					       GdkColor *color);
void gnome_druid_page_start_set_text_color    (GnomeDruidPageStart *druid_page_start,
					       GdkColor *color);
void gnome_druid_page_start_set_text          (GnomeDruidPageStart *druid_page_start,
					       const gchar *text);
void gnome_druid_page_start_set_title         (GnomeDruidPageStart *druid_page_start,
					       const gchar *title);
void gnome_druid_page_start_set_logo          (GnomeDruidPageStart *druid_page_start,
					       GdkPixbuf *logo_image);
void gnome_druid_page_start_set_watermark     (GnomeDruidPageStart *druid_page_start,
					       GdkPixbuf *watermark);

END_GNOME_DECLS

#endif /* __GNOME_DRUID_PAGE_START_H__ */
