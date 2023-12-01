/*
 * Copyright (C) 2023 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-fd-holder.h"

/**
 * NautilusFdHolder:
 *
 * A helper object which wraps the process of getting, keeping, and releasing
 * file descriptor references.
 *
 * It can be reused for multiple locations, but only one at a time.
 *
 * The original use case is for #NautilusWindowSlot to keep autofs locations
 * from getting unmounted while we are displaying them.
 *
 * For convenience, we use GFileEnumerator as the real file descriptor holder.
 * This relies on the assumption that its implementation for local files calls
 * `opendir()` on creation and `closedir()` on destruction.
 */
struct _NautilusFdHolder
{
    GObject parent_instance;

    GFile *location;

    GFileEnumerator *enumerator;
    GCancellable *enumerator_cancellable;
};

G_DEFINE_FINAL_TYPE (NautilusFdHolder, nautilus_fd_holder, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_LOCATION,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
on_enumerator_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      data)
{
    NautilusFdHolder *self = NAUTILUS_FD_HOLDER (data);
    GFile *location = G_FILE (source_object);
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileEnumerator) enumerator = g_file_enumerate_children_finish (location, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        return;
    }

    g_return_if_fail (g_file_equal (location, self->location));
    g_warn_if_fail (self->enumerator == NULL);

    g_set_object (&self->enumerator, enumerator);
}

static void
update_fd_holder (NautilusFdHolder *self)
{
    g_cancellable_cancel (self->enumerator_cancellable);
    g_clear_object (&self->enumerator_cancellable);
    g_clear_object (&self->enumerator);

    if (self->location == NULL || !g_file_is_native (self->location))
    {
        return;
    }

    self->enumerator_cancellable = g_cancellable_new ();
    g_file_enumerate_children_async (self->location,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                     G_FILE_QUERY_INFO_NONE,
                                     G_PRIORITY_LOW,
                                     self->enumerator_cancellable,
                                     on_enumerator_ready,
                                     self);
}

NautilusFdHolder *
nautilus_fd_holder_new (void)
{
    return g_object_new (NAUTILUS_TYPE_FD_HOLDER, NULL);
}

static void
nautilus_fd_holder_finalize (GObject *object)
{
    NautilusFdHolder *self = (NautilusFdHolder *) object;

    g_cancellable_cancel (self->enumerator_cancellable);
    g_clear_object (&self->enumerator_cancellable);
    g_clear_object (&self->enumerator);

    g_clear_object (&self->location);

    G_OBJECT_CLASS (nautilus_fd_holder_parent_class)->finalize (object);
}

static void
nautilus_fd_holder_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusFdHolder *self = NAUTILUS_FD_HOLDER (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, self->location);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_fd_holder_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusFdHolder *self = NAUTILUS_FD_HOLDER (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            if (g_set_object (&self->location, g_value_get_object (value)))
            {
                update_fd_holder (self);
                g_object_notify_by_pspec (object, properties[PROP_LOCATION]);
            }
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_fd_holder_class_init (NautilusFdHolderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_fd_holder_finalize;
    object_class->get_property = nautilus_fd_holder_get_property;
    object_class->set_property = nautilus_fd_holder_set_property;

    properties[PROP_LOCATION] =
        g_param_spec_object ("location", NULL, NULL,
                             G_TYPE_FILE,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_fd_holder_init (NautilusFdHolder *self)
{
}

void
nautilus_fd_holder_set_location (NautilusFdHolder *self,
                                 GFile            *location)
{
    g_return_if_fail (NAUTILUS_IS_FD_HOLDER (self));
    g_return_if_fail (location == NULL || G_IS_FILE (location));

    g_object_set (self, "location", location, NULL);
}
