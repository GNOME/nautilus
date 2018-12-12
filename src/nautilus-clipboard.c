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

typedef struct
{
    gboolean cut;
    GList *files;
} ClipboardInfo;

static GList *
convert_selection_data_to_str_list (const gchar *data)
{
    g_auto (GStrv) lines = NULL;
    guint number_of_lines;
    GList *result;

    lines = g_strsplit (data, "\n", 0);
    number_of_lines = g_strv_length (lines);
    if (number_of_lines == 0)
    {
        /* An empty string will result in g_strsplit() returning an empty
         * array, so, naturally, 0 - 1 = UINT_MAX and we read all sorts
         * of invalid memory.
         */
        return NULL;
    }
    result = NULL;

    /* Also, this skips the last line, since it would be an
     * empty string from the split */
    for (guint i = 0; i < number_of_lines - 1; i++)
    {
        result = g_list_prepend (result, g_strdup (lines[i]));
    }

    return g_list_reverse (result);
}

static char *
convert_file_list_to_string (ClipboardInfo *info,
                             gboolean       format_for_text,
                             gsize         *len)
{
    GString *uris;
    char *uri, *tmp;
    GFile *f;
    guint i;
    GList *l;

    if (format_for_text)
    {
        uris = g_string_new (NULL);
    }
    else
    {
        uris = g_string_new ("x-special/nautilus-clipboard\n");
        g_string_append (uris, info->cut ? "cut\n" : "copy\n");
    }

    for (i = 0, l = info->files; l != NULL; l = l->next, i++)
    {
        uri = nautilus_file_get_uri (l->data);

        if (format_for_text)
        {
            f = g_file_new_for_uri (uri);
            tmp = g_file_get_parse_name (f);
            g_object_unref (f);

            if (tmp != NULL)
            {
                g_string_append (uris, tmp);
                g_free (tmp);
            }
            else
            {
                g_string_append (uris, uri);
            }

            g_string_append_c (uris, '\n');
        }
        else
        {
            g_string_append (uris, uri);
            g_string_append_c (uris, '\n');
        }

        g_free (uri);
    }

    *len = uris->len;
    return g_string_free (uris, FALSE);
}

static GList *
get_item_list_from_selection_data (const gchar *selection_data)
{
    GList *items = NULL;

    if (selection_data != NULL)
    {
        gboolean valid_data = TRUE;
        /* Not sure why it's legal to assume there's an extra byte
         * past the end of the selection data that it's safe to write
         * to. But gtk_editable_selection_received does this, so I
         * think it is OK.
         */
        items = convert_selection_data_to_str_list (selection_data);
        if (items == NULL || g_strcmp0 (items->data, "x-special/nautilus-clipboard") != 0)
        {
            valid_data = FALSE;
        }
        else if (items->next == NULL)
        {
            valid_data = FALSE;
        }
        else if (g_strcmp0 (items->next->data, "cut") != 0 &&
                 g_strcmp0 (items->next->data, "copy") != 0)
        {
            valid_data = FALSE;
        }

        if (!valid_data)
        {
            g_list_free_full (items, g_free);
            items = NULL;
        }
    }

    return items;
}

gboolean
nautilus_clipboard_is_data_valid_from_selection_data (const gchar *selection_data)
{
    return nautilus_clipboard_get_uri_list_from_selection_data (selection_data) != NULL;
}

GList *
nautilus_clipboard_get_uri_list_from_selection_data (const gchar *selection_data)
{
    GList *items;

    items = get_item_list_from_selection_data (selection_data);
    if (items)
    {
        /* Line 0 is x-special/nautilus-clipboard. */
        items = g_list_remove (items, items->data);
        /* Line 1 is "cut" or "copy", so uris start at line 2. */
        items = g_list_remove (items, items->data);
    }

    return items;
}

GtkClipboard *
nautilus_clipboard_get (GtkWidget *widget)
{
    return gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (widget)),
                                          GDK_SELECTION_CLIPBOARD);
}

void
nautilus_clipboard_clear_if_colliding_uris (GtkWidget   *widget,
                                            const GList *item_uris)
{
    g_autofree gchar *data = NULL;
    GList *clipboard_item_uris, *l;
    gboolean collision;

    collision = FALSE;
    data = gtk_clipboard_wait_for_text (nautilus_clipboard_get (widget));
    if (data == NULL)
    {
        return;
    }

    clipboard_item_uris = nautilus_clipboard_get_uri_list_from_selection_data (data);

    for (l = (GList *) item_uris; l; l = l->next)
    {
        if (g_list_find_custom ((GList *) item_uris, l->data,
                                (GCompareFunc) g_strcmp0))
        {
            collision = TRUE;
            break;
        }
    }

    if (collision)
    {
        gtk_clipboard_clear (nautilus_clipboard_get (widget));
    }

    if (clipboard_item_uris)
    {
        g_list_free_full (clipboard_item_uris, g_free);
    }
}

gboolean
nautilus_clipboard_is_cut_from_selection_data (const gchar *selection_data)
{
    GList *items;
    gboolean is_cut_from_selection_data;

    items = get_item_list_from_selection_data (selection_data);
    is_cut_from_selection_data = items != NULL &&
                                 g_strcmp0 ((gchar *) items->next->data, "cut") == 0;

    g_list_free_full (items, g_free);

    return is_cut_from_selection_data;
}

static void
on_get_clipboard (GtkClipboard     *clipboard,
                  GtkSelectionData *selection_data,
                  guint             info,
                  gpointer          user_data)
{
    char **uris;
    GList *l;
    int i;
    ClipboardInfo *clipboard_info;
    GdkAtom target;

    clipboard_info = (ClipboardInfo *) user_data;

    target = gtk_selection_data_get_target (selection_data);

    if (gtk_targets_include_uri (&target, 1))
    {
        uris = g_malloc ((g_list_length (clipboard_info->files) + 1) * sizeof (char *));
        i = 0;

        for (l = clipboard_info->files; l != NULL; l = l->next)
        {
            uris[i] = nautilus_file_get_uri (l->data);
            i++;
        }

        uris[i] = NULL;

        gtk_selection_data_set_uris (selection_data, uris);

        g_strfreev (uris);
    }
    else if (gtk_targets_include_text (&target, 1))
    {
        char *str;
        gsize len;

        str = convert_file_list_to_string (clipboard_info, FALSE, &len);
        gtk_selection_data_set_text (selection_data, str, len);
        g_free (str);
    }
}

static void
on_clear_clipboard (GtkClipboard *clipboard,
                    gpointer      user_data)
{
    ClipboardInfo *clipboard_info = (ClipboardInfo *) user_data;

    nautilus_file_list_free (clipboard_info->files);

    g_free (clipboard_info);
}

void
nautilus_clipboard_prepare_for_files (GtkClipboard *clipboard,
                                      GList        *files,
                                      gboolean      cut)
{
    GtkTargetList *target_list;
    GtkTargetEntry *targets;
    int n_targets;
    ClipboardInfo *clipboard_info;

    clipboard_info = g_new (ClipboardInfo, 1);
    clipboard_info->cut = cut;
    clipboard_info->files = nautilus_file_list_copy (files);

    target_list = gtk_target_list_new (NULL, 0);
    gtk_target_list_add_uri_targets (target_list, 0);
    gtk_target_list_add_text_targets (target_list, 0);

    targets = gtk_target_table_new_from_list (target_list, &n_targets);
    gtk_target_list_unref (target_list);

    gtk_clipboard_set_with_data (clipboard,
                                 targets, n_targets,
                                 on_get_clipboard, on_clear_clipboard,
                                 clipboard_info);
    gtk_target_table_free (targets, n_targets);
}

