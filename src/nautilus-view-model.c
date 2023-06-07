#include "nautilus-view-model.h"
#include "nautilus-view-item.h"
#include "nautilus-directory.h"
#include "nautilus-global-preferences.h"

struct _NautilusViewModel
{
    GObject parent_instance;

    GHashTable *map_files_to_model;
    GHashTable *directory_reverse_map;

    GtkTreeListModel *tree_model;
    GtkSortListModel *sort_model;
    GtkMultiSelection *selection_model;

    gboolean expand_as_a_tree;
};

static inline GListStore *
get_directory_store (NautilusViewModel *self,
                     NautilusFile      *directory)
{
    GListStore *store;

    store = g_hash_table_lookup (self->directory_reverse_map, directory);
    if (store == NULL)
    {
        store = G_LIST_STORE (gtk_tree_list_model_get_model (self->tree_model));
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


static gboolean
nautilus_view_model_is_selected (GtkSelectionModel *model,
                                 guint              position)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (self->selection_model);

    return gtk_selection_model_is_selected (selection_model, position);
}

static GtkBitset *
nautilus_view_model_get_selection_in_range (GtkSelectionModel *model,
                                            guint              pos,
                                            guint              n_items)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (self->selection_model);

    return gtk_selection_model_get_selection_in_range (selection_model, pos, n_items);
}

static gboolean
nautilus_view_model_set_selection (GtkSelectionModel *model,
                                   GtkBitset         *selected,
                                   GtkBitset         *mask)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (model);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (self->selection_model);
    gboolean res;

    res = gtk_selection_model_set_selection (selection_model, selected, mask);

    return res;
}


static void
nautilus_view_model_selection_model_init (GtkSelectionModelInterface *iface)
{
    iface->is_selected = nautilus_view_model_is_selected;
    iface->get_selection_in_range = nautilus_view_model_get_selection_in_range;
    iface->set_selection = nautilus_view_model_set_selection;
}

G_DEFINE_TYPE_WITH_CODE (NautilusViewModel, nautilus_view_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                nautilus_view_model_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                                nautilus_view_model_selection_model_init))

enum
{
    PROP_0,
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
        g_object_unref (self->sort_model);
        self->sort_model = NULL;
    }

    g_clear_object (&self->tree_model);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->finalize (object);

    g_hash_table_destroy (self->map_files_to_model);
    g_hash_table_destroy (self->directory_reverse_map);
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

    if (!self->expand_as_a_tree)
    {
        return NULL;
    }

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

    return g_object_ref (G_LIST_MODEL (store));
}

static void
constructed (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->constructed (object);

    self->tree_model = gtk_tree_list_model_new (G_LIST_MODEL (g_list_store_new (NAUTILUS_TYPE_VIEW_ITEM)),
                                                FALSE, FALSE,
                                                (GtkTreeListModelCreateModelFunc) create_model_func,
                                                self, NULL);
    self->sort_model = gtk_sort_list_model_new (g_object_ref (G_LIST_MODEL (self->tree_model)), NULL);
    self->selection_model = gtk_multi_selection_new (g_object_ref (G_LIST_MODEL (self->sort_model)));

    self->map_files_to_model = g_hash_table_new (NULL, NULL);
    self->directory_reverse_map = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

    g_signal_connect_swapped (self->sort_model, "items-changed",
                              G_CALLBACK (g_list_model_items_changed), self);
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

    properties[PROP_SORTER] =
        g_param_spec_object ("sorter",
                             "", "",
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
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (user_data);

    if (nautilus_view_model_get_sorter (self) == NULL)
    {
        return GTK_ORDERING_EQUAL;
    }

    return gtk_sorter_compare (nautilus_view_model_get_sorter (self), (gpointer) a, (gpointer) b);
}

NautilusViewModel *
nautilus_view_model_new (void)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_MODEL, NULL);
}

GtkSorter *
nautilus_view_model_get_sorter (NautilusViewModel *self)
{
    GtkTreeListRowSorter *row_sorter;

    row_sorter = GTK_TREE_LIST_ROW_SORTER (gtk_sort_list_model_get_sorter (self->sort_model));

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

void
nautilus_view_model_sort (NautilusViewModel *self)
{
    g_autoptr (GtkSorter) sorter = NULL;

    /* We are not supposed to call gtk_sorter_changed() from here, so let's
     * re-set the sorter to trigger re-sorting of the list. Hold a reference
     * to keep the sorter from getting destroyed while re-setting. */
    sorter = g_object_ref (gtk_sort_list_model_get_sorter (self->sort_model));
    gtk_sort_list_model_set_sorter (self->sort_model, sorter);
}

GList *
nautilus_view_model_get_items_from_files (NautilusViewModel *self,
                                          GList             *files)
{
    GList *items = NULL;

    for (GList *l = files; l != NULL; l = l->next)
    {
        NautilusViewItem *item;

        item = nautilus_view_model_get_item_from_file (self, l->data);
        if (item != NULL)
        {
            items = g_list_prepend (items, item);
        }
    }

    return g_list_reverse (items);
}

NautilusViewItem *
nautilus_view_model_get_item_from_file (NautilusViewModel *self,
                                        NautilusFile      *file)
{
    return g_hash_table_lookup (self->map_files_to_model, file);
}

void
nautilus_view_model_remove_item (NautilusViewModel *self,
                                 NautilusViewItem  *item,
                                 NautilusDirectory *directory)
{
    NautilusFile *file;
    g_autoptr (NautilusFile) parent = NULL;
    GListStore *dir_store;
    guint i;

    file = nautilus_view_item_get_file (item);
    parent = nautilus_directory_get_corresponding_file (directory);
    dir_store = get_directory_store (self, parent);
    if (g_list_store_find (dir_store, item, &i))
    {
        g_list_store_remove (dir_store, i);
        g_hash_table_remove (self->map_files_to_model, file);
        if (nautilus_file_is_directory (file))
        {
            g_hash_table_remove (self->directory_reverse_map, file);
        }
    }
}

void
nautilus_view_model_remove_all_items (NautilusViewModel *self)
{
    g_list_store_remove_all (G_LIST_STORE (gtk_tree_list_model_get_model (self->tree_model)));
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

guint
nautilus_view_model_get_index (NautilusViewModel *self,
                               NautilusViewItem  *item)
{
    guint n_items;
    guint i = 0;

    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->sort_model));
    while (i < n_items)
    {
        g_autoptr (GtkTreeListRow) row = NULL;
        g_autoptr (NautilusViewItem) item_i = NULL;

        row = g_list_model_get_item (G_LIST_MODEL (self->sort_model), i);
        g_warn_if_fail (GTK_IS_TREE_LIST_ROW (row));

        item_i = gtk_tree_list_row_get_item (row);
        g_warn_if_fail (NAUTILUS_IS_VIEW_ITEM (item_i));

        if (item_i == item)
        {
            return i;
        }
        i++;
    }

    return G_MAXUINT;
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

void
nautilus_view_model_expand_as_a_tree (NautilusViewModel *self,
                                      gboolean           expand_as_a_tree)
{
    self->expand_as_a_tree = expand_as_a_tree;
}
