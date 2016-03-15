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

#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel.h>
#include <gdk/gdkx.h>

struct _NautilusDesktopApplication
{
  NautilusApplication parent_instance;
};

G_DEFINE_TYPE (NautilusDesktopApplication, nautilus_desktop_application, NAUTILUS_TYPE_APPLICATION)


static void
open_location_full (NautilusApplication     *self,
                    GFile                   *location,
                    NautilusWindowOpenFlags  flags,
                    GList                   *selection,
                    NautilusWindow          *target_window,
                    NautilusWindowSlot      *target_slot)
{
  gchar *uri;

  uri = g_file_get_uri (location);
  if (eel_uri_is_desktop (uri) && target_window &&
      NAUTILUS_IS_DESKTOP_WINDOW (target_window))
    {
      nautilus_window_open_location_full (target_window, location, flags, selection, NULL);
    }
  else
    {
      g_warning ("other location, use dbus to communicate with nautilus. This process is only for the desktop\n");
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
  g_signal_connect_swapped (gnome_background_preferences, "changed::" NAUTILUS_PREFERENCES_SHOW_DESKTOP,
                            G_CALLBACK (update_desktop_from_gsettings),
                            self);
  update_desktop_from_gsettings (self);
}

static void
nautilus_desktop_application_activate (GApplication *app)
{
  /* Do nothing */
}

static void
nautilus_desktop_application_startup (GApplication *app)
{
  NautilusDesktopApplication *self = NAUTILUS_DESKTOP_APPLICATION (app);

  nautilus_application_startup_common (NAUTILUS_APPLICATION (app));
  init_desktop (self);
}

static void
nautilus_desktop_application_class_init (NautilusDesktopApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  NautilusApplicationClass *parent_class = NAUTILUS_APPLICATION_CLASS (klass);

  parent_class->open_location_full = open_location_full;

  application_class->startup = nautilus_desktop_application_startup;
  application_class->activate = nautilus_desktop_application_activate;
}

static void
nautilus_desktop_application_init (NautilusDesktopApplication *self)
{
}

NautilusDesktopApplication *
nautilus_desktop_application_new (void)
{
  return g_object_new (NAUTILUS_TYPE_DESKTOP_APPLICATION,
                       "application-id", "org.gnome.NautilusDesktop",
                        NULL);
}

