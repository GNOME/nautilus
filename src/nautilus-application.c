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

#include "nautilus-application.h"

#include <adwaita.h>
#include <eel/eel-stock-dialogs.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <nautilus-extension.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_APPLICATION
#include "nautilus-debug.h"

#include "nautilus-bookmark-list.h"
#include "nautilus-clipboard.h"
#include "nautilus-dbus-launcher.h"
#include "nautilus-dbus-manager.h"
#include "nautilus-directory-private.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-freedesktop-dbus.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-module.h"
#include "nautilus-preferences-window.h"
#include "nautilus-previewer.h"
#include "nautilus-profile.h"
#include "nautilus-progress-persistence-handler.h"
#include "nautilus-self-check-functions.h"
#include "nautilus-shell-search-provider.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-tracker-utilities.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-view.h"
#include "nautilus-window-slot.h"
#include "nautilus-window.h"

typedef struct
{
    NautilusProgressPersistenceHandler *progress_handler;
    NautilusDBusManager *dbus_manager;
    NautilusFreedesktopDBus *fdb_manager;

    NautilusBookmarkList *bookmark_list;

    NautilusShellSearchProvider *search_provider;

    GList *windows;

    GHashTable *notifications;

    NautilusFileUndoManager *undo_manager;

    NautilusTagManager *tag_manager;

    NautilusDBusLauncher *dbus_launcher;

    guint previewer_selection_id;
} NautilusApplicationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusApplication, nautilus_application, ADW_TYPE_APPLICATION);

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
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);

    return priv->windows;
}

NautilusBookmarkList *
nautilus_application_get_bookmarks (NautilusApplication *self)
{
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);

    if (!priv->bookmark_list)
    {
        priv->bookmark_list = nautilus_bookmark_list_new ();
    }

    return priv->bookmark_list;
}

static gboolean
check_required_directories (NautilusApplication *self)
{
    char *user_directory;
    GSList *directories;
    gboolean ret;

    g_assert (NAUTILUS_IS_APPLICATION (self));

    nautilus_profile_start (NULL);

    ret = TRUE;

    user_directory = nautilus_get_user_directory ();

    directories = NULL;

    if (!g_file_test (user_directory, G_FILE_TEST_IS_DIR))
    {
        directories = g_slist_prepend (directories, user_directory);
    }

    if (directories != NULL)
    {
        int failed_count;
        GString *directories_as_string;
        GSList *l;
        char *error_string;
        g_autofree char *detail_string = NULL;
        AdwMessageDialog *dialog;

        ret = FALSE;

        failed_count = g_slist_length (directories);

        directories_as_string = g_string_new ((const char *) directories->data);
        for (l = directories->next; l != NULL; l = l->next)
        {
            g_string_append_printf (directories_as_string, ", %s", (const char *) l->data);
        }

        error_string = _("Oops! Something went wrong.");
        if (failed_count == 1)
        {
            detail_string = g_strdup_printf (_("Unable to create a required folder. "
                                               "Please create the following folder, or "
                                               "set permissions such that it can be created:\n%s"),
                                             directories_as_string->str);
        }
        else
        {
            detail_string = g_strdup_printf (_("Unable to create required folders. "
                                               "Please create the following folders, or "
                                               "set permissions such that they can be created:\n%s"),
                                             directories_as_string->str);
        }

        dialog = show_dialog (error_string, detail_string, NULL, GTK_MESSAGE_ERROR);
        /* We need the main event loop so the user has a chance to see the dialog. */
        gtk_application_add_window (GTK_APPLICATION (self),
                                    GTK_WINDOW (dialog));

        g_string_free (directories_as_string, TRUE);
    }

    g_slist_free (directories);
    g_free (user_directory);
    nautilus_profile_end (NULL);

    return ret;
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
nautilus_application_create_window (NautilusApplication *self)
{
    NautilusWindow *window;
    gboolean maximized;
    g_autoptr (GVariant) default_size = NULL;
    gint default_width = 0;
    gint default_height = 0;

    g_return_val_if_fail (NAUTILUS_IS_APPLICATION (self), NULL);
    nautilus_profile_start (NULL);

    window = nautilus_window_new ();

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

    DEBUG ("Creating a new navigation window");
    nautilus_profile_end (NULL);

    return window;
}

static NautilusWindowSlot *
get_window_slot_for_location (NautilusApplication *self,
                              GFile               *location)
{
    NautilusApplicationPrivate *priv;
    NautilusWindowSlot *slot;
    NautilusWindow *window;
    NautilusFile *file;
    GList *l, *sl;

    priv = nautilus_application_get_instance_private (self);
    slot = NULL;
    file = nautilus_file_get (location);

    if (!nautilus_file_is_directory (file) && !nautilus_file_is_other_locations (file) &&
        g_file_has_parent (location, NULL))
    {
        location = g_file_get_parent (location);
    }
    else
    {
        g_object_ref (location);
    }

    for (l = priv->windows; l != NULL; l = l->next)
    {
        window = l->data;

        for (sl = nautilus_window_get_slots (window); sl; sl = sl->next)
        {
            NautilusWindowSlot *current = sl->data;
            GFile *slot_location = nautilus_window_slot_get_location (current);

            if (slot_location && g_file_equal (slot_location, location))
            {
                slot = current;
                break;
            }
        }

        if (slot)
        {
            break;
        }
    }

    nautilus_file_unref (file);
    g_object_unref (location);

    return slot;
}

void
nautilus_application_open_location_full (NautilusApplication *self,
                                         GFile               *location,
                                         NautilusOpenFlags    flags,
                                         GList               *selection,
                                         NautilusWindow      *target_window,
                                         NautilusWindowSlot  *target_slot)
{
    NAUTILUS_APPLICATION_CLASS (G_OBJECT_GET_CLASS (self))->open_location_full (self,
                                                                                location,
                                                                                flags,
                                                                                selection,
                                                                                target_window,
                                                                                target_slot);
}

static void
real_open_location_full (NautilusApplication *self,
                         GFile               *location,
                         NautilusOpenFlags    flags,
                         GList               *selection,
                         NautilusWindow      *target_window,
                         NautilusWindowSlot  *target_slot)
{
    NautilusWindowSlot *active_slot = NULL;
    NautilusWindow *active_window;
    GFile *old_location = NULL;
    char *old_uri, *new_uri;
    gboolean use_same;
    GdkDisplay *display;

    use_same = TRUE;
    /* FIXME: We are having problems on getting the current focused window with
     * gtk_application_get_active_window, see https://bugzilla.gnome.org/show_bug.cgi?id=756499
     * so what we do is never rely on this on the callers, but would be cool to
     * make it work withouth explicitly setting the active window on the callers. */
    active_window = NAUTILUS_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (self)));
    /* There is no active window if the application is run with
     * --gapplication-service
     */
    if (active_window)
    {
        active_slot = nautilus_window_get_active_slot (active_window);
        /* Just for debug.*/
        if (active_slot != NULL)
        {
            old_location = nautilus_window_slot_get_location (active_slot);
        }
    }


    /* this happens at startup */
    if (old_location == NULL)
    {
        old_uri = g_strdup ("(none)");
    }
    else
    {
        old_uri = g_file_get_uri (old_location);
    }

    new_uri = g_file_get_uri (location);

    DEBUG ("Application opening location, old: %s, new: %s", old_uri, new_uri);
    nautilus_profile_start ("Application opening location, old: %s, new: %s", old_uri, new_uri);

    g_free (old_uri);
    g_free (new_uri);
    /* end debug */

    /* In case a target slot is provided, we can use it's associated window.
     * In case a target window were given as well, we give preference to the
     * slot we target at */
    if (target_slot != NULL)
    {
        target_window = nautilus_window_slot_get_window (target_slot);
    }

    g_assert (!((flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW) != 0 &&
                (flags & NAUTILUS_OPEN_FLAG_NEW_TAB) != 0));

    /* and if the flags specify so, this is overridden */
    if ((flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW) != 0)
    {
        use_same = FALSE;
    }

    /* now get/create the window */
    if (use_same)
    {
        if (!target_window)
        {
            if (!target_slot)
            {
                target_window = active_window;
            }
            else
            {
                target_window = nautilus_window_slot_get_window (target_slot);
            }
        }
    }
    else
    {
        display = active_window != NULL ?
                  gtk_root_get_display (GTK_ROOT (active_window)) :
                  gdk_display_get_default ();

        target_window = nautilus_application_create_window (self);
        /* Whatever the caller says, the slot won't be the same */
        gtk_window_set_display (GTK_WINDOW (target_window), display);
        target_slot = NULL;
    }

    g_assert (target_window != NULL);

    /* Application is the one that manages windows, so this flag shouldn't use
     * it anymore by any client */
    flags &= ~NAUTILUS_OPEN_FLAG_NEW_WINDOW;
    nautilus_window_open_location_full (target_window, location, flags, selection, target_slot);
}

static NautilusWindow *
open_window (NautilusApplication *self,
             GFile               *location)
{
    NautilusWindow *window;

    nautilus_profile_start (NULL);
    window = nautilus_application_create_window (self);

    if (location != NULL)
    {
        nautilus_application_open_location_full (self, location, 0, NULL, window, NULL);
    }
    else
    {
        GFile *home;
        home = g_file_new_for_path (g_get_home_dir ());
        nautilus_application_open_location_full (self, home, 0, NULL, window, NULL);

        g_object_unref (home);
    }

    nautilus_profile_end (NULL);

    return window;
}

void
nautilus_application_open_location (NautilusApplication *self,
                                    GFile               *location,
                                    GFile               *selection,
                                    const char          *startup_id)
{
    NautilusWindow *window;
    NautilusWindowSlot *slot;
    GList *sel_list = NULL;

    nautilus_profile_start (NULL);

    if (selection != NULL)
    {
        sel_list = g_list_prepend (sel_list, nautilus_file_get (selection));
    }

    slot = get_window_slot_for_location (self, location);

    if (!slot)
    {
        window = nautilus_application_create_window (self);
    }
    else
    {
        window = nautilus_window_slot_get_window (slot);
    }

    nautilus_application_open_location_full (self, location, 0, sel_list, window, slot);

    if (sel_list != NULL)
    {
        nautilus_file_list_free (sel_list);
    }

    nautilus_profile_end (NULL);
}

/* Note: when launched from command line we do not reach this method
 * since we manually handle the command line parameters in order to
 * parse --version, --check, etc.
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
    gboolean force_new = (g_strcmp0 (hint, "new-window") == 0);
    NautilusWindowSlot *slot = NULL;
    GFile *file;
    gint idx;

    DEBUG ("Open called on the GApplication instance; %d files", n_files);

    /* Open windows at each requested location. */
    for (idx = 0; idx < n_files; idx++)
    {
        file = files[idx];

        if (!force_new)
        {
            slot = get_window_slot_for_location (self, file);
        }

        if (!slot)
        {
            open_window (self, file);
        }
        else
        {
            /* We open the location again to update any possible selection */
            nautilus_application_open_location_full (NAUTILUS_APPLICATION (app), file, 0, NULL, NULL, slot);
        }
    }
}

static void
nautilus_application_finalize (GObject *object)
{
    NautilusApplication *self;
    NautilusApplicationPrivate *priv;

    self = NAUTILUS_APPLICATION (object);
    priv = nautilus_application_get_instance_private (self);

    g_clear_object (&priv->progress_handler);
    g_clear_object (&priv->bookmark_list);

    g_list_free (priv->windows);

    g_hash_table_destroy (priv->notifications);

    g_clear_object (&priv->undo_manager);

    g_clear_object (&priv->tag_manager);

    g_clear_object (&priv->dbus_launcher);

    G_OBJECT_CLASS (nautilus_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NautilusApplication *self,
                          GVariantDict        *options)
{
    gboolean retval = FALSE;

    if (g_variant_dict_contains (options, "check") &&
        (g_variant_dict_contains (options, G_OPTION_REMAINING) ||
         g_variant_dict_contains (options, "quit")))
    {
        g_printerr ("%s\n",
                    _("--check cannot be used with other options."));
        goto out;
    }

    if (g_variant_dict_contains (options, "quit") &&
        g_variant_dict_contains (options, G_OPTION_REMAINING))
    {
        g_printerr ("%s\n",
                    _("--quit cannot be used with URIs."));
        goto out;
    }


    if (g_variant_dict_contains (options, "select") &&
        !g_variant_dict_contains (options, G_OPTION_REMAINING))
    {
        g_printerr ("%s\n",
                    _("--select must be used with at least an URI."));
        goto out;
    }

    retval = TRUE;

out:
    return retval;
}

static int
do_perform_self_checks (void)
{
#ifndef NAUTILUS_OMIT_SELF_CHECK
    gtk_init ();

    nautilus_profile_start (NULL);
    /* Run the checks (each twice) for nautilus and libnautilus-private. */

    nautilus_run_self_checks ();
    nautilus_run_lib_self_checks ();
    eel_exit_if_self_checks_failed ();

    nautilus_run_self_checks ();
    nautilus_run_lib_self_checks ();
    eel_exit_if_self_checks_failed ();
    nautilus_profile_end (NULL);
#endif

    return EXIT_SUCCESS;
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
                                             NULL, NULL, NULL);
}

static void
action_clone_window (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
    NautilusWindowSlot *active_slot = NULL;
    NautilusWindow *active_window = NULL;
    GtkApplication *application = user_data;
    g_autoptr (GFile) location = NULL;
    NautilusView *current_view;

    active_window = NAUTILUS_WINDOW (gtk_application_get_active_window (application));
    active_slot = nautilus_window_get_active_slot (active_window);
    current_view = nautilus_window_slot_get_current_view (active_slot);

    if (current_view != NULL &&
        nautilus_view_is_searching (current_view))
    {
        location = g_file_new_for_path (g_get_home_dir ());
    }
    else
    {
        /* If the user happens to fall asleep while holding ctrl-n, or very
         * unfortunately opens a new window at a remote location, the current
         * location will be null, leading to criticals and/or failed assertions.
         *
         * Another sad thing is that checking if the view/slot is loading will
         * not work, as the loading process only really begins after the attributes
         * for the file have been fetched.
         */
        location = nautilus_window_slot_get_location (active_slot);
        if (location == NULL)
        {
            location = nautilus_window_slot_get_pending_location (active_slot);
        }
        g_object_ref (location);
    }

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (application), location,
                                             NAUTILUS_OPEN_FLAG_NEW_WINDOW, NULL, NULL, NULL);
}

static void
action_preferences (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    GtkApplication *application = user_data;
    nautilus_preferences_window_show (gtk_application_get_active_window (application));
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
    GtkWindow *window;
    GtkWidget *dialog;
    GtkApplication *application = user_data;
    GError *error = NULL;

    window = gtk_application_get_active_window (application);
    gtk_show_uri (window, "help:gnome-help/files", GDK_CURRENT_TIME);

    if (error)
    {
        dialog = adw_message_dialog_new (window ? GTK_WINDOW (window) : NULL,
                                         NULL, NULL);
        adw_message_dialog_format_heading (ADW_MESSAGE_DIALOG (dialog),
                                           _("There was an error displaying help: \n%s"),
                                           error->message);
        adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "ok", _("_OK"));
        adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "ok");

        gtk_window_present (GTK_WINDOW (dialog));
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

const static GActionEntry app_entries[] =
{
    { "new-window", action_new_window, NULL, NULL, NULL },
    { "clone-window", action_clone_window, NULL, NULL, NULL },
    { "preferences", action_preferences, NULL, NULL, NULL },
    { "about", action_about, NULL, NULL, NULL },
    { "help", action_help, NULL, NULL, NULL },
    { "quit", action_quit, NULL, NULL, NULL },
    { "kill", action_kill, NULL, NULL, NULL },
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

    DEBUG ("Calling activate");

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

            if (nautilus_is_search_directory (file))
            {
                g_autofree char *error_string = NULL;
                error_string = g_strdup_printf (_("“%s” is an internal protocol. "
                                                  "Opening this location directly is not supported."),
                                                EEL_SEARCH_URI);

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
    gint retval = -1;
    GVariantDict *options;

    nautilus_profile_start (NULL);

    options = g_application_command_line_get_options_dict (command_line);

    if (g_variant_dict_contains (options, "version"))
    {
        g_application_command_line_print (command_line,
                                          "GNOME nautilus " PACKAGE_VERSION "\n");
        retval = EXIT_SUCCESS;
        goto out;
    }

    if (!do_cmdline_sanity_checks (self, options))
    {
        retval = EXIT_FAILURE;
        goto out;
    }

    if (g_variant_dict_contains (options, "check"))
    {
        retval = do_perform_self_checks ();
        goto out;
    }

    if (g_variant_dict_contains (options, "quit"))
    {
        DEBUG ("Killing app, as requested");
        g_action_group_activate_action (G_ACTION_GROUP (application),
                                        "kill", NULL);
        goto out;
    }

    retval = nautilus_application_handle_file_args (self, options);

out:
    nautilus_profile_end (NULL);

    return retval;
}

static void
nautilus_application_init (NautilusApplication *self)
{
    static const GOptionEntry options[] =
    {
#ifndef NAUTILUS_OMIT_SELF_CHECK
        { "check", 'c', 0, G_OPTION_ARG_NONE, NULL,
          N_("Perform a quick set of self-check tests."), NULL },
#endif
        { "version", '\0', 0, G_OPTION_ARG_NONE, NULL,
          N_("Show the version of the program."), NULL },
        { "new-window", 'w', 0, G_OPTION_ARG_NONE, NULL,
          N_("Always open a new window for browsing specified URIs"), NULL },
        { "quit", 'q', 0, G_OPTION_ARG_NONE, NULL,
          N_("Quit Nautilus."), NULL },
        { "select", 's', 0, G_OPTION_ARG_NONE, NULL,
          N_("Select specified URI in parent folder."), NULL },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, N_("[URI…]") },

        /* The following are old options which have no effect anymore. We keep
         * them around for compatibility reasons, e.g. not breaking old scripts.
         */
        { "browser", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
          NULL, NULL },
        { "geometry", 'g', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, NULL,
          NULL, NULL },
        { "no-default-window", 'n', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
          NULL, NULL },
        { "no-desktop", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
          NULL, NULL },

        { NULL }
    };
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);

    priv->notifications = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);

    priv->undo_manager = nautilus_file_undo_manager_new ();
    priv->tag_manager = nautilus_tag_manager_new ();

    priv->dbus_launcher = nautilus_dbus_launcher_new ();

    nautilus_tracker_setup_miner_fs_connection ();

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
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);

    g_hash_table_add (priv->notifications, g_strdup (notification_id));
    g_application_send_notification (G_APPLICATION (self), notification_id, notification);
}

void
nautilus_application_withdraw_notification (NautilusApplication *self,
                                            const gchar         *notification_id)
{
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);
    if (!g_hash_table_contains (priv->notifications, notification_id))
    {
        return;
    }

    g_hash_table_remove (priv->notifications, notification_id);
    g_application_withdraw_notification (G_APPLICATION (self), notification_id);
}

static void
on_application_shutdown (GApplication *application,
                         gpointer      user_data)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (application);
    NautilusApplicationPrivate *priv;
    GList *notification_ids;
    GList *l;
    gchar *notification_id;

    priv = nautilus_application_get_instance_private (self);
    notification_ids = g_hash_table_get_keys (priv->notifications);
    for (l = notification_ids; l != NULL; l = l->next)
    {
        notification_id = l->data;

        g_application_withdraw_notification (application, notification_id);
    }

    g_list_free (notification_ids);

    nautilus_icon_info_clear_caches ();
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
nautilus_application_startup (GApplication *app)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);
    NautilusApplicationPrivate *priv;

    nautilus_profile_start (NULL);
    priv = nautilus_application_get_instance_private (self);

    g_application_set_resource_base_path (G_APPLICATION (self), "/org/gnome/nautilus");

    /* chain up to the GTK+ implementation early, so gtk_init()
     * is called for us.
     */
    G_APPLICATION_CLASS (nautilus_application_parent_class)->startup (G_APPLICATION (self));

    gtk_window_set_default_icon_name (APPLICATION_ID);

    /* initialize preferences and create the global GSettings objects */
    nautilus_global_preferences_init ();

    /* initialize nautilus modules */
    nautilus_profile_start ("Modules");
    nautilus_module_setup ();
    nautilus_profile_end ("Modules");

    /* attach menu-provider module callback */
    menu_provider_init_callback ();

    /* Initialize the UI handler singleton for file operations */
    priv->progress_handler = nautilus_progress_persistence_handler_new (G_OBJECT (self));

    /* Check the user's .nautilus directories and post warnings
     * if there are problems.
     */
    check_required_directories (self);

    nautilus_init_application_actions (self);

    if (g_strcmp0 (g_getenv ("RUNNING_TESTS"), "TRUE") != 0)
    {
        maybe_migrate_gtk_filechooser_preferences ();
        nautilus_tag_manager_maybe_migrate_tracker2_data (priv->tag_manager);
    }

    nautilus_profile_end (NULL);

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
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);
    priv->dbus_manager = nautilus_dbus_manager_new ();
    if (!nautilus_dbus_manager_register (priv->dbus_manager, connection, error))
    {
        return FALSE;
    }

    priv->fdb_manager = nautilus_freedesktop_dbus_new (connection);
    if (!nautilus_freedesktop_dbus_register (priv->fdb_manager, error))
    {
        return FALSE;
    }

    priv->search_provider = nautilus_shell_search_provider_new ();
    if (!nautilus_shell_search_provider_register (priv->search_provider, connection, error))
    {
        return FALSE;
    }

    priv->previewer_selection_id = nautilus_previewer_connect_selection_event (connection);

    return TRUE;
}

static void
nautilus_application_dbus_unregister (GApplication    *app,
                                      GDBusConnection *connection,
                                      const gchar     *object_path)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);
    if (priv->dbus_manager)
    {
        nautilus_dbus_manager_unregister (priv->dbus_manager);
        g_clear_object (&priv->dbus_manager);
    }

    if (priv->fdb_manager)
    {
        nautilus_freedesktop_dbus_unregister (priv->fdb_manager);
        g_clear_object (&priv->fdb_manager);
    }

    if (priv->search_provider)
    {
        nautilus_shell_search_provider_unregister (priv->search_provider);
        g_clear_object (&priv->search_provider);
    }

    if (priv->previewer_selection_id != 0)
    {
        nautilus_previewer_disconnect_selection_event (connection,
                                                       priv->previewer_selection_id);
        priv->previewer_selection_id = 0;
    }
}

static void
update_dbus_opened_locations (NautilusApplication *self)
{
    NautilusApplicationPrivate *priv;
    gint i;
    GList *l, *sl;
    GList *locations = NULL;
    gsize locations_size = 0;
    gchar **locations_array;
    NautilusWindow *window;
    GFile *location;
    const gchar *dbus_object_path = NULL;

    g_autoptr (GVariant) windows_to_locations = NULL;
    GVariantBuilder windows_to_locations_builder;

    g_return_if_fail (NAUTILUS_IS_APPLICATION (self));

    priv = nautilus_application_get_instance_private (self);
    dbus_object_path = g_application_get_dbus_object_path (G_APPLICATION (self));

    g_return_if_fail (dbus_object_path);

    g_variant_builder_init (&windows_to_locations_builder, G_VARIANT_TYPE ("a{sas}"));

    for (l = priv->windows; l != NULL; l = l->next)
    {
        guint32 id;
        g_autofree gchar *path = NULL;
        GVariantBuilder locations_in_window_builder;

        window = l->data;

        g_variant_builder_init (&locations_in_window_builder, G_VARIANT_TYPE ("as"));

        for (sl = nautilus_window_get_slots (window); sl; sl = sl->next)
        {
            NautilusWindowSlot *slot = sl->data;
            location = nautilus_window_slot_get_location (slot);

            if (location != NULL)
            {
                gchar *uri = g_file_get_uri (location);
                GList *found = g_list_find_custom (locations, uri, (GCompareFunc) g_strcmp0);

                g_variant_builder_add (&locations_in_window_builder, "s", uri);

                if (!found)
                {
                    locations = g_list_prepend (locations, uri);
                    ++locations_size;
                }
                else
                {
                    g_free (uri);
                }
            }
        }

        id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window));
        path = g_strdup_printf ("%s/window/%u", dbus_object_path, id);
        g_variant_builder_add (&windows_to_locations_builder, "{sas}", path, &locations_in_window_builder);
        g_variant_builder_clear (&locations_in_window_builder);
    }

    locations_array = g_new (gchar *, locations_size + 1);

    for (i = 0, l = locations; l; l = l->next, ++i)
    {
        /* We reuse the locations string locations saved on list */
        locations_array[i] = l->data;
    }

    locations_array[locations_size] = NULL;

    nautilus_freedesktop_dbus_set_open_locations (priv->fdb_manager,
                                                  (const gchar **) locations_array);

    windows_to_locations = g_variant_ref_sink (g_variant_builder_end (&windows_to_locations_builder));
    nautilus_freedesktop_dbus_set_open_windows_with_locations (priv->fdb_manager,
                                                               windows_to_locations);

    g_free (locations_array);
    g_list_free_full (locations, g_free);
}

static void
on_slot_location_changed (NautilusWindowSlot  *slot,
                          GParamSpec          *pspec,
                          NautilusApplication *self)
{
    update_dbus_opened_locations (self);
}

static void
on_slot_added (NautilusWindow      *window,
               NautilusWindowSlot  *slot,
               NautilusApplication *self)
{
    if (nautilus_window_slot_get_location (slot))
    {
        update_dbus_opened_locations (self);
    }

    g_signal_connect (slot, "notify::location", G_CALLBACK (on_slot_location_changed), self);
}

static void
on_slot_removed (NautilusWindow      *window,
                 NautilusWindowSlot  *slot,
                 NautilusApplication *self)
{
    update_dbus_opened_locations (self);

    g_signal_handlers_disconnect_by_func (slot, on_slot_location_changed, self);
}

static void
nautilus_application_window_added (GtkApplication *app,
                                   GtkWindow      *window)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);
    GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_added (app, window);

    if (NAUTILUS_IS_WINDOW (window))
    {
        priv->windows = g_list_prepend (priv->windows, window);
        g_signal_connect (window, "slot-added", G_CALLBACK (on_slot_added), app);
        g_signal_connect (window, "slot-removed", G_CALLBACK (on_slot_removed), app);
    }
}

static void
nautilus_application_window_removed (GtkApplication *app,
                                     GtkWindow      *window)
{
    NautilusApplication *self = NAUTILUS_APPLICATION (app);
    NautilusApplicationPrivate *priv;

    priv = nautilus_application_get_instance_private (self);

    GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_removed (app, window);

    if (NAUTILUS_IS_WINDOW (window))
    {
        priv->windows = g_list_remove_all (priv->windows, window);
        g_signal_handlers_disconnect_by_func (window, on_slot_added, app);
        g_signal_handlers_disconnect_by_func (window, on_slot_removed, app);
    }

    /* if this was the last window, close the previewer */
    if (g_list_length (priv->windows) == 0)
    {
        nautilus_previewer_call_close ();
        nautilus_progress_persistence_handler_make_persistent (priv->progress_handler);
    }
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

    class->open_location_full = real_open_location_full;

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
    g_autoptr (GFile) location = NULL;
    NautilusWindow *window;

    location = nautilus_query_get_location (query);
    window = open_window (self, location);
    nautilus_window_search (window, query);
}

gboolean
nautilus_application_is_sandboxed (void)
{
    static gboolean ret;

    static gsize init = 0;
    if (g_once_init_enter (&init))
    {
        ret = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
        g_once_init_leave (&init, 1);
    }

    return ret;
}
