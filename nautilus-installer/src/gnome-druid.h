/* gnome-druid.h
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
#ifndef __GNOME_DRUID_H__
#define __GNOME_DRUID_H__

#include <gtk/gtk.h>
#include "gnome-druid-page.h"

#define GNOME_TYPE_DRUID            (gnome_druid_get_type ())
#define GNOME_DRUID(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DRUID, GnomeDruid))
#define GNOME_DRUID_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DRUID, GnomeDruidClass))
#define GNOME_IS_DRUID(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DRUID))
#define GNOME_IS_DRUID_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DRUID))


typedef struct _GnomeDruid       GnomeDruid;
typedef struct _GnomeDruidClass  GnomeDruidClass;

struct _GnomeDruid
{
	GtkContainer parent;
	GtkWidget *back;
	GtkWidget *next;
	GtkWidget *cancel;
	GtkWidget *finish;

	/*< private >*/
	GnomeDruidPage *current;
	GList *children;
	gboolean show_finish; /* if TRUE, then we are showing the finish button instead of the next button */
};
struct _GnomeDruidClass
{
	GtkContainerClass parent_class;
	
	void     (*cancel)	(GnomeDruid *druid);
};


GtkType    gnome_druid_get_type              (void);
GtkWidget *gnome_druid_new                   (void);
void	   gnome_druid_set_buttons_sensitive (GnomeDruid *druid,
					      gboolean back_sensitive,
					      gboolean next_sensitive,
					      gboolean cancel_sensitive);
void	   gnome_druid_set_show_finish       (GnomeDruid *druid, gboolean show_finish);
void       gnome_druid_prepend_page          (GnomeDruid *druid, GnomeDruidPage *page);
void       gnome_druid_insert_page           (GnomeDruid *druid, GnomeDruidPage *back_page, GnomeDruidPage *page);
void       gnome_druid_append_page           (GnomeDruid *druid, GnomeDruidPage *page);
void	   gnome_druid_set_page              (GnomeDruid *druid, GnomeDruidPage *page);


#endif /* __GNOME_DRUID_H__ */
