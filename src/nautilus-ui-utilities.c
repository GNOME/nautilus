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

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>


char *
nautilus_capitalize_str (const char *string)
{
    char *capitalized = NULL;

    if (string == NULL)
    {
        return NULL;
    }

    if (g_utf8_validate (string, -1, NULL))
    {
        g_autofree gunichar *ucs4 = NULL;
        ucs4 = g_utf8_to_ucs4 (string, -1, NULL, NULL, NULL);
        if (ucs4 != NULL)
        {
            ucs4[0] = g_unichar_toupper (ucs4[0]);
            capitalized = g_ucs4_to_utf8 (ucs4, -1, NULL, NULL, NULL);
        }
    }

    if (capitalized == NULL)
    {
        return g_strdup (string);
    }

    return capitalized;
}

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

/**
 * nautilus_g_menu_model_find_by_string:
 * @model: the #GMenuModel with items to search
 * @attribute: the menu item attribute to compare with
 * @string: the string to match the value of @attribute
 *
 * This will search for an item in the model which has got the @attribute and
 * whose value is equal to @string.
 *
 * It is assumed that @attribute has the a GVariant format string "s".
 *
 * Returns: The index of the first match in the model, or -1 if no item matches.
 */
gint
nautilus_g_menu_model_find_by_string (GMenuModel  *model,
                                      const gchar *attribute,
                                      const gchar *string)
{
    gint item_index = -1;
    gint n_items;

    n_items = g_menu_model_get_n_items (model);
    for (gint i = 0; i < n_items; i++)
    {
        g_autofree gchar *value = NULL;
        if (g_menu_model_get_item_attribute (model, i, attribute, "s", &value) &&
            g_strcmp0 (value, string) == 0)
        {
            item_index = i;
            break;
        }
    }
    return item_index;
}

typedef struct
{
    NautilusMode mode;
    const char *name;
} ModeAndName;

static ModeAndName mode_map[] =
{
    { NAUTILUS_MODE_BROWSE, "browse" },
    { NAUTILUS_MODE_OPEN_FILE, "open-file" },
    { NAUTILUS_MODE_OPEN_FILES, "open-files" },
    { NAUTILUS_MODE_OPEN_FOLDER, "open-folder" },
    { NAUTILUS_MODE_OPEN_FOLDERS, "open-folders" },
    { NAUTILUS_MODE_SAVE_FILE, "save-file" },
    { NAUTILUS_MODE_SAVE_FILES, "save-files" },
    { 0, NULL }
};

static const char *
get_name_for_mode (NautilusMode mode)
{
    for (guint i = 0; mode_map[i].name != NULL; i++)
    {
        if (mode_map[i].mode == mode)
        {
            return mode_map[i].name;
        }
    }

    g_return_val_if_reached (NULL);
}

static void
filter_menu_model (GMenuModel *model,
                   const char *filter_key,
                   const char *filter_value)
{
    for (gint i = g_menu_model_get_n_items (model) - 1; i >= 0; i--)
    {
        g_autoptr (GMenuModel) section = NULL;
        g_autofree char *value = NULL;

        if (g_menu_model_get_item_attribute (model, i, filter_key, "s", &value))
        {
            g_auto (GStrv) tokens = g_strsplit (value, ",", 0);

            if (!g_strv_contains ((const gchar * const *) tokens, filter_value))
            {
                g_menu_remove (G_MENU (model), i);
                continue;
            }
        }

        section = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);

        if (section != NULL)
        {
            filter_menu_model (section, filter_key, filter_value);
        }
    }
}

void
nautilus_g_menu_model_set_for_mode (GMenuModel   *model,
                                    NautilusMode  mode)
{
    const char *mode_name = get_name_for_mode (mode);
    filter_menu_model (model, "show-in-mode", mode_name);
}

void
nautilus_g_menu_model_set_for_view (GMenuModel *model,
                                    const char *view_name)
{
    filter_menu_model (model, "show-in-view", view_name);
}

/**
 * nautilus_g_menu_replace_string_in_item:
 * @menu: the #GMenu to modify
 * @i: the position of the item to change
 * @attribute: the menu item attribute to change
 * @string: the string to change the value of @attribute to
 *
 * This will replace the item at @position with a new item which is identical
 * except that it has @attribute set to @string.
 *
 * This is useful e.g. when want to change the menu model of a #GtkPopover and
 * you have a pointer to its menu model but not to the popover itself, so you
 * can't just set a new model. With this method, the GtkPopover is notified of
 * changes in its model and updates its contents accordingly.
 *
 * It is assumed that @attribute has the a GVariant format string "s".
 */
void
nautilus_g_menu_replace_string_in_item (GMenu       *menu,
                                        gint         i,
                                        const gchar *attribute,
                                        const gchar *string)
{
    GMenuModel *menu_model = G_MENU_MODEL (menu);
    g_return_if_fail (i > -1 && i < g_menu_model_get_n_items (menu_model));

    g_autofree gchar *old_string = NULL;

    if (g_menu_model_get_item_attribute (menu_model, i, attribute, "s", &old_string) &&
        g_strcmp0 (old_string, string) == 0)
    {
        /* Value is already set */
        return;
    }

    g_autoptr (GMenuItem) item = g_menu_item_new_from_model (menu_model, i);
    g_return_if_fail (item != NULL);

    if (string != NULL)
    {
        g_menu_item_set_attribute (item, attribute, "s", string);
    }
    else
    {
        g_menu_item_set_attribute (item, attribute, NULL);
    }

    g_menu_remove (menu, i);
    g_menu_insert_item (menu, i, item);
}

static GdkPixbuf *filmholes_left = NULL;
static GdkPixbuf *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
    if (filmholes_left == NULL)
    {
        filmholes_left = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/image/filmholes.png", NULL);
    }
    if (filmholes_right == NULL &&
        filmholes_left != NULL)
    {
        filmholes_right = gdk_pixbuf_flip (filmholes_left, TRUE);
    }

    return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GtkSnapshot *snapshot,
                         gdouble      width,
                         gdouble      height)
{
    g_autoptr (GdkTexture) left_texture = NULL;
    g_autoptr (GdkTexture) right_texture = NULL;
    int holes_width, holes_height;

    if (!ensure_filmholes ())
    {
        return;
    }

    holes_width = gdk_pixbuf_get_width (filmholes_left);
    holes_height = gdk_pixbuf_get_height (filmholes_left);

    /* Left */
    gtk_snapshot_push_repeat (snapshot,
                              &GRAPHENE_RECT_INIT (0, 0, holes_width, height),
                              NULL);
    left_texture = gdk_texture_new_for_pixbuf (filmholes_left);
    gtk_snapshot_append_texture (snapshot,
                                 left_texture,
                                 &GRAPHENE_RECT_INIT (0, 0, holes_width, holes_height));
    gtk_snapshot_pop (snapshot);

    /* Right */
    gtk_snapshot_push_repeat (snapshot,
                              &GRAPHENE_RECT_INIT (width - holes_width, 0, holes_width, height),
                              NULL);
    right_texture = gdk_texture_new_for_pixbuf (filmholes_right);
    gtk_snapshot_append_texture (snapshot,
                                 right_texture,
                                 &GRAPHENE_RECT_INIT (width - holes_width, 0, holes_width, holes_height));
    gtk_snapshot_pop (snapshot);
}

gboolean
nautilus_date_time_is_between_dates (GDateTime *date,
                                     GDateTime *initial_date,
                                     GDateTime *end_date)
{
    gboolean in_between;

    /* Silently ignore errors */
    if (date == NULL || g_date_time_to_unix (date) == 0)
    {
        return FALSE;
    }

    /* For the end date, we want to make end_date inclusive,
     * for that the difference between the start of the day and the in_between
     * has to be more than -1 day
     */
    in_between = g_date_time_difference (date, initial_date) > 0 &&
                 g_date_time_difference (end_date, date) / G_TIME_SPAN_DAY > -1;

    return in_between;
}

AdwMessageDialog *
show_dialog (const gchar    *primary_text,
             const gchar    *secondary_text,
             GtkWindow      *parent,
             GtkMessageType  type)
{
    GtkWidget *dialog;

    g_return_val_if_fail (parent != NULL, NULL);

    dialog = adw_message_dialog_new (parent, primary_text, secondary_text);
    adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "ok", _("_OK"));
    adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "ok");

    gtk_window_present (GTK_WINDOW (dialog));

    return ADW_MESSAGE_DIALOG (dialog);
}

static void
notify_unmount_done (GMountOperation *op,
                     const gchar     *message,
                     gpointer         user_data)
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
        icon = g_mount_get_symbolic_icon (G_MOUNT (user_data));
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
                     const gchar     *message,
                     gpointer         user_data)
{
    NautilusApplication *application;
    GNotification *unmount;
    gchar *notification_id;
    GIcon *icon;
    gchar **strings;

    application = nautilus_application_get_default ();
    strings = g_strsplit (message, "\n", 0);
    icon = g_mount_get_icon (G_MOUNT (user_data));

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
        notify_unmount_done (op, message, user_data);
    }
    else
    {
        notify_unmount_show (op, message, user_data);
    }
}

void
show_unmount_progress_aborted_cb (GMountOperation *op,
                                  gpointer         user_data)
{
    notify_unmount_done (op, NULL, user_data);
}

static float
get_dash_width (guint size)
{
    switch (size)
    {
        case NAUTILUS_LIST_ICON_SIZE_SMALL:
        {
            /* We don't want to draw borders for the smallest size. */
            g_assert_not_reached ();
        }

        case NAUTILUS_LIST_ICON_SIZE_MEDIUM:
        {
            return 1.5;
        }

        case NAUTILUS_GRID_ICON_SIZE_SMALL:
        {
            return 2.0;
        }

        /* case NAUTILUS_LIST_ICON_SIZE_LARGE */
        case NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS:
        {
            return 2.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_MEDIUM:
        {
            return 3.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_LARGE:
        {
            return 4.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE:
        {
            return 5.0;
        }

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }
}

static float
get_dash_length (guint size)
{
    switch (size)
    {
        case NAUTILUS_LIST_ICON_SIZE_SMALL:
        {
            /* We don't want to draw borders for the smallest size. */
            g_assert_not_reached ();
        }

        case NAUTILUS_LIST_ICON_SIZE_MEDIUM:
        {
            return 6.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_SMALL:
        {
            return 10.0;
        }

        /* case NAUTILUS_LIST_ICON_SIZE_LARGE */
        case NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS:
        {
            return 10.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_MEDIUM:
        {
            return 15.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_LARGE:
        {
            return 20;
        }

        case NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE:
        {
            return 25;
        }

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }
}

static float
get_dash_radius (guint size)
{
    switch (size)
    {
        case NAUTILUS_LIST_ICON_SIZE_SMALL:
        {
            /* We don't want to draw borders for the smallest size. */
            g_assert_not_reached ();
        }

        case NAUTILUS_LIST_ICON_SIZE_MEDIUM:
        {
            return 4;
        }

        case NAUTILUS_GRID_ICON_SIZE_SMALL:
        {
            return 5.33;
        }

        /* case NAUTILUS_LIST_ICON_SIZE_LARGE */
        case NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS:
        {
            return 5.33;
        }

        case NAUTILUS_GRID_ICON_SIZE_MEDIUM:
        {
            return 8.0;
        }

        case NAUTILUS_GRID_ICON_SIZE_LARGE:
        {
            return 10.66;
        }

        case NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE:
        {
            return 13.33;
        }

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }
}

#define DASH_STROKE_FRACTION 13.0 / 20.0

void
nautilus_ui_draw_icon_dashed_border (GtkSnapshot     *snapshot,
                                     graphene_rect_t *rect,
                                     GdkRGBA          color)
{
    float width = rect->size.width;
    float stroke_width = get_dash_width (width);
    const float ideal_dash_length = get_dash_length (width) + stroke_width;
    const float radius = get_dash_radius (width);
    /* Need to inset the rectangle and the dash length by the dash width. */
    graphene_rect_t *dash_bounds = graphene_rect_inset (rect,
                                                        stroke_width / 2.0,
                                                        stroke_width / 2.0);
    /* Calculate the path length and divide it by the number of dashes to have
     * an exact fractional dash length with no overlap at the start/end point.
     */
    float border_length = dash_bounds->size.width * 2 +
                          dash_bounds->size.height * 2 +
                          2 * G_PI * radius - 8 * radius;
    const float number_of_dashes = round (border_length / ideal_dash_length);
    float dash_length = border_length / number_of_dashes;
    float dash_pattern[] =
    {
        dash_length * DASH_STROKE_FRACTION,
        dash_length * (1 - DASH_STROKE_FRACTION),
    };
    graphene_size_t arc_size = GRAPHENE_SIZE_INIT (radius, radius);
    GskRoundedRect round_rect;
    GskPathBuilder *path_builder = gsk_path_builder_new ();
    g_autoptr (GskStroke) stroke = gsk_stroke_new (stroke_width);

    gsk_rounded_rect_init (&round_rect, dash_bounds, &arc_size, &arc_size, &arc_size, &arc_size);

    gsk_path_builder_add_rounded_rect (path_builder, &round_rect);
    gsk_path_builder_close (path_builder);

    g_autoptr (GskPath) path = gsk_path_builder_free_to_path (path_builder);

    gsk_stroke_set_dash (stroke, dash_pattern, G_N_ELEMENTS (dash_pattern));
    gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);

    gtk_snapshot_append_stroke (snapshot, path, stroke, &color);
}

void
nautilus_ui_draw_symbolic_icon (GtkSnapshot           *snapshot,
                                const gchar           *icon_name,
                                const graphene_rect_t *rect,
                                GdkRGBA                color,
                                int                    scale)
{
    g_autoptr (GIcon) gicon = g_themed_icon_new (icon_name);
    g_autoptr (NautilusIconInfo) icon = nautilus_icon_info_lookup (gicon,
                                                                   2.0 * rect->size.width,
                                                                   scale);
    g_autoptr (GdkPaintable) paintable = nautilus_icon_info_get_paintable (icon);
    const GdkRGBA colors[] = {color};

    g_assert (GTK_IS_SYMBOLIC_PAINTABLE (paintable));

    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &rect->origin);
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                              snapshot,
                                              rect->size.width,
                                              rect->size.height,
                                              colors,
                                              G_N_ELEMENTS (colors));
    gtk_snapshot_restore (snapshot);
}
