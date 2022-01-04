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

#if 0 && NAUTILUS_CLIPBOARD_NEEDS_GTK4_REIMPLEMENTATION
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
        uris = g_string_new (info->cut ? "cut" : "copy");
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

            /* skip newline for last element */
            if (i + 1 < g_list_length (info->files))
            {
                g_string_append_c (uris, '\n');
            }
        }
        else
        {
            g_string_append_c (uris, '\n');
            g_string_append (uris, uri);
        }

        g_free (uri);
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

GList *
nautilus_clipboard_get_uri_list_from_selection_data (GtkSelectionData *selection_data)
{
    GList *items;

    items = get_item_list_from_selection_data (selection_data);
    if (items)
    {
        /* Line 0 is "cut" or "copy", so uris start at line 1. */
        items = g_list_remove (items, items->data);
    }

    return items;
}

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

gboolean
nautilus_clipboard_is_cut_from_selection_data (GtkSelectionData *selection_data)
{
    GList *items;
    gboolean is_cut_from_selection_data;

    items = get_item_list_from_selection_data (selection_data);
    is_cut_from_selection_data = items != NULL &&
                                 g_strcmp0 ((gchar *) items->data, "cut") == 0;

    g_list_free_full (items, g_free);

    return is_cut_from_selection_data;
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
    gtk_target_list_add (target_list, copied_files_atom, 0, 0);
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

GdkAtom
nautilus_clipboard_get_atom (void)
{
    if (!copied_files_atom)
    {
        copied_files_atom = gdk_atom_intern_static_string ("x-special/gnome-copied-files");
    }

    return copied_files_atom;
}
#endif
