/* eel-gtk-extensions.c - implementation of new functions that operate on
 *                         gtk classes. Perhaps some of these should be
 *                         rolled into gtk someday.
 *
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Authors: John Sullivan <sullivan@eazel.com>
 *           Ramiro Estrugo <ramiro@eazel.com>
 *           Darin Adler <darin@eazel.com>
 */

#include <config.h>
#include "eel-gtk-extensions.h"

#include "eel-glib-extensions.h"
#include "eel-string.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <math.h>

/* This number is fairly arbitrary. Long enough to show a pretty long
 * menu title, but not so long to make a menu grotesquely wide.
 */
#define MAXIMUM_MENU_TITLE_LENGTH       48

/* Used for window position & size sanity-checking. The sizes are big enough to prevent
 * at least normal-sized gnome panels from obscuring the window at the screen edges.
 */
#define MINIMUM_ON_SCREEN_WIDTH         100
#define MINIMUM_ON_SCREEN_HEIGHT        100


/**
 * eel_gtk_window_get_geometry_string:
 * @window: a #GtkWindow
 *
 * Obtains the geometry string for this window, suitable for
 * set_geometry_string(); assumes the window has NorthWest gravity
 *
 * Return value: geometry string, must be freed
 **/
char *
eel_gtk_window_get_geometry_string (GtkWindow *window)
{
    char *str;
    int w, h, x, y;

    g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
    g_return_val_if_fail (gtk_window_get_gravity (window) ==
                          GDK_GRAVITY_NORTH_WEST, NULL);

    gtk_window_get_position (window, &x, &y);
    gtk_window_get_size (window, &w, &h);

    str = g_strdup_printf ("%dx%d+%d+%d", w, h, x, y);

    return str;
}

GtkMenuItem *
eel_gtk_menu_append_separator (GtkMenu *menu)
{
    return eel_gtk_menu_insert_separator (menu, -1);
}

GtkMenuItem *
eel_gtk_menu_insert_separator (GtkMenu *menu,
                               int      index)
{
    GtkWidget *menu_item;

    menu_item = gtk_separator_menu_item_new ();
    gtk_widget_show (menu_item);
    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menu_item, index);

    return GTK_MENU_ITEM (menu_item);
}
