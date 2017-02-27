/* nautilus-desktop-canvas-view.c - implementation of canvas view for managing the desktop.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.mou
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
 *  Authors: Mike Engber <engber@eazel.com>
 *           Gene Z. Ragan <gzr@eazel.com>
 *           Miguel de Icaza <miguel@ximian.com>
 */

#include <config.h>
#include <stdlib.h>

#include "nautilus-desktop-canvas-view.h"

#include "nautilus-desktop-canvas-view-container.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-desktop-directory.h"

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <src/nautilus-directory-notify.h>
#include <src/nautilus-file-changes-queue.h>
#include <src/nautilus-file-operations.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-ui-utilities.h>
#include <src/nautilus-global-preferences.h>
#include <src/nautilus-link.h>
#include <src/nautilus-metadata.h>
#include <src/nautilus-monitor.h>
#include <src/nautilus-program-choosing.h>
#include <src/nautilus-trash-monitor.h>
#include <src/nautilus-files-view.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct NautilusDesktopCanvasViewDetails
{
    GdkWindow *root_window;
};

static void     default_zoom_level_changed (gpointer user_data);
static void     real_update_context_menus (NautilusFilesView *view);
static char *real_get_backing_uri (NautilusFilesView *view);
static void     real_check_empty_states (NautilusFilesView *view);
static void     nautilus_desktop_canvas_view_update_canvas_container_fonts (NautilusDesktopCanvasView *view);
static void     font_changed_callback (gpointer callback_data);

G_DEFINE_TYPE (NautilusDesktopCanvasView, nautilus_desktop_canvas_view, NAUTILUS_TYPE_CANVAS_VIEW)

static char *desktop_directory;

#define get_canvas_container(w) nautilus_canvas_view_get_canvas_container (NAUTILUS_CANVAS_VIEW (w))

#define POPUP_PATH_CANVAS_APPEARANCE            "/selection/Canvas Appearance Items"

static void
canvas_container_set_workarea (NautilusCanvasContainer *canvas_container,
                               GdkScreen               *screen,
                               long                    *workareas,
                               int                      n_items)
{
    int left, right, top, bottom;
    int screen_width, screen_height;
    int scale;
    int i;

    left = right = top = bottom = 0;

    screen_width = gdk_screen_get_width (screen);
    screen_height = gdk_screen_get_height (screen);

    scale = gdk_window_get_scale_factor (gdk_screen_get_root_window (screen));
    scale = scale ? scale : 1;

    for (i = 0; i < n_items; i += 4)
    {
        int x = workareas [i] / scale;
        int y = workareas [i + 1] / scale;
        int width = workareas [i + 2] / scale;
        int height = workareas [i + 3] / scale;

        if ((x + width) > screen_width || (y + height) > screen_height)
        {
            continue;
        }

        left = MAX (left, x);
        right = MAX (right, screen_width - width - x);
        top = MAX (top, y);
        bottom = MAX (bottom, screen_height - height - y);
    }

    nautilus_canvas_container_set_margins (canvas_container,
                                           left, right, top, bottom);
}

static void
net_workarea_changed (NautilusDesktopCanvasView *canvas_view,
                      GdkWindow                 *window)
{
    long *nworkareas = NULL;
    long *workareas = NULL;
    GdkAtom type_returned;
    int format_returned;
    int length_returned;
    NautilusCanvasContainer *canvas_container;
    GdkScreen *screen;

    g_return_if_fail (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (canvas_view));

    canvas_container = get_canvas_container (canvas_view);

    /* Find the number of desktops so we know how long the
     * workareas array is going to be (each desktop will have four
     * elements in the workareas array describing
     * x,y,width,height) */
    gdk_error_trap_push ();
    if (!gdk_property_get (window,
                           gdk_atom_intern ("_NET_NUMBER_OF_DESKTOPS", FALSE),
                           gdk_x11_xatom_to_atom (XA_CARDINAL),
                           0, 4, FALSE,
                           &type_returned,
                           &format_returned,
                           &length_returned,
                           (guchar **) &nworkareas))
    {
        g_warning ("Can not calculate _NET_NUMBER_OF_DESKTOPS");
    }
    if (gdk_error_trap_pop ()
        || nworkareas == NULL
        || type_returned != gdk_x11_xatom_to_atom (XA_CARDINAL)
        || format_returned != 32)
    {
        g_warning ("Can not calculate _NET_NUMBER_OF_DESKTOPS");
    }

    /* Note : gdk_property_get() is broken (API documents admit
     * this).  As a length argument, it expects the number of
     * _bytes_ of data you require.  Internally, gdk_property_get
     * converts that value to a count of 32 bit (4 byte) elements.
     * However, the length returned is in bytes, but is calculated
     * via the count of returned elements * sizeof(long).  This
     * means on a 64 bit system, the number of bytes you have to
     * request does not correspond to the number of bytes you get
     * back, and is the reason for the workaround below.
     */
    gdk_error_trap_push ();
    if (nworkareas == NULL || (*nworkareas < 1)
        || !gdk_property_get (window,
                              gdk_atom_intern ("_NET_WORKAREA", FALSE),
                              gdk_x11_xatom_to_atom (XA_CARDINAL),
                              0, ((*nworkareas) * 4 * 4), FALSE,
                              &type_returned,
                              &format_returned,
                              &length_returned,
                              (guchar **) &workareas))
    {
        g_warning ("Can not get _NET_WORKAREA");
        workareas = NULL;
    }

    if (gdk_error_trap_pop ()
        || workareas == NULL
        || type_returned != gdk_x11_xatom_to_atom (XA_CARDINAL)
        || ((*nworkareas) * 4 * sizeof (long)) != length_returned
        || format_returned != 32)
    {
        g_warning ("Can not determine workarea, guessing at layout");
        nautilus_canvas_container_set_margins (canvas_container,
                                               0, 0, 0, 0);
    }
    else
    {
        screen = gdk_window_get_screen (window);

        canvas_container_set_workarea
            (canvas_container, screen, workareas, length_returned / sizeof (long));
    }

    if (nworkareas != NULL)
    {
        g_free (nworkareas);
    }

    if (workareas != NULL)
    {
        g_free (workareas);
    }
}

static GdkFilterReturn
desktop_canvas_view_property_filter (GdkXEvent *gdk_xevent,
                                     GdkEvent  *event,
                                     gpointer   data)
{
    XEvent *xevent = gdk_xevent;
    NautilusDesktopCanvasView *canvas_view;

    canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (data);

    switch (xevent->type)
    {
        case PropertyNotify:
        {
            if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA"))
            {
                net_workarea_changed (canvas_view, event->any.window);
            }
        }
        break;

        default:
        {
        }
        break;
    }

    return GDK_FILTER_CONTINUE;
}

static guint
real_get_id (NautilusFilesView *view)
{
    return NAUTILUS_VIEW_DESKTOP_ID;
}

static void
nautilus_desktop_canvas_view_dispose (GObject *object)
{
    NautilusDesktopCanvasView *canvas_view;

    canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (object);

    g_signal_handlers_disconnect_by_func (nautilus_icon_view_preferences,
                                          default_zoom_level_changed,
                                          canvas_view);
    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          font_changed_callback,
                                          canvas_view);
    g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
                                          nautilus_files_view_update_context_menus,
                                          canvas_view);

    G_OBJECT_CLASS (nautilus_desktop_canvas_view_parent_class)->dispose (object);
}

static void
nautilus_desktop_canvas_view_end_loading (NautilusFilesView *view,
                                          gboolean           all_files_seen)
{
    gboolean needs_reorganization;
    gchar *stored_size_icon;
    guint current_zoom;
    guint current_icon_size;
    gchar *current_icon_size_string;
    NautilusFile *file;

    NAUTILUS_FILES_VIEW_CLASS (nautilus_desktop_canvas_view_parent_class)->end_loading (view, all_files_seen);

    if (!all_files_seen)
    {
        return;
    }

    file = nautilus_files_view_get_directory_as_file (view);
    g_return_if_fail (file != NULL);

    stored_size_icon = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_DESKTOP_ICON_SIZE, NULL);
    current_zoom = nautilus_canvas_container_get_zoom_level (get_canvas_container (view));
    current_icon_size = nautilus_canvas_container_get_icon_size_for_zoom_level (current_zoom);
    needs_reorganization = stored_size_icon == NULL || atoi (stored_size_icon) != current_icon_size;

    if (needs_reorganization)
    {
        current_icon_size_string = g_strdup_printf ("%d", current_icon_size);
        nautilus_canvas_view_clean_up_by_name (NAUTILUS_CANVAS_VIEW (view));
        nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_DESKTOP_ICON_SIZE,
                                    NULL, current_icon_size_string);

        g_free (current_icon_size_string);
    }

    g_free (stored_size_icon);
}

static NautilusCanvasContainer *
real_create_canvas_container (NautilusCanvasView *canvas_view)
{
    return NAUTILUS_CANVAS_CONTAINER (nautilus_desktop_canvas_view_container_new ());
}

static void
nautilus_desktop_canvas_view_class_init (NautilusDesktopCanvasViewClass *class)
{
    NautilusFilesViewClass *vclass;
    NautilusCanvasViewClass *canvas_class;

    vclass = NAUTILUS_FILES_VIEW_CLASS (class);
    canvas_class = NAUTILUS_CANVAS_VIEW_CLASS (class);


    G_OBJECT_CLASS (class)->dispose = nautilus_desktop_canvas_view_dispose;

    canvas_class->create_canvas_container = real_create_canvas_container;

    vclass->update_context_menus = real_update_context_menus;
    vclass->get_view_id = real_get_id;
    vclass->end_loading = nautilus_desktop_canvas_view_end_loading;
    vclass->get_backing_uri = real_get_backing_uri;
    vclass->check_empty_states = real_check_empty_states;

    g_type_class_add_private (class, sizeof (NautilusDesktopCanvasViewDetails));
}

static void
unrealized_callback (GtkWidget                 *widget,
                     NautilusDesktopCanvasView *desktop_canvas_view)
{
    g_return_if_fail (desktop_canvas_view->details->root_window != NULL);

    /* Remove the property filter */
    gdk_window_remove_filter (desktop_canvas_view->details->root_window,
                              desktop_canvas_view_property_filter,
                              desktop_canvas_view);
    desktop_canvas_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget                 *widget,
                   NautilusDesktopCanvasView *desktop_canvas_view)
{
    GdkWindow *root_window;
    GdkScreen *screen;

    g_return_if_fail (desktop_canvas_view->details->root_window == NULL);

    screen = gtk_widget_get_screen (widget);
    root_window = gdk_screen_get_root_window (screen);

    desktop_canvas_view->details->root_window = root_window;

    /* Read out the workarea geometry and update the icon container accordingly */
    net_workarea_changed (desktop_canvas_view, root_window);

    /* Setup the property filter */
    gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter (root_window,
                           desktop_canvas_view_property_filter,
                           desktop_canvas_view);
}

static void
desktop_canvas_container_realize (GtkWidget                 *widget,
                                  NautilusDesktopCanvasView *desktop_canvas_view)
{
    GdkWindow *bin_window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    gdk_window_set_background_rgba (bin_window, &transparent);
}

static NautilusCanvasZoomLevel
get_default_zoom_level (void)
{
    NautilusCanvasZoomLevel default_zoom_level;

    default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
                                              NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

    return CLAMP (default_zoom_level, NAUTILUS_CANVAS_ZOOM_LEVEL_SMALL, NAUTILUS_CANVAS_ZOOM_LEVEL_LARGE);
}

static void
set_up_zoom_level (NautilusDesktopCanvasView *desktop_canvas_view)
{
    NautilusCanvasZoomLevel new_level;

    new_level = get_default_zoom_level ();
    nautilus_canvas_container_set_zoom_level (get_canvas_container (desktop_canvas_view),
                                              new_level);
}

static void
default_zoom_level_changed (gpointer user_data)
{
    NautilusCanvasZoomLevel new_level;
    NautilusDesktopCanvasView *desktop_canvas_view;
    gint new_icon_size;
    NautilusFile *file;
    gchar *new_icon_size_string;

    desktop_canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (user_data);
    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (user_data));
    new_level = get_default_zoom_level ();
    new_icon_size = nautilus_canvas_container_get_icon_size_for_zoom_level (new_level);
    new_icon_size_string = g_strdup_printf ("%d", new_icon_size);

    nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_DESKTOP_ICON_SIZE,
                                NULL, new_icon_size_string);
    set_up_zoom_level (desktop_canvas_view);

    g_free (new_icon_size_string);
}

static void
font_changed_callback (gpointer callback_data)
{
    g_return_if_fail (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (callback_data));

    nautilus_desktop_canvas_view_update_canvas_container_fonts (NAUTILUS_DESKTOP_CANVAS_VIEW (callback_data));
}

static void
nautilus_desktop_canvas_view_update_canvas_container_fonts (NautilusDesktopCanvasView *canvas_view)
{
    NautilusCanvasContainer *canvas_container;
    char *font;

    canvas_container = get_canvas_container (canvas_view);
    g_assert (canvas_container != NULL);

    font = g_settings_get_string (nautilus_desktop_preferences,
                                  NAUTILUS_PREFERENCES_DESKTOP_FONT);

    nautilus_canvas_container_set_font (canvas_container, font);

    g_free (font);
}

static const gchar *
get_control_center_command (const gchar **params_out)
{
    gchar *path;
    const gchar *retval;
    const gchar *params;
    const gchar *xdg_current_desktop;
    gchar **desktop_names;
    gboolean is_unity;
    int i;

    path = NULL;
    retval = NULL;
    params = NULL;

    xdg_current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

    /* Detect the Unity-based environments */
    is_unity = FALSE;
    if (xdg_current_desktop != NULL)
    {
        desktop_names = g_strsplit (xdg_current_desktop, ":", 0);
        for (i = 0; desktop_names[i]; ++i)
        {
            if (!g_strcmp0 (desktop_names[i], "Unity"))
            {
                is_unity = TRUE;
                break;
            }
        }
        g_strfreev (desktop_names);
    }

    /* In Unity look for unity-control-center */
    if (is_unity)
    {
        path = g_find_program_in_path ("unity-control-center");
        if (path != NULL)
        {
            retval = "unity-control-center";
            params = "appearance";
            goto out;
        }
    }

    /* Otherwise look for gnome-control-center */
    path = g_find_program_in_path ("gnome-control-center");
    if (path != NULL)
    {
        retval = "gnome-control-center";
        params = "background";
    }

out:
    g_free (path);
    if (params_out != NULL)
    {
        *params_out = params;
    }

    return retval;
}

static void
action_change_background (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
    const gchar *control_center_cmd, *params;

    g_assert (NAUTILUS_FILES_VIEW (user_data));

    control_center_cmd = get_control_center_command (&params);
    if (control_center_cmd == NULL)
    {
        return;
    }

    nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (user_data)),
                                              control_center_cmd,
                                              FALSE,
                                              params, NULL);
}

static void
action_empty_trash (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    nautilus_file_operations_empty_trash (GTK_WIDGET (user_data));
}

static void
action_stretch (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    nautilus_canvas_container_show_stretch_handles
        (get_canvas_container (user_data));
}

static void
action_unstretch (GSimpleAction *action,
                  GVariant      *state,
                  gpointer       user_data)
{
    nautilus_canvas_container_unstretch (get_canvas_container (user_data));
}

static void
action_organize_desktop_by_name (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    nautilus_canvas_view_clean_up_by_name (NAUTILUS_CANVAS_VIEW (user_data));
}

static gboolean
trash_link_is_selection (NautilusFilesView *view)
{
    GList *selection;
    NautilusDesktopLink *link;
    gboolean result;

    result = FALSE;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    if ((g_list_length (selection) == 1) &&
        NAUTILUS_IS_DESKTOP_ICON_FILE (selection->data))
    {
        link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (selection->data));
        /* link may be NULL if the link was recently removed (unmounted) */
        if (link != NULL &&
            nautilus_desktop_link_get_link_type (link) == NAUTILUS_DESKTOP_LINK_TRASH)
        {
            result = TRUE;
        }
        if (link)
        {
            g_object_unref (link);
        }
    }

    nautilus_file_list_free (selection);

    return result;
}

const GActionEntry desktop_view_entries[] =
{
    { "change-background", action_change_background },
    { "organize-desktop-by-name", action_organize_desktop_by_name },
    { "empty-trash", action_empty_trash },
    { "stretch", action_stretch },
    { "unstretch", action_unstretch },
};

static void
real_check_empty_states (NautilusFilesView *view)
{
    /* Do nothing */
}

static char *
real_get_backing_uri (NautilusFilesView *view)
{
    gchar *uri;
    NautilusDirectory *directory;
    NautilusDirectory *model;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    model = nautilus_files_view_get_model (view);

    if (model == NULL)
    {
        return NULL;
    }

    directory = nautilus_desktop_directory_get_real_directory (NAUTILUS_DESKTOP_DIRECTORY (model));

    uri = nautilus_directory_get_uri (directory);

    nautilus_directory_unref (directory);

    return uri;
}

static void
real_update_context_menus (NautilusFilesView *view)
{
    NautilusCanvasContainer *canvas_container;
    NautilusDesktopCanvasView *desktop_view;
    GAction *action;
    GActionGroup *view_action_group;
    GList *selection;
    int selection_count;

    g_assert (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (view));

    NAUTILUS_FILES_VIEW_CLASS (nautilus_desktop_canvas_view_parent_class)->update_context_menus (view);

    view_action_group = nautilus_files_view_get_action_group (view);
    desktop_view = NAUTILUS_DESKTOP_CANVAS_VIEW (view);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    selection_count = g_list_length (selection);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "empty-trash");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), trash_link_is_selection (view));

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "keep-aligned");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "organize-desktop-by-name");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "change-background");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "properties");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection_count > 0);

    /* Stretch */
    canvas_container = get_canvas_container (desktop_view);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "stretch");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection_count == 1 &&
                                 canvas_container != NULL &&
                                 !nautilus_canvas_container_has_stretch_handles (canvas_container));

    nautilus_file_list_free (selection);

    /* Unstretch */
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "unstretch");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), canvas_container != NULL &&
                                 nautilus_canvas_container_is_stretched (canvas_container));
}

static void
nautilus_desktop_canvas_view_init (NautilusDesktopCanvasView *desktop_canvas_view)
{
    NautilusCanvasContainer *canvas_container;
    GtkAllocation allocation;
    GActionGroup *view_action_group;
    GtkAdjustment *hadj, *vadj;

    desktop_canvas_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_canvas_view,
                                                                NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW,
                                                                NautilusDesktopCanvasViewDetails);

    if (desktop_directory == NULL)
    {
        desktop_directory = nautilus_get_desktop_directory ();
    }

    canvas_container = get_canvas_container (desktop_canvas_view);

    nautilus_canvas_container_set_is_fixed_size (canvas_container, TRUE);
    nautilus_canvas_container_set_is_desktop (canvas_container, TRUE);
    nautilus_canvas_container_set_store_layout_timestamps (canvas_container, TRUE);

    /* Set allocation to be at 0, 0 */
    gtk_widget_get_allocation (GTK_WIDGET (canvas_container), &allocation);
    allocation.x = 0;
    allocation.y = 0;
    gtk_widget_set_allocation (GTK_WIDGET (canvas_container), &allocation);

    gtk_widget_queue_resize (GTK_WIDGET (canvas_container));

    hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas_container));
    vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas_container));

    gtk_adjustment_set_value (hadj, 0);
    gtk_adjustment_set_value (vadj, 0);

    nautilus_files_view_ignore_hidden_file_preferences
        (NAUTILUS_FILES_VIEW (desktop_canvas_view));

    nautilus_files_view_set_show_foreign (NAUTILUS_FILES_VIEW (desktop_canvas_view),
                                          FALSE);

    g_signal_connect_object (canvas_container, "realize",
                             G_CALLBACK (desktop_canvas_container_realize), desktop_canvas_view, 0);

    g_signal_connect_object (desktop_canvas_view, "realize",
                             G_CALLBACK (realized_callback), desktop_canvas_view, 0);
    g_signal_connect_object (desktop_canvas_view, "unrealize",
                             G_CALLBACK (unrealized_callback), desktop_canvas_view, 0);

    g_signal_connect_swapped (nautilus_icon_view_preferences,
                              "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                              G_CALLBACK (default_zoom_level_changed),
                              desktop_canvas_view);

    g_signal_connect_swapped (nautilus_desktop_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DESKTOP_FONT,
                              G_CALLBACK (font_changed_callback),
                              desktop_canvas_view);

    set_up_zoom_level (desktop_canvas_view);
    nautilus_desktop_canvas_view_update_canvas_container_fonts (desktop_canvas_view);

    g_signal_connect_swapped (gnome_lockdown_preferences,
                              "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                              G_CALLBACK (nautilus_files_view_update_context_menus),
                              desktop_canvas_view);

    view_action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (desktop_canvas_view));

    g_action_map_add_action_entries (G_ACTION_MAP (view_action_group),
                                     desktop_view_entries,
                                     G_N_ELEMENTS (desktop_view_entries),
                                     NAUTILUS_FILES_VIEW (desktop_canvas_view));
}

NautilusFilesView *
nautilus_desktop_canvas_view_new (NautilusWindowSlot *slot)
{
    NautilusFilesView *view;

    view = g_object_new (NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW,
                         "window-slot", slot,
                         "supports-zooming", FALSE,
                         "supports-auto-layout", FALSE,
                         "supports-manual-layout", TRUE,
                         "supports-scaling", TRUE,
                         "supports-keep-aligned", TRUE,
                         NULL);

    if (g_object_is_floating (view))
    {
        g_object_ref_sink (view);
    }

    return view;
}
