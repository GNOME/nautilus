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
#include <math.h>
#include <string.h>
#include <glib/gi18n.h>

static GMenuModel *
find_gmenu_model (GMenuModel  *model,
                  const gchar *model_id)
{
    gint i, n_items;
    GMenuModel *insertion_model = NULL;

    n_items = g_menu_model_get_n_items (model);

    for (i = 0; i < n_items && !insertion_model; i++)
    {
        gchar *id = NULL;
        if (g_menu_model_get_item_attribute (model, i, "id", "s", &id) &&
            g_strcmp0 (id, model_id) == 0)
        {
            insertion_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
            if (!insertion_model)
            {
                insertion_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
            }
        }
        else
        {
            GMenuModel *submodel;
            GMenuModel *submenu;
            gint j, j_items;

            submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);

            if (!submodel)
            {
                submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
            }

            if (!submodel)
            {
                continue;
            }

            j_items = g_menu_model_get_n_items (submodel);
            for (j = 0; j < j_items; j++)
            {
                submenu = g_menu_model_get_item_link (submodel, j, G_MENU_LINK_SUBMENU);
                if (submenu)
                {
                    insertion_model = find_gmenu_model (submenu, model_id);
                    g_object_unref (submenu);
                }

                if (insertion_model)
                {
                    break;
                }
            }

            g_object_unref (submodel);
        }

        g_free (id);
    }

    return insertion_model;
}

/*
 * The original GMenu is modified adding to the section @submodel_name
 * the items in @gmenu_to_merge.
 * @gmenu_to_merge should be a list of menu items.
 */
void
nautilus_gmenu_merge (GMenu       *original,
                      GMenu       *gmenu_to_merge,
                      const gchar *submodel_name,
                      gboolean     prepend)
{
    gint i, n_items;
    GMenuModel *submodel;
    GMenuItem *item;

    g_return_if_fail (G_IS_MENU (original));
    g_return_if_fail (G_IS_MENU (gmenu_to_merge));

    submodel = find_gmenu_model (G_MENU_MODEL (original), submodel_name);

    g_return_if_fail (submodel != NULL);

    n_items = g_menu_model_get_n_items (G_MENU_MODEL (gmenu_to_merge));

    for (i = 0; i < n_items; i++)
    {
        if (prepend)
        {
            item = g_menu_item_new_from_model (G_MENU_MODEL (gmenu_to_merge),
                                               n_items - i - 1);
            g_menu_prepend_item (G_MENU (submodel), item);
        }
        else
        {
            item = g_menu_item_new_from_model (G_MENU_MODEL (gmenu_to_merge), i);
            g_menu_append_item (G_MENU (submodel), item);
        }
        g_object_unref (item);
    }

    g_object_unref (submodel);
}

/*
 * The GMenu @menu is modified adding to the section @submodel_name
 * the item @item.
 */
void
nautilus_gmenu_add_item_in_submodel (GMenu       *menu,
                                     GMenuItem   *item,
                                     const gchar *submodel_name,
                                     gboolean     prepend)
{
    GMenuModel *submodel;

    g_return_if_fail (G_IS_MENU (menu));
    g_return_if_fail (G_IS_MENU_ITEM (item));

    submodel = find_gmenu_model (G_MENU_MODEL (menu), submodel_name);

    g_return_if_fail (submodel != NULL);
    if (prepend)
    {
        g_menu_prepend_item (G_MENU (submodel), item);
    }
    else
    {
        g_menu_append_item (G_MENU (submodel), item);
    }

    g_object_unref (submodel);
}

/**
 * nautilus_pop_up_context_menu_at_pointer:
 *
 * Pop up a context menu at the pointer's position. If @event_button is NULL,
 * the current even will be assumed by gtk_menu_popup_at_pointer().
 */
void
nautilus_pop_up_context_menu_at_pointer (GtkWidget      *parent,
                                         GMenu          *menu,
                                         const GdkEvent *event)
{
    g_autoptr (GtkWidget) gtk_menu = NULL;

    g_return_if_fail (G_IS_MENU (menu));
    g_return_if_fail (GTK_IS_WIDGET (parent));

    gtk_menu = gtk_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_menu_attach_to_widget (GTK_MENU (gtk_menu), parent, NULL);

    gtk_menu_popup_at_pointer (GTK_MENU (gtk_menu), event);

    g_object_ref_sink (gtk_menu);
}

#define NAUTILUS_THUMBNAIL_FRAME_LEFT 3
#define NAUTILUS_THUMBNAIL_FRAME_TOP 3
#define NAUTILUS_THUMBNAIL_FRAME_RIGHT 3
#define NAUTILUS_THUMBNAIL_FRAME_BOTTOM 3

static GdkPaintable *
embed_paintable_in_frame (GdkPaintable *paintable,
                          const char   *frame_image_url,
                          GtkBorder    *slice_width,
                          GtkBorder    *border_width)
{
    g_autoptr (GtkSnapshot) snapshot = NULL;
    int width;
    int height;
    g_autofree char *css_str = NULL;
    g_autoptr (GtkCssProvider) provider = NULL;
    g_autoptr (GtkStyleContext) context = NULL;
    g_autoptr (GtkWidgetPath) path = NULL;
    graphene_rect_t bounds;

    snapshot = gtk_snapshot_new ();
    width = gdk_paintable_get_intrinsic_width (paintable);
    height = gdk_paintable_get_intrinsic_height (paintable);
    css_str = g_strdup_printf (".embedded-image {"
                               "    border-image-source: url(\"%s\");"
                               "    border-image-slice: %d %d %d %d;"
                               "    border-image-width: %dpx %dpx %dpx %dpx;"
                               "}",
                               frame_image_url,
                               slice_width->top, slice_width->right, slice_width->bottom, slice_width->left,
                               border_width->top, border_width->right, border_width->bottom, border_width->left);
    provider = gtk_css_provider_new ();
    context = gtk_style_context_new ();
    path = gtk_widget_path_new ();
    bounds = GRAPHENE_RECT_INIT (border_width->left, border_width->top,
                                 width - border_width->left - border_width->right,
                                 height - border_width->top - border_width->bottom);

    gtk_css_provider_load_from_data (provider, css_str, -1);

    gtk_style_context_set_path (context, path);
    gtk_style_context_add_provider (context,
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);

    gtk_snapshot_push_clip (snapshot, &bounds);

    gdk_paintable_snapshot (paintable, GDK_SNAPSHOT (snapshot), width, height);

    gtk_style_context_add_class (context, "embedded-image");

    gtk_snapshot_render_frame (snapshot, context, 0, 0, width, height);

    gtk_snapshot_pop (snapshot);

    return gtk_snapshot_free_to_paintable (snapshot, NULL);
}

void
nautilus_ui_frame_image (GdkPaintable **paintable)
{
    g_autoptr (GdkPaintable) framed_paintable = NULL;
    GtkBorder border;

    g_return_if_fail (paintable != NULL);

    border.left = NAUTILUS_THUMBNAIL_FRAME_LEFT;
    border.top = NAUTILUS_THUMBNAIL_FRAME_TOP;
    border.right = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
    border.bottom = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;

    framed_paintable = embed_paintable_in_frame (*paintable,
                                                 "resource:///org/gnome/nautilus/icons/thumbnail_frame.png",
                                                 &border, &border);

    g_set_object (paintable, framed_paintable);
}

static GdkTexture *filmholes_left = NULL;
static GdkPaintable *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
    if (filmholes_left == NULL)
    {
        filmholes_left = gdk_texture_new_from_resource ("/org/gnome/nautilus/icons/filmholes.png");
    }
    if (filmholes_right == NULL &&
        filmholes_left != NULL)
    {
        GtkSnapshot *snapshot;
        int width;
        int height;
        graphene_rect_t bounds;
        graphene_vec3_t axis;
        graphene_matrix_t matrix;

        snapshot = gtk_snapshot_new ();
        width = gdk_texture_get_width (filmholes_left);
        height = gdk_texture_get_height (filmholes_left);
        bounds = GRAPHENE_RECT_INIT (0, 0, width, height);

        graphene_vec3_init (&axis, 0, 1, 0);
        graphene_matrix_init_rotate (&matrix, 180, &axis);

        gtk_snapshot_push_transform (snapshot, &matrix);

        gtk_snapshot_append_texture (snapshot, filmholes_left, &bounds);

        gtk_snapshot_pop (snapshot);

        filmholes_right = gtk_snapshot_free_to_paintable (snapshot, NULL);
    }

    return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GdkPaintable **paintable)
{
    GtkSnapshot *snapshot;
    int width;
    int height;
    int holes_width;
    int holes_height;
    int i;
    g_autoptr (GdkPaintable) framed_paintable = NULL;

    if (!ensure_filmholes ())
    {
        return;
    }

    g_return_if_fail (paintable != NULL);
    g_return_if_fail (*paintable != NULL);

    snapshot = gtk_snapshot_new ();
    width = gdk_paintable_get_intrinsic_width (*paintable);
    height = gdk_paintable_get_intrinsic_height (*paintable);
    holes_width = gdk_texture_get_width (filmholes_left);
    holes_height = gdk_texture_get_height (filmholes_left);

    gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

    for (i = 0; i < height; i += holes_height)
    {
        gtk_snapshot_offset (snapshot, 0, holes_height);

        gdk_paintable_snapshot (GDK_PAINTABLE (filmholes_left),
                                GDK_SNAPSHOT (snapshot),
                                holes_width, holes_height);
    }

    gtk_snapshot_offset (snapshot, width - holes_width, 0);

    for (i = 0; i < height; i += holes_height)
    {
        gtk_snapshot_offset (snapshot, 0, -holes_height);

        gdk_paintable_snapshot (filmholes_right,
                                GDK_SNAPSHOT (snapshot),
                                holes_width, holes_height);
    }

    gtk_snapshot_pop (snapshot);

    framed_paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);

    g_set_object (paintable, framed_paintable);
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

GtkDialog *
show_dialog (const gchar *primary_text,
             const gchar *secondary_text,
             GtkWindow   *parent,
             GtkMessageType type)
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
