/* nautilus-dnd.h - Common Drag & drop handling code
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 *          Ettore Perazzoli <ettore@gnu.org>
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-directory.h"
#include "nautilus-dnd.h"
#include "nautilus-file-utilities.h"
#include "nautilus-files-view-dnd.h"
#include "nautilus-tag-manager.h"

static gboolean
check_same_fs (NautilusFile *file1,
               NautilusFile *file2)
{
    char *id1, *id2;
    gboolean result;

    result = FALSE;

    if (file1 != NULL && file2 != NULL)
    {
        id1 = nautilus_file_get_filesystem_id (file1);
        id2 = nautilus_file_get_filesystem_id (file2);

        if (id1 != NULL && id2 != NULL)
        {
            result = (strcmp (id1, id2) == 0);
        }

        g_free (id1);
        g_free (id2);
    }

    return result;
}

static gboolean
source_is_deletable (GFile *file)
{
    NautilusFile *naut_file;
    gboolean ret;

    /* if there's no a cached NautilusFile, it returns NULL */
    naut_file = nautilus_file_get (file);
    if (naut_file == NULL)
    {
        return FALSE;
    }

    ret = nautilus_file_can_delete (naut_file);
    nautilus_file_unref (naut_file);

    return ret;
}

#if 0 && NAUTILUS_DND_NEEDS_GTK4_REIMPLEMENTATION
static void
append_drop_action_menu_item (GtkWidget          *menu,
                              const char         *text,
                              GdkDragAction       action,
                              gboolean            sensitive,
                              DropActionMenuData *damd)
{
    GtkWidget *menu_item;

    menu_item = gtk_button_new_with_mnemonic (text);
    gtk_widget_set_sensitive (menu_item, sensitive);
    gtk_box_append (GTK_BOX (menu), menu_item);

    gtk_widget_add_css_class (menu_item, "flat");

    g_object_set_data (G_OBJECT (menu_item),
                       "action",
                       GINT_TO_POINTER (action));

    g_signal_connect (menu_item, "clicked",
                      G_CALLBACK (drop_action_activated_callback),
                      damd);

    gtk_widget_show (menu_item);
}
#endif
/* Pops up a menu of actions to perform on dropped files */
GdkDragAction
nautilus_drag_drop_action_ask (GtkWidget     *widget,
                               GdkDragAction  actions)
{
#if 0
    GtkWidget *popover;
    GtkWidget *menu;
    GtkWidget *menu_item;
    DropActionMenuData damd;

    /* Create the menu and set the sensitivity of the items based on the
     * allowed actions.
     */
    popover = gtk_popover_new (widget);
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);

    menu = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top (menu, 6);
    gtk_widget_set_margin_bottom (menu, 6);
    gtk_widget_set_margin_start (menu, 6);
    gtk_widget_set_margin_end (menu, 6);
    gtk_popover_set_child (GTK_POPOVER (popover), menu);
    gtk_widget_show (menu);

    append_drop_action_menu_item (menu, _("_Move Here"),
                                  GDK_ACTION_MOVE,
                                  (actions & GDK_ACTION_MOVE) != 0,
                                  &damd);

    append_drop_action_menu_item (menu, _("_Copy Here"),
                                  GDK_ACTION_COPY,
                                  (actions & GDK_ACTION_COPY) != 0,
                                  &damd);

    append_drop_action_menu_item (menu, _("_Link Here"),
                                  GDK_ACTION_LINK,
                                  (actions & GDK_ACTION_LINK) != 0,
                                  &damd);

    menu_item = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_append (GTK_BOX (menu), menu_item);
    gtk_widget_show (menu_item);

    append_drop_action_menu_item (menu, _("Cancel"), 0, TRUE, &damd);

    damd.chosen = 0;
    damd.loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (popover, "closed",
                      G_CALLBACK (menu_deactivate_callback),
                      &damd);

    gtk_grab_add (popover);

    /* We don't have pointer coords here. Just pick the center of the widget. */
    gtk_popover_set_pointing_to (GTK_POPOVER (popover),
                                 &(GdkRectangle){ .x = 0.5 * gtk_widget_get_allocated_width (widget),
                                                  .y = 0.5 * gtk_widget_get_allocated_height (widget),
                                                  .width = 0, .height = 0 });

    gtk_popover_popup (GTK_POPOVER (popover));

    g_main_loop_run (damd.loop);

    gtk_grab_remove (popover);

    g_main_loop_unref (damd.loop);

    g_object_ref_sink (popover);
    g_object_unref (popover);

    return damd.chosen;
#endif
    return 0;
}

GdkDragAction
nautilus_dnd_get_preferred_action (NautilusFile *target_file,
                                   GFile        *dropped)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (GFile) target_location = NULL;
    g_autoptr (NautilusFile) dropped_file = NULL;
    gboolean same_fs;
    gboolean source_deletable;

    g_return_val_if_fail (NAUTILUS_IS_FILE (target_file), 0);
    g_return_val_if_fail (dropped == NULL || G_IS_FILE (dropped), 0);

    target_location = nautilus_file_get_location (target_file);
    if (g_file_equal (target_location, dropped))
    {
        return 0;
    }

    /* First check target imperatives */
    directory = nautilus_directory_get_for_file (target_file);

    if (nautilus_is_file_roller_installed () &&
        nautilus_file_is_archive (target_file))
    {
        return GDK_ACTION_COPY;
    }
    else if (nautilus_file_is_starred_location (target_file))
    {
        if (nautilus_tag_manager_can_star_contents (nautilus_tag_manager_get (), dropped))
        {
            return GDK_ACTION_COPY;
        }
        else
        {
            return 0;
        }
    }
    else if (!nautilus_file_is_directory (target_file) ||
             !nautilus_file_can_write (target_file) ||
             !nautilus_directory_is_editable (directory))
    {
        /* No other file type other than archives and directories currently
         * accepts drops */
        return 0;
    }
    else if (nautilus_file_is_in_trash (target_file))
    {
        return GDK_ACTION_MOVE;
    }

    if (g_file_has_uri_scheme (dropped, "trash"))
    {
        return GDK_ACTION_MOVE;
    }

    dropped_file = nautilus_file_get (dropped);
    same_fs = check_same_fs (target_file, dropped_file);
    source_deletable = source_is_deletable (dropped);
    if (same_fs && source_deletable)
    {
        return GDK_ACTION_MOVE;
    }

    return GDK_ACTION_COPY;
}

void
nautilus_dnd_perform_drop (NautilusFilesView *view,
                           const GValue      *value,
                           GdkDragAction      action,
                           GFile             *target_location)
{
    g_autofree gchar *target_uri = g_file_get_uri (target_location);

    if (!gdk_drag_action_is_unique (action))
    {
        /* TODO: Implement */
    }
    else if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
        nautilus_files_view_handle_text_drop (view,
                                              g_value_get_string (value),
                                              target_uri, action);
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_file_list = g_value_get_boxed (value);
        GList *source_uri_list = NULL;

        for (GSList *l = source_file_list; l != NULL; l = l->next)
        {
            source_uri_list = g_list_prepend (source_uri_list, g_file_get_uri (l->data));
        }
        source_uri_list = g_list_reverse (source_uri_list);

        nautilus_files_view_drop_proxy_received_uris (view,
                                                      source_uri_list,
                                                      target_uri,
                                                      action);
        g_list_free_full (source_uri_list, g_free);
    }
}

#define MAX_DRAWN_DRAG_ICONS 10

GdkPaintable *
get_paintable_for_drag_selection (GList *selection,
                                  int    scale)
{
    g_autoqueue (GdkPaintable) icons = g_queue_new ();
    g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
    NautilusFileIconFlags flags;
    GdkPaintable *icon;
    guint n_icons;
    guint icon_size = NAUTILUS_DRAG_SURFACE_ICON_SIZE;
    float dx;
    float dy;
    /* A wide shadow for the pile of icons gives a sense of floating. */
    GskShadow stack_shadow = {.color = {0, 0, 0, .alpha = 0.15}, .dx = 0, .dy = 2, .radius = 10 };
    /* A slight shadow swhich makes each icon in the stack look separate. */
    GskShadow icon_shadow = {.color = {0, 0, 0, .alpha = 0.30}, .dx = 0, .dy = 1, .radius = 1 };

    g_return_val_if_fail (NAUTILUS_IS_FILE (selection->data), NULL);

    /* The selection list is reversed compared to what the user sees. Get the
     * first items by starting from the end of the list. */
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;
    for (GList *l = g_list_last (selection);
         l != NULL && g_queue_get_length (icons) <= MAX_DRAWN_DRAG_ICONS;
         l = l->prev)
    {
        icon = nautilus_file_get_icon_paintable (l->data, icon_size, scale, flags);
        g_queue_push_tail (icons, icon);
    }

    /* When there are 2 or 3 identical icons, we need to space them more,
     * otherwise it would be hard to tell there is more than one icon at all.
     * The more icons we have, the easier it is to notice multiple icons are
     * stacked, and the more compact we want to be.
     *
     *  1 icon          2 icons         3 icons         4+ icons
     *  .--------.      .--------.      .--------.      .--------.
     *  |        |      |        |      |        |      |        |
     *  |        |      |        |      |        |      |        |
     *  |        |      |        |      |        |      |        |
     *  |        |      |        |      |        |      |        |
     *  '--------'      |--------|      |--------|      |--------|
     *                  |        |      |        |      |--------|
     *                  |        |      |--------|      |--------|
     *                  '--------'      |        |      |--------|
     *                                  '--------'      '--------'
     */
    n_icons = g_queue_get_length (icons);
    dx = (n_icons % 2 == 1) ? 6 : -6;
    dy = (n_icons == 2) ? 10 : (n_icons == 3) ? 6 : (n_icons >= 4) ? 4 : 0;

    /* We want the first icon on top of every other. So we need to start drawing
     * the stack from the bottom, that is, from the last icon. This requires us
     * to jump to the last position and then move upwards one step at a time.
     * Also, add 10px horizontal offset, for shadow, to workaround this GTK bug:
     * https://gitlab.gnome.org/GNOME/gtk/-/issues/2341
     */
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (10 + (dx / 2), dy * n_icons));
    gtk_snapshot_push_shadow (snapshot, &stack_shadow, 1);
    for (GList *l = g_queue_peek_tail_link (icons); l != NULL; l = l->prev)
    {
        double w = gdk_paintable_get_intrinsic_width (l->data);
        double h = gdk_paintable_get_intrinsic_height (l->data);
        /* Offsets needed to center thumbnails. Floored to keep images sharp. */
        float x = floor ((icon_size - w) / 2);
        float y = floor ((icon_size - h) / 2);

        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-dx, -dy));

        /* Alternate horizontal offset direction to give a rough pile look. */
        dx = -dx;

        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
        gtk_snapshot_push_shadow (snapshot, &icon_shadow, 1);

        gdk_paintable_snapshot (l->data, snapshot, w, h);

        gtk_snapshot_pop (snapshot); /* End of icon shadow */
        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-x, -y));
    }
    gtk_snapshot_pop (snapshot); /* End of stack shadow */

    return gtk_snapshot_to_paintable (snapshot, NULL);
}
