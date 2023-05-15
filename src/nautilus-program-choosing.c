/* nautilus-program-choosing.c - functions for selecting and activating
 *                                programs for opening/viewing particular files.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Author: John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "nautilus-program-choosing.h"

#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window.h"
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>

#include <gdk/gdk.h>

static void
add_file_to_recent (NautilusFile *file,
                    GAppInfo     *application)
{
    GtkRecentData recent_data;
    char *uri;

    uri = nautilus_file_get_activation_uri (file);
    if (uri == NULL)
    {
        uri = nautilus_file_get_uri (file);
    }

    /* do not add trash:// etc */
    if (eel_uri_is_trash (uri) ||
        eel_uri_is_search (uri) ||
        eel_uri_is_recent (uri))
    {
        g_free (uri);
        return;
    }

    recent_data.display_name = NULL;
    recent_data.description = NULL;

    recent_data.mime_type = nautilus_file_get_mime_type (file);
    recent_data.app_name = g_strdup (g_get_application_name ());
    recent_data.app_exec = g_strdup (g_app_info_get_commandline (application));

    recent_data.groups = NULL;
    recent_data.is_private = FALSE;

    gtk_recent_manager_add_full (gtk_recent_manager_get_default (),
                                 uri, &recent_data);

    g_free (recent_data.mime_type);
    g_free (recent_data.app_name);
    g_free (recent_data.app_exec);

    g_free (uri);
}
void
nautilus_launch_application_for_mount (GAppInfo  *app_info,
                                       GMount    *mount,
                                       GtkWindow *parent_window)
{
    GFile *root;
    NautilusFile *file;
    GList *files;

    root = g_mount_get_root (mount);
    file = nautilus_file_get (root);
    g_object_unref (root);

    files = g_list_append (NULL, file);
    nautilus_launch_application (app_info,
                                 files,
                                 parent_window);

    g_list_free_full (files, (GDestroyNotify) nautilus_file_unref);
}

/**
 * nautilus_launch_application:
 *
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs.
 *
 * @application: The application to be launched.
 * @uris: The files whose locations should be passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
nautilus_launch_application (GAppInfo  *application,
                             GList     *files,
                             GtkWindow *parent_window)
{
    GList *uris, *l;

    uris = NULL;
    for (l = files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, nautilus_file_get_activation_uri (l->data));
    }
    uris = g_list_reverse (uris);
    nautilus_launch_application_by_uri (application, uris,
                                        parent_window);
    g_list_free_full (uris, g_free);
}

static GdkAppLaunchContext *
get_launch_context (GtkWindow *parent_window)
{
    GdkDisplay *display;
    GdkAppLaunchContext *launch_context;

    if (parent_window != NULL)
    {
        display = gtk_widget_get_display (GTK_WIDGET (parent_window));
    }
    else
    {
        display = gdk_display_get_default ();
    }

    launch_context = gdk_display_get_app_launch_context (display);

    return launch_context;
}

void
nautilus_launch_application_by_uri (GAppInfo  *application,
                                    GList     *uris,
                                    GtkWindow *parent_window)
{
    char *uri;
    GList *locations, *l;
    GFile *location;
    NautilusFile *file;
    gboolean result;
    GError *error;
    g_autoptr (GdkAppLaunchContext) launch_context = NULL;
    NautilusIconInfo *icon;
    int count, total;

    g_assert (uris != NULL);

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length (uris);
    locations = NULL;
    for (l = uris; l != NULL; l = l->next)
    {
        uri = l->data;

        location = g_file_new_for_uri (uri);
        if (g_file_is_native (location))
        {
            count++;
        }
        locations = g_list_prepend (locations, location);
    }
    locations = g_list_reverse (locations);

    launch_context = get_launch_context (parent_window);

    file = nautilus_file_get_by_uri (uris->data);
    icon = nautilus_file_get_icon (file,
                                   48, gtk_widget_get_scale_factor (GTK_WIDGET (parent_window)),
                                   0);
    nautilus_file_unref (file);
    if (icon)
    {
        gdk_app_launch_context_set_icon_name (launch_context,
                                              nautilus_icon_info_get_used_name (icon));
        g_object_unref (icon);
    }

    error = NULL;

    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        result = g_app_info_launch (application,
                                    locations,
                                    G_APP_LAUNCH_CONTEXT (launch_context),
                                    &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        result = g_app_info_launch_uris (application,
                                         uris,
                                         G_APP_LAUNCH_CONTEXT (launch_context),
                                         &error);
    }

    if (result)
    {
        for (l = uris; l != NULL; l = l->next)
        {
            file = nautilus_file_get_by_uri (l->data);
            add_file_to_recent (file, application);
            nautilus_file_unref (file);
        }
    }

    g_list_free_full (locations, g_object_unref);
}

static void
launch_application_from_command_internal (const gchar *full_command,
                                          GdkDisplay  *display,
                                          gboolean     use_terminal)
{
    GAppInfoCreateFlags flags;
    g_autoptr (GError) error = NULL;
    g_autoptr (GAppInfo) app = NULL;

    flags = G_APP_INFO_CREATE_NONE;
    if (use_terminal)
    {
        flags = G_APP_INFO_CREATE_NEEDS_TERMINAL;
    }

    app = g_app_info_create_from_commandline (full_command, NULL, flags, &error);
    if (app != NULL && !(use_terminal && display == NULL))
    {
        g_autoptr (GdkAppLaunchContext) context = NULL;

        context = gdk_display_get_app_launch_context (display);

        g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (context), &error);
    }

    if (error != NULL)
    {
        g_message ("Could not start app: %s", error->message);
    }
}

/**
 * nautilus_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @...: Passed as parameters to the application after quoting each of them.
 */
void
nautilus_launch_application_from_command (GdkDisplay *display,
                                          const char *command_string,
                                          gboolean    use_terminal,
                                          ...)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    char *parameter;
    va_list ap;

    full_command = g_strdup (command_string);

    va_start (ap, use_terminal);

    while ((parameter = va_arg (ap, char *)) != NULL)
    {
        quoted_parameter = g_shell_quote (parameter);
        tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
        g_free (quoted_parameter);

        g_free (full_command);
        full_command = tmp;
    }

    va_end (ap);

    launch_application_from_command_internal (full_command, display, use_terminal);

    g_free (full_command);
}

/**
 * nautilus_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @parameters: Passed as parameters to the application after quoting each of them.
 */
void
nautilus_launch_application_from_command_array (GdkDisplay         *display,
                                                const char         *command_string,
                                                gboolean            use_terminal,
                                                const char * const *parameters)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    const char * const *p;

    full_command = g_strdup (command_string);

    if (parameters != NULL)
    {
        for (p = parameters; *p != NULL; p++)
        {
            quoted_parameter = g_shell_quote (*p);
            tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
            g_free (quoted_parameter);

            g_free (full_command);
            full_command = tmp;
        }
    }

    launch_application_from_command_internal (full_command, display, use_terminal);

    g_free (full_command);
}

void
nautilus_launch_desktop_file (const char  *desktop_file_uri,
                              const GList *parameter_uris,
                              GtkWindow   *parent_window)
{
    GError *error;
    char *message, *desktop_file_path;
    const GList *p;
    GList *files;
    int total, count;
    GFile *file, *desktop_file;
    GDesktopAppInfo *app_info;
    GdkAppLaunchContext *context;

    /* Don't allow command execution from remote locations
     * to partially mitigate the security
     * risk of executing arbitrary commands.
     */
    desktop_file = g_file_new_for_uri (desktop_file_uri);
    desktop_file_path = g_file_get_path (desktop_file);
    if (!g_file_is_native (desktop_file))
    {
        g_free (desktop_file_path);
        g_object_unref (desktop_file);
        show_dialog (_("Sorry, but you cannot execute commands from a remote site."),
                     _("This is disabled due to security considerations."),
                     parent_window,
                     GTK_MESSAGE_ERROR);

        return;
    }
    g_object_unref (desktop_file);

    app_info = g_desktop_app_info_new_from_filename (desktop_file_path);
    g_free (desktop_file_path);
    if (app_info == NULL)
    {
        show_dialog (_("There was an error launching the app."),
                     NULL,
                     parent_window,
                     GTK_MESSAGE_ERROR);
        return;
    }

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length ((GList *) parameter_uris);
    files = NULL;
    for (p = parameter_uris; p != NULL; p = p->next)
    {
        file = g_file_new_for_uri ((const char *) p->data);
        if (g_file_is_native (file))
        {
            count++;
        }
        files = g_list_prepend (files, file);
    }

    /* check if this app only supports local files */
    if (g_app_info_supports_files (G_APP_INFO (app_info)) &&
        !g_app_info_supports_uris (G_APP_INFO (app_info)) &&
        parameter_uris != NULL)
    {
        if (count == 0)
        {
            /* all files are non-local */
            show_dialog (_("This drop target only supports local files."),
                         _("To open non-local files copy them to a local folder and then drop them again."),
                         parent_window,
                         GTK_MESSAGE_ERROR);

            g_list_free_full (files, g_object_unref);
            g_object_unref (app_info);
            return;
        }
        else if (count != total)
        {
            /* some files are non-local */
            show_dialog (_("This drop target only supports local files."),
                         _("To open non-local files copy them to a local folder and then"
                           " drop them again. The local files you dropped have already been opened."),
                         parent_window,
                         GTK_MESSAGE_WARNING);
        }
    }

    error = NULL;
    context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (parent_window)));
    /* TODO: Ideally we should accept a timestamp here instead of using GDK_CURRENT_TIME */
    gdk_app_launch_context_set_timestamp (context, GDK_CURRENT_TIME);
    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        g_app_info_launch (G_APP_INFO (app_info),
                           files,
                           G_APP_LAUNCH_CONTEXT (context),
                           &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        g_app_info_launch_uris (G_APP_INFO (app_info),
                                (GList *) parameter_uris,
                                G_APP_LAUNCH_CONTEXT (context),
                                &error);
    }
    if (error != NULL)
    {
        message = g_strconcat (_("Details: "), error->message, NULL);
        show_dialog (_("There was an error launching the app."),
                     message,
                     parent_window,
                     GTK_MESSAGE_ERROR);

        g_error_free (error);
        g_free (message);
    }

    g_list_free_full (files, g_object_unref);
    g_object_unref (context);
    g_object_unref (app_info);
}
