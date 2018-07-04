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

#include "nautilus-clipboard.h"

#include "nautilus-content-provider.h"
#include "nautilus-file.h"

static int
file_uri_compare (gconstpointer a,
                  gconstpointer b)
{
    NautilusFile *file;
    g_autofree char *uri = NULL;

    file = NAUTILUS_FILE (a);
    uri = nautilus_file_get_uri (file);

    return g_strcmp0 (uri, b);
}

void
nautilus_clipboard_clear_if_colliding_uris (GtkWidget *widget,
                                            GList     *uris)
{
    GdkClipboard *clipboard;
    GdkContentProvider *provider;

    clipboard = gtk_widget_get_clipboard (widget);
    provider = gdk_clipboard_get_content (clipboard);
    if (NAUTILUS_IS_CONTENT_PROVIDER (provider))
    {
        GList *files;

        files = nautilus_content_provider_get_files (NAUTILUS_CONTENT_PROVIDER (provider));

        if (g_list_find_custom (files, uris, file_uri_compare) != NULL)
        {
            gdk_clipboard_set_content (clipboard, NULL);
        }
    }
}
