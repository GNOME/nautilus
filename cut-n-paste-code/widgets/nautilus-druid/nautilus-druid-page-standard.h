/* nautilus-druid-page-standard.h
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
#ifndef __NAUTILUS_DRUID_PAGE_STANDARD_H__
#define __NAUTILUS_DRUID_PAGE_STANDARD_H__

#include <gtk/gtk.h>
#include <libgnomeui/gnome-canvas.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID_PAGE_STANDARD            (nautilus_druid_page_standard_get_type ())
#define NAUTILUS_DRUID_PAGE_STANDARD(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID_PAGE_STANDARD, NautilusDruidPageStandard))
#define NAUTILUS_DRUID_PAGE_STANDARD_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID_PAGE_STANDARD, NautilusDruidPageStandardClass))
#define NAUTILUS_IS_DRUID_PAGE_STANDARD(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID_PAGE_STANDARD))
#define NAUTILUS_IS_DRUID_PAGE_STANDARD_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID_PAGE_STANDARD))


typedef struct _NautilusDruidPageStandard        NautilusDruidPageStandard;
typedef struct _NautilusDruidPageStandardPrivate NautilusDruidPageStandardPrivate;
typedef struct _NautilusDruidPageStandardClass   NautilusDruidPageStandardClass;

struct _NautilusDruidPageStandard
{
	NautilusDruidPage parent;

	GtkWidget *vbox;
	GdkPixbuf *logo_image;

	gchar *title;

	GdkColor background_color;
	GdkColor logo_background_color;
	GdkColor title_color;
	
	/*< private >*/
	NautilusDruidPageStandardPrivate *_priv;
};
struct _NautilusDruidPageStandardClass
{
	NautilusDruidPageClass parent_class;
};


GtkType    nautilus_druid_page_standard_get_type      (void);
GtkWidget *nautilus_druid_page_standard_new           (void);
GtkWidget *nautilus_druid_page_standard_new_with_vals (const gchar *title, GdkPixbuf *logo);
void nautilus_druid_page_standard_set_bg_color        (NautilusDruidPageStandard *druid_page_standard,
						    GdkColor *color);
void nautilus_druid_page_standard_set_logo_bg_color   (NautilusDruidPageStandard *druid_page_standard,
						    GdkColor *color);
void nautilus_druid_page_standard_set_title_color     (NautilusDruidPageStandard *druid_page_standard,
						    GdkColor *color);
void nautilus_druid_page_standard_set_title           (NautilusDruidPageStandard *druid_page_standard,
						    const gchar *title);
void nautilus_druid_page_standard_set_logo            (NautilusDruidPageStandard *druid_page_standard,
						    GdkPixbuf *logo_image);

END_GNOME_DECLS

#endif /* __NAUTILUS_DRUID_PAGE_STANDARD_H__ */

