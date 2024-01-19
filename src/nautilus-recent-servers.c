/* nautilusgtkplacesview.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include <glib/gi18n.h>

#include <gio/gio.h>

#include "nautilus-recent-servers.h"

struct _NautilusRecentServers
{
    GObject parent_instance;

    GHashTable *server_infos;

    GFile *server_list_file;
    GFileMonitor *server_list_monitor;

    guint loading : 1;
};

static void        populate_servers (NautilusRecentServers *self);

static void        nautilus_recent_servers_set_loading (NautilusRecentServers *self,
                                                        gboolean               loading);

G_DEFINE_TYPE (NautilusRecentServers, nautilus_recent_servers, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_LOADING,
    LAST_PROP
};

enum
{
    ADDED,
    CHANGED,
    REMOVED,
    LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP];
static guint signals[LAST_SIGNAL];

static void
server_file_changed_cb (NautilusRecentServers *self)
{
    populate_servers (self);
}

static GBookmarkFile *
server_list_load (NautilusRecentServers *self)
{
    GBookmarkFile *bookmarks;
    GError *error = NULL;
    char *datadir;
    char *filename;

    bookmarks = g_bookmark_file_new ();
    datadir = g_build_filename (g_get_user_config_dir (), "gtk-4.0", NULL);
    filename = g_build_filename (datadir, "servers", NULL);

    g_mkdir_with_parents (datadir, 0700);
    g_bookmark_file_load_from_file (bookmarks, filename, &error);

    if (error)
    {
        if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
            /* only warn if the file exists */
            g_warning ("Unable to open server bookmarks: %s", error->message);
            g_clear_pointer (&bookmarks, g_bookmark_file_free);
        }

        g_clear_error (&error);
    }

    /* Monitor the file in case it's modified outside this code */
    if (!self->server_list_monitor)
    {
        self->server_list_file = g_file_new_for_path (filename);

        if (self->server_list_file)
        {
            self->server_list_monitor = g_file_monitor_file (self->server_list_file,
                                                             G_FILE_MONITOR_NONE,
                                                             NULL,
                                                             &error);

            if (error)
            {
                g_warning ("Cannot monitor server file: %s", error->message);
                g_clear_error (&error);
            }
            else
            {
                g_signal_connect_swapped (self->server_list_monitor,
                                          "changed",
                                          G_CALLBACK (server_file_changed_cb),
                                          self);
            }
        }

        g_clear_object (&self->server_list_file);
    }

    g_free (datadir);
    g_free (filename);

    return bookmarks;
}

static void
server_list_save (GBookmarkFile *bookmarks)
{
    char *filename;

    filename = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "servers", NULL);
    g_bookmark_file_to_file (bookmarks, filename, NULL);
    g_free (filename);
}

G_GNUC_UNUSED static void
server_list_add_server (NautilusRecentServers *self,
                        GFile                 *file)
{
    GBookmarkFile *bookmarks;
    GFileInfo *info;
    GError *error;
    char *title;
    char *uri;

    GDateTime *now;

    error = NULL;
    bookmarks = server_list_load (self);

    if (!bookmarks)
    {
        return;
    }

    uri = g_file_get_uri (file);

    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                              G_FILE_QUERY_INFO_NONE,
                              NULL,
                              &error);
    title = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

    g_bookmark_file_set_title (bookmarks, uri, title);
    now = g_date_time_new_now_utc ();
    g_bookmark_file_set_visited_date_time (bookmarks, uri, now);
    g_date_time_unref (now);
    g_bookmark_file_add_application (bookmarks, uri, NULL, NULL);

    server_list_save (bookmarks);

    g_bookmark_file_free (bookmarks);
    g_clear_object (&info);
    g_free (title);
    g_free (uri);
}

G_GNUC_UNUSED static void
server_list_remove_server (NautilusRecentServers *self,
                           const char            *uri)
{
    GBookmarkFile *bookmarks;

    bookmarks = server_list_load (self);

    if (!bookmarks)
    {
        return;
    }

    g_bookmark_file_remove_item (bookmarks, uri, NULL);
    server_list_save (bookmarks);

    g_bookmark_file_free (bookmarks);
}

static void
nautilus_recent_servers_finalize (GObject *object)
{
    NautilusRecentServers *self = (NautilusRecentServers *) object;

    g_clear_object (&self->server_list_file);
    g_clear_object (&self->server_list_monitor);
    g_clear_pointer (&self->server_infos, g_hash_table_destroy);

    G_OBJECT_CLASS (nautilus_recent_servers_parent_class)->finalize (object);
}

static void
nautilus_recent_servers_dispose (GObject *object)
{
    NautilusRecentServers *self = (NautilusRecentServers *) object;

    if (self->server_list_monitor)
    {
        g_signal_handlers_disconnect_by_func (self->server_list_monitor, server_file_changed_cb, object);
    }

    G_OBJECT_CLASS (nautilus_recent_servers_parent_class)->dispose (object);
}

static void
nautilus_recent_servers_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    NautilusRecentServers *self = NAUTILUS_RECENT_SERVERS (object);

    switch (prop_id)
    {
        case PROP_LOADING:
        {
            g_value_set_boolean (value, nautilus_recent_servers_get_loading (self));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static GFileInfo *
server_file_info_new (const char *uri)
{
    GFileInfo *info = g_file_info_new ();
    g_autofree char *random_name = g_dbus_generate_guid ();
    g_autoptr (GIcon) icon = g_themed_icon_new ("folder-remote");
    g_autoptr (GIcon) symbolic_icon = g_themed_icon_new ("folder-remote-symbolic");

    g_file_info_set_name (info, random_name);
    g_file_info_set_icon (info, icon);
    g_file_info_set_symbolic_icon (info, symbolic_icon);
    g_file_info_set_content_type (info, "inode/directory");
    g_file_info_set_file_type (info, G_FILE_TYPE_SHORTCUT);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL, TRUE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);

    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, uri);

    return info;
}

static int
_date_time_equal_steal_1st (GDateTime *date_time1__to_be_stolen,
                            GDateTime *date_time2)
{
    g_autoptr (GDateTime) date_time1 = date_time1__to_be_stolen;

    return ((date_time1 == NULL && date_time2 == NULL) ||
            g_date_time_equal (date_time1, date_time2));
}

static void
populate_servers (NautilusRecentServers *self)
{
    GBookmarkFile *server_list;
    char **uris;
    gsize num_uris;

    server_list = server_list_load (self);

    if (!server_list)
    {
        return;
    }

    nautilus_recent_servers_set_loading (self, TRUE);

    uris = g_bookmark_file_get_uris (server_list, &num_uris);

    if (!uris)
    {
        g_bookmark_file_free (server_list);
        nautilus_recent_servers_set_loading (self, FALSE);
        return;
    }

    g_autoptr (GList) old_infos = g_hash_table_get_values (self->server_infos);
    g_autoptr (GList) new_infos = NULL;
    g_autoptr (GList) changed_infos = NULL;

    for (gsize i = 0; i < num_uris; i++)
    {
        const gchar *uri = uris[i];
        GFileInfo *info = g_hash_table_lookup (self->server_infos, uri);
        gboolean new = FALSE;

        if (info != NULL)
        {
            old_infos = g_list_remove (old_infos, info);
        }
        else
        {
            new = TRUE;
            info = server_file_info_new (uri);
            g_hash_table_insert (self->server_infos, g_strdup (uri), info);
            new_infos = g_list_prepend (new_infos, info);
        }

        g_autofree char *name = g_bookmark_file_get_title (server_list, uri, NULL);
        GDateTime *added = g_bookmark_file_get_added_date_time (server_list, uri, NULL);
        GDateTime *visited = g_bookmark_file_get_visited_date_time (server_list, uri, NULL);
        GDateTime *modified = g_bookmark_file_get_modified_date_time (server_list, uri, NULL);

        if (new ||
            g_strcmp0 (g_file_info_get_display_name (info), name) != 0 ||
            !_date_time_equal_steal_1st (g_file_info_get_creation_date_time (info), added) ||
            !_date_time_equal_steal_1st (g_file_info_get_access_date_time (info), visited) ||
            !_date_time_equal_steal_1st (g_file_info_get_modification_date_time (info), modified))
        {
            g_file_info_set_display_name (info, name);
            g_file_info_set_creation_date_time (info, added);
            g_file_info_set_access_date_time (info, visited);
            g_file_info_set_modification_date_time (info, modified);

            if (!new)
            {
                changed_infos = g_list_prepend (changed_infos, info);
            }
        }
    }

    if (old_infos != NULL)
    {
        g_signal_emit (self, signals[REMOVED], 0, old_infos);
    }
    if (changed_infos != NULL)
    {
        g_signal_emit (self, signals[CHANGED], 0, changed_infos);
    }
    if (new_infos != NULL)
    {
        g_signal_emit (self, signals[ADDED], 0, new_infos);
    }

    for (GList *l = old_infos; l != NULL; l = l->next)
    {
        const char *old_uri = g_file_info_get_attribute_string (l->data, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);

        g_hash_table_remove (self->server_infos, old_uri);
    }

    nautilus_recent_servers_set_loading (self, FALSE);

    g_strfreev (uris);
    g_bookmark_file_free (server_list);
}

static void
nautilus_recent_servers_class_init (NautilusRecentServersClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_recent_servers_finalize;
    object_class->dispose = nautilus_recent_servers_dispose;
    object_class->get_property = nautilus_recent_servers_get_property;

    properties[PROP_LOADING] =
        g_param_spec_boolean ("loading",
                              "Loading",
                              "Whether the view is loading locations",
                              FALSE,
                              G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

    g_object_class_install_properties (object_class, LAST_PROP, properties);

    signals[ADDED] = g_signal_new ("added",
                                   G_TYPE_FROM_CLASS (object_class),
                                   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                   g_cclosure_marshal_VOID__POINTER,
                                   G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[CHANGED] = g_signal_new ("changed",
                                     G_TYPE_FROM_CLASS (object_class),
                                     G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                     g_cclosure_marshal_VOID__POINTER,
                                     G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[REMOVED] = g_signal_new ("removed",
                                     G_TYPE_FROM_CLASS (object_class),
                                     G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                     g_cclosure_marshal_VOID__POINTER,
                                     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
nautilus_recent_servers_init (NautilusRecentServers *self)
{
    self->server_infos = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

NautilusRecentServers *
nautilus_recent_servers_new (void)
{
    return g_object_new (NAUTILUS_TYPE_RECENT_SERVERS, NULL);
}

void
nautilus_recent_servers_force_reload (NautilusRecentServers *self)
{
    populate_servers (self);
}

/* (transfer full) */
GList *
nautilus_recent_servers_get_infos (NautilusRecentServers *self)
{
    g_return_val_if_fail (NAUTILUS_IS_RECENT_SERVERS (self), FALSE);

    GList *server_infos = g_hash_table_get_values (self->server_infos);

    g_list_foreach (server_infos, (GFunc) g_object_ref, NULL);
    return server_infos;
}

gboolean
nautilus_recent_servers_get_loading (NautilusRecentServers *self)
{
    g_return_val_if_fail (NAUTILUS_IS_RECENT_SERVERS (self), FALSE);

    return self->loading;
}

static void
nautilus_recent_servers_set_loading (NautilusRecentServers *self,
                                     gboolean               loading)
{
    g_return_if_fail (NAUTILUS_IS_RECENT_SERVERS (self));

    if (self->loading != loading)
    {
        self->loading = loading;
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOADING]);
    }
}
