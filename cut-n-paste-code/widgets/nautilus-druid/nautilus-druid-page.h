/* nautilus-druid-page.h
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
#ifndef __NAUTILUS_DRUID_PAGE_H__
#define __NAUTILUS_DRUID_PAGE_H__

#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID_PAGE            (nautilus_druid_page_get_type ())
#define NAUTILUS_DRUID_PAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID_PAGE, NautilusDruidPage))
#define NAUTILUS_DRUID_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID_PAGE, NautilusDruidPageClass))
#define NAUTILUS_IS_DRUID_PAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID_PAGE))
#define NAUTILUS_IS_DRUID_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID_PAGE))


typedef struct _NautilusDruidPage        NautilusDruidPage;
typedef struct _NautilusDruidPagePrivate NautilusDruidPagePrivate;
typedef struct _NautilusDruidPageClass   NautilusDruidPageClass;

struct _NautilusDruidPage
{
	GtkBin parent;
	NautilusDruidPagePrivate *_priv;
};
struct _NautilusDruidPageClass
{
	GtkBinClass parent_class;

	gboolean (*next)	(NautilusDruidPage *druid_page, GtkWidget *druid);
	void     (*prepare)	(NautilusDruidPage *druid_page, GtkWidget *druid);
	gboolean (*back)	(NautilusDruidPage *druid_page, GtkWidget *druid);
	void     (*finish)	(NautilusDruidPage *druid_page, GtkWidget *druid);
	gboolean (*cancel)	(NautilusDruidPage *druid_page, GtkWidget *druid);
};


GtkType  nautilus_druid_page_get_type (void);
gboolean nautilus_druid_page_next     (NautilusDruidPage *druid_page);
void     nautilus_druid_page_prepare  (NautilusDruidPage *druid_page);
gboolean nautilus_druid_page_back     (NautilusDruidPage *druid_page);
gboolean nautilus_druid_page_cancel   (NautilusDruidPage *druid_page);
void     nautilus_druid_page_finish   (NautilusDruidPage *druid_page);

END_GNOME_DECLS

#endif /* __NAUTILUS_DRUID_PAGE_H__ */




