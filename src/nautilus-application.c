/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
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
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */
#define G_LOG_DOMAIN "nautilus-application"

#include "nautilus-application.h"

#include <adwaita.h>
#include <eel/eel-stock-dialogs.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <nautilus-extension.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <xdp-gnome/externalwindow.h>

#include "nautilus-bookmark-list.h"
#include "nautilus-clipboard.h"
#include "nautilus-date-utilities.h"
#include "nautilus-dbus-launcher.h"
#include "nautilus-dbus-manager.h"
#include "nautilus-directory-private.h"
#include "nautilus-grid-cell.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-files-view.h"
#include "nautilus-freedesktop-dbus.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-label-cell.h"
#include "nautilus-module.h"
#include "nautilus-name-cell.h"
#include "nautilus-portal.h"
#include "nautilus-preferences-dialog.h"
#include "nautilus-previewer.h"
#include "nautilus-progress-persistence-handler.h"
#include "nautilus-scheme.h"
#include "nautilus-shell-search-provider.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-localsearch-utilities.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window.h"

struct _NautilusApplication
{
    AdwApplication parent_instance;

    NautilusProgressPersistenceHandler *progress_handler;
    NautilusDBusManager *dbus_manager;
    NautilusFreedesktopDBus *fdb_manager;
    NautilusPortal *portal_implementation;

    NautilusBookmarkList *bookmark_list;

    NautilusShellSearchProvider *search_provider;

    GList *windows;

    GHashTable *notifications;

    NautilusFileUndoManager *undo_manager;

    NautilusTagManager *tag_manager;

    GUnixMountMonitor *mount_monitor;

    NautilusDBusLauncher *dbus_launcher;

    guint dbus_location_update_timeout_id;
};

G_DEFINE_FINAL_TYPE (NautilusApplication, nautilus_application, ADW_TYPE_APPLICATION)

void
nautilus_application_set_accelerator (GApplication *app,
                                      const gchar  *action_name,
                                      const gchar  *accel)
{
    const gchar *vaccels[] =
    {
        accel,
        NULL
    };

    gtk_application_set_accels_for_action (GTK_APPLICATION (app), action_name, vaccels);
}

void
nautilus_application_set_accelerators (GApplication  *app,
                                       const gchar   *action_name,
                                       const gchar  **accels)
{
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), action_name, accels);
}

GList *
nautilus_application_get_windows (NautilusApplication *self)
{
    return self->windows;
}

NautilusBookmarkList *
nautilus_application_get_bookmarks (NautilusApplication *self)
{
    if (!self->bookmark_list)
    {
        self->bookmark_list = nautilus_bookmark_list_new ();
    }

    return self->bookmark_list;
}

static void
check_required_directories (NautilusApplication *self)
{
    g_autofree char *user_directory = nautilus_get_user_directory ();

    if (!g_file_test (user_directory, G_FILE_TEST_IS_DIR))
    {
        nautilus_show_ok_dialog (
            _("No Home Directory"),
            _("Make sure the directory exists and has correct access permissions set."),
            NULL);
    }
}

static void
menu_provider_items_updated_handler (NautilusMenuProvider *provider,
                                     GtkWidget            *parent_window,
                                     gpointer              data)
{
    g_signal_emit_by_name (nautilus_signaller_get_current (),
                           "popup-menu-changed");
}

static void
menu_provider_init_callback (void)
{
    GList *providers;
    GList *l;

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);

    for (l = providers; l != NULL; l = l->next)
    {
        NautilusMenuProvider *provider = NAUTILUS_MENU_PROVIDER (l->data);

        g_signal_connect_after (G_OBJECT (provider), "items-updated",
                                (GCallback) menu_provider_items_updated_handler,
                                NULL);
    }

    nautilus_module_extension_list_free (providers);
}

NautilusWindow *
nautilus_application_create_window (NautilusApplication *self,
                                    const char          *startup_id)
{
    NautilusWindow *window;
    gboolean maximized;
    g_autoptr (GVariant) default_size = NULL;
    gint default_width = 0;
    gint default_height = 0;

    g_return_val_if_fail (NAUTILUS_IS_APPLICATION (self), NULL);

    window = nautilus_window_new ();
    if (startup_id)
    {
        gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
    }

    maximized = g_settings_get_boolean
                    (nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED);
    if (maximized)
    {
        gtk_window_maximize (GTK_WINDOW (window));
    }
    else
    {
        gtk_window_unmaximize (GTK_WINDOW (window));
    }
    default_size = g_settings_get_value (nautilus_window_state,
                                         NAUTILUS_WINDOW_STATE_INITIAL_SIZE);

    g_variant_get (default_size, "(ii)", &default_width, &default_height);

    gtk_window_set_default_size (GTK_WINDOW (window),
                                 MAX (NAUTILUS_WINDOW_MIN_WIDTH, default_width),
                                 MAX (NAUTILUS_WINDOW_MIN_HEIGHT, default_height));

    if (g_strcmp0 (PROFILE, "") != 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (window), "devel");
    }

    g_debug ("Creating a new navigation window");

    return window;
}

static NautilusWindow *
get_window_with_location (NautilusApplication *self,
                          GFile               *location)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_existing (location);
    g_autoptr (GFile) searched_location = NULL;

    if (file != NULL &&
        !nautilus_file_is_directory (file) &&
        g_file_has_parent (location, NULL))
    {
        searched_location = g_file_get_parent (location);
    }
    else
    {
        searched_location = g_object_ref (location);
    }

    g_return_val_if_fail (searched_location != NULL, NULL);

    /* Check active window as a first priority */
    NautilusWindow *active_window = NAUTILUS_WINDOW (
        gtk_application_get_active_window (GTK_APPLICATION (self)));
    if (active_window != NULL && nautilus_window_has_open_location (active_window, location))
    {
        return active_window;
    }

    for (GList *l = self->windows; l != NULL; l = l->next)
    {
        NautilusWindow *window = l->data;

        if (nautilus_window_has_open_location (window, location))
        {
            return window;
        }
    }

    return NULL;
}

NautilusWindow *
nautilus_application_open_location_full (NautilusApplication *self,
                                         GFile               *location,
                                         NautilusOpenFlags    flags,
                                         NautilusFileList    *selection,
                                         const char          *startup_id)
{
    NautilusWindow *active_window;
    GdkDisplay *display;

    /* FIXME: We are having problems on getting the current focused window with
     * gtk_application_get_active_window, see https://bugzilla.gnome.org/show_bug.cgi?id=756499
     * so what we do is never rely on this on the callers, but would be cool to
     * make it work without explicitly setting the active window on the callers. */
    active_window = NAUTILUS_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (self)));
    /* There is no active window if the application is run with
     * --gapplication-service
     */

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_autofree char *uri = g_file_get_uri (location);

        g_debug ("Application opening location: %s", uri);
    }

    /* Only either flag can be set */
    g_warn_if_fail ((flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW) == 0 ||
                    (flags & NAUTILUS_OPEN_FLAG_NEW_TAB) == 0);

    NautilusWindow *target_window = NULL;

    if ((flags & NAUTILUS_OPEN_FLAG_REUSE_EXISTING) != 0)
    {
        /* Look for window that alredy shows location */
        target_window = get_window_with_location (self, location);
    }
    else if ((flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW) == 0)
    {
        /* Reuse active window */
        target_window = active_window;
    }

    if (target_window == NULL)
    {
        display = active_window != NULL ?
                  gtk_root_get_display (GTK_ROOT (active_window)) :
                  gdk_display_get_default ();

        target_window = nautilus_application_create_window (self, startup_id);
        gtk_window_set_display (GTK_WINDOW (target_window), display);
    }

    g_assert (target_window != NULL);

    /* Application is the one that manages windows, so this flag shouldn't use
     * it anymore by any client */
    flags &= ~NAUTILUS_OPEN_FLAG_NEW_WINDOW;
    nautilus_window_open_location_full (target_window, location, flags, selection);

    return target_window;
}

void
nautilus_application_open_location (NautilusApplication *self,
                                    GFile               *location,
                                    GFile               *selection,
                                    const char          *startup_id)
{
    g_autolist (NautilusFile) sel_list = NULL;
    g_autofree char *location_uri = g_file_get_uri (location);

    if (location_uri[0] == '\0')
    {
        return;
    }

    if (selection != NULL)
    {
        sel_list = g_list_prepend (sel_list, nautilus_file_get (selection));
    }

    nautilus_application_open_location_full (self, location, NAUTILUS_OPEN_FLAG_REUSE_EXISTING,
                                             sel_list, startup_id);
}

/* Note: when launched from command line we do not reach this method
 * since we manually handle the command line parameters in order to
 * parse --version, etc.
 * However this method is called when open () is called via dbus, for
 * instance when gtk_uri_open () is called from outside.
 */
static void
nautilus_application_open (GApplication  *app,
                           GFile        **files,
                           gint           n_files,
                           const gchar   *hint)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    /* Either open new window or re-open existing location to update selection */
    NautilusOpenFlags flags = g_strcmp0 (hint, "new-window") == 0
                              ? NAUTILUS_OPEN_FLAG_NEW_WINDOW
                              : NAUTILUS_OPEN_FLAG_REUSE_EXISTING;

    g_debug ("Open called on the GApplication instance; %d files", n_files);

    /* Open windows at each requested location. */
    for (int idx = 0; idx < n_files; idx++)
    {
        GFile *file = files[idx];

        g_return_if_fail (file != NULL);

        nautilus_application_open_location_full (self, file, flags, NULL, NULL);
    }
}

static void
nautilus_application_finalize (GObject *object)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (object);

    g_clear_object (&self->progress_handler);
    g_clear_object (&self->bookmark_list);

    g_list_free (self->windows);

    g_hash_table_destroy (self->notifications);

    nautilus_module_teardown ();

    g_clear_object (&self->undo_manager);

    g_clear_object (&self->tag_manager);

    g_clear_object (&self->mount_monitor);

    g_clear_object (&self->dbus_launcher);

    nautilus_trash_monitor_clear ();

    g_clear_handle_id (&self->dbus_location_update_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (GVariantDict *options)
{
    if (g_variant_dict_contains (options, "quit") &&
        g_variant_dict_contains (options, G_OPTION_REMAINING))
    {
        g_printerr ("%s\n",
                    _("--quit cannot be used with URIs."));
        return FALSE;
    }


    if (g_variant_dict_contains (options, "select") &&
        !g_variant_dict_contains (options, G_OPTION_REMAINING))
    {
        g_printerr ("%s\n",
                    _("--select must be used with at least an URI."));
        return FALSE;
    }

    return TRUE;
}

static void
nautilus_application_select (NautilusApplication  *self,
                             GFile               **files,
                             gint                  len)
{
    int i;
    GFile *file;
    GFile *parent;

    for (i = 0; i < len; i++)
    {
        file = files[i];
        parent = g_file_get_parent (file);
        if (parent != NULL)
        {
            nautilus_application_open_location (self, parent, file, NULL);
            g_object_unref (parent);
        }
        else
        {
            nautilus_application_open_location (self, file, NULL, NULL);
        }
    }
}

static void
action_new_window (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
    NautilusApplication *application;
    g_autoptr (GFile) home = NULL;

    application = NAUTILUS_APPLICATION (user_data);
    home = g_file_new_for_path (g_get_home_dir ());

    nautilus_application_open_location_full (application, home,
                                             NAUTILUS_OPEN_FLAG_NEW_WINDOW,
                                             NULL, NULL);
}

static void
action_clone_window (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
    GtkApplication *application = user_data;
    NautilusWindow *active_window = NAUTILUS_WINDOW (gtk_application_get_active_window (application));
    GFile *window_location = nautilus_window_get_active_location (active_window);
    g_autoptr (GFile) cloned_location = NULL;

    if (window_location == NULL || g_file_has_uri_scheme (window_location, SCHEME_SEARCH))
    {
        cloned_location = g_file_new_for_path (g_get_home_dir ());
    }
    else
    {
        cloned_location = g_object_ref (window_location);
    }

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (application), cloned_location,
                                             NAUTILUS_OPEN_FLAG_NEW_WINDOW, NULL, NULL);
}

static void
action_preferences (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    GtkApplication *application = user_data;
    GtkWidget *active_window = GTK_WIDGET (gtk_application_get_active_window (application));
    nautilus_preferences_dialog_show (active_window);
}

static void
action_search_settings (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
    GtkApplication *self = user_data;
    const char *parameters = "('launch-panel', [<('search', [<'locations'>])>], @a{sv} {})";

    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                 NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 g_variant_new_parsed (parameters),
                                 gtk_application_get_active_window (self));
}

static void
action_about (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    GtkApplication *application = user_data;

    nautilus_window_show_about_dialog (NAUTILUS_WINDOW (gtk_application_get_active_window (application)));
}

static void
action_help (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    AdwDialog *dialog;
    GtkApplication *application = user_data;
    GError *error = NULL;
    GtkWindow *window = gtk_application_get_active_window (application);
    g_autoptr (GtkUriLauncher) launcher = gtk_uri_launcher_new ("help:gnome-help/files");

    gtk_uri_launcher_launch (launcher, window, NULL, NULL, NULL);

    if (error)
    {
        dialog = adw_alert_dialog_new (_("There was an error displaying help"), error->message);
        adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", _("_OK"));
        adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");

        adw_dialog_present (dialog, GTK_WIDGET (window));
        g_error_free (error);
    }
}

static void
action_kill (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    AdwApplication *application = user_data;

    /* we have been asked to force quit */
    g_application_quit (G_APPLICATION (application));
}

static void
action_quit (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (user_data);
    GList *windows, *l;

    windows = nautilus_application_get_windows (self);
    /* make a copy, since the original list will be modified when destroying
     * a window, making this list invalid */
    windows = g_list_copy (windows);
    for (l = windows; l != NULL; l = l->next)
    {
        nautilus_window_close (l->data);
    }

    g_list_free (windows);
}

static const GActionEntry app_entries[] =
{
    { .name = "new-window", .activate = action_new_window },
    { .name = "clone-window", .activate = action_clone_window },
    { .name = "preferences", .activate = action_preferences },
    { .name = "search-settings", .activate = action_search_settings },
    { .name = "about", .activate = action_about },
    { .name = "help", .activate = action_help },
    { .name = "quit", .activate = action_quit },
    { .name = "kill", .activate = action_kill },
};

static void
nautilus_init_application_actions (NautilusApplication *app)
{
    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     app_entries, G_N_ELEMENTS (app_entries),
                                     app);


    nautilus_application_set_accelerator (G_APPLICATION (app),
                                          "app.clone-window", "<Primary>n");
    nautilus_application_set_accelerator (G_APPLICATION (app),
                                          "app.help", "F1");
    nautilus_application_set_accelerator (G_APPLICATION (app),
                                          "app.quit", "<Primary>q");
    nautilus_application_set_accelerator (G_APPLICATION (app),
                                          "app.preferences", "<Primary>comma");
}

static void
nautilus_application_activate (GApplication *app)
{
    GFile **files;

    g_debug ("Calling activate");

    files = g_malloc0 (2 * sizeof (GFile *));
    files[0] = g_file_new_for_path (g_get_home_dir ());
    nautilus_application_open (app, files, 1, NULL);

    g_object_unref (files[0]);
    g_free (files);
}

static gint
nautilus_application_handle_file_args (NautilusApplication *self,
                                       GVariantDict        *options)
{
    GFile **files;
    GFile *file;
    gint idx, len;
    g_autofree const gchar **remaining = NULL;
    GPtrArray *file_array;

    g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&s", &remaining);

    /* Convert args to GFiles */
    file_array = g_ptr_array_new_full (0, g_object_unref);

    if (remaining)
    {
        for (idx = 0; remaining[idx] != NULL; idx++)
        {
            gchar *cwd;

            g_variant_dict_lookup (options, "cwd", "s", &cwd);
            if (cwd == NULL)
            {
                file = g_file_new_for_commandline_arg (remaining[idx]);
            }
            else
            {
                file = g_file_new_for_commandline_arg_and_cwd (remaining[idx], cwd);
                g_free (cwd);
            }

            if (g_file_has_uri_scheme (file, SCHEME_SEARCH))
            {
                g_autofree char *error_string = NULL;
                error_string = g_strdup_printf (_("“%s” is an internal protocol. "
                                                  "Opening this location directly is not supported."),
                                                SCHEME_SEARCH ":");

                g_printerr ("%s\n", error_string);
            }
            else
            {
                g_ptr_array_add (file_array, file);
            }
        }
    }
    else if (g_variant_dict_contains (options, "new-window"))
    {
        file = g_file_new_for_path (g_get_home_dir ());
        g_ptr_array_add (file_array, file);
    }
    else
    {
        g_ptr_array_unref (file_array);

        /* No command line options or files, just activate the application */
        nautilus_application_activate (G_APPLICATION (self));
        return EXIT_SUCCESS;
    }

    len = file_array->len;
    files = (GFile **) file_array->pdata;

    if (g_variant_dict_contains (options, "select"))
    {
        nautilus_application_select (self, files, len);
    }
    else
    {
        /* Create new windows */
        nautilus_application_open (G_APPLICATION (self), files, len,
                                   g_variant_dict_contains (options, "new-window") ? "new-window" : "");
    }

    g_ptr_array_unref (file_array);

    return EXIT_SUCCESS;
}

static gint
nautilus_application_command_line (GApplication            *application,
                                   GApplicationCommandLine *command_line)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (application);
    GVariantDict *options = g_application_command_line_get_options_dict (command_line);

    if (g_variant_dict_contains (options, "version"))
    {
        g_application_command_line_print (command_line,
                                          "GNOME nautilus " PACKAGE_VERSION "\n");
        return EXIT_SUCCESS;
    }
    else if (!do_cmdline_sanity_checks (options))
    {
        return EXIT_FAILURE;
    }
    else if (g_variant_dict_contains (options, "quit"))
    {
        g_debug ("Killing app, as requested");
        g_action_group_activate_action (G_ACTION_GROUP (application),
                                        "kill", NULL);
        return -1;
    }
    else
    {
        return nautilus_application_handle_file_args (self, options);
    }
}

static void
nautilus_application_init (NautilusApplication *self)
{
    static const GOptionEntry options[] =
    {
        {
            "version", '\0', 0, G_OPTION_ARG_NONE, NULL,
            N_("Show the version of the program."), NULL
        },
        {
            "new-window", 'w', 0, G_OPTION_ARG_NONE, NULL,
            N_("Always open a new window for browsing specified URIs"), NULL
        },
        {
            "quit", 'q', 0, G_OPTION_ARG_NONE, NULL,
            N_("Quit Nautilus."), NULL
        },
        {
            "select", 's', 0, G_OPTION_ARG_NONE, NULL,
            N_("Select specified URI in parent folder."), NULL
        },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, N_("[URI…]") },

        /* The following are old options which have no effect anymore. We keep
         * them around for compatibility reasons, e.g. not breaking old scripts.
         */
        {
            "browser", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
            NULL, NULL
        },
        {
            "geometry", 'g', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, NULL,
            NULL, NULL
        },
        {
            "no-default-window", 'n', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
            NULL, NULL
        },
        {
            "no-desktop", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
            NULL, NULL
        },

        { NULL }
    };

    self->notifications = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);

    self->undo_manager = nautilus_file_undo_manager_new ();
    self->tag_manager = nautilus_tag_manager_new ();

    /* Retain a mount monitor so GIO's caching works. This helps to speed
     * up various filesystem queries, e.g. those determining whether to
     * recurse during searches. */
    self->mount_monitor = g_unix_mount_monitor_get ();

    self->dbus_launcher = nautilus_dbus_launcher_new ();

    nautilus_localsearch_setup_miner_fs_connection ();

    g_application_add_main_option_entries (G_APPLICATION (self), options);

    nautilus_ensure_extension_points ();
    nautilus_ensure_extension_builtins ();

    nautilus_clipboard_register ();
}

NautilusApplication *
nautilus_application_get_default (void)
{
    NautilusApplication *self;

    self = NAUTILUS_APPLICATION (g_application_get_default ());

    return self;
}

void
nautilus_application_send_notification (NautilusApplication *self,
                                        const gchar         *notification_id,
                                        GNotification       *notification)
{
    g_hash_table_add (self->notifications, g_strdup (notification_id));
    g_application_send_notification (G_APPLICATION (self), notification_id, notification);
}

void
nautilus_application_withdraw_notification (NautilusApplication *self,
                                            const gchar         *notification_id)
{
    if (!g_hash_table_contains (self->notifications, notification_id))
    {
        return;
    }

    g_hash_table_remove (self->notifications, notification_id);
    g_application_withdraw_notification (G_APPLICATION (self), notification_id);
}

static void
on_application_shutdown (GApplication *application,
                         gpointer      user_data)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (application);
    GList *notification_ids;
    GList *l;
    gchar *notification_id;

    notification_ids = g_hash_table_get_keys (self->notifications);
    for (l = notification_ids; l != NULL; l = l->next)
    {
        notification_id = l->data;

        g_application_withdraw_notification (application, notification_id);
    }

    g_list_free (notification_ids);

    nautilus_icon_info_clear_caches ();

    nautilus_grid_cell_clear_cache ();
    nautilus_name_cell_clear_cache ();
    nautilus_label_cell_clear_cache ();
}

static void
icon_theme_changed_callback (GtkIconTheme *icon_theme,
                             gpointer      user_data)
{
    /* Clear all pixmap caches as the icon => pixmap lookup changed */
    nautilus_icon_info_clear_caches ();

    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static void
maybe_migrate_gtk_filechooser_preferences (void)
{
    if (!g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_MIGRATED_GTK_SETTINGS))
    {
        g_autoptr (GSettingsSchema) schema = NULL;

        /* We don't depend on GTK 3. Check whether its schema is installed. */
        schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                                  "org.gtk.Settings.FileChooser",
                                                  FALSE);
        if (schema != NULL)
        {
            g_autoptr (GSettings) gtk3_settings = NULL;

            gtk3_settings = g_settings_new_with_path ("org.gtk.Settings.FileChooser",
                                                      "/org/gtk/settings/file-chooser/");
            g_settings_set_boolean (gtk_filechooser_preferences,
                                    NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                                    g_settings_get_boolean (gtk3_settings,
                                                            NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST));
            g_settings_set_boolean (gtk_filechooser_preferences,
                                    NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                                    g_settings_get_boolean (gtk3_settings,
                                                            NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES));
        }
        g_settings_set_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_MIGRATED_GTK_SETTINGS,
                                TRUE);
    }
}

static void
nautilus_application_identify_to_portal (GApplication *app)
{
    GDBusConnection *session_bus = g_application_get_dbus_connection (app);
    if (session_bus == NULL)
    {
        return;
    }

    GVariantBuilder builder;
    g_variant_builder_init_static (&builder, G_VARIANT_TYPE_VARDICT);

    /* Intentionally ignore errors */
    g_dbus_connection_call (session_bus,
                            "org.freedesktop.portal.Desktop",
                            "/org/freedesktop/portal/desktop",
                            "org.freedesktop.host.portal.Registry",
                            "Register",
                            g_variant_new ("(sa{sv})",
                                           APPLICATION_ID,
                                           &builder),
                            NULL,
                            G_DBUS_CALL_FLAGS_NO_AUTO_START,
                            -1,
                            NULL, NULL, NULL);
}

static void
nautilus_application_startup (GApplication *app)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    g_application_set_resource_base_path (G_APPLICATION (self), "/org/gnome/nautilus");

    /* Register the app with the host app registry in XDG Desktop Portal before
     * we initialize GDK display, which on Wayland uses Settings portal.
     * This is needed for subsequent portal calls to access app id, which is
     * necessary for working suspend/logout inhibition.
     * See https://gitlab.gnome.org/GNOME/nautilus/-/issues/3874 */
    if (!nautilus_application_is_sandboxed ())
    {
        nautilus_application_identify_to_portal (app);
    }

    /* Initialize GDK display (for wayland-x11-interop protocol) before GTK does
     * it during the chain-up. */
    g_autoptr (GError) error = NULL;
    GdkDisplay *display = init_external_window_display (&error);
    if (display == NULL || error != NULL)
    {
        g_message ("Failed to initialize display server connection: %s",
                   error->message);
    }

    /* Chain up to the GtkApplication implementation early, so that gtk_init()
     * is called for us.
     */
    G_APPLICATION_CLASS (nautilus_application_parent_class)->startup (G_APPLICATION (self));

    g_assert (display == NULL || gdk_display_get_default () == display);

    gtk_window_set_default_icon_name (APPLICATION_ID);

    /* initialize preferences and create the global GSettings objects */
    nautilus_global_preferences_init ();

    /* initialize data preference watchers */
    nautilus_date_setup_preferences ();

    /* initialize nautilus modules */
    nautilus_module_setup ();

    /* attach menu-provider module callback */
    menu_provider_init_callback ();

    /* Initialize the UI handler singleton for file operations */
    self->progress_handler = nautilus_progress_persistence_handler_new (G_OBJECT (self));

    /* Check the user's .nautilus directories and post warnings
     * if there are problems.
     */
    check_required_directories (self);

    nautilus_init_application_actions (self);

    if (!g_test_initialized ())
    {
        maybe_migrate_gtk_filechooser_preferences ();
    }

    g_signal_connect (self, "shutdown", G_CALLBACK (on_application_shutdown), NULL);

    g_signal_connect_object (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                             "changed",
                             G_CALLBACK (icon_theme_changed_callback),
                             NULL, 0);
}

static gboolean
nautilus_application_dbus_register (GApplication     *app,
                                    GDBusConnection  *connection,
                                    const gchar      *object_path,
                                    GError          **error)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    self->dbus_manager = nautilus_dbus_manager_new ();
    if (!nautilus_dbus_manager_register (self->dbus_manager, connection, error))
    {
        return FALSE;
    }

    self->fdb_manager = nautilus_freedesktop_dbus_new ();
    if (!nautilus_freedesktop_dbus_register (self->fdb_manager, connection, error))
    {
        return FALSE;
    }

    if (!g_test_initialized ())
    {
        self->portal_implementation = nautilus_portal_new ();
        if (!nautilus_portal_register (self->portal_implementation, connection, error))
        {
            return FALSE;
        }
    }

    self->search_provider = nautilus_shell_search_provider_new ();
    if (!nautilus_shell_search_provider_register (self->search_provider, connection, error))
    {
        return FALSE;
    }

    nautilus_previewer_setup ();

    return TRUE;
}

static void
nautilus_application_dbus_unregister (GApplication    *app,
                                      GDBusConnection *connection,
                                      const gchar     *object_path)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    if (self->dbus_manager)
    {
        nautilus_dbus_manager_unregister (self->dbus_manager);
        g_clear_object (&self->dbus_manager);
    }

    if (self->fdb_manager)
    {
        nautilus_freedesktop_dbus_unregister (self->fdb_manager);
        g_clear_object (&self->fdb_manager);
    }

    if (self->portal_implementation != NULL)
    {
        nautilus_portal_unregister (self->portal_implementation);
        g_clear_object (&self->portal_implementation);
    }

    if (self->search_provider)
    {
        nautilus_shell_search_provider_unregister (self->search_provider);
        g_clear_object (&self->search_provider);
    }

    nautilus_previewer_teardown (connection);
}

static void
update_dbus_opened_locations (NautilusApplication *self)
{
    self->dbus_location_update_timeout_id = 0;

    g_autoptr (GHashTable) hashed_locations = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                     g_free, NULL);
    const gchar *dbus_object_path = g_application_get_dbus_object_path (G_APPLICATION (self));
    g_auto (GVariantBuilder) windows_to_locations_builder = G_VARIANT_BUILDER_INIT (
        G_VARIANT_TYPE ("a{sas}"));

    g_return_if_fail (dbus_object_path != NULL);

    for (GList *l = self->windows; l != NULL; l = l->next)
    {
        NautilusWindow *window = l->data;

        g_auto (GVariantBuilder) locations_in_window_builder = G_VARIANT_BUILDER_INIT (
            G_VARIANT_TYPE_STRING_ARRAY);

        g_autoptr (GFileList) locations = nautilus_window_get_locations (window);

        for (GList *ll = locations; ll != NULL; ll = ll->next)
        {
            GFile *location = ll->data;
            g_autofree char *uri = g_file_get_uri (location);

            g_variant_builder_add (&locations_in_window_builder, "s", uri);

            if (!g_hash_table_contains (hashed_locations, uri))
            {
                g_hash_table_add (hashed_locations, g_steal_pointer (&uri));
            }
        }

        guint32 id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window));
        g_autofree gchar *path = g_strdup_printf ("%s/window/%u", dbus_object_path, id);
        g_variant_builder_add (&windows_to_locations_builder, "{sas}", path, &locations_in_window_builder);
    }

    g_autoptr (GPtrArray) open_locations = g_hash_table_steal_all_keys (hashed_locations);
    /* Make array NULL-terminated */
    g_ptr_array_add (open_locations, NULL);

    nautilus_freedesktop_dbus_set_open_locations (self->fdb_manager,
                                                  (const gchar **) open_locations->pdata);

    g_autoptr (GVariant) windows_to_locations = g_variant_ref_sink (
        g_variant_builder_end (&windows_to_locations_builder));
    nautilus_freedesktop_dbus_set_open_windows_with_locations (self->fdb_manager,
                                                               windows_to_locations);
}

static void
schedule_dbus_location_update (NautilusApplication *self)
{
    static guint dbus_update_delay_ms = 100;

    /* Avoid updating too often by bundling multiple signals */
    if (self->dbus_location_update_timeout_id == 0)
    {
        self->dbus_location_update_timeout_id = g_timeout_add_once (
            dbus_update_delay_ms, (GSourceOnceFunc) update_dbus_opened_locations, self);
    }
}

static void
nautilus_application_window_added (GtkApplication *app,
                                   GtkWindow      *window)
{
    g_return_if_fail (NAUTILUS_IS_APPLICATION (app));

    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_added (app, window);

    if (NAUTILUS_IS_WINDOW (window))
    {
        self->windows = g_list_prepend (self->windows, window);
        g_signal_connect_swapped (window, "locations-changed",
                                  G_CALLBACK (schedule_dbus_location_update), app);
    }
}

static void
nautilus_application_window_removed (GtkApplication *app,
                                     GtkWindow      *window)
{
    g_return_if_fail (NAUTILUS_IS_APPLICATION (app));

    NautilusApplication *self = NAUTILUS_APPLICATION (app);

    GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_removed (app, window);

    if (NAUTILUS_IS_WINDOW (window))
    {
        self->windows = g_list_remove_all (self->windows, window);
        g_signal_handlers_disconnect_by_func (window, schedule_dbus_location_update, app);
    }

    /* if this was the last window, close the previewer */
    if (self->windows == NULL)
    {
        nautilus_previewer_call_close ();
        nautilus_progress_persistence_handler_make_persistent (self->progress_handler);
    }

    schedule_dbus_location_update (self);
}

/* Manage the local instance command line options. This is only necessary to
 * resolve correctly relative paths, since if the main instance resolve them in
 * open(), it will do it with its current cwd, which may not be correct for the
 * non main GApplication instance */
static gint
nautilus_application_handle_local_options (GApplication *app,
                                           GVariantDict *options)
{
    gchar *cwd;

    cwd = g_get_current_dir ();
    g_variant_dict_insert (options, "cwd", "s", cwd);
    g_free (cwd);

    return -1;
}

static void
nautilus_application_class_init (NautilusApplicationClass *class)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;
    GtkApplicationClass *gtkapp_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->finalize = nautilus_application_finalize;

    application_class = G_APPLICATION_CLASS (class);
    application_class->startup = nautilus_application_startup;
    application_class->activate = nautilus_application_activate;
    application_class->dbus_register = nautilus_application_dbus_register;
    application_class->dbus_unregister = nautilus_application_dbus_unregister;
    application_class->open = nautilus_application_open;
    application_class->command_line = nautilus_application_command_line;
    application_class->handle_local_options = nautilus_application_handle_local_options;

    gtkapp_class = GTK_APPLICATION_CLASS (class);
    gtkapp_class->window_added = nautilus_application_window_added;
    gtkapp_class->window_removed = nautilus_application_window_removed;
}

NautilusApplication *
nautilus_application_new (void)
{
    return g_object_new (NAUTILUS_TYPE_APPLICATION,
                         "application-id", APPLICATION_ID,
                         "flags", G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_HANDLES_OPEN,
                         "inactivity-timeout", 12000,
                         NULL);
}

void
nautilus_application_search (NautilusApplication *self,
                             NautilusQuery       *query)
{
    g_autoptr (GFile) home = g_file_new_for_path (g_get_home_dir ());

    NautilusWindow *window = nautilus_application_open_location_full (
        self, home, NAUTILUS_OPEN_FLAG_NEW_WINDOW, NULL, NULL);

    nautilus_window_search (window, query);
}

gboolean
nautilus_application_is_sandboxed (void)
{
    static gboolean ret;

    static gsize init = 0;
    if (g_once_init_enter (&init))
    {
        ret = xdp_portal_running_under_sandbox ();

        g_once_init_leave (&init, 1);
    }

    return ret;
}
