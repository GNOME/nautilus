/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "nautilus-view-item-filter.h"

#include <gio/gio.h>

#include "nautilus-view-item.h"

enum
{
    PROP_0,
    PROP_FILE_FILTER,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

struct _NautilusViewItemFilter
{
    GtkFilter parent_instance;

    GtkFilterMatch strictness;

    GtkFileFilter *file_filter;
    GFileInfo *dummy_object;
};

G_DEFINE_TYPE (NautilusViewItemFilter, nautilus_view_item_filter, GTK_TYPE_FILTER)

static GtkFilterMatch
nautilus_view_item_filter_get_strictness (GtkFilter *filter)
{
    NautilusViewItemFilter *self = NAUTILUS_VIEW_ITEM_FILTER (filter);

    return self->strictness;
}

static gboolean
nautilus_view_item_filter_match (GtkFilter *filter,
                                 gpointer   item)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_ITEM (item), FALSE);

    NautilusViewItemFilter *self = NAUTILUS_VIEW_ITEM_FILTER (filter);
    NautilusFile *file = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM (item));

    if (nautilus_file_opens_in_view (file))
    {
        /* The current use case (FileChooser) never hides folders. */
        return TRUE;
    }

    GFileInfo *info = self->dummy_object;

    g_file_info_set_display_name (info, nautilus_file_get_display_name (file));
    g_file_info_set_content_type (info, nautilus_file_get_mime_type (file));

    return gtk_filter_match (GTK_FILTER (self->file_filter), info);
}

static void
nautilus_view_item_filter_finalize (GObject *object)
{
    NautilusViewItemFilter *self = NAUTILUS_VIEW_ITEM_FILTER (object);

    g_clear_object (&self->file_filter);
    g_clear_object (&self->dummy_object);

    G_OBJECT_CLASS (nautilus_view_item_filter_parent_class)->finalize (object);
}

static void
nautilus_view_item_filter_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    NautilusViewItemFilter *self = NAUTILUS_VIEW_ITEM_FILTER (object);

    switch (prop_id)
    {
        case PROP_FILE_FILTER:
        {
            nautilus_view_item_filter_set_file_filter (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_item_filter_class_init (NautilusViewItemFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkFilterClass *filter_class = GTK_FILTER_CLASS (klass);

    object_class->finalize = nautilus_view_item_filter_finalize;
    object_class->set_property = nautilus_view_item_filter_set_property;

    filter_class->get_strictness = nautilus_view_item_filter_get_strictness;
    filter_class->match = nautilus_view_item_filter_match;

    properties[PROP_FILE_FILTER] = g_param_spec_object ("file-filter", NULL, NULL,
                                                        GTK_TYPE_FILE_FILTER,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_view_item_filter_init (NautilusViewItemFilter *self)
{
    self->strictness = GTK_FILTER_MATCH_ALL;
    self->dummy_object = g_file_info_new ();
}

NautilusViewItemFilter *
nautilus_view_item_filter_new (void)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ITEM_FILTER, NULL);
}

void
nautilus_view_item_filter_set_file_filter (NautilusViewItemFilter *self,
                                           GtkFileFilter          *file_filter)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM_FILTER (self));

    if (!g_set_object (&self->file_filter, file_filter))
    {
        return;
    }

    GtkFilterMatch old_strictness = self->strictness;
    GtkFilterChange change = GTK_FILTER_CHANGE_DIFFERENT;

    self->strictness = (file_filter != NULL ?
                        GTK_FILTER_MATCH_SOME :
                        GTK_FILTER_MATCH_ALL);

    if (self->strictness != old_strictness)
    {
        change = (self->strictness == GTK_FILTER_MATCH_ALL ?
                  GTK_FILTER_CHANGE_LESS_STRICT :
                  GTK_FILTER_CHANGE_MORE_STRICT);
    }

    gtk_filter_changed (GTK_FILTER (self), change);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE_FILTER]);
}
