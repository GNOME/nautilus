/* nautilus-druid.h
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
/* TODO: allow setting bgcolor for all pages globally */
#ifndef __NAUTILUS_DRUID_H__
#define __NAUTILUS_DRUID_H__

#include <gtk/gtk.h>
#include <widgets/nautilus-druid/nautilus-druid-page.h>
#include <libgnome/gnome-defs.h>
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID            (nautilus_druid_get_type ())
#define NAUTILUS_DRUID(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID, NautilusDruid))
#define NAUTILUS_DRUID_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID, NautilusDruidClass))
#define NAUTILUS_IS_DRUID(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID))
#define NAUTILUS_IS_DRUID_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID))


typedef struct _NautilusDruid        NautilusDruid;
typedef struct _NautilusDruidPrivate NautilusDruidPrivate;
typedef struct _NautilusDruidClass   NautilusDruidClass;

struct _NautilusDruid
{
	GtkContainer parent;
	GtkWidget *back;
	GtkWidget *next;
	GtkWidget *cancel;
	GtkWidget *finish;

	/*< private >*/
	NautilusDruidPrivate *_priv;
};
struct _NautilusDruidClass
{
	GtkContainerClass parent_class;
	
	void     (*cancel)	(NautilusDruid *druid);
};


GtkType    nautilus_druid_get_type              (void);
GtkWidget *nautilus_druid_new                   (void);
void	   nautilus_druid_set_buttons_sensitive (NautilusDruid *druid,
					      gboolean back_sensitive,
					      gboolean next_sensitive,
					      gboolean cancel_sensitive);
void	   nautilus_druid_set_show_finish       (NautilusDruid *druid, gboolean show_finish);
void       nautilus_druid_prepend_page          (NautilusDruid *druid, NautilusDruidPage *page);
void       nautilus_druid_insert_page           (NautilusDruid *druid, NautilusDruidPage *back_page, NautilusDruidPage *page);
void       nautilus_druid_append_page           (NautilusDruid *druid, NautilusDruidPage *page);
void	   nautilus_druid_set_page              (NautilusDruid *druid, NautilusDruidPage *page);

END_GNOME_DECLS

#endif /* __NAUTILUS_DRUID_H__ */
