/* gnome-druid-page-finish.h
 * Copyright (C) 1999  Red Hat, Inc.
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
#ifndef __GNOME_DRUID_PAGE_FINISH_H__
#define __GNOME_DRUID_PAGE_FINISH_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define GNOME_TYPE_DRUID_PAGE_FINISH            (gnome_druid_page_finish_get_type ())
#define GNOME_DRUID_PAGE_FINISH(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DRUID_PAGE_FINISH, GnomeDruidPageFinish))
#define GNOME_DRUID_PAGE_FINISH_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DRUID_PAGE_FINISH, GnomeDruidPageFinishClass))
#define GNOME_IS_DRUID_PAGE_FINISH(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DRUID_PAGE_FINISH))
#define GNOME_IS_DRUID_PAGE_FINISH_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DRUID_PAGE_FINISH))


typedef struct _GnomeDruidPageFinish        GnomeDruidPageFinish;
typedef struct _GnomeDruidPageFinishPrivate GnomeDruidPageFinishPrivate;
typedef struct _GnomeDruidPageFinishClass   GnomeDruidPageFinishClass;

struct _GnomeDruidPageFinish
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
	GnomeDruidPageFinishPrivate *_priv;
};
struct _GnomeDruidPageFinishClass
{
  GnomeDruidPageClass parent_class;
};


GtkType    gnome_druid_page_finish_get_type    (void);
GtkWidget *gnome_druid_page_finish_new         (void);
GtkWidget *gnome_druid_page_finish_new_with_vals(const gchar *title,
					       const gchar* text,
					       GdkPixbuf *logo,
					       GdkPixbuf *watermark);
void gnome_druid_page_finish_set_bg_color      (GnomeDruidPageFinish *druid_page_finish,
					       GdkColor *color);
void gnome_druid_page_finish_set_textbox_color (GnomeDruidPageFinish *druid_page_finish,
					       GdkColor *color);
void gnome_druid_page_finish_set_logo_bg_color (GnomeDruidPageFinish *druid_page_finish,
					       GdkColor *color);
void gnome_druid_page_finish_set_title_color   (GnomeDruidPageFinish *druid_page_finish,
					       GdkColor *color);
void gnome_druid_page_finish_set_text_color    (GnomeDruidPageFinish *druid_page_finish,
					       GdkColor *color);
void gnome_druid_page_finish_set_text          (GnomeDruidPageFinish *druid_page_finish,
					       const gchar *text);
void gnome_druid_page_finish_set_title         (GnomeDruidPageFinish *druid_page_finish,
					       const gchar *title);
void gnome_druid_page_finish_set_logo          (GnomeDruidPageFinish *druid_page_finish,
					       GdkPixbuf *logo_image);
void gnome_druid_page_finish_set_watermark     (GnomeDruidPageFinish *druid_page_finish,
					       GdkPixbuf *watermark);

END_GNOME_DECLS

#endif /* __GNOME_DRUID_PAGE_FINISH_H__ */
