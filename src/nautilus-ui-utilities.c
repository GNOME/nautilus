/* nautilus-ui-utilities.c - helper functions for GtkUIManager stuff
 *
 *  Copyright (C) 2004 Red Hat, Inc.
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
 *  Authors: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include "nautilus-ui-utilities.h"
#include "nautilus-icon-info.h"
#include "nautilus-application.h"
#include <eel/eel-graphic-effects.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libgd/gd.h>
#include <string.h>
#include <glib/gi18n.h>

/**
 * nautilus_gmenu_set_from_model:
 * @target_menu: the #GMenu to be filled
 * @source_model: (nullable): a #GMenuModel to copy items from
 *
 * This will replace the content of @target_menu with a copy of all items from
 * @source_model.
 *
 * If the @source_model is empty (i.e., its item count is 0), or if it is %NULL,
 * then the @target_menu is left empty.
 */
void
nautilus_gmenu_set_from_model (GMenu      *target_menu,
                               GMenuModel *source_model)
{
    g_return_if_fail (G_IS_MENU (target_menu));
    g_return_if_fail (source_model == NULL || G_IS_MENU_MODEL (source_model));

    /* First, empty the menu... */
    g_menu_remove_all (target_menu);

    /* ...then, repopulate it (maybe). */
    if (source_model != NULL)
    {
        gint n_items;

        n_items = g_menu_model_get_n_items (source_model);
        for (gint i = 0; i < n_items; i++)
        {
            g_autoptr (GMenuItem) item = NULL;
            item = g_menu_item_new_from_model (source_model, i);
            g_menu_append_item (target_menu, item);
        }
    }
}

#define NAUTILUS_THUMBNAIL_FRAME_LEFT 3
#define NAUTILUS_THUMBNAIL_FRAME_TOP 3
#define NAUTILUS_THUMBNAIL_FRAME_RIGHT 3
#define NAUTILUS_THUMBNAIL_FRAME_BOTTOM 3

void
nautilus_ui_frame_image (GdkPixbuf **pixbuf)
{
    GtkBorder border;
    GdkPixbuf *pixbuf_with_frame;

    border.left = NAUTILUS_THUMBNAIL_FRAME_LEFT;
    border.top = NAUTILUS_THUMBNAIL_FRAME_TOP;
    border.right = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
    border.bottom = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;

    pixbuf_with_frame = gd_embed_image_in_frame (*pixbuf,
                                                 "resource:///org/gnome/nautilus/icons/thumbnail_frame.png",
                                                 &border, &border);
    g_object_unref (*pixbuf);

    *pixbuf = pixbuf_with_frame;
}

static GdkPixbuf *filmholes_left = NULL;
static GdkPixbuf *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
    if (filmholes_left == NULL)
    {
        filmholes_left = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/filmholes.png", NULL);
    }
    if (filmholes_right == NULL &&
        filmholes_left != NULL)
    {
        filmholes_right = gdk_pixbuf_flip (filmholes_left, TRUE);
    }

    return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GdkPixbuf **pixbuf)
{
    int width, height;
    int holes_width, holes_height;
    int i;

    if (!ensure_filmholes ())
    {
        return;
    }

    width = gdk_pixbuf_get_width (*pixbuf);
    height = gdk_pixbuf_get_height (*pixbuf);
    holes_width = gdk_pixbuf_get_width (filmholes_left);
    holes_height = gdk_pixbuf_get_height (filmholes_left);

    for (i = 0; i < height; i += holes_height)
    {
        gdk_pixbuf_composite (filmholes_left, *pixbuf, 0, i,
                              MIN (width, holes_width),
                              MIN (height - i, holes_height),
                              0, i, 1, 1, GDK_INTERP_NEAREST, 255);
    }

    for (i = 0; i < height; i += holes_height)
    {
        gdk_pixbuf_composite (filmholes_right, *pixbuf,
                              width - holes_width, i,
                              MIN (width, holes_width),
                              MIN (height - i, holes_height),
                              width - holes_width, i,
                              1, 1, GDK_INTERP_NEAREST, 255);
    }
}

gboolean
nautilus_file_date_in_between (guint64    unix_file_time,
                               GDateTime *initial_date,
                               GDateTime *end_date)
{
    GDateTime *date;
    gboolean in_between;

    /* Silently ignore errors */
    if (unix_file_time == 0)
    {
        return FALSE;
    }

    date = g_date_time_new_from_unix_local (unix_file_time);

    /* For the end date, we want to make end_date inclusive,
     * for that the difference between the start of the day and the in_between
     * has to be more than -1 day
     */
    in_between = g_date_time_difference (date, initial_date) > 0 &&
                 g_date_time_difference (end_date, date) / G_TIME_SPAN_DAY > -1;

    g_date_time_unref (date);

    return in_between;
}

static const gchar *
get_text_for_days_ago (gint     days,
                       gboolean prefix_with_since)
{
    if (days < 7)
    {
        /* days */
        return prefix_with_since ?
               ngettext ("Since %d day ago", "Since %d days ago", days) :
               ngettext ("%d day ago", "%d days ago", days);
    }
    if (days < 30)
    {
        /* weeks */
        return prefix_with_since ?
               ngettext ("Since last week", "Since %d weeks ago", days / 7) :
               ngettext ("Last week", "%d weeks ago", days / 7);
    }
    if (days < 365)
    {
        /* months */
        return prefix_with_since ?
               ngettext ("Since last month", "Since %d months ago", days / 30) :
               ngettext ("Last month", "%d months ago", days / 30);
    }

    /* years */
    return prefix_with_since ?
           ngettext ("Since last year", "Since %d years ago", days / 365) :
           ngettext ("Last year", "%d years ago", days / 365);
}

gchar *
get_text_for_date_range (GPtrArray *date_range,
                         gboolean   prefix_with_since)
{
    gint days;
    gint normalized;
    GDateTime *initial_date;
    GDateTime *end_date;
    gchar *formatted_date;
    gchar *label;

    if (!date_range)
    {
        return NULL;
    }

    initial_date = g_ptr_array_index (date_range, 0);
    end_date = g_ptr_array_index (date_range, 1);
    days = g_date_time_difference (end_date, initial_date) / G_TIME_SPAN_DAY;
    formatted_date = g_date_time_format (initial_date, "%x");

    if (days < 1)
    {
        label = g_strdup (formatted_date);
    }
    else
    {
        if (days < 7)
        {
            /* days */
            normalized = days;
        }
        else if (days < 30)
        {
            /* weeks */
            normalized = days / 7;
        }
        else if (days < 365)
        {
            /* months */
            normalized = days / 30;
        }
        else
        {
            /* years */
            normalized = days / 365;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        label = g_strdup_printf (get_text_for_days_ago (days,
                                                        prefix_with_since),
                                 normalized);
#pragma GCC diagnostic pop
    }

    g_free (formatted_date);

    return label;
}

#define TIME_ZONE_RATE_LIMIT 2000000

GTimeZone *
nautilus_get_time_zone (void)
{
    static GTimeZone *cached_time_zone = NULL;
    static gint64 last_read_time = 0;
    gint64 current_time;

    current_time = g_get_monotonic_time ();

    if (g_once_init_enter (&last_read_time))
    {
        cached_time_zone = g_time_zone_new_local ();
        g_once_init_leave (&last_read_time, current_time);
    }

    if (current_time - last_read_time > TIME_ZONE_RATE_LIMIT)
    {
        GTimeZone *old_time_zone = cached_time_zone;

        last_read_time = current_time;
        cached_time_zone = g_time_zone_new_local ();

        g_time_zone_unref (old_time_zone);
    }

    return g_time_zone_ref (cached_time_zone);
}

GtkDialog *
show_dialog (const gchar    *primary_text,
             const gchar    *secondary_text,
             GtkWindow      *parent,
             GtkMessageType  type)
{
    GtkWidget *dialog;

    g_return_val_if_fail (parent != NULL, NULL);

    dialog = gtk_message_dialog_new (parent,
                                     GTK_DIALOG_MODAL,
                                     type,
                                     GTK_BUTTONS_OK,
                                     "%s", primary_text);

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", secondary_text);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    gtk_widget_show (dialog);

    g_signal_connect (GTK_DIALOG (dialog), "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);

    return GTK_DIALOG (dialog);
}

static void
notify_unmount_done (GMountOperation *op,
                     const gchar     *message)
{
    NautilusApplication *application;
    gchar *notification_id;

    application = nautilus_application_get_default ();
    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_withdraw_notification (application, notification_id);

    if (message != NULL)
    {
        GNotification *unplug;
        GIcon *icon;
        gchar **strings;

        strings = g_strsplit (message, "\n", 0);
        icon = g_themed_icon_new ("media-removable-symbolic");
        unplug = g_notification_new (strings[0]);
        g_notification_set_body (unplug, strings[1]);
        g_notification_set_icon (unplug, icon);

        nautilus_application_send_notification (application, notification_id, unplug);
        g_object_unref (unplug);
        g_object_unref (icon);
        g_strfreev (strings);
    }

    g_free (notification_id);
}

static void
notify_unmount_show (GMountOperation *op,
                     const gchar     *message)
{
    NautilusApplication *application;
    GNotification *unmount;
    gchar *notification_id;
    GIcon *icon;
    gchar **strings;

    application = nautilus_application_get_default ();
    strings = g_strsplit (message, "\n", 0);
    icon = g_themed_icon_new ("media-removable");

    unmount = g_notification_new (strings[0]);
    g_notification_set_body (unmount, strings[1]);
    g_notification_set_icon (unmount, icon);
    g_notification_set_priority (unmount, G_NOTIFICATION_PRIORITY_URGENT);

    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_send_notification (application, notification_id, unmount);
    g_object_unref (unmount);
    g_object_unref (icon);
    g_strfreev (strings);
    g_free (notification_id);
}

void
show_unmount_progress_cb (GMountOperation *op,
                          const gchar     *message,
                          gint64           time_left,
                          gint64           bytes_left,
                          gpointer         user_data)
{
    if (bytes_left == 0)
    {
        notify_unmount_done (op, message);
    }
    else
    {
        notify_unmount_show (op, message);
    }
}

void
show_unmount_progress_aborted_cb (GMountOperation *op,
                                  gpointer         user_data)
{
    notify_unmount_done (op, NULL);
}
