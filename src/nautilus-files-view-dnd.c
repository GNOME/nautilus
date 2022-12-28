/*
 * nautilus-view-dnd.c: DnD helpers for NautilusFilesView
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ettore Perazzoli
 *          Darin Adler <darin@bentspoon.com>
 *          John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>

#include "nautilus-files-view-dnd.h"

#include "nautilus-files-view.h"
#include "nautilus-application.h"

#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <glib/gi18n.h>

#include "nautilus-clipboard.h"
#include "nautilus-dnd.h"
#include "nautilus-global-preferences.h"
#include "nautilus-ui-utilities.h"

#define GET_ANCESTOR(obj) \
    GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (obj), GTK_TYPE_WINDOW))

#define MAX_LEN_FILENAME 64
#define MIN_LEN_FILENAME 8

static char *
get_drop_filename (const char *text)
{
    char *filename;
    char trimmed[MAX_LEN_FILENAME];
    int i;
    int last_word = -1;
    int end_sentence = -1;
    int last_nonspace = -1;
    int start_sentence = -1;
    int num_attrs;
    PangoLogAttr *attrs;
    gchar *current_char;

    num_attrs = MIN (g_utf8_strlen (text, -1), MAX_LEN_FILENAME) + 1;
    attrs = g_new (PangoLogAttr, num_attrs);
    g_utf8_strncpy (trimmed, text, num_attrs - 1);
    pango_get_log_attrs (trimmed, -1, -1, pango_language_get_default (), attrs, num_attrs);

    /* since the end of the text will always match a word boundary don't include it */
    for (i = 0; (i < num_attrs - 1); i++)
    {
        if (attrs[i].is_sentence_start && start_sentence == -1)
        {
            start_sentence = i;
        }
        if (!attrs[i].is_white)
        {
            last_nonspace = i;
        }
        if (attrs[i].is_word_boundary)
        {
            last_word = last_nonspace;
        }
        if (attrs[i].is_sentence_end)
        {
            end_sentence = last_nonspace;
            break;
        }
    }
    g_free (attrs);

    if (end_sentence > 0)
    {
        i = end_sentence;
    }
    else
    {
        i = last_word;
    }

    if (i - start_sentence > MIN_LEN_FILENAME)
    {
        g_autofree char *substring = g_utf8_substring (trimmed, start_sentence, i);
        filename = g_strdup_printf ("%s.txt", substring);
    }
    else
    {
        /* Translator: This is the filename used for when you dnd text to a directory */
        filename = g_strdup (_("Dropped Text.txt"));
    }

    /* Remove any invalid characters */
    for (current_char = filename;
         *current_char;
         current_char = g_utf8_next_char (current_char))
    {
        if (G_IS_DIR_SEPARATOR (g_utf8_get_char (current_char)))
        {
            *current_char = '-';
        }
    }

    return filename;
}

void
nautilus_files_view_handle_text_drop (NautilusFilesView *view,
                                      const char        *text,
                                      const char        *target_uri,
                                      GdkDragAction      action)
{
    gsize length;
    char *container_uri;
    char *filename;

    if (text == NULL)
    {
        return;
    }

    g_return_if_fail (action == GDK_ACTION_COPY);

    container_uri = NULL;
    if (target_uri == NULL)
    {
        container_uri = nautilus_files_view_get_backing_uri (view);
        g_assert (container_uri != NULL);
    }

    length = strlen (text);

    /* try to get text to use as a filename */
    filename = get_drop_filename (text);

    nautilus_files_view_new_file_with_initial_contents (view,
                                                        target_uri != NULL ? target_uri : container_uri,
                                                        filename,
                                                        text,
                                                        length);
    g_free (filename);
    g_free (container_uri);
}

void
nautilus_files_view_drop_proxy_received_uris (NautilusFilesView *view,
                                              const GList       *source_uri_list,
                                              const char        *target_uri,
                                              GdkDragAction      action)
{
    g_autofree char *container_uri = NULL;
    g_autoptr (GFile) source_location = g_file_new_for_uri (source_uri_list->data);
    g_autoptr (GFile) target_location = g_file_new_for_uri (target_uri);

    if (target_uri == NULL)
    {
        container_uri = nautilus_files_view_get_backing_uri (view);
        g_assert (container_uri != NULL);
    }
    if (g_file_has_parent (source_location, target_location) &&
        action & GDK_ACTION_MOVE)
    {
        /* By default dragging to the same directory is allowed so that
         * users can duplicate a file using the CTRL modifier key.  Prevent
         * an accidental MOVE, by rejecting what would be an error anyways. */
        return;
    }

    if ((action != GDK_ACTION_COPY) &&
        (action != GDK_ACTION_MOVE) &&
        (action != GDK_ACTION_LINK))
    {
        show_dialog (_("Drag and drop is not supported."),
                     _("An invalid drag type was used."),
                     GET_ANCESTOR (view),
                     GTK_MESSAGE_WARNING);
        return;
    }

    nautilus_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
                                                source_uri_list);

    nautilus_files_view_move_copy_items (view, source_uri_list,
                                         target_uri != NULL ? target_uri : container_uri,
                                         action);
}

void
nautilus_files_view_handle_hover (NautilusFilesView *view,
                                  const char        *target_uri)
{
    NautilusWindowSlot *slot;
    GFile *location;
    GFile *current_location;
    NautilusFile *target_file;
    gboolean target_is_dir;
    gboolean open_folder_on_hover;

    slot = nautilus_files_view_get_nautilus_window_slot (view);

    location = g_file_new_for_uri (target_uri);
    target_file = nautilus_file_get_existing (location);
    target_is_dir = nautilus_file_get_file_type (target_file) == G_FILE_TYPE_DIRECTORY;
    current_location = nautilus_window_slot_get_location (slot);
    open_folder_on_hover = g_settings_get_boolean (nautilus_preferences,
                                                   NAUTILUS_PREFERENCES_OPEN_FOLDER_ON_DND_HOVER);

    if (target_is_dir && open_folder_on_hover &&
        !(current_location != NULL && g_file_equal (location, current_location)))
    {
        nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                 location, NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE,
                                                 NULL, NULL, slot);
    }
    g_object_unref (location);
    nautilus_file_unref (target_file);
}
