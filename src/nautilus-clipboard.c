/* nautilus-clipboard.c
 *
 * Nautilus Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

/* The .files member contains elements of type NautilusFile. */
struct _NautilusClipboard
{
    gboolean cut;
    GList *files;
};

/* Boxed type used to wrap this struct in a clipboard GValue. */
G_DEFINE_BOXED_TYPE (NautilusClipboard, nautilus_clipboard,
                     nautilus_clipboard_copy, nautilus_clipboard_free)

static char *
nautilus_clipboard_to_string (NautilusClipboard *clip)
{
    GString *uris;
    char *uri;
    guint i;
    GList *l;

    uris = g_string_new (clip->cut ? "cut" : "copy");

    for (i = 0, l = clip->files; l != NULL; l = l->next, i++)
    {
        uri = nautilus_file_get_uri (l->data);

        g_string_append_c (uris, '\n');
        g_string_append (uris, uri);

        g_free (uri);
    }

    return g_string_free (uris, FALSE);
}

static NautilusClipboard *
nautilus_clipboard_from_string (char    *string,
                                GError **error)
{
    NautilusClipboard *clip;
    g_auto (GStrv) lines = NULL;
    g_autolist (NautilusFile) files = NULL;

    if (string == NULL)
    {
        *error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Clipboard string cannot be NULL.");
        return NULL;
    }

    lines = g_strsplit (string, "\n", 0);

    if (g_strcmp0 (lines[0], "cut") != 0 && g_strcmp0 (lines[0], "copy") != 0)
    {
        *error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Nautilus Clipboard must begin with 'cut' or 'copy'.");
        return NULL;
    }

    /* Line 0 is "cut" or "copy", so uris start at line 1. */
    for (int i = 1; lines[i] != NULL; i++)
    {
        if (g_strcmp0 (lines[i], "") == 0)
        {
            continue;
        }
        else if (!g_uri_is_valid (lines[i], G_URI_FLAGS_NONE, error))
        {
            return NULL;
        }
        files = g_list_prepend (files, nautilus_file_get_by_uri (lines[i]));
    }

    clip = g_new0 (NautilusClipboard, 1);
    files = g_list_reverse (files);
    clip->files = g_steal_pointer (&files);
    clip->cut = g_str_equal (lines[0], "cut");

    return clip;
}

#if 0 && NAUTILUS_DND_NEEDS_GTK4_REIMPLEMENTATION
void
nautilus_clipboard_clear_if_colliding_uris (GtkWidget   *widget,
                                            const GList *item_uris)
{
    GtkSelectionData *data;
    GList *clipboard_item_uris, *l;
    gboolean collision;

    collision = FALSE;
    data = gtk_clipboard_wait_for_contents (gtk_widget_get_clipboard (widget),
                                            copied_files_atom);
    if (data == NULL)
    {
        return;
    }

    clipboard_item_uris = nautilus_clipboard_get_uri_list_from_selection_data (data);

    for (l = (GList *) item_uris; l; l = l->next)
    {
        if (g_list_find_custom ((GList *) clipboard_item_uris, l->data,
                                (GCompareFunc) g_strcmp0))
        {
            collision = TRUE;
            break;
        }
    }

    if (collision)
    {
        gtk_clipboard_clear (gtk_widget_get_clipboard (widget));
    }

    if (clipboard_item_uris)
    {
        g_list_free_full (clipboard_item_uris, g_free);
    }
}
#endif

/*
 * This asumes the implementation of GTK_TYPE_FILE_LIST is a GSList<GFile>.
 * As of writing this, the API docs don't provide for this assumption.
 */
static GSList *
convert_file_list_to_gdk_file_list (NautilusClipboard *clip)
{
    GSList *file_list = NULL;
    for (GList *l = clip->files; l != NULL; l = l->next)
    {
        file_list = g_slist_prepend (file_list,
                                     nautilus_file_get_location (l->data));
    }
    return g_slist_reverse (file_list);
}

static void
nautilus_clipboard_serialize (GdkContentSerializer *serializer)
{
    NautilusClipboard *clip;
    g_autofree gchar *str = NULL;
    g_autoptr (GError) error = NULL;

    clip = g_value_get_boxed (gdk_content_serializer_get_value (serializer));

    str = nautilus_clipboard_to_string (clip);

    if (g_output_stream_printf (gdk_content_serializer_get_output_stream (serializer),
                                NULL,
                                gdk_content_serializer_get_cancellable (serializer),
                                &error,
                                "%s", str))
    {
        gdk_content_serializer_return_success (serializer);
    }
    else
    {
        gdk_content_serializer_return_error (serializer, error);
    }
}

static void
nautilus_clipboard_deserialize_finish (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
    GdkContentDeserializer *deserializer = user_data;
    GOutputStream *output = G_OUTPUT_STREAM (source);
    GError *error = NULL;
    g_autofree gchar *string = NULL;
    g_autoptr (NautilusClipboard) clip = NULL;

    if (g_output_stream_splice_finish (output, result, &error) < 0)
    {
        gdk_content_deserializer_return_error (deserializer, error);
        return;
    }

    /* write terminating NULL */
    if (g_output_stream_write (output, "", 1, NULL, &error) < 0 ||
        !g_output_stream_close (output, NULL, &error))
    {
        gdk_content_deserializer_return_error (deserializer, error);
        return;
    }

    string = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (output));

    clip = nautilus_clipboard_from_string (string, &error);

    if (clip == NULL)
    {
        gdk_content_deserializer_return_error (deserializer, error);
        return;
    }

    g_value_set_boxed (gdk_content_deserializer_get_value (deserializer), clip);
    gdk_content_deserializer_return_success (deserializer);
}

static void
nautilus_clipboard_deserialize (GdkContentDeserializer *deserializer)
{
    g_autoptr (GOutputStream) output = NULL;

    output = g_memory_output_stream_new_resizable ();
    g_output_stream_splice_async (output,
                                  gdk_content_deserializer_get_input_stream (deserializer),
                                  G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                  gdk_content_deserializer_get_priority (deserializer),
                                  gdk_content_deserializer_get_cancellable (deserializer),
                                  nautilus_clipboard_deserialize_finish,
                                  deserializer);
}

/**
 * nautilus_clipboard_peek_files:
 * @clip: The current local clipboard value.
 *
 * Returns: (transfer none): The internal GList of GFile objects.
 */
GList *
nautilus_clipboard_peek_files (NautilusClipboard *clip)
{
    return clip->files;
}

/**
 * nautilus_clipboard_get_uri_list:
 * @clip: The current local clipboard value.
 *
 * Returns: (transfer full): A GList of URI strings.
 */
GList *
nautilus_clipboard_get_uri_list (NautilusClipboard *clip)
{
    GList *uris = NULL;

    for (GList *l = clip->files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, nautilus_file_get_uri (l->data));
    }

    return g_list_reverse (uris);
}

gboolean
nautilus_clipboard_is_cut (NautilusClipboard *clip)
{
    return clip->cut;
}

NautilusClipboard *
nautilus_clipboard_copy (NautilusClipboard *clip)
{
    NautilusClipboard *new_clip = g_new0 (NautilusClipboard, 1);

    new_clip->cut = clip->cut;
    new_clip->files = nautilus_file_list_copy (clip->files);

    return new_clip;
}

void
nautilus_clipboard_free (NautilusClipboard *clip)
{
    nautilus_file_list_free (clip->files);
    g_free (clip);
}

void
nautilus_clipboard_prepare_for_files (GdkClipboard *clipboard,
                                      GList        *files,
                                      gboolean      cut)
{
    g_autoptr (NautilusClipboard) clip = NULL;
    g_autoslist (GFile) file_list = NULL;
    GdkContentProvider *providers[2];
    g_autoptr (GdkContentProvider) provider = NULL;

    clip = g_new (NautilusClipboard, 1);
    clip->cut = cut;
    clip->files = nautilus_file_list_copy (files);

    file_list = convert_file_list_to_gdk_file_list (clip);

    providers[0] = gdk_content_provider_new_typed (NAUTILUS_TYPE_CLIPBOARD, clip);
    providers[1] = gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, file_list);

    provider = gdk_content_provider_new_union (providers, 2);
    gdk_clipboard_set_content (clipboard, provider);
}

void
nautilus_clipboard_register (void)
{
    /*
     * While it'is not a public API and the format is not documented, some apps
     * have come to use this atom/mime type to integrate with our clipboard.
     */
    const gchar *nautilus_clipboard_mime_type = "x-special/gnome-copied-files";

    gdk_content_register_serializer (NAUTILUS_TYPE_CLIPBOARD,
                                     nautilus_clipboard_mime_type,
                                     nautilus_clipboard_serialize,
                                     NULL,
                                     NULL);
    gdk_content_register_deserializer (nautilus_clipboard_mime_type,
                                       NAUTILUS_TYPE_CLIPBOARD,
                                       nautilus_clipboard_deserialize,
                                       NULL,
                                       NULL);
}
