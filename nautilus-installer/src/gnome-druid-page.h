/* gnome-druid-page.h
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
#ifndef __GNOME_DRUID_PAGE_H__
#define __GNOME_DRUID_PAGE_H__

#include <gtk/gtkbin.h>

#define GNOME_TYPE_DRUID_PAGE            (gnome_druid_page_get_type ())
#define GNOME_DRUID_PAGE(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DRUID_PAGE, GnomeDruidPage))
#define GNOME_DRUID_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DRUID_PAGE, GnomeDruidPageClass))
#define GNOME_IS_DRUID_PAGE(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DRUID_PAGE))
#define GNOME_IS_DRUID_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DRUID_PAGE))


typedef struct _GnomeDruidPage       GnomeDruidPage;
typedef struct _GnomeDruidPageClass  GnomeDruidPageClass;

struct _GnomeDruidPage
{
	GtkBin parent;
};
struct _GnomeDruidPageClass
{
	GtkBinClass parent_class;

	gboolean (*next)	(GnomeDruidPage *druid_page, GtkWidget *druid);
	void     (*prepare)	(GnomeDruidPage *druid_page, GtkWidget *druid);
	gboolean (*back)	(GnomeDruidPage *druid_page, GtkWidget *druid);
	void     (*finish)	(GnomeDruidPage *druid_page, GtkWidget *druid);
	gboolean (*cancel)	(GnomeDruidPage *druid_page, GtkWidget *druid);
};


GtkType  gnome_druid_page_get_type (void);
gboolean gnome_druid_page_next     (GnomeDruidPage *druid_page);
void     gnome_druid_page_prepare  (GnomeDruidPage *druid_page);
gboolean gnome_druid_page_back     (GnomeDruidPage *druid_page);
gboolean gnome_druid_page_cancel   (GnomeDruidPage *druid_page);
void     gnome_druid_page_finish   (GnomeDruidPage *druid_page);

#endif /* __GNOME_DRUID_PAGE_H__ */




