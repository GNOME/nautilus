/*
 * nautilus-application: main Nautilus application class.
 *
 * SPDX-FileCopyrightText: 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_APPLICATION (nautilus_application_get_type())
G_DECLARE_FINAL_TYPE (NautilusApplication, nautilus_application,
                      NAUTILUS, APPLICATION, AdwApplication)

NautilusApplication * nautilus_application_new (void);

NautilusWindow *
nautilus_application_create_window (NautilusApplication *self);

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

NautilusWindow *
nautilus_application_open_location_full (NautilusApplication *application,
                                         GFile               *location,
                                         NautilusOpenFlags    flags,
                                         NautilusFileList    *selection,
                                         const char          *startup_id);

NautilusApplication *nautilus_application_get_default (void);

/* Notification category strings to use with g_notification_set_catergory ().
 * From https://specifications.freedesktop.org/notification/latest/categories.html */

#define XDG_NOTIFICATION_CATEGORY_TRANSFER "transfer"
#define XDG_NOTIFICATION_CATEGORY_TRANSFER_COMPLETE "transfer.complete"
#define XDG_NOTIFICATION_CATEGORY_DEVICE "device"
#define XDG_NOTIFICATION_CATEGORY_DEVICE_REMOVED "device.removed"

void nautilus_application_send_notification (NautilusApplication *self,
                                             const gchar         *notification_id,
                                             GNotification       *notification);
void nautilus_application_withdraw_notification (NautilusApplication *self,
                                                 const gchar         *notification_id);

NautilusBookmarkList *
     nautilus_application_get_bookmarks  (NautilusApplication *application);

void nautilus_application_search (NautilusApplication *application,
                                  NautilusQuery       *query);
gboolean nautilus_application_is_sandboxed (void);
G_END_DECLS
