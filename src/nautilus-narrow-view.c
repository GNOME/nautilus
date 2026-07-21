/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-narrow-view.h"

#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-narrow-cell.h"
#include "nautilus-list-base-private.h"
#include "nautilus-view-cell.h"
#include "nautilus-view-item.h"
#include "nautilus-view-model.h"

#include <glib/gi18n.h>

struct _NautilusNarrowView
{
    NautilusListBase parent_instance;

    GtkListView *view_ui;

    gint zoom_level;

    gboolean directories_first;

    GQuark sort_attribute;
    gboolean reversed;
};

G_DEFINE_TYPE (NautilusNarrowView, nautilus_narrow_view, NAUTILUS_TYPE_LIST_BASE)

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static const NautilusViewInfo narrow_view_info =
{
    .view_id = NAUTILUS_VIEW_LIST_ID,
    .zoom_level_min = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_max = NAUTILUS_LIST_ZOOM_LEVEL_LARGE,
    .zoom_level_standard = NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM,
};

static NautilusViewInfo
real_get_view_info (NautilusListBase *list_base)
{
    return narrow_view_info;
}

static gint
nautilus_narrow_view_sort (gconstpointer a,
                           gconstpointer b,
                           gpointer      user_data)
{
    NautilusNarrowView *self = user_data;
    NautilusFile *file_a;
    NautilusFile *file_b;

    file_a = nautilus_view_item_get_file ((NautilusViewItem *) a);
    file_b = nautilus_view_item_get_file ((NautilusViewItem *) b);

    return nautilus_file_compare_for_sort_by_attribute_q (file_a, file_b,
                                                          self->sort_attribute,
                                                          self->directories_first,
                                                          self->reversed);
}

static void
update_sort_directories_first (NautilusNarrowView *self)
{
    NautilusFile *directory_as_file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));
    NautilusViewModel *model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    /* Always treat directories as normal items in search and recent. Recent
     * can accidentally contain directories when they were picked via file chooser. */
    if (nautilus_file_is_in_search (directory_as_file) ||
        nautilus_file_is_in_recent (directory_as_file))
    {
        self->directories_first = FALSE;
    }
    else
    {
        self->directories_first = g_settings_get_boolean (gtk_filechooser_preferences,
                                                          NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    }

    if (model != NULL)
    {
        nautilus_view_model_sort (model);
    }
}

static void
nautilus_narrow_view_setup_directory (NautilusListBase  *list_base,
                                      NautilusDirectory *new_directory)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base);

    NAUTILUS_LIST_BASE_CLASS (nautilus_narrow_view_parent_class)->setup_directory (list_base, new_directory);

    update_sort_directories_first (self);
}

static guint
get_icon_size_for_zoom_level (NautilusListZoomLevel zoom_level)
{
    switch (zoom_level)
    {
        case NAUTILUS_LIST_ZOOM_LEVEL_SMALL:
        {
            return NAUTILUS_LIST_ICON_SIZE_SMALL;
        }
        break;

        case NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM:
        {
            return NAUTILUS_LIST_ICON_SIZE_MEDIUM;
        }
        break;

        case NAUTILUS_LIST_ZOOM_LEVEL_LARGE:
        {
            return NAUTILUS_LIST_ICON_SIZE_LARGE;
        }
        break;
    }
    g_return_val_if_reached (NAUTILUS_LIST_ICON_SIZE_MEDIUM);
}

static gint
get_default_zoom_level (void)
{
    int default_zoom_level = g_settings_get_enum (nautilus_list_view_preferences,
                                                  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

    /* Sanitize preference value */
    return CLAMP (default_zoom_level,
                  narrow_view_info.zoom_level_min,
                  narrow_view_info.zoom_level_max);
}

static void
real_set_zoom_level (NautilusListBase *list_base,
                     int               new_level)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base);

    g_return_if_fail (new_level >= narrow_view_info.zoom_level_min &&
                      new_level <= narrow_view_info.zoom_level_max);

    self->zoom_level = new_level;

    if (g_settings_get_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL) != new_level)
    {
        g_settings_set_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                             new_level);
    }

    g_object_notify (G_OBJECT (self), "icon-size");

    if (self->zoom_level == NAUTILUS_LIST_ZOOM_LEVEL_SMALL)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self), "compact");
    }
    else
    {
        gtk_widget_remove_css_class (GTK_WIDGET (self), "compact");
    }
}

static void
on_narrow_view_item_activated (GtkListView *narrow_view,
                               guint        position,
                               gpointer     user_data)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (user_data);

    nautilus_list_base_activate_selection (NAUTILUS_LIST_BASE (self), FALSE);
}

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base_view);

    return get_icon_size_for_zoom_level (self->zoom_level);
}

static int
real_get_zoom_level (NautilusListBase *list_base_view)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base_view);

    return self->zoom_level;
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base_view);

    gtk_list_view_scroll_to (self->view_ui, position, flags, scroll);
}

static GVariant *
real_get_sort_state (NautilusListBase *list_base)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base);

    return g_variant_take_ref (g_variant_new ("(sb)",
                                              g_quark_to_string (self->sort_attribute),
                                              self->reversed));
}

static void
real_set_enable_rubberband (NautilusListBase *list_base,
                            gboolean          enabled)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base);

    gtk_list_view_set_enable_rubberband (self->view_ui, enabled);
}

static void
real_set_sort_state (NautilusListBase *list_base,
                     GVariant         *value)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (list_base);
    const gchar *target_name;
    NautilusViewModel *model = nautilus_list_base_get_model (list_base);
    g_autoptr (GtkCustomSorter) sorter = NULL;

    /* Sort state binding should be set after the model is set.*/
    g_return_if_fail (model != NULL);

    g_variant_get (value, "(&sb)", &target_name, &self->reversed);
    self->sort_attribute = g_quark_from_string (target_name);

    sorter = gtk_custom_sorter_new (nautilus_narrow_view_sort, self, NULL);
    nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));
}

static void
dispose (GObject *object)
{
    G_OBJECT_CLASS (nautilus_narrow_view_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_narrow_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = get_view_item (listitem);

    g_return_if_fail (item != NULL);

    GtkWidget *cell = gtk_list_item_get_child (listitem);

    nautilus_view_item_set_item_ui (item, cell);
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = NULL;

    item = get_view_item (listitem);

    /* item may be NULL when row has just been destroyed. */
    if (item != NULL)
    {
        nautilus_view_item_set_item_ui (item, NULL);
    }
}

static void
setup_cell (GtkSignalListItemFactory *factory,
            GtkListItem              *listitem,
            gpointer                  user_data)
{
    NautilusNarrowView *self = NAUTILUS_NARROW_VIEW (user_data);
    NautilusNarrowCell *cell = nautilus_narrow_cell_new (NAUTILUS_LIST_BASE (self));

    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), NAUTILUS_VIEW_CELL (cell), GTK_WIDGET (cell));

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * listitem:accessible-name :- listitem:item:item:file:a11y-name */
    GtkExpression *expression;
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_FILE, expression, "a11y-name");
    gtk_expression_bind (expression, listitem, "accessible-label", listitem);
}

static void
on_model_changed (NautilusNarrowView *self)
{
    NautilusViewModel *model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    if (model != NULL)
    {
        gtk_list_view_set_enable_rubberband (GTK_LIST_VIEW (self->view_ui),
                                             !nautilus_view_model_get_single_selection (model));
    }

    gtk_list_view_set_model (self->view_ui, GTK_SELECTION_MODEL (model));
}

static GtkListView *
create_view_ui (NautilusNarrowView *self)
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    GtkWidget *widget = gtk_list_view_new (NULL, factory);

    gtk_widget_set_hexpand (widget, TRUE);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkListView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (widget), FALSE);
    gtk_list_view_set_tab_behavior (GTK_LIST_VIEW (widget), GTK_LIST_TAB_ITEM);

    gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    _("Content View"),
                                    GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
                                    _("View of the current location"),
                                    -1);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (widget, "activate", G_CALLBACK (on_narrow_view_item_activated), self);

    return GTK_LIST_VIEW (widget);
}

static void
nautilus_narrow_view_class_init (NautilusNarrowViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = dispose;
    object_class->finalize = finalize;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_enable_rubberband = real_set_enable_rubberband;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
    list_base_view_class->setup_directory = nautilus_narrow_view_setup_directory;
}

static void
nautilus_narrow_view_init (NautilusNarrowView *self)
{
    GtkWidget *scrolled_window = nautilus_list_base_get_scrolled_window (NAUTILUS_LIST_BASE (self));

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-narrow-view");

    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));

    g_signal_connect_swapped (self, "notify::model", G_CALLBACK (on_model_changed), self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   GTK_WIDGET (self->view_ui));

    g_signal_connect_object (gtk_filechooser_preferences,
                             "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                             G_CALLBACK (update_sort_directories_first),
                             self,
                             G_CONNECT_SWAPPED);

    nautilus_list_base_set_zoom_level (NAUTILUS_LIST_BASE (self), get_default_zoom_level ());
}

NautilusNarrowView *
nautilus_narrow_view_new (void)
{
    return g_object_new (NAUTILUS_TYPE_NARROW_VIEW, NULL);
}
