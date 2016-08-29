/* nautilus-desktop-application.c
 *
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "nautilus-desktop-application.h"
#include "nautilus-desktop-window.h"
#include "nautilus-desktop-directory.h"
#include "nautilus-file-utilities.h"

#include "nautilus-freedesktop-generated.h"

#include <src/nautilus-global-preferences.h>
#include <eel/eel.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <glib/gi18n.h>

static NautilusFreedesktopFileManager1 *freedesktop_proxy = NULL;

struct _NautilusDesktopApplication
{
    NautilusApplication parent_instance;

    gboolean force;
    GCancellable *freedesktop_cancellable;
};

G_DEFINE_TYPE (NautilusDesktopApplication, nautilus_desktop_application, NAUTILUS_TYPE_APPLICATION)

static void
on_show_folders (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
    GError *error = NULL;

    nautilus_freedesktop_file_manager1_call_show_items_finish (freedesktop_proxy,
                                                               res,
                                                               &error);
    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        g_warning ("Unable to show items with File Manager freedesktop proxy: %s", error->message);
    }

    g_clear_error (&error);
}

static void
open_location_on_dbus (NautilusDesktopApplication *self,
                       const gchar                *uri)
{
    const gchar *uris[] = { uri, NULL };

    nautilus_freedesktop_file_manager1_call_show_folders (freedesktop_proxy,
                                                          uris,
                                                          "",
                                                          self->freedesktop_cancellable,
                                                          on_show_folders,
                                                          self);
}

static void
open_location_full (NautilusApplication     *app,
                    GFile                   *location,
                    NautilusWindowOpenFlags  flags,
                    GList                   *selection,
                    NautilusWindow          *target_window,
                    NautilusWindowSlot      *target_slot)
{
    NautilusDesktopApplication *self = NAUTILUS_DESKTOP_APPLICATION (app);
    gchar *uri;

    uri = g_file_get_uri (location);
    if (eel_uri_is_desktop (uri) && target_window &&
        NAUTILUS_IS_DESKTOP_WINDOW (target_window))
    {
        nautilus_window_open_location_full (target_window, location, flags, selection, NULL);
    }
    else
    {
        if (freedesktop_proxy)
        {
            open_location_on_dbus (self, uri);
        }
        else
        {
            g_warning ("cannot open folder on desktop, freedesktop bus not ready\n");
        }
    }

    g_free (uri);
}

static void
nautilus_application_set_desktop_visible (NautilusDesktopApplication *self,
                                          gboolean                    visible)
{
    GtkWidget *desktop_window;

    if (visible)
    {
        nautilus_desktop_window_ensure ();
    }
    else
    {
        desktop_window = nautilus_desktop_window_get ();
        if (desktop_window != NULL)
        {
            gtk_widget_destroy (desktop_window);
        }
    }
}

static void
update_desktop_from_gsettings (NautilusDesktopApplication *self)
{
    GdkDisplay *display;
    gboolean visible;

#ifdef GDK_WINDOWING_X11
    display = gdk_display_get_default ();
    visible = g_settings_get_boolean (gnome_background_preferences,
                                      NAUTILUS_PREFERENCES_SHOW_DESKTOP);
    visible = visible || self->force;

    if (!GDK_IS_X11_DISPLAY (display))
    {
        if (visible)
        {
            g_warning ("Desktop icons only supported on X11. Desktop not created");
        }

        return;
    }

    nautilus_application_set_desktop_visible (self, visible);

    return;
#endif

    g_warning ("Desktop icons only supported on X11. Desktop not created");
}

static void
init_desktop (NautilusDesktopApplication *self)
{
    if (!self->force)
    {
        g_signal_connect_swapped (gnome_background_preferences, "changed::" NAUTILUS_PREFERENCES_SHOW_DESKTOP,
                                  G_CALLBACK (update_desktop_from_gsettings),
                                  self);
    }
    update_desktop_from_gsettings (self);
}

static void
nautilus_desktop_application_activate (GApplication *app)
{
    /* Do nothing */
}

static gint
nautilus_desktop_application_command_line (GApplication            *application,
                                           GApplicationCommandLine *command_line)
{
    NautilusDesktopApplication *self = NAUTILUS_DESKTOP_APPLICATION (application);
    GVariantDict *options;

    options = g_application_command_line_get_options_dict (command_line);

    if (g_variant_dict_contains (options, "force"))
    {
        self->force = TRUE;
    }

    init_desktop (self);

    return EXIT_SUCCESS;
}

static void
nautilus_desktop_application_startup (GApplication *app)
{
    NautilusDesktopApplication *self = NAUTILUS_DESKTOP_APPLICATION (app);
    GError *error = NULL;

    nautilus_application_startup_common (NAUTILUS_APPLICATION (app));
    self->freedesktop_cancellable = g_cancellable_new ();
    freedesktop_proxy = nautilus_freedesktop_file_manager1_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                                                   "org.freedesktop.FileManager1",
                                                                                   "/org/freedesktop/FileManager1",
                                                                                   self->freedesktop_cancellable,
                                                                                   &error);

    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        g_warning ("Unable to create File Manager freedesktop proxy: %s", error->message);
    }

    g_clear_error (&error);
}

static void
nautilus_desktop_application_dispose (GObject *object)
{
    NautilusDesktopApplication *self = NAUTILUS_DESKTOP_APPLICATION (object);

    g_clear_object (&self->freedesktop_cancellable);


    G_OBJECT_CLASS (nautilus_desktop_application_parent_class)->dispose (object);
}

static void
nautilus_desktop_application_class_init (NautilusDesktopApplicationClass *klass)
{
    GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    NautilusApplicationClass *parent_class = NAUTILUS_APPLICATION_CLASS (klass);

    parent_class->open_location_full = open_location_full;

    application_class->startup = nautilus_desktop_application_startup;
    application_class->activate = nautilus_desktop_application_activate;
    application_class->command_line = nautilus_desktop_application_command_line;

    gobject_class->dispose = nautilus_desktop_application_dispose;
}

static void
nautilus_desktop_ensure_builtins (void)
{
    /* Ensure the type so it can be registered early as a directory extension provider*/
    g_type_ensure (NAUTILUS_TYPE_DESKTOP_DIRECTORY);
}

const GOptionEntry desktop_options[] =
{
    { "force", '\0', 0, G_OPTION_ARG_NONE, NULL,
      N_("Always manage the desktop (ignore the GSettings preference)."), NULL },
    { NULL }
};

static void
nautilus_desktop_application_init (NautilusDesktopApplication *self)
{
    self->force = FALSE;

    g_application_add_main_option_entries (G_APPLICATION (self), desktop_options);
    nautilus_ensure_extension_points ();
    nautilus_ensure_extension_builtins ();
    nautilus_desktop_ensure_builtins ();
}

NautilusDesktopApplication *
nautilus_desktop_application_new (void)
{
    return g_object_new (NAUTILUS_TYPE_DESKTOP_APPLICATION,
                         "application-id", "org.gnome.NautilusDesktop",
                         "register-session", TRUE,
                         "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                         NULL);
}
