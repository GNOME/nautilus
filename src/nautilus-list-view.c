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
#include "nautilus-search-directory.h"
#include "nautilus-star-cell.h"
#include "nautilus-tag-manager.h"

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

struct _NautilusListView
{
    NautilusListBase parent_instance;

    GtkColumnView *view_ui;

    GActionGroup *action_group;
    gint zoom_level;

    gboolean directories_first;
    gboolean expand_as_a_tree;

    GQuark path_attribute_q;
    GFile *file_path_base_location;

    GtkColumnViewColumn *star_column;
    GtkWidget *column_editor;
    GHashTable *factory_to_column_map;
    gboolean column_header_was_clicked;
};

G_DEFINE_TYPE (NautilusListView, nautilus_list_view, NAUTILUS_TYPE_LIST_BASE)

static const char *default_columns_for_recent[] =
{
    "name", "size", "recency", NULL
};

static const char *default_columns_for_trash[] =
{
    "name", "size", "trashed_on", NULL
};

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

static void
apply_columns_settings (NautilusListView  *self,
                        char             **column_order,
                        char             **visible_columns)
{
    g_autolist (NautilusColumn) all_columns = NULL;
    NautilusFile *file;
    NautilusDirectory *directory;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GList) view_columns = NULL;
    GListModel *old_view_columns;
    g_autoptr (GHashTable) visible_columns_hash = NULL;
    g_autoptr (GHashTable) old_view_columns_hash = NULL;
    int column_i = 0;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));
    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (self));
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        g_autoptr (NautilusQuery) query = NULL;

        query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
        location = nautilus_query_get_location (query);
    }
    else
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
        nautilus_is_starred_directory (location))
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
real_scroll_to_item (NautilusListBase *list_base_view,
                     guint             position)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (list_base_view);
    GtkWidget *child;

    child = gtk_widget_get_last_child (GTK_WIDGET (self->view_ui));

    while (child != NULL && !GTK_IS_LIST_VIEW (child))
    {
        child = gtk_widget_get_prev_sibling (child);
    }

    if (child != NULL)
    {
        gtk_widget_activate_action (child, "list.scroll-to-item", "u", position);
    }
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

static char **
get_default_visible_columns (NautilusListView *self)
{
    NautilusFile *file;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));

    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

static char **
get_visible_columns (NautilusListView *self)
{
    NautilusFile *file;
    g_autofree gchar **visible_columns = NULL;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));

    visible_columns = nautilus_file_get_metadata_list (file,
                                                       NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);
    if (visible_columns == NULL || visible_columns[0] == NULL)
    {
        return get_default_visible_columns (self);
    }

    return g_steal_pointer (&visible_columns);
}

static char **
get_default_column_order (NautilusListView *self)
{
    NautilusFile *file;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));

    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

static char **
get_column_order (NautilusListView *self)
{
    NautilusFile *file;
    g_autofree gchar **column_order = NULL;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));

    column_order = nautilus_file_get_metadata_list (file,
                                                    NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

    if (column_order != NULL && column_order[0] != NULL)
    {
        return g_steal_pointer (&column_order);
    }

    return get_default_column_order (self);
}
static void
update_columns_settings_from_metadata_and_preferences (NautilusListView *self)
{
    g_auto (GStrv) column_order = get_column_order (self);
    g_auto (GStrv) visible_columns = get_visible_columns (self);

    apply_columns_settings (self, column_order, visible_columns);
}

static GFile *
get_base_location (NautilusListView *self)
{
    NautilusDirectory *directory;
    GFile *base_location = NULL;

    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (self));
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        g_autoptr (NautilusQuery) query = NULL;
        g_autoptr (GFile) location = NULL;

        query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
        location = nautilus_query_get_location (query);

        if (!nautilus_is_recent_directory (location) &&
            !nautilus_is_starred_directory (location) &&
            !nautilus_is_trash_directory (location))
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

    nautilus_files_view_activate_selection (NAUTILUS_FILES_VIEW (self));
}

static GtkColumnView *
create_view_ui (NautilusListView *self)
{
    NautilusViewModel *model;
    GtkWidget *widget;

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    widget = gtk_column_view_new (GTK_SELECTION_MODEL (model));

    gtk_widget_set_hexpand (widget, TRUE);


    /* We don't use the built-in child activation feature for click because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkColumnView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_column_view_set_single_click_activate (GTK_COLUMN_VIEW (widget), FALSE);
    gtk_column_view_set_enable_rubberband (GTK_COLUMN_VIEW (widget), TRUE);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (widget, "activate", G_CALLBACK (on_column_view_item_activated), self);

    return GTK_COLUMN_VIEW (widget);
}

static void
column_chooser_changed_callback (NautilusColumnChooser *chooser,
                                 NautilusListView      *view)
{
    NautilusFile *file;
    char **visible_columns;
    char **column_order;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (view));

    nautilus_column_chooser_get_settings (chooser,
                                          &visible_columns,
                                          &column_order);

    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                     visible_columns);
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                     column_order);

    apply_columns_settings (view, column_order, visible_columns);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_set_from_arrays (NautilusColumnChooser  *chooser,
                                NautilusListView       *view,
                                char                  **visible_columns,
                                char                  **column_order)
{
    g_signal_handlers_block_by_func
        (chooser, G_CALLBACK (column_chooser_changed_callback), view);

    nautilus_column_chooser_set_settings (chooser,
                                          visible_columns,
                                          column_order);

    g_signal_handlers_unblock_by_func
        (chooser, G_CALLBACK (column_chooser_changed_callback), view);
}

static void
column_chooser_set_from_settings (NautilusColumnChooser *chooser,
                                  NautilusListView      *view)
{
    char **visible_columns;
    char **column_order;

    visible_columns = get_visible_columns (view);
    column_order = get_column_order (view);

    column_chooser_set_from_arrays (chooser, view,
                                    visible_columns, column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_use_default_callback (NautilusColumnChooser *chooser,
                                     NautilusListView      *view)
{
    NautilusFile *file;
    char **default_columns;
    char **default_order;

    file = nautilus_files_view_get_directory_as_file
               (NAUTILUS_FILES_VIEW (view));

    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

    /* set view values ourselves, as new metadata could not have been
     * updated yet.
     */
    default_columns = get_default_visible_columns (view);
    default_order = get_default_column_order (view);

    apply_columns_settings (view, default_order, default_columns);
    column_chooser_set_from_arrays (chooser, view,
                                    default_columns, default_order);

    g_strfreev (default_columns);
    g_strfreev (default_order);
}

static GtkWidget *
create_column_editor (NautilusListView *view)
{
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *window;
    AdwWindowTitle *window_title;
    GtkWidget *box;
    GtkWidget *column_chooser;
    NautilusFile *file;
    char *name;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-list-view-column-editor.ui");

    window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    gtk_window_set_transient_for (GTK_WINDOW (window),
                                  GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (view))));

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (view));
    name = nautilus_file_get_display_name (file);
    window_title = ADW_WINDOW_TITLE (gtk_builder_get_object (builder, "window_title"));
    adw_window_title_set_subtitle (window_title, name);
    g_free (name);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));

    column_chooser = nautilus_column_chooser_new (file);
    gtk_widget_set_vexpand (column_chooser, TRUE);
    gtk_box_append (GTK_BOX (box), column_chooser);

    g_signal_connect (column_chooser, "changed",
                      G_CALLBACK (column_chooser_changed_callback),
                      view);
    g_signal_connect (column_chooser, "use-default",
                      G_CALLBACK (column_chooser_use_default_callback),
                      view);

    column_chooser_set_from_settings
        (NAUTILUS_COLUMN_CHOOSER (column_chooser), view);

    return window;
}

static void
action_visible_columns (GSimpleAction *action,
                        GVariant      *state,
                        gpointer       user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);

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
    GtkColumnViewSorter *column_view_sorter = GTK_COLUMN_VIEW_SORTER (sorter);
    GActionGroup *action_group;
    gboolean reversed;
    GtkColumnViewColumn *primary;
    const char *sort_text;

    primary = gtk_column_view_sorter_get_primary_sort_column (column_view_sorter);

    if (primary == NULL)
    {
        return;
    }

    reversed = gtk_column_view_sorter_get_primary_sort_order (column_view_sorter);
    action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));

    sort_text = gtk_column_view_column_get_id (primary);

    self->column_header_was_clicked = TRUE;

    g_action_group_change_action_state (action_group, "sort",
                                        g_variant_new ("(sb)", sort_text, reversed));
}

static void
action_sort_order_changed (GSimpleAction *action,
                           GVariant      *value,
                           gpointer       user_data)
{
    const gchar *target_name;
    gboolean reversed;
    NautilusListView *self;
    GListModel *view_columns;
    g_autoptr (GtkColumnViewColumn) sort_column = NULL;
    GtkSorter *sorter;

    /* Don't resort if the action is in the same state as before */
    if (g_variant_equal (value, g_action_get_state (G_ACTION (action))))
    {
        return;
    }

    self = NAUTILUS_LIST_VIEW (user_data);
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

    sorter = gtk_column_view_get_sorter (self->view_ui);

    /* Initiate default sort or sort from the view menu, but not when the header was clicked */
    if (!self->column_header_was_clicked)
    {
        g_signal_handlers_block_by_func (sorter, on_sorter_changed, self);
        gtk_column_view_sort_by_column (self->view_ui, NULL, FALSE);
        gtk_column_view_sort_by_column (self->view_ui, sort_column, reversed);
        g_signal_handlers_unblock_by_func (sorter, on_sorter_changed, self);
    }

    self->column_header_was_clicked = FALSE;

    set_directory_sort_metadata (nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self)),
                                 target_name,
                                 reversed);

    g_simple_action_set_state (action, value);
}

static void
set_zoom_level (NautilusListView *self,
                guint             new_level)
{
    self->zoom_level = new_level;

    nautilus_list_base_set_icon_size (NAUTILUS_LIST_BASE (self),
                                      get_icon_size_for_zoom_level (new_level));

    if (self->zoom_level == NAUTILUS_LIST_ZOOM_LEVEL_SMALL)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self), "compact");
    }
    else
    {
        gtk_widget_remove_css_class (GTK_WIDGET (self), "compact");
    }

    nautilus_files_view_update_toolbar_menus (NAUTILUS_FILES_VIEW (self));
}

static void
action_zoom_to_level (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);
    int zoom_level;

    zoom_level = g_variant_get_int32 (state);
    set_zoom_level (self, zoom_level);
    g_simple_action_set_state (G_SIMPLE_ACTION (action), state);

    if (g_settings_get_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL) != zoom_level)
    {
        g_settings_set_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                             zoom_level);
    }
}

const GActionEntry list_view_entries[] =
{
    { "visible-columns", action_visible_columns },
    { "sort", NULL, "(sb)", "('invalid',false)", action_sort_order_changed },
    { "zoom-to-level", NULL, NULL, "1", action_zoom_to_level }
};

static void
real_begin_loading (NautilusFilesView *files_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (files_view);
    NautilusViewModel *model;
    NautilusFile *file;

    NAUTILUS_FILES_VIEW_CLASS (nautilus_list_view_parent_class)->begin_loading (files_view);

    update_columns_settings_from_metadata_and_preferences (self);

    self->expand_as_a_tree = g_settings_get_boolean (nautilus_list_view_preferences,
                                                     NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

    self->path_attribute_q = 0;
    g_clear_object (&self->file_path_base_location);
    file = nautilus_files_view_get_directory_as_file (files_view);
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

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_expand_as_a_tree (model, self->expand_as_a_tree);
}

static void
real_bump_zoom_level (NautilusFilesView *files_view,
                      int                zoom_increment)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (files_view);
    NautilusListZoomLevel new_level;

    new_level = self->zoom_level + zoom_increment;

    if (new_level >= NAUTILUS_LIST_ZOOM_LEVEL_SMALL &&
        new_level <= NAUTILUS_LIST_ZOOM_LEVEL_LARGE)
    {
        g_action_group_change_action_state (self->action_group,
                                            "zoom-to-level",
                                            g_variant_new_int32 (new_level));
    }
}

static gint
get_default_zoom_level (void)
{
    NautilusListZoomLevel default_zoom_level;

    default_zoom_level = g_settings_get_enum (nautilus_list_view_preferences,
                                              NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

    /* Sanitize preference value. */
    return CLAMP (default_zoom_level,
                  NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
                  NAUTILUS_LIST_ZOOM_LEVEL_LARGE);
}

static void
real_restore_standard_zoom_level (NautilusFilesView *files_view)
{
    NautilusListView *self;

    self = NAUTILUS_LIST_VIEW (files_view);
    g_action_group_change_action_state (self->action_group,
                                        "zoom-to-level",
                                        g_variant_new_int32 (NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM));
}

static gboolean
real_can_zoom_in (NautilusFilesView *files_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (files_view);

    return self->zoom_level < NAUTILUS_LIST_ZOOM_LEVEL_LARGE;
}

static gboolean
real_can_zoom_out (NautilusFilesView *files_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (files_view);

    return self->zoom_level > NAUTILUS_LIST_ZOOM_LEVEL_SMALL;
}

static gboolean
real_is_zoom_level_default (NautilusFilesView *files_view)
{
    NautilusListView *self;
    guint icon_size;

    self = NAUTILUS_LIST_VIEW (files_view);
    icon_size = get_icon_size_for_zoom_level (self->zoom_level);

    return icon_size == NAUTILUS_LIST_ICON_SIZE_MEDIUM;
}

static void
real_sort_directories_first_changed (NautilusFilesView *files_view)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (files_view);

    self->directories_first = nautilus_files_view_should_sort_directories_first (NAUTILUS_FILES_VIEW (self));

    nautilus_view_model_sort (nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self)));
}

static guint
real_get_view_id (NautilusFilesView *files_view)
{
    return NAUTILUS_VIEW_LIST_ID;
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
    NautilusViewItem *item;

    g_signal_handlers_disconnect_by_func (directory, on_subdirectory_done_loading, row);

    item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
    nautilus_view_item_set_loading (item, FALSE);
}

static void
on_row_expanded_changed (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gobject);
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);
    NautilusViewItem *item;
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

static void
on_item_click_released_workaround (GtkGestureClick *gesture,
                                   gint             n_press,
                                   gdouble          x,
                                   gdouble          y,
                                   gpointer         user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListView *self = NAUTILUS_LIST_VIEW (nautilus_view_cell_get_view (cell));
    GdkModifierType modifiers;

    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    if (n_press == 1 &&
        modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
    {
        NautilusViewModel *model;
        g_autoptr (NautilusViewItem) item = NULL;
        guint i;

        model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
        item = nautilus_view_cell_get_item (cell);
        g_return_if_fail (item != NULL);
        i = nautilus_view_model_get_index (model, item);

        gtk_widget_activate_action (GTK_WIDGET (cell),
                                    "list.select-item",
                                    "(ubb)",
                                    i,
                                    modifiers & GDK_CONTROL_MASK,
                                    modifiers & GDK_SHIFT_MASK);
    }
}

/* This whole event handler is a workaround to a GtkColumnView bug: it
 * activates the list|select-item action twice, which may cause the
 * second activation to reverse the effects of the first:
 * https://gitlab.gnome.org/GNOME/gtk/-/issues/4819
 *
 * As a workaround, we are going to activate the action a 3rd time.
 * The third time is the charm, as the saying goes. */
static void
setup_selection_click_workaround (NautilusViewCell *cell)
{
    GtkEventController *controller;

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (cell), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), GDK_BUTTON_PRIMARY);
    g_signal_connect (controller, "released", G_CALLBACK (on_item_click_released_workaround), cell);
}

static void
setup_name_cell (GtkSignalListItemFactory *factory,
                 GtkListItem              *listitem,
                 gpointer                  user_data)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (user_data);
    NautilusViewCell *cell;

    cell = nautilus_name_cell_new (NAUTILUS_LIST_BASE (self));
    setup_cell_common (listitem, cell);

    nautilus_name_cell_set_path (NAUTILUS_NAME_CELL (cell),
                                 self->path_attribute_q,
                                 self->file_path_base_location);
    if (NAUTILUS_IS_SEARCH_DIRECTORY (nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (self))))
    {
        nautilus_name_cell_show_snippet (NAUTILUS_NAME_CELL (cell));
    }

    setup_selection_click_workaround (cell);

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

static gboolean
transform_model_n_items_to_boolean (GBinding     *binding,
                                    const GValue *from_value,
                                    GValue       *to_value,
                                    gpointer      data)
{
    guint n_items = g_value_get_uint (from_value);
    gboolean result;

    result = n_items == 0 ? TRUE : FALSE;
    g_value_set_boolean (to_value, result);

    return TRUE;
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

    g_object_bind_property_full (model, "n-items",
                                 expander, "hide-expander",
                                 G_BINDING_SYNC_CREATE,
                                 transform_model_n_items_to_boolean,
                                 NULL,
                                 NULL, NULL);
}

static void
bind_name_cell (GtkSignalListItemFactory *factory,
                GtkListItem              *listitem,
                gpointer                  user_data)
{
    GtkWidget *cell;
    NautilusListView *self = user_data;
    NautilusViewItem *item;

    cell = gtk_list_item_get_child (listitem);
    item = listitem_get_view_item (listitem);

    nautilus_view_item_set_item_ui (item, gtk_list_item_get_child (listitem));

    if (nautilus_view_cell_once (NAUTILUS_VIEW_CELL (cell)))
    {
        GtkWidget *row_widget;

        /* At the time of ::setup emission, the item ui has got no parent yet,
         * that's why we need to complete the widget setup process here, on the
         * first time ::bind is emitted. */
        row_widget = gtk_widget_get_parent (gtk_widget_get_parent (cell));

        gtk_accessible_update_relation (GTK_ACCESSIBLE (row_widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, cell, NULL,
                                        -1);
    }

    if (self->expand_as_a_tree)
    {
        GtkTreeExpander *expander = nautilus_name_cell_get_expander (NAUTILUS_NAME_CELL (cell));
        GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_list_item_get_item (listitem));

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
                  GtkListItem              *listitem,
                  gpointer                  user_data)
{
    NautilusListView *self = user_data;
    NautilusViewItem *item;

    item = listitem_get_view_item (listitem);
    if (item == NULL)
    {
        /* The row is gone */
        return;
    }
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (item));

    nautilus_view_item_set_item_ui (item, NULL);

    if (self->expand_as_a_tree)
    {
        g_signal_handlers_disconnect_by_func (gtk_list_item_get_item (listitem),
                                              on_row_expanded_changed,
                                              self);
        g_signal_handlers_disconnect_by_func (gtk_list_item_get_item (listitem),
                                              on_row_children_changed,
                                              self);
    }
}

static void
setup_star_cell (GtkSignalListItemFactory *factory,
                 GtkListItem              *listitem,
                 gpointer                  user_data)
{
    NautilusViewCell *cell;

    cell = nautilus_star_cell_new (NAUTILUS_LIST_BASE (user_data));
    setup_cell_common (listitem, cell);
    setup_selection_click_workaround (cell);
}

static void
setup_label_cell (GtkSignalListItemFactory *factory,
                  GtkListItem              *listitem,
                  gpointer                  user_data)
{
    NautilusListView *self = user_data;
    NautilusColumn *nautilus_column;
    NautilusViewCell *cell;

    nautilus_column = g_hash_table_lookup (self->factory_to_column_map, factory);

    cell = nautilus_label_cell_new (NAUTILUS_LIST_BASE (user_data), nautilus_column);
    setup_cell_common (listitem, cell);
    setup_selection_click_workaround (cell);
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
    NautilusWindowSlot *slot;

    slot = nautilus_files_view_get_nautilus_window_slot (NAUTILUS_FILES_VIEW (self));
    nautilus_window_slot_queue_reload (slot);
}

static void
nautilus_list_view_init (NautilusListView *self)
{
    NautilusViewModel *model;
    GtkWidget *content_widget;
    GtkSorter *column_view_sorter;
    g_autoptr (GtkCustomSorter) directories_sorter = NULL;
    g_autoptr (GtkMultiSorter) sorter = NULL;

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

    self->directories_first = nautilus_files_view_should_sort_directories_first (NAUTILUS_FILES_VIEW (self));
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

    self->action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     list_view_entries,
                                     G_N_ELEMENTS (list_view_entries),
                                     self);

    self->zoom_level = get_default_zoom_level ();
    g_action_group_change_action_state (nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self)),
                                        "zoom-to-level", g_variant_new_int32 (self->zoom_level));
}

static void
nautilus_list_view_dispose (GObject *object)
{
    NautilusListView *self = NAUTILUS_LIST_VIEW (object);
    NautilusViewModel *model;

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_set_sorter (model, NULL);

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
    NautilusFilesViewClass *files_view_class = NAUTILUS_FILES_VIEW_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = nautilus_list_view_dispose;
    object_class->finalize = nautilus_list_view_finalize;

    files_view_class->begin_loading = real_begin_loading;
    files_view_class->bump_zoom_level = real_bump_zoom_level;
    files_view_class->can_zoom_in = real_can_zoom_in;
    files_view_class->can_zoom_out = real_can_zoom_out;
    files_view_class->sort_directories_first_changed = real_sort_directories_first_changed;
    files_view_class->get_view_id = real_get_view_id;
    files_view_class->restore_standard_zoom_level = real_restore_standard_zoom_level;
    files_view_class->is_zoom_level_default = real_is_zoom_level_default;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_view_ui = real_get_view_ui;
    list_base_view_class->scroll_to_item = real_scroll_to_item;
}

NautilusListView *
nautilus_list_view_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_LIST_VIEW,
                         "window-slot", slot,
                         NULL);
}
