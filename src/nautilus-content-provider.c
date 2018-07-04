/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-content-provider.h"

#include "nautilus-file.h"
#include "nautilus-files-view.h"

/* NautilusFileList */

GType nautilus_file_list_get_type (void) G_GNUC_CONST;

G_DEFINE_BOXED_TYPE (NautilusFileList, nautilus_file_list,
                     (GBoxedCopyFunc) nautilus_file_list_copy,
                     (GBoxedFreeFunc) nautilus_file_list_free)

/* NautilusContentProvider */

typedef struct
{
    GdkContentProvider parent_instance;

    GList *files;

    GString *string;
} NautilusContentProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusContentProvider, nautilus_content_provider,
                            GDK_TYPE_CONTENT_PROVIDER)

static void
nautilus_content_provider_init (NautilusContentProvider *self)
{
    NautilusContentProviderPrivate *priv;

    priv = nautilus_content_provider_get_instance_private (self);

    priv->files = NULL;
    priv->string = NULL;
}

static void
nautilus_content_provider_dispose (GObject *object)
{
    NautilusContentProvider *self;
    NautilusContentProviderPrivate *priv;

    self = NAUTILUS_CONTENT_PROVIDER (object);
    priv = nautilus_content_provider_get_instance_private (self);

    g_clear_pointer (&priv->files, nautilus_file_list_free);

    G_OBJECT_CLASS (nautilus_content_provider_parent_class)->dispose (object);
}

static void
nautilus_content_provider_finalize (GObject *object)
{
    NautilusContentProvider *self;
    NautilusContentProviderPrivate *priv;

    self = NAUTILUS_CONTENT_PROVIDER (object);
    priv = nautilus_content_provider_get_instance_private (self);

    if (priv->string != NULL)
    {
        g_string_free (priv->string, TRUE);
        priv->string = NULL;
    }

    G_OBJECT_CLASS (nautilus_content_provider_parent_class)->finalize (object);
}

static GdkContentFormats *
nautilus_content_provider_ref_formats (GdkContentProvider *provider)
{
    NautilusContentProvider *self;
    NautilusContentProviderPrivate *priv;
    GdkContentFormatsBuilder *builder;
    GdkContentFormats *formats;

    self = NAUTILUS_CONTENT_PROVIDER (provider);
    priv = nautilus_content_provider_get_instance_private (self);
    builder = gdk_content_formats_builder_new ();

    if (priv->files != NULL)
    {
        gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_FILE_LIST);
        gdk_content_formats_builder_add_gtype (builder, NAUTILUS_TYPE_FILE_LIST);
    }

    formats = gdk_content_formats_builder_free_to_formats (builder);

    if (priv->files != NULL)
    {
        formats = gtk_content_formats_add_text_targets (formats);
        formats = gtk_content_formats_add_uri_targets (formats);
    }

    return formats;
}

static GdkContentFormats *
nautilus_content_provider_ref_storable_formats (GdkContentProvider *provider)
{
    return gdk_content_formats_new_for_gtype (NAUTILUS_TYPE_FILE_LIST);
}

static void
nautilus_content_provider_write_mime_type_done (GObject      *source_object,
                                                GAsyncResult *res,
                                                gpointer      user_data)
{
    GOutputStream *stream;
    GError *error;
    GTask *task;

    stream = G_OUTPUT_STREAM (source_object);
    error = NULL;
    task = G_TASK (user_data);

    if (g_output_stream_write_all_finish (stream, res, NULL, &error))
    {
        g_task_return_boolean (task, TRUE);
    }
    else
    {
        g_task_return_error (task, error);
    }
}

static void
nautilus_content_provider_write_mime_type_async (GdkContentProvider  *provider,
                                                 const char          *mime_type,
                                                 GOutputStream       *stream,
                                                 int                  io_priority,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)

{
    GdkContentProviderClass *provider_class;
    GdkAtom targets[] =
    {
        g_intern_string (mime_type),
    };
    int n_targets;

    provider_class = GDK_CONTENT_PROVIDER_CLASS (nautilus_content_provider_parent_class);
    n_targets = G_N_ELEMENTS (targets);

    if (gtk_targets_include_text (targets, n_targets) ||
        gtk_targets_include_uri (targets, n_targets))
    {
        GTask *task;
        NautilusContentProvider *self;
        NautilusContentProviderPrivate *priv;

        task = g_task_new (provider, cancellable, callback, user_data);
        self = NAUTILUS_CONTENT_PROVIDER (provider);
        priv = nautilus_content_provider_get_instance_private (self);

        g_task_set_priority (task, io_priority);
        g_task_set_source_tag (task, nautilus_content_provider_write_mime_type_async);

        if (priv->string == NULL)
        {
            priv->string = g_string_new (NULL);

            for (GList *l = priv->files; l != NULL; l = l->next)
            {
                NautilusFile *file;
                g_autofree char *uri = NULL;

                file = NAUTILUS_FILE (l->data);
                uri = nautilus_file_get_uri (file);

                g_string_append_printf (priv->string, "%s\r\n", uri);
            }
        }

        g_output_stream_write_all_async (stream,
                                         priv->string->str, priv->string->len,
                                         io_priority,
                                         cancellable,
                                         nautilus_content_provider_write_mime_type_done,
                                         task);
    }
    else
    {
        provider_class->write_mime_type_async (provider, mime_type, stream,
                                               io_priority, cancellable,
                                               callback, user_data);
    }
}

static gboolean
nautilus_content_provider_write_mime_type_finish (GdkContentProvider  *provider,
                                                  GAsyncResult        *result,
                                                  GError             **error)
{
    GTask *task;
    gpointer tag;

    task = G_TASK (result);

    g_return_val_if_fail (g_task_is_valid (task, provider), FALSE);

    tag = g_task_get_source_tag (task);

    g_return_val_if_fail (tag == nautilus_content_provider_write_mime_type_async, FALSE);

    return g_task_propagate_boolean (task, error);
}

static GList *
nautilus_file_list_to_g_file_list (GList *nautilus_file_list)
{
    GList *g_file_list = NULL;

    for (GList *l = nautilus_file_list; l != NULL; l = l->next)
    {
        NautilusFile *nautilus_file;
        GFile *g_file;

        nautilus_file = l->data;
        g_file = nautilus_file_get_location (nautilus_file);

        g_file_list = g_list_prepend (g_file_list, g_file);
    }

    g_file_list = g_list_reverse (g_file_list);

    return g_file_list;
}

static gboolean
nautilus_content_provider_get_value (GdkContentProvider  *provider,
                                     GValue              *value,
                                     GError             **error)
{
    NautilusContentProvider *self;
    NautilusContentProviderPrivate *priv;
    GdkContentProviderClass *provider_class;

    self = NAUTILUS_CONTENT_PROVIDER (provider);
    priv = nautilus_content_provider_get_instance_private (self);
    provider_class = GDK_CONTENT_PROVIDER_CLASS (nautilus_content_provider_parent_class);

    if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_FILE_LIST))
    {
        GList *list;

        list = nautilus_file_list_copy (priv->files);

        g_value_set_boxed (value, list);

        return TRUE;
    }
    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GList *list;

        list = nautilus_file_list_to_g_file_list (priv->files);

        g_value_set_boxed (value, list);

        return TRUE;
    }

    return provider_class->get_value (provider, value, error);
}

static void
nautilus_content_provider_class_init (NautilusContentProviderClass *klass)
{
    GObjectClass *object_class;
    GdkContentProviderClass *provider_class;

    object_class = G_OBJECT_CLASS (klass);
    provider_class = GDK_CONTENT_PROVIDER_CLASS (klass);

    object_class->dispose = nautilus_content_provider_dispose;
    object_class->finalize = nautilus_content_provider_finalize;

    provider_class->ref_formats = nautilus_content_provider_ref_formats;
    provider_class->ref_storable_formats = nautilus_content_provider_ref_storable_formats;
    provider_class->write_mime_type_async = nautilus_content_provider_write_mime_type_async;
    provider_class->write_mime_type_finish = nautilus_content_provider_write_mime_type_finish;
    provider_class->get_value = nautilus_content_provider_get_value;
}

GList *
nautilus_content_provider_get_files (NautilusContentProvider *self)
{
    NautilusContentProviderPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_CONTENT_PROVIDER (self), NULL);

    priv = nautilus_content_provider_get_instance_private (self);

    return priv->files;
}

GdkContentProvider *
nautilus_content_provider_new_for_selection (NautilusView *view,
                                             gboolean      cut)
{
    NautilusContentProvider *provider;
    NautilusContentProviderPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    if (cut)
    {
        provider = g_object_new (NAUTILUS_TYPE_CUT_CONTENT_PROVIDER, NULL);
    }
    else
    {
        provider = g_object_new (NAUTILUS_TYPE_CONTENT_PROVIDER, NULL);
    }

    priv = nautilus_content_provider_get_instance_private (provider);

    priv->files = nautilus_view_get_selection (view);

    return GDK_CONTENT_PROVIDER (provider);
}

/* NautilusCutContentProvider */

struct _NautilusCutContentProvider
{
    NautilusContentProvider parent_instance;
};

G_DEFINE_TYPE (NautilusCutContentProvider, nautilus_cut_content_provider,
               NAUTILUS_TYPE_CONTENT_PROVIDER)

static void
nautilus_cut_content_provider_init (NautilusCutContentProvider *self)
{
}

static void
nautilus_cut_content_provider_class_init (NautilusCutContentProviderClass *klass)
{
}
