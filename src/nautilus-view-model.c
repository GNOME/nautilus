#include "nautilus-view-model.h"
#include "nautilus-view-item-model.h"
#include "nautilus-global-preferences.h"

struct _NautilusViewModel
{
    GObject parent_instance;

    GHashTable *map_files_to_model;
    GListStore *internal_model;
    NautilusViewModelSortData *sort_data;
};

G_DEFINE_TYPE (NautilusViewModel, nautilus_view_model, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_SORT_TYPE,
    PROP_G_MODEL,
    N_PROPS
};

static void
finalize (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->finalize (object);

    g_hash_table_destroy (self->map_files_to_model);
    if (self->sort_data)
    {
        g_free (self->sort_data);
    }
    g_object_unref (self->internal_model);
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
        case PROP_SORT_TYPE:
        {
            g_value_set_object (value, self->sort_data);
        }
        break;

        case PROP_G_MODEL:
        {
            g_value_set_object (value, self->internal_model);
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
        case PROP_SORT_TYPE:
        {
            nautilus_view_model_set_sort_type (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
constructed (GObject *object)
{
    NautilusViewModel *self = NAUTILUS_VIEW_MODEL (object);

    G_OBJECT_CLASS (nautilus_view_model_parent_class)->constructed (object);

    self->internal_model = g_list_store_new (NAUTILUS_TYPE_VIEW_ITEM_MODEL);
    self->map_files_to_model = g_hash_table_new (NULL, NULL);
}

static void
nautilus_view_model_class_init (NautilusViewModelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;
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
    NautilusFile *file_a;
    NautilusFile *file_b;

    file_a = nautilus_view_item_model_get_file (NAUTILUS_VIEW_ITEM_MODEL ((gpointer) a));
    file_b = nautilus_view_item_model_get_file (NAUTILUS_VIEW_ITEM_MODEL ((gpointer) b));

    return nautilus_file_compare_for_sort (file_a, file_b,
                                           self->sort_data->sort_type,
                                           self->sort_data->directories_first,
                                           self->sort_data->reversed);
}

NautilusViewModel *
nautilus_view_model_new ()
{
    return g_object_new (NAUTILUS_TYPE_VIEW_MODEL, NULL);
}

void
nautilus_view_model_set_sort_type (NautilusViewModel         *self,
                                   NautilusViewModelSortData *sort_data)
{
    if (self->sort_data)
    {
        g_free (self->sort_data);
    }

    self->sort_data = g_new (NautilusViewModelSortData, 1);
    self->sort_data->sort_type = sort_data->sort_type;
    self->sort_data->reversed = sort_data->reversed;
    self->sort_data->directories_first = sort_data->directories_first;

    g_list_store_sort (self->internal_model, compare_data_func, self);
}

NautilusViewModelSortData *
nautilus_view_model_get_sort_type (NautilusViewModel *self)
{
    return self->sort_data;
}

GListStore *
nautilus_view_model_get_g_model (NautilusViewModel *self)
{
    return self->internal_model;
}

GQueue *
nautilus_view_model_get_items_from_files (NautilusViewModel *self,
                                          GQueue            *files)
{
    GList *l;
    NautilusViewItemModel *item_model;
    GQueue *item_models;

    item_models = g_queue_new ();
    for (l = g_queue_peek_head_link (files); l != NULL; l = l->next)
    {
        NautilusFile *file1;
        gint i = 0;

        file1 = NAUTILUS_FILE (l->data);
        while ((item_model = g_list_model_get_item (G_LIST_MODEL (self->internal_model), i)))
        {
            NautilusFile *file2;
            g_autofree gchar *file1_uri = NULL;
            g_autofree gchar *file2_uri = NULL;

            file2 = nautilus_view_item_model_get_file (item_model);
            file1_uri = nautilus_file_get_uri (file1);
            file2_uri = nautilus_file_get_uri (file2);
            if (g_strcmp0 (file1_uri, file2_uri) == 0)
            {
                g_queue_push_tail (item_models, item_model);
                break;
            }

            i++;
        }
    }

    return item_models;
}

NautilusViewItemModel *
nautilus_view_model_get_item_from_file (NautilusViewModel *self,
                                        NautilusFile      *file)
{
    return g_hash_table_lookup (self->map_files_to_model, file);
}

void
nautilus_view_model_remove_item (NautilusViewModel     *self,
                                 NautilusViewItemModel *item)
{
    NautilusViewItemModel *item_model;
    gint i;

    i = 0;
    item_model = NULL;
    while ((item_model = g_list_model_get_item (G_LIST_MODEL (self->internal_model), i)))
    {
        if (item_model == item)
        {
            break;
        }

        i++;
    }

    if (item_model != NULL)
    {
        NautilusFile *file;

        file = nautilus_view_item_model_get_file (item_model);
        g_list_store_remove (self->internal_model, i);
        g_hash_table_remove (self->map_files_to_model, file);
    }
}

void
nautilus_view_model_remove_all_items (NautilusViewModel *self)
{
    g_list_store_remove_all (self->internal_model);
    g_hash_table_remove_all (self->map_files_to_model);
}

void
nautilus_view_model_add_item (NautilusViewModel     *self,
                              NautilusViewItemModel *item)
{
    g_hash_table_insert (self->map_files_to_model,
                         nautilus_view_item_model_get_file (item),
                         item);
    g_list_store_insert_sorted (self->internal_model, item, compare_data_func, self);
}

void
nautilus_view_model_add_items (NautilusViewModel *self,
                               GQueue            *items)
{
    g_autofree gpointer *array = NULL;
    GList *l;
    int i = 0;

    array = g_malloc_n (g_queue_get_length (items),
                        sizeof (NautilusViewItemModel *));

    for (l = g_queue_peek_head_link (items); l != NULL; l = l->next)
    {
        array[i] = l->data;
        g_hash_table_insert (self->map_files_to_model,
                             nautilus_view_item_model_get_file (l->data),
                             l->data);
        i++;
    }

    g_list_store_splice (self->internal_model,
                         g_list_model_get_n_items (G_LIST_MODEL (self->internal_model)),
                         0, array, g_queue_get_length (items));

    g_list_store_sort (self->internal_model, compare_data_func, self);
}
