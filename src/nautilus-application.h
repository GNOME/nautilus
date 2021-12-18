/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

G_BEGIN_DECLS
#define NAUTILUS_TYPE_APPLICATION (nautilus_application_get_type())
G_DECLARE_DERIVABLE_TYPE (NautilusApplication, nautilus_application, NAUTILUS, APPLICATION, GtkApplication)

struct _NautilusApplicationClass {
	GtkApplicationClass parent_class;

        void  (*open_location_full) (NautilusApplication     *application,
                                     GFile                   *location,
                                     NautilusOpenFlags        flags,
                                     GList                   *selection,
                                     NautilusWindow          *target_window,
                                     NautilusWindowSlot      *target_slot);
};

NautilusApplication * nautilus_application_new (void);

NautilusWindow *     nautilus_application_create_window (NautilusApplication *application,
							 GdkScreen           *screen);

void nautilus_application_set_accelerator (GApplication *app,
					   const gchar  *action_name,
					   const gchar  *accel);

void nautilus_application_set_accelerators (GApplication *app,
					    const gchar  *action_name,
					    const gchar **accels);

GList * nautilus_application_get_windows (NautilusApplication *application);

void nautilus_application_open_location (NautilusApplication *application,
					 GFile *location,
					 GFile *selection,
					 const char *startup_id);

void nautilus_application_open_location_full (NautilusApplication     *application,
                                              GFile                   *location,
                                              NautilusOpenFlags        flags,
                                              GList                   *selection,
                                              NautilusWindow          *target_window,
                                              NautilusWindowSlot      *target_slot);

NautilusApplication *nautilus_application_get_default (void);
void nautilus_application_send_notification (NautilusApplication *self,
                                             const gchar         *notification_id,
                                             GNotification       *notification);
void nautilus_application_withdraw_notification (NautilusApplication *self,
                                                 const gchar         *notification_id);

NautilusBookmarkList *
     nautilus_application_get_bookmarks  (NautilusApplication *application);
void nautilus_application_edit_bookmarks (NautilusApplication *application,
					  NautilusWindow      *window);

GtkWidget * nautilus_application_connect_server (NautilusApplication *application,
						 NautilusWindow      *window);

void nautilus_application_search (NautilusApplication *application,
                                  NautilusQuery       *query);
void nautilus_application_startup_common (NautilusApplication *application);
G_END_DECLS
