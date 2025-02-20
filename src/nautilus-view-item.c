/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-view-item.h"

struct _NautilusViewItem
{
    GObject parent_instance;
    gboolean is_cut;
    gboolean drag_accept;
    gboolean loading;
    NautilusFile *file;
    GtkWidget *item_ui;
};

G_DEFINE_TYPE (NautilusViewItem, nautilus_view_item, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_FILE,
    PROP_IS_CUT,
    PROP_DRAG_ACCEPT,
    PROP_LOADING,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

enum
{
    FILE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
nautilus_view_item_dispose (GObject *object)
{
    NautilusViewItem *self = NAUTILUS_VIEW_ITEM (object);

    g_clear_weak_pointer (&self->item_ui);

    G_OBJECT_CLASS (nautilus_view_item_parent_class)->dispose (object);
}

static void
nautilus_view_item_finalize (GObject *object)
{
    NautilusViewItem *self = NAUTILUS_VIEW_ITEM (object);

    g_clear_object (&self->file);

    G_OBJECT_CLASS (nautilus_view_item_parent_class)->finalize (object);
}

static void
nautilus_view_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusViewItem *self = NAUTILUS_VIEW_ITEM (object);

    switch (prop_id)
    {
        case PROP_FILE:
        {
            g_value_set_object (value, self->file);
        }
        break;

        case PROP_IS_CUT:
        {
            g_value_set_boolean (value, self->is_cut);
        }
        break;

        case PROP_DRAG_ACCEPT:
        {
            g_value_set_boolean (value, self->drag_accept);
        }
        break;

        case PROP_LOADING:
        {
            g_value_set_boolean (value, self->loading);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusViewItem *self = NAUTILUS_VIEW_ITEM (object);

    switch (prop_id)
    {
        case PROP_FILE:
        {
            self->file = g_value_dup_object (value);
        }
        break;

        case PROP_IS_CUT:
        {
            self->is_cut = g_value_get_boolean (value);
        }
        break;

        case PROP_DRAG_ACCEPT:
        {
            self->drag_accept = g_value_get_boolean (value);
        }
        break;

        case PROP_LOADING:
        {
            nautilus_view_item_set_loading (self, g_value_get_boolean (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_item_init (NautilusViewItem *self)
{
}

static void
nautilus_view_item_class_init (NautilusViewItemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_view_item_dispose;
    object_class->finalize = nautilus_view_item_finalize;
    object_class->get_property = nautilus_view_item_get_property;
    object_class->set_property = nautilus_view_item_set_property;

    properties[PROP_IS_CUT] = g_param_spec_boolean ("is-cut",
                                                    "", "",
                                                    FALSE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DRAG_ACCEPT] = g_param_spec_boolean ("drag-accept",
                                                         "", "",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_FILE] = g_param_spec_object ("file",
                                                 "", "",
                                                 NAUTILUS_TYPE_FILE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_LOADING] = g_param_spec_boolean ("loading", NULL, NULL,
                                                     FALSE,
                                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[FILE_CHANGED] = g_signal_new ("file-changed",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);
}

NautilusViewItem *
nautilus_view_item_new (NautilusFile *file)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ITEM,
                         "file", file,
                         NULL);
}

void
nautilus_view_item_set_cut (NautilusViewItem *self,
                            gboolean          is_cut)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (self));

    g_object_set (self, "is-cut", is_cut, NULL);
}

void
nautilus_view_item_set_drag_accept (NautilusViewItem *self,
                                    gboolean          drag_accept)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (self));

    g_object_set (self, "drag-accept", drag_accept, NULL);
}

gint priorization_timeout = 0;

static void
free_weak_ref (gpointer data)
{
    GWeakRef *weak_ref = data;
    g_weak_ref_clear (weak_ref);
    g_free (weak_ref);
}

static GPtrArray *
get_priority_array (void)
{
    static GPtrArray *priority_files = NULL;

    if (G_UNLIKELY (priority_files == NULL))
    {
        priority_files = g_ptr_array_new_with_free_func (free_weak_ref);
    }

    return priority_files;
}

static void
prioritize_idle_callback (gpointer data)
{
    GPtrArray *priority_files = get_priority_array ();

    priorization_timeout = 0;

    for (gint i = priority_files->len - 1; i >= 0; i--)
    {
        g_autoptr (NautilusViewItem) item = g_weak_ref_get ((GWeakRef *) priority_files->pdata[i]);

        if (item != NULL)
        {
            NautilusFile *file = nautilus_view_item_get_file (item);

            nautilus_file_prioritize (file);
            g_ptr_array_remove_index_fast (priority_files, i);
        }
    }
}

void
nautilus_view_item_prioritize (NautilusViewItem *self,
                               gboolean          prioritize)
{
    GPtrArray *priority_files = get_priority_array ();

    if (prioritize)
    {
        GWeakRef *weak_ref = g_new0 (GWeakRef, 1);

        g_weak_ref_init (weak_ref, self);
        g_ptr_array_add (priority_files, weak_ref);

        if (priorization_timeout == 0)
        {
            /* Allow handling files in batches by adding a short delay */
            priorization_timeout = g_timeout_add_once (5, prioritize_idle_callback, NULL);
        }
    }
    else
    {
        for (guint i = 0; i < priority_files->len; i++)
        {
            g_autoptr (NautilusViewItem) reffed_item = g_weak_ref_get ((GWeakRef *) priority_files->pdata[i]);

            if (reffed_item != NULL && reffed_item == self)
            {
                g_ptr_array_remove_index_fast (priority_files, i);
                break;
            }
        }
    }
}

gboolean
nautilus_view_item_get_loading (NautilusViewItem *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_ITEM (self), FALSE);

    return self->loading;
}

void
nautilus_view_item_set_loading (NautilusViewItem *self,
                                gboolean          loading)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (self));

    if (self->loading != loading)
    {
        self->loading = loading;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
}

NautilusFile *
nautilus_view_item_get_file (NautilusViewItem *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_ITEM (self), NULL);

    return self->file;
}

GtkWidget *
nautilus_view_item_get_item_ui (NautilusViewItem *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_ITEM (self), NULL);

    return self->item_ui;
}

void
nautilus_view_item_set_item_ui (NautilusViewItem *self,
                                GtkWidget        *item_ui)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (self));

    g_set_weak_pointer (&self->item_ui, item_ui);
}

void
nautilus_view_item_file_changed (NautilusViewItem *self)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (self));

    g_signal_emit (self, signals[FILE_CHANGED], 0);
}
