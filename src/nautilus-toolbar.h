/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_TOOLBAR_H__
#define __NAUTILUS_TOOLBAR_H__

#include <gtk/gtk.h>

#include "nautilus-window.h"

#define NAUTILUS_TYPE_TOOLBAR nautilus_toolbar_get_type()
#define NAUTILUS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_TOOLBAR, NautilusToolbar))
#define NAUTILUS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TOOLBAR, NautilusToolbarClass))
#define NAUTILUS_IS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_TOOLBAR))
#define NAUTILUS_IS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TOOLBAR))
#define NAUTILUS_TOOLBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_TOOLBAR, NautilusToolbarClass))

typedef struct _NautilusToolbar NautilusToolbar;
typedef struct _NautilusToolbarPrivate NautilusToolbarPrivate;
typedef struct _NautilusToolbarClass NautilusToolbarClass;

typedef enum {
	NAUTILUS_TOOLBAR_MODE_PATH_BAR,
	NAUTILUS_TOOLBAR_MODE_LOCATION_ENTRY,
} NautilusToolbarMode;

struct _NautilusToolbar {
	GtkHeaderBar parent;

	/* private */
	NautilusToolbarPrivate *priv;
};

struct _NautilusToolbarClass {
	GtkHeaderBarClass parent_class;
};

GType nautilus_toolbar_get_type (void);

GtkWidget *nautilus_toolbar_new (NautilusWindow *window);

GtkWidget *nautilus_toolbar_get_path_bar (NautilusToolbar *self);
GtkWidget *nautilus_toolbar_get_location_entry (NautilusToolbar *self);

void nautilus_toolbar_set_show_main_bar (NautilusToolbar *self,
					 gboolean show_main_bar);
void nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
					       gboolean show_location_entry);
void nautilus_toolbar_action_menu_add_item (NautilusToolbar *self,
					    GMenuItem       *item,
					    const gchar     *section_name);
void nautilus_toolbar_reset_menus (NautilusToolbar *self);

void nautilus_toolbar_sync_navigation_buttons (NautilusToolbar *self);
void nautilus_toolbar_view_menu_widget_set_zoom_level (NautilusToolbar *self,
						       gdouble level);
void nautilus_toolbar_update_view_mode (NautilusToolbar *self,
					const gchar *view_mode);

void nautilus_toolbar_show_sort_menu (NautilusToolbar *self);
void nautilus_toolbar_show_sort_trash_time (NautilusToolbar *self);
void nautilus_toolbar_show_sort_search_relevance (NautilusToolbar *self);
void nautilus_toolbar_show_visible_columns (NautilusToolbar *self);
void nautilus_toolbar_show_stop (NautilusToolbar *self);
void nautilus_toolbar_show_reload (NautilusToolbar *self);
void nautilus_toolbar_hide_stop (NautilusToolbar *self);
void nautilus_toolbar_hide_reload (NautilusToolbar *self);

#endif /* __NAUTILUS_TOOLBAR_H__ */
