#include "nautilus-view-model.h"
#include "nautilus-view-item.h"
#include "nautilus-directory.h"
#include "nautilus-global-preferences.h"

/**
 * NautilusViewModel:
 *
 * Internal structure goes like this:
 *
 * selection_model : GtkSelectionModel<GtkTreeListRow<NautilusViewItem>>
 *  |
 *  +-- sort_model : GtkSectionModel<GtkTreeListRow<NautilusViewItem>>
 *       |
 *       +-- tree_model : GtkTreeListModel<GtkTreeListRow<NautilusViewItem>>
 *            |
 *            +-- root_filter_model : GtkFilterListModel<NautilusViewItem>
 *            |    |
 *            |    +-- GListStore<NautilusViewItem>
 *            |
 *         (0...n) GtkFilterListModel<NautilusViewItem>  //subdirectories
 *                 |
 *                 +-- GListStore<NautilusViewItem>
 *
 * The overall model item type is GtkTreeListRow, but the :filter and :sorter
 * properties are meant for internal models whose item type is NautilusViewItem.
 */

struct _NautilusViewModel
{
    GObject parent_instance;

    GHashTable *map_files_to_model;
    GHashTable *directory_reverse_map;

    GtkFilterListModel *root_filter_model;
    GtkTreeListModel *tree_model;
    GtkSortListModel *sort_model;
    GtkSelectionModel *selection_model;

    gboolean single_selection;
    gboolean expand_as_a_tree;
    GList *cut_files;
};

static inline GListStore *
get_directory_store (NautilusViewModel *self,
                     NautilusFile      *directory)
{
    GListStore *store;

    store = g_hash_table_lookup (self->directory_reverse_map, directory);
    if (store == NULL)
    {
        store = G_LIST_STORE (gtk_filter_list_model_get_model (self->root_filter_model));
    }

    return store;
}

static inline GtkTreeListRow *
get_child_row (NautilusViewModel *self,
               GtkTreeListRow    *parent,
               guint              position)
{
    if (parent != NULL)
    {
        return gtk_tree_list_row_get_child_row (parent, position);
    }
    else
    {
        return gtk_tree_list_model_get_child_row (self->tree_model, position);
    }
}

static GType
nautilus_view_model_get_item_type (GListModel *list)
{
    return GTK_TYPE_TREE_LIST_ROW;
}

static guint
nautilus_view_model_get_n_items (GListModel *list)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (list);

    if (self->tree_model == NULL)
    {
        return 0;
    }

    return g_list_model_get_n_items (G_LIST_MODEL (self->tree_model));
}

static gpointer
nautilus_view_model_get_item (GListModel *list,
                              guint       position)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (list);

    if (self->sort_model == NULL)
    {
        return NULL;
    }

    return g_list_model_get_item (G_LIST_MODEL (self->sort_model), position);
}

static void
nautilus_view_model_list_model_init (GListModelInterface *iface)
{
    iface->get_item_type = nautilus_view_model_get_item_type;
    iface->get_n_items = nautilus_view_model_get_n_items;
    iface->get_item = nautilus_view_model_get_item;
}

static void
nautilus_view_model_get_section (GtkSectionModel *model,
                                 guint            position,
                                 guint           *out_start,
                                 guint           *out_end)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    gtk_section_model_get_section (GTK_SECTION_MODEL (self->sort_model), position, out_start, out_end);
}

static void
nautilus_view_model_section_model_init (GtkSectionModelInterface *iface)
{
    iface->get_section = nautilus_view_model_get_section;
}

static gboolean
nautilus_view_model_is_selected (GtkSelectionModel *model,
                                 guint              position)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_is_selected (self->selection_model, position);
}

static GtkBitset *
nautilus_view_model_get_selection_in_range (GtkSelectionModel *model,
                                            guint              pos,
                                            guint              n_items)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_get_selection_in_range (self->selection_model, pos, n_items);
}

static gboolean
nautilus_view_model_select_item (GtkSelectionModel *model,
                                 guint              position,
                                 gboolean           unselect_rest)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_select_item (self->selection_model, position, unselect_rest);
}

static gboolean
nautilus_view_model_set_selection (GtkSelectionModel *model,
                                   GtkBitset         *selected,
                                   GtkBitset         *mask)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_set_selection (self->selection_model, selected, mask);
}


static gboolean
nautilus_view_model_unselect_item (GtkSelectionModel *model,
                                   guint              position)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_unselect_item (self->selection_model, position);
}

static gboolean
nautilus_view_model_unselect_all (GtkSelectionModel *model)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);

    return gtk_selection_model_unselect_all (self->selection_model);
}


static void
nautilus_view_model_selection_model_init (GtkSelectionModelInterface *iface)
{
    iface->is_selected = nautilus_view_model_is_selected;
    iface->get_selection_in_range = nautilus_view_model_get_selection_in_range;
    iface->select_item = nautilus_view_model_select_item;
    iface->set_selection = nautilus_view_model_set_selection;
    iface->unselect_item = nautilus_view_model_unselect_item;
    iface->unselect_all = nautilus_view_model_unselect_all;
}

G_DEFINE_TYPE_WITH_CODE (NautilusViewModel, nautilus_view_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                nautilus_view_model_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL,
                                                nautilus_view_model_section_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                                nautilus_view_model_selection_model_init))

enum
{
    PROP_0,
    PROP_FILTER,
    PROP_SINGLE_SELECTION,
    PROP_SORTER,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
dispose (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    if (self->selection_model != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->selection_model,
                                              gtk_selection_model_selection_changed,
                                              self);
        g_object_unref (self->selection_model);
        self->selection_model = NULL;
    }

    if (self->sort_model != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->sort_model,
                                              g_list_model_items_changed,
                                              self);
        g_signal_handlers_disconnect_by_func (self->sort_model,
                                              gtk_section_model_sections_changed,
                                              self);
        g_object_unref (self->sort_model);
        self->sort_model = NULL;
    }

    g_clear_object (&self->tree_model);
    g_clear_object (&self->root_filter_model);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->finalize (object);

    g_hash_table_destroy (self->map_files_to_model);
    g_hash_table_destroy (self->directory_reverse_map);

    g_clear_list (&self->cut_files, g_object_unref);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    switch (prop_id)
    {
        case PROP_FILTER:
        {
            g_value_set_object (value, nautilus_view_model_get_filter (self));
        }
        break;

        case PROP_SINGLE_SELECTION:
        {
            g_value_set_boolean (value, nautilus_view_model_get_single_selection (self));
        }
        break;

        case PROP_SORTER:
        {
            g_value_set_object (value, nautilus_view_model_get_sorter (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    switch (prop_id)
    {
        case PROP_FILTER:
        {
            nautilus_view_model_set_filter (self, g_value_get_object (value));
        }
        break;

        case PROP_SINGLE_SELECTION:
        {
            self->single_selection = g_value_get_boolean (value);
        }
        break;

        case PROP_SORTER:
        {
            nautilus_view_model_set_sorter (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static GListModel *
create_model_func (GObject           *item,
                   NautilusViewModel *self)
{
    NautilusFile *file;
    GListStore *store;

    file = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM (item));
    if (!nautilus_file_is_directory (file))
    {
        return NULL;
    }

    store = g_hash_table_lookup (self->directory_reverse_map, file);
    if (store == NULL)
    {
        store = g_list_store_new (NAUTILUS_TYPE_VIEW_ITEM);
        g_hash_table_insert (self->directory_reverse_map, file, store);
    }

    GtkFilterListModel *filter_model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (store)), NULL);

    g_object_bind_property (self->root_filter_model, "filter",
                            filter_model, "filter",
                            G_BINDING_SYNC_CREATE);

    return G_LIST_MODEL (filter_model);
}

static void
constructed (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->constructed (object);

    self->root_filter_model = gtk_filter_list_model_new (G_LIST_MODEL (g_list_store_new (NAUTILUS_TYPE_VIEW_ITEM)), NULL);

    self->tree_model = gtk_tree_list_model_new (g_object_ref (G_LIST_MODEL (self->root_filter_model)),
                                                FALSE, FALSE,
                                                (GtkTreeListModelCreateModelFunc) create_model_func,
                                                self, NULL);
    self->sort_model = gtk_sort_list_model_new (g_object_ref (G_LIST_MODEL (self->tree_model)), NULL);

    if (self->single_selection)
    {
        GtkSingleSelection *single = gtk_single_selection_new (NULL);

        gtk_single_selection_set_autoselect (single, FALSE);
        gtk_single_selection_set_can_unselect (single, TRUE);

        gtk_single_selection_set_model (single, G_LIST_MODEL (self->sort_model));
        self->selection_model = GTK_SELECTION_MODEL (single);
    }
    else
    {
        self->selection_model = GTK_SELECTION_MODEL (gtk_multi_selection_new (g_object_ref (G_LIST_MODEL (self->sort_model))));
    }

    self->map_files_to_model = g_hash_table_new (NULL, NULL);
    self->directory_reverse_map = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

    g_signal_connect_swapped (self->sort_model, "items-changed",
                              G_CALLBACK (g_list_model_items_changed), self);
    g_signal_connect_swapped (self->sort_model, "sections-changed",
                              G_CALLBACK (gtk_section_model_sections_changed), self);
    g_signal_connect_swapped (self->selection_model, "selection-changed",
                              G_CALLBACK (gtk_selection_model_selection_changed), self);
}

static void
nautilus_view_model_class_init (NautilusViewModelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = dispose;
    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    properties[PROP_FILTER] =
        g_param_spec_object ("filter", NULL, NULL,
                             GTK_TYPE_FILTER,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    properties[PROP_SINGLE_SELECTION] =
        g_param_spec_boolean ("single-selection", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_SORTER] =
        g_param_spec_object ("sorter", NULL, NULL,
                             GTK_TYPE_SORTER,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_view_model_init (NautilusViewModel *self)
{
}

static gint
compare_data_func (gconstpointer a,
                   gconstpointer b,
                   gpointer      user_data)
{
    NautilusViewModel *self = (NautilusViewModel *) user_data;

    if (nautilus_view_model_get_sorter (self) == NULL)
    {
        return GTK_ORDERING_EQUAL;
    }

    return gtk_sorter_compare (nautilus_view_model_get_sorter (self), (gpointer) a, (gpointer) b);
}

NautilusViewModel *
nautilus_view_model_new (gboolean single_selection)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_MODEL,
                         "single-selection", single_selection,
                         NULL);
}

GtkFilter *
nautilus_view_model_get_filter (NautilusViewModel *self)
{
    return gtk_filter_list_model_get_filter (self->root_filter_model);
}

void
nautilus_view_model_set_filter (NautilusViewModel *self,
                                GtkFilter         *filter)
{
    if (self->root_filter_model == NULL ||
        gtk_filter_list_model_get_filter (self->root_filter_model) == filter)
    {
        return;
    }

    gtk_filter_list_model_set_filter (self->root_filter_model, filter);
    /* Subdirectory filter models are synchronized through bindings. */

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTER]);
}

gboolean
nautilus_view_model_get_single_selection (NautilusViewModel *self)
{
    return self->single_selection;
}

GtkSorter *
nautilus_view_model_get_sorter (NautilusViewModel *self)
{
    GtkTreeListRowSorter *row_sorter;

    row_sorter = (GtkTreeListRowSorter *) gtk_sort_list_model_get_sorter (self->sort_model);

    return row_sorter != NULL ? gtk_tree_list_row_sorter_get_sorter (row_sorter) : NULL;
}

void
nautilus_view_model_set_sorter (NautilusViewModel *self,
                                GtkSorter         *sorter)
{
    g_autoptr (GtkTreeListRowSorter) row_sorter = NULL;

    row_sorter = gtk_tree_list_row_sorter_new (NULL);

    gtk_tree_list_row_sorter_set_sorter (row_sorter, sorter);
    gtk_sort_list_model_set_sorter (self->sort_model, GTK_SORTER (row_sorter));

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

/**
 * Set the section sorter, effectively enabling sections.
 *
 * Unlike the regular sorter, which compares NautilusViewItem objects, this one
 * compares the GtkTreeListRows objects which wrap the NautilusViewItem objects.
 */
void
nautilus_view_model_set_section_sorter (NautilusViewModel *self,
                                        GtkSorter         *section_sorter)
{
    gtk_sort_list_model_set_section_sorter (self->sort_model, GTK_SORTER (section_sorter));
}

void
nautilus_view_model_sort (NautilusViewModel *self)
{
    GtkSorter *sorter = nautilus_view_model_get_sorter (self);

    if (sorter != NULL)
    {
        gtk_sorter_changed (sorter, GTK_SORTER_CHANGE_DIFFERENT);
    }
}

GList *
nautilus_view_model_get_sorted_items_for_files (NautilusViewModel *self,
                                                GList             *files)
{
    GList *items = NULL;

    for (GList *l = files; l != NULL; l = l->next)
    {
        NautilusViewItem *item;

        item = nautilus_view_model_get_item_for_file (self, l->data);
        if (item != NULL)
        {
            items = g_list_prepend (items, item);
        }
    }

    return g_list_sort_with_data (g_list_copy (items), compare_data_func, self);
}

NautilusViewItem *
nautilus_view_model_get_item_for_file (NautilusViewModel *self,
                                       NautilusFile      *file)
{
    return g_hash_table_lookup (self->map_files_to_model, file);
}

void
nautilus_view_model_remove_items (NautilusViewModel *self,
                                  GHashTable        *items,
                                  NautilusDirectory *directory)
{
    g_autoptr (NautilusFile) parent = nautilus_directory_get_corresponding_file (directory);
    GListStore *dir_store = get_directory_store (self, parent);
    guint range_start, n_items_in_range = 0;

    /* Remove contiguous item ranges to minimize ::items-changed emissions.
     * Remove after passing the range to not impact the index. */
    for (gint i = g_list_model_get_n_items (G_LIST_MODEL (dir_store)) - 1;
         i >= 0 && g_hash_table_size (items) > 0; i--)
    {
        g_autoptr (NautilusViewItem) item = g_list_model_get_item (G_LIST_MODEL (dir_store), i);
        NautilusFile *file = NULL;

        if (!g_hash_table_steal_extended (items, item, NULL, (gpointer *) &file))
        {
            if (n_items_in_range > 0)
            {
                g_list_store_splice (dir_store, range_start, n_items_in_range, NULL, 0);
                n_items_in_range = 0;
            }

            continue;
        }

        g_hash_table_remove (self->map_files_to_model, file);
        if (nautilus_file_is_directory (file))
        {
            g_hash_table_remove (self->directory_reverse_map, file);
        }

        /* The previous item is contiguous, keep growing the range. */
        n_items_in_range++;
        range_start = i;
    }

    if (n_items_in_range > 0)
    {
        /* Flush the leftover range. */
        g_list_store_splice (dir_store, range_start, n_items_in_range, NULL, 0);
    }

    if (g_hash_table_size (items) > 0)
    {
        g_warning ("Failed to remove %u item(s)", g_hash_table_size (items));
    }
}

void
nautilus_view_model_remove_all_items (NautilusViewModel *self)
{
    g_list_store_remove_all (G_LIST_STORE (gtk_filter_list_model_get_model (self->root_filter_model)));
    g_hash_table_remove_all (self->map_files_to_model);
    g_hash_table_remove_all (self->directory_reverse_map);
}

void
nautilus_view_model_add_item (NautilusViewModel *self,
                              NautilusViewItem  *item)
{
    NautilusFile *file;
    g_autoptr (NautilusFile) parent = NULL;

    file = nautilus_view_item_get_file (item);
    parent = nautilus_file_get_parent (file);

    g_list_store_append (get_directory_store (self, parent), item);
    g_hash_table_insert (self->map_files_to_model, file, item);
}

static void
splice_items_into_common_parent (NautilusViewModel *self,
                                 GPtrArray         *items,
                                 NautilusFile      *common_parent)
{
    GListStore *dir_store;

    dir_store = get_directory_store (self, common_parent);
    g_list_store_splice (dir_store,
                         g_list_model_get_n_items (G_LIST_MODEL (dir_store)),
                         0, items->pdata, items->len);
}

void
nautilus_view_model_add_items (NautilusViewModel *self,
                               GList             *items)
{
    g_autoptr (GPtrArray) array = g_ptr_array_new ();
    g_autoptr (NautilusFile) previous_parent = NULL;
    g_autoptr (GList) sorted_items = NULL;
    NautilusViewItem *item;

    /* The first added file becomes the initial focus and scroll anchor, so we
     * need to sort items before adding them to the internal model. */
    sorted_items = g_list_sort_with_data (g_list_copy (items), compare_data_func, self);

    for (GList *l = sorted_items; l != NULL; l = l->next)
    {
        g_autoptr (NautilusFile) parent = NULL;

        item = NAUTILUS_VIEW_ITEM (l->data);
        parent = nautilus_file_get_parent (nautilus_view_item_get_file (item));

        if (previous_parent != NULL && previous_parent != parent)
        {
            /* The pending items share a common parent. */
            splice_items_into_common_parent (self, array, previous_parent);

            /* Clear pending items and start a new with a new parent. */
            g_ptr_array_unref (array);
            array = g_ptr_array_new ();
        }
        g_set_object (&previous_parent, parent);

        g_ptr_array_add (array, item);
        g_hash_table_insert (self->map_files_to_model,
                             nautilus_view_item_get_file (item),
                             item);
    }

    if (previous_parent != NULL)
    {
        /* Flush the pending items. */
        splice_items_into_common_parent (self, array, previous_parent);
    }
}

void
nautilus_view_model_clear_subdirectory (NautilusViewModel *self,
                                        NautilusViewItem  *item)
{
    NautilusFile *file;
    GListModel *children;
    guint n_children = 0;

    g_return_if_fail (NAUTILUS_IS_VIEW_MODEL (self));
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (item));

    file = nautilus_view_item_get_file (item);
    children = G_LIST_MODEL (g_hash_table_lookup (self->directory_reverse_map, file));
    n_children = (children != NULL) ? g_list_model_get_n_items (children) : 0;
    for (guint i = 0; i < n_children; i++)
    {
        g_autoptr (NautilusViewItem) child = g_list_model_get_item (children, i);

        if (nautilus_file_is_directory (nautilus_view_item_get_file (child)))
        {
            /* Clear recursively */
            nautilus_view_model_clear_subdirectory (self, child);
        }
    }
    g_hash_table_remove (self->directory_reverse_map, file);
}

static inline void
collapse_all_rows (NautilusViewModel *self)
{
    guint n_root_items = g_list_model_get_n_items (gtk_tree_list_model_get_model (self->tree_model));

    for (guint i = 0; i < n_root_items; i++)
    {
        g_autoptr (GtkTreeListRow) root_level_row = gtk_tree_list_model_get_child_row (self->tree_model, i);

        gtk_tree_list_row_set_expanded (root_level_row, FALSE);
    }
}

void
nautilus_view_model_expand_as_a_tree (NautilusViewModel *self,
                                      gboolean           expand_as_a_tree)
{
    if (self->expand_as_a_tree && !expand_as_a_tree)
    {
        collapse_all_rows (self);
    }

    self->expand_as_a_tree = expand_as_a_tree;
}

void
nautilus_view_model_set_cut_files (NautilusViewModel *self,
                                   GList             *cut_files)
{
    NautilusViewItem *item;

    for (GList *l = self->cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_for_file (self, l->data);
        if (item != NULL)
        {
            nautilus_view_item_set_cut (item, FALSE);
        }
    }
    g_clear_list (&self->cut_files, g_object_unref);

    for (GList *l = cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_for_file (self, l->data);
        if (item != NULL)
        {
            self->cut_files = g_list_prepend (self->cut_files,
                                              g_object_ref (l->data));
            nautilus_view_item_set_cut (item, TRUE);
        }
    }
}
