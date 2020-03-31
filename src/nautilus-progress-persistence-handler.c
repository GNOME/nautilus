/*
 * nautilus-progress-persistence-handler.c: file operation progress notification handler.
 *
 * Copyright (C) 2007, 2011, 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *          Carlos Soriano <csoriano@gnome.com>
 *
 */

#include <config.h>

#include "nautilus-progress-persistence-handler.h"

#include "nautilus-application.h"
#include "nautilus-progress-info-widget.h"

#include <glib/gi18n.h>

#include "nautilus-progress-info.h"
#include "nautilus-progress-info-manager.h"

struct _NautilusProgressPersistenceHandler
{
    GObject parent_instance;

    NautilusProgressInfoManager *manager;

    NautilusApplication *app;
    guint active_infos;
};

G_DEFINE_TYPE (NautilusProgressPersistenceHandler, nautilus_progress_persistence_handler, G_TYPE_OBJECT);

/* Our policy for showing progress notification is the following:
 * - file operations that end within two seconds do not get notified in any way
 * - if no file operations are running, and one passes the two seconds
 *   timeout, a window is displayed with the progress
 * - if the window is closed, we show a resident notification, depending on
 *   the capabilities of the notification daemon running in the session
 * - if some file operations are running, and another one passes the two seconds
 *   timeout, and the window is showing, we add it to the window directly
 * - in the same case, but when the window is not showing, we update the resident
 *   notification, changing its message
 * - when one file operation finishes, if it's not the last one, we only update the
 *   resident notification's message
 * - in the same case, if it's the last one, we close the resident notification,
 *   and trigger a transient one
 * - in the same case, but the window was showing, we just hide the window
 */

static gboolean server_has_persistence (void);

static void
show_file_transfers (NautilusProgressPersistenceHandler *self)
{
    GFile *home;

    home = g_file_new_for_path (g_get_home_dir ());
    nautilus_application_open_location (self->app, home, NULL, NULL);

    g_object_unref (home);
}

static void
action_show_file_transfers (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
    NautilusProgressPersistenceHandler *self;

    self = NAUTILUS_PROGRESS_PERSISTENCE_HANDLER (user_data);
    show_file_transfers (self);
}

static GActionEntry progress_persistence_entries[] =
{
    { "show-file-transfers", action_show_file_transfers, NULL, NULL, NULL }
};

static void
progress_persistence_handler_update_notification (NautilusProgressPersistenceHandler *self)
{
    GNotification *notification;
    gchar *body;

    if (!server_has_persistence ())
    {
        return;
    }

    notification = g_notification_new (_("File Operations"));
    g_notification_set_default_action (notification, "app.show-file-transfers");
    g_notification_add_button (notification, _("Show Details"),
                               "app.show-file-transfers");

    body = g_strdup_printf (ngettext ("%'d file operation active",
                                      "%'d file operations active",
                                      self->active_infos),
                            self->active_infos);
    g_notification_set_body (notification, body);

    nautilus_application_send_notification (self->app,
                                            "progress", notification);

    g_object_unref (notification);
    g_free (body);
}

void
nautilus_progress_persistence_handler_make_persistent (NautilusProgressPersistenceHandler *self)
{
    GList *windows;

    windows = nautilus_application_get_windows (self->app);
    if (self->active_infos > 0 && windows == NULL)
    {
        progress_persistence_handler_update_notification (self);
    }
}

static void
progress_persistence_handler_show_complete_notification (NautilusProgressPersistenceHandler *self)
{
    GNotification *complete_notification;

    if (!server_has_persistence ())
    {
        return;
    }

    complete_notification = g_notification_new (_("File Operations"));
    g_notification_set_body (complete_notification,
                             _("All file operations have been successfully completed"));
    nautilus_application_send_notification (self->app,
                                            "transfer-complete",
                                            complete_notification);

    g_object_unref (complete_notification);
}

static void
progress_persistence_handler_hide_notification (NautilusProgressPersistenceHandler *self)
{
    if (!server_has_persistence ())
    {
        return;
    }

    nautilus_application_withdraw_notification (self->app,
                                                "progress");
}

static void
progress_info_finished_cb (NautilusProgressInfo               *info,
                           NautilusProgressPersistenceHandler *self)
{
    GtkWindow *last_active_window;

    self->active_infos--;

    last_active_window = gtk_application_get_active_window (GTK_APPLICATION (self->app));

    if (self->active_infos > 0)
    {
        if (last_active_window == NULL)
        {
            progress_persistence_handler_update_notification (self);
        }
    }
    else
    {
        if ((last_active_window == NULL) || !gtk_window_has_toplevel_focus (last_active_window))
        {
            progress_persistence_handler_hide_notification (self);
            progress_persistence_handler_show_complete_notification (self);
        }
    }
}

static void
handle_new_progress_info (NautilusProgressPersistenceHandler *self,
                          NautilusProgressInfo               *info)
{
    GList *windows;
    g_signal_connect (info, "finished",
                      G_CALLBACK (progress_info_finished_cb), self);

    self->active_infos++;
    windows = nautilus_application_get_windows (self->app);

    if (windows == NULL)
    {
        progress_persistence_handler_update_notification (self);
    }
}

typedef struct
{
    NautilusProgressInfo *info;
    NautilusProgressPersistenceHandler *self;
} TimeoutData;

static void
timeout_data_free (TimeoutData *data)
{
    g_clear_object (&data->self);
    g_clear_object (&data->info);

    g_slice_free (TimeoutData, data);
}

static TimeoutData *
timeout_data_new (NautilusProgressPersistenceHandler *self,
                  NautilusProgressInfo               *info)
{
    TimeoutData *retval;

    retval = g_slice_new0 (TimeoutData);
    retval->self = g_object_ref (self);
    retval->info = g_object_ref (info);

    return retval;
}

static gboolean
new_op_started_timeout (TimeoutData *data)
{
    NautilusProgressInfo *info = data->info;
    NautilusProgressPersistenceHandler *self = data->self;

    if (nautilus_progress_info_get_is_paused (info))
    {
        return TRUE;
    }

    if (!nautilus_progress_info_get_is_finished (info))
    {
        handle_new_progress_info (self, info);
    }

    timeout_data_free (data);

    return FALSE;
}

static void
release_application (NautilusProgressInfo               *info,
                     NautilusProgressPersistenceHandler *self)
{
    /* release the GApplication hold we acquired */
    g_application_release (g_application_get_default ());
}

static void
progress_info_started_cb (NautilusProgressInfo               *info,
                          NautilusProgressPersistenceHandler *self)
{
    TimeoutData *data;

    /* hold GApplication so we never quit while there's an operation pending */
    g_application_hold (g_application_get_default ());

    g_signal_connect (info, "finished",
                      G_CALLBACK (release_application), self);

    data = timeout_data_new (self, info);

    /* timeout for the progress window to appear */
    g_timeout_add_seconds (2,
                           (GSourceFunc) new_op_started_timeout,
                           data);
}

static void
new_progress_info_cb (NautilusProgressInfoManager        *manager,
                      NautilusProgressInfo               *info,
                      NautilusProgressPersistenceHandler *self)
{
    g_signal_connect (info, "started",
                      G_CALLBACK (progress_info_started_cb), self);
}

static void
nautilus_progress_persistence_handler_dispose (GObject *obj)
{
    NautilusProgressPersistenceHandler *self = NAUTILUS_PROGRESS_PERSISTENCE_HANDLER (obj);

    g_clear_object (&self->manager);

    G_OBJECT_CLASS (nautilus_progress_persistence_handler_parent_class)->dispose (obj);
}

static gboolean
server_has_persistence (void)
{
    static gboolean retval = FALSE;
    GDBusConnection *conn;
    GVariant *result;
    char **cap, **caps;
    static gboolean initialized = FALSE;

    if (initialized)
    {
        return retval;
    }
    initialized = TRUE;

    conn = g_application_get_dbus_connection (g_application_get_default ());
    result = g_dbus_connection_call_sync (conn,
                                          "org.freedesktop.Notifications",
                                          "/org/freedesktop/Notifications",
                                          "org.freedesktop.Notifications",
                                          "GetCapabilities",
                                          g_variant_new ("()"),
                                          G_VARIANT_TYPE ("(as)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1, NULL, NULL);

    if (result == NULL)
    {
        return FALSE;
    }

    g_variant_get (result, "(^a&s)", &caps);

    for (cap = caps; *cap != NULL; cap++)
    {
        if (g_strcmp0 ("persistence", *cap) == 0)
        {
            retval = TRUE;
        }
    }

    g_free (caps);
    g_variant_unref (result);

    return retval;
}

static void
nautilus_progress_persistence_handler_init (NautilusProgressPersistenceHandler *self)
{
    self->manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect (self->manager, "new-progress-info",
                      G_CALLBACK (new_progress_info_cb), self);
}

static void
nautilus_progress_persistence_handler_class_init (NautilusProgressPersistenceHandlerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = nautilus_progress_persistence_handler_dispose;
}

NautilusProgressPersistenceHandler *
nautilus_progress_persistence_handler_new (GObject *app)
{
    NautilusProgressPersistenceHandler *self;

    self = g_object_new (NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER, NULL);
    self->app = NAUTILUS_APPLICATION (app);

    g_action_map_add_action_entries (G_ACTION_MAP (self->app),
                                     progress_persistence_entries, G_N_ELEMENTS (progress_persistence_entries),
                                     self);
    return self;
}
