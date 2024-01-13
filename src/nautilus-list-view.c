/*
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2022 GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

/* Needed for NautilusColumn. */
#include <nautilus-extension.h>

#include "nautilus-list-base-private.h"
#include "nautilus-list-view.h"

#include "nautilus-column-chooser.h"
#include "nautilus-column-utilities.h"
#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-label-cell.h"
#include "nautilus-metadata.h"
#include "nautilus-name-cell.h"
#include "nautilus-scheme.h"
#include "nautilus-search-directory.h"
#include "nautilus-star-cell.h"
#include "nautilus-tag-manager.h"

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

struct _NautilusListView
{
    NautilusListBase parent_instance;

    GtkColumnView *view_ui;

    NautilusSearchDirectory *search_directory;

    gint zoom_level;

    gboolean directories_first;
    gboolean expand_as_a_tree;

    GQuark path_attribute_q;
    GFile *file_path_base_location;

    GtkColumnViewColumn *star_column;
    GtkWidget *column_editor;
    GHashTable *factory_to_column_map;
};

G_DEFINE_TYPE (NautilusListView, nautilus_list_view, NAUTILUS_TYPE_LIST_BASE)

#define get_view_item(cell) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_column_view_cell_get_item (cell)))))

static const NautilusViewInfo list_view_info =
{
    .view_id = NAUTILUS_VIEW_LIST_ID,
    .zoom_level_min = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_max = NAUTILUS_LIST_ZOOM_LEVEL_LARGE,
    .zoom_level_standard = NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM,
};

static NautilusViewInfo
real_get_view_info (NautilusListBase *list_base)
{
    return list_view_info;
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

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base_view);

    return get_icon_size_for_zoom_level (self->zoom_level);
}

static GtkWidget *
real_get_view_ui (NautilusListBase *list_base_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base_view);

    return GTK_WIDGET (self->view_ui);
}

static int
real_get_zoom_level (NautilusListBase *list_base_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base_view);

    return self->zoom_level;
}

static void
apply_columns_settings (NautilusListView  *self,
                        char             **column_order,
                        char             **visible_columns)
{
    g_autolist (NautilusColumn) all_columns = NULL;
    NautilusFile *file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));
    g_autoptr (GFile) location = NULL;
    g_autoptr (GList) view_columns = NULL;
    GListModel *old_view_columns;
    g_autoptr (GHashTable) visible_columns_hash = NULL;
    g_autoptr (GHashTable) old_view_columns_hash = NULL;
    int column_i = 0;

    if (self->search_directory != NULL)
    {
        location = nautilus_query_get_location (nautilus_search_directory_get_query (self->search_directory));
    }

    if (location == NULL)
    {
        location = nautilus_file_get_location (file);
    }

    all_columns = nautilus_get_columns_for_file (file);
    all_columns = nautilus_sort_columns (all_columns, column_order);

    /* hash table to lookup if a given column should be visible */
    visible_columns_hash = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) g_free);
    /* always show name column */
    g_hash_table_insert (visible_columns_hash, g_strdup ("name"), g_strdup ("name"));

    /* always show star column if supported */
    if (nautilus_tag_manager_can_star_contents (nautilus_tag_manager_get (), location) ||
        g_file_has_uri_scheme (location, SCHEME_STARRED))
    {
        g_hash_table_insert (visible_columns_hash, g_strdup ("starred"), g_strdup ("starred"));
    }

    if (visible_columns != NULL)
    {
        for (int i = 0; visible_columns[i] != NULL; ++i)
        {
            g_hash_table_insert (visible_columns_hash,
                                 g_ascii_strdown (visible_columns[i], -1),
                                 g_ascii_strdown (visible_columns[i], -1));
        }
    }

    old_view_columns_hash = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   (GDestroyNotify) g_free,
                                                   NULL);
    old_view_columns = gtk_column_view_get_columns (self->view_ui);
    for (guint i = 0; i < g_list_model_get_n_items (old_view_columns); i++)
    {
        g_autoptr (GtkColumnViewColumn) view_column = NULL;
        GtkListItemFactory *factory;
        NautilusColumn *nautilus_column;
        gchar *name;

        view_column = g_list_model_get_item (old_view_columns, i);
        factory = gtk_column_view_column_get_factory (view_column);
        nautilus_column = g_hash_table_lookup (self->factory_to_column_map, factory);
        g_object_get (nautilus_column, "name", &name, NULL);
        g_hash_table_insert (old_view_columns_hash, name, view_column);
    }

    for (GList *l = all_columns; l != NULL; l = l->next)
    {
        g_autofree char *name = NULL;
        g_autofree char *lowercase = NULL;

        g_object_get (G_OBJECT (l->data), "name", &name, NULL);
        lowercase = g_ascii_strdown (name, -1);

        if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL)
        {
            GtkColumnViewColumn *view_column;

            view_column = g_hash_table_lookup (old_view_columns_hash, name);
            if (view_column != NULL)
            {
                view_columns = g_list_prepend (view_columns, view_column);
            }
        }
    }

    view_columns = g_list_reverse (view_columns);

    /* hide columns that are not present in the configuration */
    for (guint i = 0; i < g_list_model_get_n_items (old_view_columns); i++)
    {
        g_autoptr (GtkColumnViewColumn) view_column = NULL;

        view_column = g_list_model_get_item (old_view_columns, i);
        if (g_list_find (view_columns, view_column) == NULL)
        {
            gtk_column_view_column_set_visible (view_column, FALSE);
        }
        else
        {
            gtk_column_view_column_set_visible (view_column, TRUE);
        }
    }

    /* place columns in the correct order */
    for (GList *l = view_columns; l != NULL; l = l->next, column_i++)
    {
        gtk_column_view_insert_column (self->view_ui, column_i, l->data);
    }
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base_view);

    gtk_column_view_scroll_to (self->view_ui, position, NULL, flags, scroll);
}

static gint
nautilus_list_view_sort (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
    GQuark attribute_q = GPOINTER_TO_UINT (user_data);
    NautilusFile *file_a = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) a));
    NautilusFile *file_b = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) b));

    /* The reversed argument is FALSE because the columnview sorter handles that
     * itself and if we don't want to reverse the reverse. The directories_first
     * argument is also FALSE for the same reason: we don't want the columnview
     * sorter to reverse it (it would display directories last!); instead we
     * handle directories_first in a separate sorter. */
    return nautilus_file_compare_for_sort_by_attribute_q (file_a, file_b,
                                                          attribute_q,
                                                          FALSE /* directories_first */,
                                                          FALSE /* reversed */);
}

static gint
sort_directories_func (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
    gboolean *directories_first = user_data;

    if (*directories_first)
    {
        NautilusFile *file_a = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) a));
        NautilusFile *file_b = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) b));
        gboolean a_is_directory = nautilus_file_is_directory (file_a);
        gboolean b_is_directory = nautilus_file_is_directory (file_b);

        if (a_is_directory && !b_is_directory)
        {
            return GTK_ORDERING_SMALLER;
        }
        if (b_is_directory && !a_is_directory)
        {
            return GTK_ORDERING_LARGER;
        }
    }
    return GTK_ORDERING_EQUAL;
}

static void
update_columns_settings_from_metadata_and_preferences (NautilusListView *self)
{
    NautilusFile *file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));
    g_auto (GStrv) column_order = nautilus_column_get_column_order (file);
    g_auto (GStrv) visible_columns = nautilus_column_get_visible_columns (file);

    apply_columns_settings (self, column_order, visible_columns);
}

static GFile *
get_base_location (NautilusListView *self)
{
    GFile *base_location = NULL;

    if (self->search_directory != NULL)
    {
        g_autoptr (GFile) location = NULL;

        location = nautilus_query_get_location (nautilus_search_directory_get_query (self->search_directory));

        if (location != NULL &&
            !g_file_has_uri_scheme (location, SCHEME_RECENT) &&
            !g_file_has_uri_scheme (location, SCHEME_STARRED) &&
            !g_file_has_uri_scheme (location, SCHEME_TRASH))
        {
            base_location = g_steal_pointer (&location);
        }
    }

    return base_location;
}

static void
on_column_view_item_activated (GtkGridView *grid_view,
                               guint        position,
                               gpointer     user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);

    nautilus_list_base_activate_selection (NAUTILUS_LIST_BASE (self), FALSE);
}

static void
setup_row (GtkSignalListItemFactory *factory,
           GtkColumnViewRow         *columnviewrow,
           gpointer                  user_data)
{
    GtkExpression *expression;

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * columnviewrow:accessible-name :- columnviewrow:item:item:file:display-name */
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_FILE, expression, "display-name");
    gtk_expression_bind (expression, columnviewrow, "accessible-label", columnviewrow);
}

static GtkColumnView *
create_view_ui (NautilusListView *self)
{
    NautilusViewModel *model;
    GtkWidget *widget;
    g_autoptr (GtkListItemFactory) row_factory = gtk_signal_list_item_factory_new ();

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    widget = gtk_column_view_new (GTK_SELECTION_MODEL (model));

    gtk_widget_set_hexpand (widget, TRUE);

    g_signal_connect (row_factory, "setup", G_CALLBACK (setup_row), self);

    /* We don't use the built-in child activation feature for click because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkColumnView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_column_view_set_single_click_activate (GTK_COLUMN_VIEW (widget), FALSE);
    gtk_column_view_set_enable_rubberband (GTK_COLUMN_VIEW (widget), TRUE);
    gtk_column_view_set_tab_behavior (GTK_COLUMN_VIEW (widget), GTK_LIST_TAB_ITEM);
    gtk_column_view_set_row_factory (GTK_COLUMN_VIEW (widget), row_factory);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (widget, "activate", G_CALLBACK (on_column_view_item_activated), self);

    return GTK_COLUMN_VIEW (widget);
}

static GtkWidget *
create_column_editor (NautilusListView *view)
{
    GtkWidget *column_chooser;
    NautilusFile *file;

    file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (view));
    column_chooser = nautilus_column_chooser_new (file);
    gtk_window_set_transient_for (GTK_WINDOW (column_chooser),
                                  GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (view))));

    g_signal_connect_swapped (column_chooser, "changed",
                              G_CALLBACK (apply_columns_settings),
                              view);

    return column_chooser;
}

void
nautilus_list_view_present_column_editor (NautilusListView *self)
{
    if (self->column_editor == NULL)
    {
        self->column_editor = create_column_editor (self);
        g_object_add_weak_pointer (G_OBJECT (self->column_editor),
                                   (gpointer *) &self->column_editor);
    }

    gtk_window_present (GTK_WINDOW (self->column_editor));
}

static void
on_sorter_changed (GtkSorter       *sorter,
                   GtkSorterChange  change,
                   gpointer         user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);

    /* Notify about changes not effected by nautilus_list_base_set_sort_state(),
     * such as when the user clicks the column headers. This is important to
     * update the selected item in the sort menu, assuming this property is
     * bound to the state of the "view.sort" action. */
    g_object_notify (G_OBJECT (self), "sort-state");
}

static GVariant *
real_get_sort_state (NautilusListBase *list_base)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base);
    GtkColumnViewSorter *column_view_sorter = GTK_COLUMN_VIEW_SORTER (gtk_column_view_get_sorter (self->view_ui));
    GtkColumnViewColumn *primary;
    const char *sort_text;
    gboolean reversed;

    primary = gtk_column_view_sorter_get_primary_sort_column (column_view_sorter);

    if (primary == NULL)
    {
        return NULL;
    }

    sort_text = gtk_column_view_column_get_id (primary);
    reversed = gtk_column_view_sorter_get_primary_sort_order (column_view_sorter);

    return g_variant_take_ref (g_variant_new ("(sb)", sort_text, reversed));
}

static void
real_set_sort_state (NautilusListBase *list_base,
                     GVariant         *value)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base);
    GtkSorter *column_view_sorter = gtk_column_view_get_sorter (self->view_ui);
    const gchar *target_name;
    gboolean reversed;
    GListModel *view_columns;
    g_autoptr (GtkColumnViewColumn) sort_column = NULL;

    g_variant_get (value, "(&sb)", &target_name, &reversed);

    view_columns = gtk_column_view_get_columns (self->view_ui);
    for (guint i = 0; i < g_list_model_get_n_items (view_columns); i++)
    {
        g_autoptr (GtkColumnViewColumn) view_column = NULL;
        const char *attribute;

        view_column = g_list_model_get_item (view_columns, i);
        attribute = gtk_column_view_column_get_id (view_column);
        if (g_strcmp0 (target_name, attribute) == 0)
        {
            sort_column = g_steal_pointer (&view_column);
            break;
        }
    }

    g_signal_handlers_block_by_func (column_view_sorter, on_sorter_changed, self);
    /* Clear sorting before setting new sort column, to avoid double triangle,
     * as per https://gitlab.gnome.org/GNOME/gtk/-/issues/4696#note_1578945 */
    gtk_column_view_sort_by_column (self->view_ui, NULL, FALSE);

    gtk_column_view_sort_by_column (self->view_ui, sort_column, reversed);

    g_signal_handlers_unblock_by_func (column_view_sorter, on_sorter_changed, self);
}

static void
real_set_zoom_level (NautilusListBase *list_base,
                     int               new_level)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base);

    g_return_if_fail (new_level >= list_view_info.zoom_level_min &&
                      new_level <= list_view_info.zoom_level_max);

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
update_sort_directories_first (NautilusListView *self)
{
    NautilusFile *directory_as_file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));

    if (nautilus_file_is_in_search (directory_as_file))
    {
        self->directories_first = FALSE;
    }
    else
    {
        self->directories_first = g_settings_get_boolean (gtk_filechooser_preferences,
                                                          NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    }

    nautilus_view_model_sort (nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self)));
}

static void
nautilus_list_view_setup_directory (NautilusListBase  *list_base,
                                    NautilusDirectory *new_directory)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base);
    NautilusViewModel *model;
    NautilusFile *file;

    NAUTILUS_LIST_BASE_CLASS (nautilus_list_view_parent_class)->setup_directory (list_base, new_directory);

    g_clear_object (&self->search_directory);
    if (NAUTILUS_IS_SEARCH_DIRECTORY (new_directory))
    {
        self->search_directory = g_object_ref (NAUTILUS_SEARCH_DIRECTORY (new_directory));
    }

    update_columns_settings_from_metadata_and_preferences (self);

    self->expand_as_a_tree = g_settings_get_boolean (nautilus_list_view_preferences,
                                                     NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

    self->path_attribute_q = 0;
    g_clear_object (&self->file_path_base_location);
    file = nautilus_list_base_get_directory_as_file (list_base);
    if (nautilus_file_is_in_trash (file))
    {
        self->path_attribute_q = g_quark_from_string ("trash_orig_path");
        self->file_path_base_location = get_base_location (self);
    }
    else if (nautilus_file_is_in_search (file) ||
             nautilus_file_is_in_recent (file) ||
             nautilus_file_is_in_starred (file))
    {
        self->path_attribute_q = g_quark_from_string ("where");
        self->file_path_base_location = get_base_location (self);

        /* Forcefully disabling tree in these special locations because this
         * view and its model currently don't expect the same file appearing
         * more than once.
         *
         * NautilusFilesView still has support for the same file being present
         * in multiple directories (struct FileAndDirectory), so, if someone
         * cares enough about expanding folders in these special locations:
         * TODO: Making the model items aware of their current model instead of
         * relying on `nautilus_file_get_parent()`. */
        self->expand_as_a_tree = FALSE;
    }

    update_sort_directories_first (self);

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_expand_as_a_tree (model, self->expand_as_a_tree);
}

static gint
get_default_zoom_level (void)
{
    gint default_zoom_level = g_settings_get_enum (nautilus_list_view_preferences,
                                                   NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

    /* Sanitize preference value. */
    return CLAMP (default_zoom_level,
                  list_view_info.zoom_level_min,
                  list_view_info.zoom_level_max);
}

static NautilusViewItem *
real_get_backing_item (NautilusListBase *list_base)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base);

    if (!self->expand_as_a_tree)
    {
        return NULL;
    }

    /* If we are using tree expanders use the items parent, unless it
     * is an expanded folder, in which case we should use that folder directly.
     * When dealing with multiple selections, use the same rules, but only
     * if a common parent exists. */
    NautilusViewModel *model = nautilus_list_base_get_model (list_base);
    g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (model));
    g_autoptr (GtkTreeListRow) common_parent = NULL;
    GtkBitsetIter iter;
    guint i;

    for (gtk_bitset_iter_init_first (&iter, selection, &i);
         gtk_bitset_iter_is_valid (&iter);
         gtk_bitset_iter_next (&iter, &i))
    {
        g_autoptr (GtkTreeListRow) row = g_list_model_get_item (G_LIST_MODEL (model), i);
        g_autoptr (GtkTreeListRow) parent = gtk_tree_list_row_get_parent (row);
        GtkTreeListRow *current_parent = gtk_tree_list_row_get_expanded (row) ? row : parent;

        if (current_parent == NULL)
        {
            return NULL;
        }

        if (common_parent == NULL)
        {
            common_parent = g_object_ref (current_parent);
        }
        else if (current_parent != common_parent)
        {
            g_clear_object (&common_parent);
            break;
        }
    }

    if (common_parent != NULL)
    {
        return gtk_tree_list_row_get_item (common_parent);
    }

    return NULL;
}

typedef struct
{
    NautilusListView *self;
    NautilusViewItem *item;
    NautilusDirectory *directory;
} UnloadDelayData;

static void
unload_delay_data_free (UnloadDelayData *unload_data)
{
    g_clear_weak_pointer (&unload_data->self);
    g_clear_object (&unload_data->item);
    g_clear_object (&unload_data->directory);

    g_free (unload_data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (UnloadDelayData, unload_delay_data_free)

static UnloadDelayData *
unload_delay_data_new (NautilusListView  *self,
                       NautilusViewItem  *item,
                       NautilusDirectory *directory)
{
    UnloadDelayData *unload_data;

    unload_data = g_new0 (UnloadDelayData, 1);
    g_set_weak_pointer (&unload_data->self, self);
    g_set_object (&unload_data->item, item);
    g_set_object (&unload_data->directory, directory);

    return unload_data;
}

static gboolean
unload_file_timeout (gpointer data)
{
    g_autoptr (UnloadDelayData) unload_data = data;
    NautilusListView *self = unload_data->self;
    NautilusViewModel *model;

    if (unload_data->self == NULL)
    {
        return G_SOURCE_REMOVE;
    }
    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (model)); i++)
    {
        g_autoptr (GtkTreeListRow) row = g_list_model_get_item (G_LIST_MODEL (model), i);
        g_autoptr (NautilusViewItem) item = gtk_tree_list_row_get_item (row);
        if (item != NULL && item == unload_data->item)
        {
            if (gtk_tree_list_row_get_expanded (row))
            {
                /* It has been expanded again before the timeout. Do nothing. */
                return G_SOURCE_REMOVE;
            }
            break;
        }
    }

    if (nautilus_files_view_has_subdirectory (NAUTILUS_FILES_VIEW (self),
                                              unload_data->directory))
    {
        nautilus_files_view_remove_subdirectory (NAUTILUS_FILES_VIEW (self),
                                                 unload_data->directory);
    }

    /* The model holds a GListStore for every subdirectory. Empty it. */
    nautilus_view_model_clear_subdirectory (model, unload_data->item);

    return G_SOURCE_REMOVE;
}

static void
on_subdirectory_done_loading (NautilusDirectory *directory,
                              GtkTreeListRow    *row)
{
    g_autoptr (NautilusViewItem) item = NULL;

    g_signal_handlers_disconnect_by_func (directory, on_subdirectory_done_loading, row);

    item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
    nautilus_view_item_set_loading (item, FALSE);

    if (!nautilus_directory_is_not_empty (directory))
    {
        GtkWidget *name_cell = nautilus_view_item_get_item_ui (item);
        GtkTreeExpander *expander = nautilus_name_cell_get_expander (NAUTILUS_NAME_CELL (name_cell));

        gtk_tree_expander_set_hide_expander (expander, TRUE);
    }
}

static void
on_row_expanded_changed (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gobject);
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);
    g_autoptr (NautilusViewItem) item = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    gboolean expanded;

    item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
    if (item == NULL)
    {
        /* Row has been destroyed. */
        return;
    }

    directory = nautilus_directory_get_for_file (nautilus_view_item_get_file (item));
    expanded = gtk_tree_list_row_get_expanded (row);
    if (expanded)
    {
        if (!nautilus_files_view_has_subdirectory (NAUTILUS_FILES_VIEW (self), directory))
        {
            nautilus_files_view_add_subdirectory (NAUTILUS_FILES_VIEW (self), directory);
        }
        if (!nautilus_directory_are_all_files_seen (directory))
        {
            nautilus_view_item_set_loading (item, TRUE);

            g_signal_connect_object (directory,
                                     "done-loading",
                                     G_CALLBACK (on_subdirectory_done_loading),
                                     row,
                                     0);
        }
    }
    else
    {
        nautilus_view_item_set_loading (item, FALSE);
        g_timeout_add_seconds (COLLAPSE_TO_UNLOAD_DELAY,
                               unload_file_timeout,
                               unload_delay_data_new (self, item, directory));
    }
}

static gboolean
tree_expander_shortcut_cb (GtkWidget *widget,
                           GVariant  *args,
                           gpointer   user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (widget);
    GtkWidget *child;
    g_autoptr (NautilusViewItem) item = NULL;
    GtkTreeExpander *expander;
    GtkTreeListRow *row;
    GtkTextDirection direction = gtk_widget_get_direction (widget);
    char *action;
    guint keyval = GPOINTER_TO_INT (user_data);

    if (!self->expand_as_a_tree)
    {
        return FALSE;
    }

    /* Hack to find the focus item. */
    child = gtk_root_get_focus (gtk_widget_get_root (widget));
    while (child != NULL && !NAUTILUS_IS_VIEW_CELL (child))
    {
        child = gtk_widget_get_first_child (child);
    }

    if (!NAUTILUS_IS_VIEW_CELL (child))
    {
        return FALSE;
    }

    /* The name cell might not be the first column (the user may have changed
     * column order), but the expander is always on the name cell. */
    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (child));
    expander = nautilus_name_cell_get_expander (NAUTILUS_NAME_CELL (nautilus_view_item_get_item_ui (item)));
    row = gtk_tree_expander_get_list_row (expander);
    /* End of hack. */

    if ((keyval == GDK_KEY_Right && direction == GTK_TEXT_DIR_LTR) ||
        (keyval == GDK_KEY_Left && direction == GTK_TEXT_DIR_RTL))
    {
        action = "listitem.expand";
    }
    else
    {
        action = "listitem.collapse";
    }

    if (!gtk_tree_list_row_get_expanded (row) &&
        g_strcmp0 (action, "listitem.collapse") == 0)
    {
        g_autoptr (GtkTreeListRow) parent = gtk_tree_list_row_get_parent (row);

        if (parent != NULL)
        {
            guint parent_position = gtk_tree_list_row_get_position (parent);

            g_return_val_if_fail (parent_position != GTK_INVALID_LIST_POSITION, FALSE);

            gtk_column_view_scroll_to (self->view_ui, parent_position, NULL,
                                       GTK_LIST_SCROLL_FOCUS | GTK_LIST_SCROLL_SELECT, NULL);
        }
    }
    else
    {
        gtk_widget_activate_action (GTK_WIDGET (expander), action, NULL);
    }

    return TRUE;
}

static void
setup_name_cell (GtkSignalListItemFactory *factory,
                 GtkColumnViewCell        *listitem,
                 gpointer                  user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);
    NautilusViewCell *cell;

    cell = nautilus_name_cell_new (NAUTILUS_LIST_BASE (self));
    gtk_column_view_cell_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell);
    setup_cell_hover_inner_target (cell, nautilus_name_cell_get_content (NAUTILUS_NAME_CELL (cell)));

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    nautilus_name_cell_set_path (NAUTILUS_NAME_CELL (cell),
                                 self->path_attribute_q,
                                 self->file_path_base_location);
    if (self->search_directory != NULL)
    {
        nautilus_name_cell_show_snippet (NAUTILUS_NAME_CELL (cell));
    }

    if (self->expand_as_a_tree)
    {
        GtkTreeExpander *expander;

        expander = nautilus_name_cell_get_expander (NAUTILUS_NAME_CELL (cell));
        gtk_tree_expander_set_indent_for_icon (expander, TRUE);
        g_object_bind_property (listitem, "item",
                                expander, "list-row",
                                G_BINDING_SYNC_CREATE);
    }
}

static void
on_n_items_notify (GObject    *object,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
    GListModel *model = G_LIST_MODEL (object);
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (user_data);
    guint n_items = g_list_model_get_n_items (model);

    gtk_tree_expander_set_hide_expander (expander, n_items == 0);
}

static void
on_row_children_changed (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GtkTreeExpander *expander = user_data;
    GListModel *model = gtk_tree_list_row_get_children (GTK_TREE_LIST_ROW (gobject));

    if (model == NULL)
    {
        return;
    }

    g_signal_connect_object (model, "notify::n-items",
                             G_CALLBACK (on_n_items_notify), expander,
                             0);
}

static void
bind_name_cell (GtkSignalListItemFactory *factory,
                GtkColumnViewCell        *listitem,
                gpointer                  user_data)
{
    GtkWidget *cell;
    NautilusListView *self = user_data;
    g_autoptr (NautilusViewItem) item = NULL;

    cell = gtk_column_view_cell_get_child (listitem);
    item = get_view_item (listitem);

    nautilus_view_item_set_item_ui (item, gtk_column_view_cell_get_child (listitem));

    if (self->expand_as_a_tree)
    {
        GtkTreeExpander *expander = nautilus_name_cell_get_expander (NAUTILUS_NAME_CELL (cell));
        GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_column_view_cell_get_item (listitem));

        g_signal_connect_object (row,
                                 "notify::expanded",
                                 G_CALLBACK (on_row_expanded_changed),
                                 self, 0);
        g_signal_connect_object (row,
                                 "notify::children",
                                 G_CALLBACK (on_row_children_changed),
                                 expander, 0);
    }
}

static void
unbind_name_cell (GtkSignalListItemFactory *factory,
                  GtkColumnViewCell        *listitem,
                  gpointer                  user_data)
{
    NautilusListView *self = user_data;
    g_autoptr (NautilusViewItem) item = NULL;

    item = get_view_item (listitem);
    if (item == NULL)
    {
        /* The row is gone */
        return;
    }
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (item));

    nautilus_view_item_set_item_ui (item, NULL);

    if (self->expand_as_a_tree)
    {
        g_signal_handlers_disconnect_by_func (gtk_column_view_cell_get_item (listitem),
                                              on_row_expanded_changed,
                                              self);
        g_signal_handlers_disconnect_by_func (gtk_column_view_cell_get_item (listitem),
                                              on_row_children_changed,
                                              self);
    }
}

static void
setup_star_cell (GtkSignalListItemFactory *factory,
                 GtkColumnViewCell        *listitem,
                 gpointer                  user_data)
{
    NautilusViewCell *cell;

    cell = nautilus_star_cell_new (NAUTILUS_LIST_BASE (user_data));
    gtk_column_view_cell_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell);
    setup_cell_hover (cell);
}

static void
setup_label_cell (GtkSignalListItemFactory *factory,
                  GtkColumnViewCell        *listitem,
                  gpointer                  user_data)
{
    NautilusListView *self = user_data;
    NautilusColumn *nautilus_column;
    NautilusViewCell *cell;

    nautilus_column = g_hash_table_lookup (self->factory_to_column_map, factory);

    cell = nautilus_label_cell_new (NAUTILUS_LIST_BASE (user_data), nautilus_column);
    gtk_column_view_cell_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell);
    setup_cell_hover (cell);
}

static void
setup_view_columns (NautilusListView *self)
{
    GtkListItemFactory *factory;
    g_autolist (NautilusColumn) nautilus_columns = NULL;

    nautilus_columns = nautilus_get_all_columns ();

    self->factory_to_column_map = g_hash_table_new_full (g_direct_hash,
                                                         g_direct_equal,
                                                         NULL,
                                                         g_object_unref);

    for (GList *l = nautilus_columns; l != NULL; l = l->next)
    {
        NautilusColumn *nautilus_column = NAUTILUS_COLUMN (l->data);
        g_autofree gchar *name = NULL;
        g_autofree gchar *label = NULL;
        GQuark attribute_q = 0;
        GtkSortType sort_order;
        g_autoptr (GtkCustomSorter) sorter = NULL;
        g_autoptr (GtkColumnViewColumn) view_column = NULL;

        g_object_get (nautilus_column,
                      "name", &name,
                      "label", &label,
                      "attribute_q", &attribute_q,
                      "default-sort-order", &sort_order,
                      NULL);

        sorter = gtk_custom_sorter_new (nautilus_list_view_sort,
                                        GUINT_TO_POINTER (attribute_q),
                                        NULL);

        factory = gtk_signal_list_item_factory_new ();
        view_column = gtk_column_view_column_new (NULL, factory);
        gtk_column_view_column_set_expand (view_column, FALSE);
        gtk_column_view_column_set_resizable (view_column, TRUE);
        gtk_column_view_column_set_title (view_column, label);
        gtk_column_view_column_set_sorter (view_column, GTK_SORTER (sorter));

        if (!strcmp (name, "name"))
        {
            g_signal_connect (factory, "setup", G_CALLBACK (setup_name_cell), self);
            g_signal_connect (factory, "bind", G_CALLBACK (bind_name_cell), self);
            g_signal_connect (factory, "unbind", G_CALLBACK (unbind_name_cell), self);

            gtk_column_view_column_set_expand (view_column, TRUE);
        }
        else if (g_strcmp0 (name, "starred") == 0)
        {
            g_signal_connect (factory, "setup", G_CALLBACK (setup_star_cell), self);

            gtk_column_view_column_set_title (view_column, "");
            gtk_column_view_column_set_resizable (view_column, FALSE);

            self->star_column = view_column;
        }
        else
        {
            g_signal_connect (factory, "setup", G_CALLBACK (setup_label_cell), self);
        }

        gtk_column_view_append_column (self->view_ui, view_column);
        gtk_column_view_column_set_id (view_column, name);

        g_hash_table_insert (self->factory_to_column_map,
                             factory,
                             g_object_ref (nautilus_column));
    }
}

static void
nautilus_list_view_reload (NautilusListView *self)
{
    gtk_widget_activate_action (GTK_WIDGET (self), "win.reload", NULL);
}

static void
nautilus_list_view_init (NautilusListView *self)
{
    NautilusViewModel *model;
    GtkWidget *content_widget;
    GtkSorter *column_view_sorter;
    g_autoptr (GtkCustomSorter) directories_sorter = NULL;
    g_autoptr (GtkMultiSorter) sorter = NULL;
    GtkEventController *controller;
    GtkShortcut *shortcut;

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-list-view");

    g_signal_connect_object (nautilus_list_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                             G_CALLBACK (update_columns_settings_from_metadata_and_preferences),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (nautilus_list_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                             G_CALLBACK (update_columns_settings_from_metadata_and_preferences),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (nautilus_list_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
                             G_CALLBACK (nautilus_list_view_reload),
                             self,
                             G_CONNECT_SWAPPED);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));

    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));

    setup_view_columns (self);

    directories_sorter = gtk_custom_sorter_new (sort_directories_func, &self->directories_first, NULL);

    sorter = gtk_multi_sorter_new ();
    column_view_sorter = gtk_column_view_get_sorter (self->view_ui);
    gtk_multi_sorter_append (sorter, g_object_ref (GTK_SORTER (directories_sorter)));
    gtk_multi_sorter_append (sorter, g_object_ref (column_view_sorter));

    g_signal_connect (column_view_sorter, "changed", G_CALLBACK (on_sorter_changed), self);
    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_widget),
                                   GTK_WIDGET (self->view_ui));

    g_signal_connect_object (gtk_filechooser_preferences,
                             "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                             G_CALLBACK (update_sort_directories_first),
                             self,
                             G_CONNECT_SWAPPED);

    nautilus_list_base_set_zoom_level (NAUTILUS_LIST_BASE (self), get_default_zoom_level ());

    /* Set up tree expand/collapse shortcuts in capture phase otherwise they
     * would be handled by GtkListBase's cursor movement shortcuts. */
    controller = gtk_shortcut_controller_new ();
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Left, 0),
                                 gtk_callback_action_new (tree_expander_shortcut_cb, GINT_TO_POINTER (GDK_KEY_Left), NULL));
    gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
    shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Right, 0),
                                 gtk_callback_action_new (tree_expander_shortcut_cb, GINT_TO_POINTER (GDK_KEY_Right), NULL));
    gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
}

static void
nautilus_list_view_dispose (GObject *object)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (object);
    NautilusViewModel *model;

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_set_sorter (model, NULL);

    g_clear_object (&self->search_directory);

    g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
                                          update_columns_settings_from_metadata_and_preferences,
                                          self);
    g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
                                          nautilus_list_view_reload,
                                          self);

    g_clear_object (&self->file_path_base_location);
    g_clear_pointer (&self->factory_to_column_map, g_hash_table_destroy);

    g_signal_handlers_disconnect_by_func (gtk_column_view_get_sorter (self->view_ui), on_sorter_changed, self);

    G_OBJECT_CLASS (nautilus_list_view_parent_class)->dispose (object);
}

static void
nautilus_list_view_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_list_view_parent_class)->finalize (object);
}

static void
nautilus_list_view_class_init (NautilusListViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = nautilus_list_view_dispose;
    object_class->finalize = nautilus_list_view_finalize;

    list_base_view_class->get_backing_item = real_get_backing_item;
    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_view_ui = real_get_view_ui;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
    list_base_view_class->setup_directory = nautilus_list_view_setup_directory;
}

NautilusListView *
nautilus_list_view_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_LIST_VIEW,
                         "window-slot", slot,
                         NULL);
}
