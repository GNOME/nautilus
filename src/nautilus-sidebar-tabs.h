/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the tabs widget for the index panel.
 *
 */

#ifndef __NAUTILUS_INDEX_TABS_H__
#define __NAUTILUS_INDEX_TABS_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define NAUTILUS_TYPE_INDEX_TABS	 (nautilus_index_tabs_get_type ())
#define NAUTILUS_INDEX_TABS(obj)	 (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INDEX_TABS, NautilusIndexTabs))
#define NAUTILUS_INDEX_TABS_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INDEX_TABS, NautilusIndexTabsClass))
#define NAUTILUS_IS_INDEX_TABS(obj)	 (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INDEX_TABS))
#define NAUTILUS_IS_INDEX_TABS_CLASS(klass)	 (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INDEX_TABS))

typedef struct _NautilusIndexTabs	NautilusIndexTabs;
typedef struct _NautilusIndexTabsClass	NautilusIndexTabsClass;

typedef struct _NautilusIndexTabsDetails NautilusIndexTabsDetails;
typedef struct _tabItem tabItem;

struct _NautilusIndexTabs
{
  GtkWidget widget;
  NautilusIndexTabsDetails *details;  
};

struct _NautilusIndexTabsClass
{
  GtkMiscClass parent_class;
};


GtkType	   	nautilus_index_tabs_get_type(void);
GtkWidget* 	nautilus_index_tabs_new(gint container_width);

gboolean	nautilus_index_tabs_add_view(NautilusIndexTabs *index_tabs, const gchar *name, GtkWidget *new_view, gint page_num);
gchar*          nautilus_index_tabs_get_title_from_index(NautilusIndexTabs *index_tabs, gint which_tab);
gint		nautilus_index_tabs_hit_test(NautilusIndexTabs *index_tabs, double x, double y);
void		nautilus_index_tabs_remove_view(NautilusIndexTabs *index_tabs, const gchar *name);
void		nautilus_index_tabs_select_tab(NautilusIndexTabs *index_tabs, gint which_tab);
void		nautilus_index_tabs_set_title(NautilusIndexTabs* index_tabs, const gchar *new_title);
void		nautilus_index_tabs_set_title_mode(NautilusIndexTabs* index_tabs, gboolean is_title_mode);
void		nautilus_index_tabs_set_visible(NautilusIndexTabs* index_tabs, const gchar *name, gboolean is_visible);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __NAUTILUS_INDEX_TABS_H__ */
