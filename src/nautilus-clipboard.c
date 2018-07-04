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

static void
clipboard_info_free (gpointer boxed)
{
    ClipboardInfo *clipboard_info;

    clipboard_info = boxed;

    nautilus_file_list_free (clipboard_info->files);

    g_free (clipboard_info);
}

static gpointer
clipboard_info_copy (gpointer boxed)
{
    ClipboardInfo *clipboard_info;
    ClipboardInfo *new_clipboard_info;

    clipboard_info = boxed;
    new_clipboard_info = g_new (ClipboardInfo, 1);

    new_clipboard_info->cut = clipboard_info->cut;
    new_clipboard_info->files = nautilus_file_list_copy (clipboard_info->files);

    return new_clipboard_info;
}

#define NAUTILUS_TYPE_CLIPBOARD_INFO nautilus_clipboard_info_get_type ()

typedef struct _NautilusClipboardInfo
{
    gboolean cut;
    GList *files;
} ClipboardInfo;

G_DEFINE_BOXED_TYPE (NautilusClipboardInfo, nautilus_clipboard_info,
                     clipboard_info_free, clipboard_info_copy)

static GList *
convert_lines_to_str_list (char **lines)
{
    int i;
    GList *result;

    if (lines[0] == NULL)
    {
        return NULL;
    }

    result = NULL;
    for (i = 0; lines[i] != NULL; i++)
    {
        result = g_list_prepend (result, g_strdup (lines[i]));
    }
    return g_list_reverse (result);
}

static char *
convert_file_list_to_string (ClipboardInfo *info,
                             gsize         *len)
{
    GString *uris;
    guint i;
    GList *l;

    uris = g_string_new (NULL);

    for (i = 0, l = info->files; l != NULL; l = l->next, i++)
    {
        g_autofree char *uri = NULL;
        g_autoptr (GFile) f = NULL;
        g_autofree char *tmp = NULL;

        uri = nautilus_file_get_uri (l->data);
        f = g_file_new_for_uri (uri);
        tmp = g_file_get_parse_name (f);

        if (tmp != NULL)
        {
            g_string_append (uris, tmp);
        }
        else
        {
            g_string_append (uris, uri);
        }

        /* skip newline for last element */
        if (i + 1 < g_list_length (info->files))
        {
            g_string_append_c (uris, '\n');
        }
    }

    *len = uris->len;
    return g_string_free (uris, FALSE);
}

static GList *
get_item_list_from_selection_data (GtkSelectionData *selection_data)
{
    GList *items;
    char **lines;

    if (gtk_selection_data_get_data_type (selection_data) != copied_files_atom
        || gtk_selection_data_get_length (selection_data) <= 0)
    {
        items = NULL;
    }
    else
    {
        gchar *data;
        /* Not sure why it's legal to assume there's an extra byte
         * past the end of the selection data that it's safe to write
         * to. But gtk_editable_selection_received does this, so I
         * think it is OK.
         */
        data = (gchar *) gtk_selection_data_get_data (selection_data);
        data[gtk_selection_data_get_length (selection_data)] = '\0';
        lines = g_strsplit (data, "\n", 0);
        items = convert_lines_to_str_list (lines);
        g_strfreev (lines);
    }

    return items;
}

static int
compare_file_uri_func (gconstpointer a,
                       gconstpointer b)
{
    g_autofree char *uri;

    uri = nautilus_file_get_uri (a);

    return g_strcmp0 (uri, b);
}

static gboolean
clipboard_is_empty (GtkWidget *widget)
{
    GdkClipboard *clipboard;
    GdkContentFormats *formats;

    nautilus_clipboard_init ();

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    clipboard = gtk_widget_get_clipboard (widget);
    formats = gdk_clipboard_get_formats (clipboard);

    return gdk_content_formats_contain_gtype (NAUTILUS_TYPE_CLIPBOARD_INFO);
}

static ClipboardInfo *
get_clipboard_info (GtkWidget *widget)
{
    GdkClipboard *clipboard;
    GdkContentProvider *provider;
    GValue value;
    ClipboardInfo *clipboard_info;

    nautilus_clipboard_init ();

    if (nautilus_clipboard_is_empty (widget))
    {
        return NULL;
    }

    clipboard = gtk_widget_get_clipboard (widget);
    provider = gdk_clipboard_get_content (clipboard);
    if (provider == NULL)
    {
        return NULL;
    }

    if (!gdk_content_provider_get_value (provider, &value))
    {
        return NULL;
    }

    clipboard_info = g_value_get_boxed (&value);
}

void
nautilus_clipboard_clear (GtkWidget *widget)
{
    GdkClipboard *clipboard;

    g_return_if_fail (GTK_IS_WIDGET (widget));

    clipboard = gtk_widget_get_clipboard (widget);

    gdk_clipboard_set_content (clipboard, NULL);
}

void
nautilus_clipboard_clear_if_colliding_uris (GtkWidget *widget,
                                            GList     *item_uris)
{
    ClipboardInfo *clipboard_info;

    clipboard_info = get_clipboard_info (widget);
    if (clipboard_info == NULL)
    {
        return;
    }

    for (GList *l = item_uris; l != NULL; l = l->next)
    {
        if (g_list_find_custom (clipboard_info->files, l->data, compare_file_uri_func))
        {
            nautilus_clipboard_clear (widget);
            break;
        }
    }
}

GList *
nautilus_clipboard_get_files (GtkWidget *widget,
                              gboolean  *cut)
{
    ClipboardInfo *clipboard_info;

    nautilus_clipboard_init ();

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    if (cut != NULL)
    {
        *cut = FALSE;
    }

    clipboard_info = get_clipboard_info (widget);
    if (clipboard_info == NULL)
    {
        return NULL;
    }

    if (cut != NULL)
    {
        *cut = clipboard_info->cut;
    }

    return clipboard_info->files;
}

void
nautilus_clipboard_prepare_for_files (GtkWidget *widget,
                                      GList     *files,
                                      gboolean   cut)
{
    ClipboardInfo *clipboard_info;
    GdkClipboard *clipboard;

    nautilus_clipboard_init ();

    g_return_if_fail (GTK_IS_WIDGET (widget));

    clipboard_info = g_new (ClipboardInfo, 1);
    clipboard = gtk_widget_get_clipboard (widget);

    clipboard_info->cut = cut;
    clipboard_info->files = nautilus_file_list_copy (files);

    gdk_clipboard_set (clipboard, NAUTILUS_TYPE_CLIPBOARD_INFO, clipboard_info);

    clipboard_info_free (clipboard_info);
}

static void
on_stream_write_all (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    GOutputStream *stream;
    GError *error = NULL;

    stream = G_OUTPUT_STREAM (source_object);

    if (g_output_stream_write_all_finish (stream, res, NULL, &error))
    {
        gdk_content_serializer_return_success (user_data);
    }
    else
    {
        gdk_content_serializer_return_error (user_data, error);
    }
}

static void
file_list_serialize (GdkContentSerializer *serializer,
                     char                 *string,
                     size_t                length)
{
    GOutputStream *stream;
    int priority;
    GCancellable *cancellable;

    stream = gdk_content_serializer_get_output_stream (serializer);
    priority = gdk_content_serializer_get_priority (serializer);
    cancellable = gdk_content_serializer_get_cancellable (serializer);

    g_output_stream_write_all_async (stream,
                                     string, length,
                                     priority,
                                     cancellable,
                                     on_stream_write_all, serializer);

    gdk_content_serializer_set_task_data (serializer, string);
}

static void
file_list_serialize_uri (GdkContentSerializer *serializer)
{
    const GValue *value;
    ClipboardInfo *clipboard_info;
    GString *string;

    value = gdk_content_serializer_get_value (serializer);
    clipboard_info = g_value_get_boxed (value);
    string = g_string_new (NULL);

    for (GList *l = clipboard_info->files; l != NULL; l = l->next)
    {
        g_autofree char *uri = NULL;

        uri = nautilus_file_get_uri (l->data);

        g_string_append (string, uri);
        g_string_append (string, "\r\n");
    }

    file_list_serialize (serializer, string->str, string->len);

    g_string_free (string, FALSE);
}

static void
file_list_serialize_text (GdkContentSerializer *serializer)
{
    const GValue *value;
    ClipboardInfo *clipboard_info;
    GString *string;

    value = gdk_content_serializer_get_value (serializer);
    clipboard_info = g_value_get_boxed (value);
    string = g_string_new (NULL);

    string->str = convert_file_list_to_string (clipboard_info, &string->len);

    file_list_serialize (serializer, string->str, string->len);

    g_string_free (string, FALSE);
}

gpointer
register_serializers (gpointer data)
{
    gdk_content_register_serializer (NAUTILUS_TYPE_CLIPBOARD_INFO,
                                     "text/uri-list",
                                     file_list_serialize_uri,
                                     NULL,
                                     NULL);
    gdk_content_register_serializer (NAUTILUS_TYPE_CLIPBOARD_INFO,
                                     "text/plain;charset=utf-8",
                                     file_list_serialize_text,
                                     NULL,
                                     NULL);

    return NULL;
}

void
nautilus_clipboard_init (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, register_serializers, NULL);
}
