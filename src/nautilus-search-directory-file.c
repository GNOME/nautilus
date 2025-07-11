/*
 *  nautilus-search-directory-file.c: Subclass of NautilusFile to help implement the
 *  searches
 *
 *  Copyright (C) 2005 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Anders Carlsson <andersca@imendio.com>
 */

#include "nautilus-search-directory-file.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-enums.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-keyfile-metadata.h"
#include "nautilus-query.h"
#include "nautilus-search-directory.h"

struct _NautilusSearchDirectoryFile
{
    NautilusFile parent_instance;

    gchar *metadata_filename;
};

G_DEFINE_TYPE (NautilusSearchDirectoryFile, nautilus_search_directory_file, NAUTILUS_TYPE_FILE);


static void
search_directory_file_monitor_add (NautilusFile           *file,
                                   gconstpointer           client,
                                   NautilusFileAttributes  attributes)
{
    /* No need for monitoring, we always emit changed when files
     *  are added/removed, and no other metadata changes */

    /* Update display name, in case this didn't happen yet */
    nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));
}

static void
search_directory_file_monitor_remove (NautilusFile  *file,
                                      gconstpointer  client)
{
    /* Do nothing here, we don't have any monitors */
}

static void
search_directory_file_call_when_ready (NautilusFile           *file,
                                       NautilusFileAttributes  file_attributes,
                                       NautilusFileCallback    callback,
                                       gpointer                callback_data)
{
    /* Update display name, in case this didn't happen yet */
    nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));

    if (callback != NULL)
    {
        /* All data for directory-as-file is always up to date */
        (*callback)(file, callback_data);
    }
}

static void
search_directory_file_cancel_call_when_ready (NautilusFile         *file,
                                              NautilusFileCallback  callback,
                                              gpointer              callback_data)
{
    /* Do nothing here, we don't have any pending calls */
}

static gboolean
search_directory_file_check_if_ready (NautilusFile           *file,
                                      NautilusFileAttributes  attributes)
{
    return TRUE;
}

static gboolean
search_directory_file_get_item_count (NautilusFile *file,
                                      guint        *count,
                                      gboolean     *count_unreadable)
{
    GList *file_list;

    if (count)
    {
        NautilusDirectory *directory;

        directory = nautilus_file_get_directory (file);
        file_list = nautilus_directory_get_file_list (directory);

        *count = g_list_length (file_list);

        nautilus_file_list_free (file_list);
    }

    return TRUE;
}

static NautilusRequestStatus
search_directory_file_get_deep_counts (NautilusFile *file,
                                       guint        *directory_count,
                                       guint        *file_count,
                                       guint        *unreadable_directory_count,
                                       goffset      *total_size)
{
    NautilusDirectory *directory;
    NautilusFile *dir_file;
    GList *file_list, *l;
    guint dirs, files;
    GFileType type;

    directory = nautilus_file_get_directory (file);
    file_list = nautilus_directory_get_file_list (directory);

    dirs = files = 0;
    for (l = file_list; l != NULL; l = l->next)
    {
        dir_file = NAUTILUS_FILE (l->data);
        type = nautilus_file_get_file_type (dir_file);
        if (type == G_FILE_TYPE_DIRECTORY)
        {
            dirs++;
        }
        else
        {
            files++;
        }
    }

    if (directory_count != NULL)
    {
        *directory_count = dirs;
    }
    if (file_count != NULL)
    {
        *file_count = files;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        /* FIXME: Maybe we want to calculate this? */
        *total_size = 0;
    }

    nautilus_file_list_free (file_list);

    return NAUTILUS_REQUEST_DONE;
}

static void
search_directory_file_set_metadata (NautilusFile *file,
                                    const char   *key,
                                    const char   *value)
{
    NautilusSearchDirectoryFile *search_file;

    search_file = NAUTILUS_SEARCH_DIRECTORY_FILE (file);
    nautilus_keyfile_metadata_set_string (file,
                                          search_file->metadata_filename,
                                          "directory", key, value);
}

static void
search_directory_file_set_metadata_as_list (NautilusFile  *file,
                                            const char    *key,
                                            char         **value)
{
    NautilusSearchDirectoryFile *search_file;

    search_file = NAUTILUS_SEARCH_DIRECTORY_FILE (file);
    nautilus_keyfile_metadata_set_stringv (file,
                                           search_file->metadata_filename,
                                           "directory", key, (const gchar **) value);
}

void
nautilus_search_directory_file_update_display_name (NautilusSearchDirectoryFile *search_file)
{
    NautilusFile *file;
    NautilusDirectory *directory;
    char *display_name;
    gboolean changed;


    display_name = NULL;
    file = NAUTILUS_FILE (search_file);
    directory = nautilus_file_get_directory (file);
    if (directory != NULL)
    {
        NautilusSearchDirectory *search_dir = NAUTILUS_SEARCH_DIRECTORY (directory);
        NautilusQuery *query = nautilus_search_directory_get_query (search_dir);

        if (query != NULL)
        {
            g_autofree char *query_text = nautilus_query_get_text (query);

            if (query_text != NULL && query_text[0] != '\0')
            {
                display_name = g_strdup_printf (_("Search for “%s”"), query_text);
            }
        }
    }

    if (display_name == NULL)
    {
        display_name = g_strdup (_("Search"));
    }

    changed = nautilus_file_set_display_name (file, display_name, NULL, TRUE);
    if (changed)
    {
        nautilus_file_emit_changed (file);
    }

    g_free (display_name);
}

static void
nautilus_search_directory_file_init (NautilusSearchDirectoryFile *search_file)
{
    NautilusFile *file;
    gchar *xdg_dir;

    file = NAUTILUS_FILE (search_file);

    xdg_dir = nautilus_get_user_directory ();
    search_file->metadata_filename = g_build_filename (xdg_dir,
                                                       "search-metadata",
                                                       NULL);
    g_free (xdg_dir);

    file->details->got_file_info = TRUE;
    file->details->mime_type = g_ref_string_new_intern ("x-directory/normal");
    file->details->type = G_FILE_TYPE_DIRECTORY;
    file->details->size = 0;

    file->details->file_info_is_up_to_date = TRUE;

    file->details->custom_icon = NULL;
    file->details->activation_uri = NULL;

    file->details->directory_count = 0;
    file->details->got_directory_count = TRUE;
    file->details->directory_count_is_up_to_date = TRUE;

    nautilus_file_set_display_name (file, _("Search"), NULL, TRUE);
}

static void
nautilus_search_directory_file_finalize (GObject *object)
{
    NautilusSearchDirectoryFile *search_file;

    search_file = NAUTILUS_SEARCH_DIRECTORY_FILE (object);

    g_free (search_file->metadata_filename);

    G_OBJECT_CLASS (nautilus_search_directory_file_parent_class)->finalize (object);
}

static void
nautilus_search_directory_file_class_init (NautilusSearchDirectoryFileClass *klass)
{
    GObjectClass *object_class;
    NautilusFileClass *file_class;

    object_class = G_OBJECT_CLASS (klass);
    file_class = NAUTILUS_FILE_CLASS (klass);

    object_class->finalize = nautilus_search_directory_file_finalize;

    file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

    file_class->monitor_add = search_directory_file_monitor_add;
    file_class->monitor_remove = search_directory_file_monitor_remove;
    file_class->call_when_ready = search_directory_file_call_when_ready;
    file_class->cancel_call_when_ready = search_directory_file_cancel_call_when_ready;
    file_class->check_if_ready = search_directory_file_check_if_ready;
    file_class->get_item_count = search_directory_file_get_item_count;
    file_class->get_deep_counts = search_directory_file_get_deep_counts;
    file_class->set_metadata = search_directory_file_set_metadata;
    file_class->set_metadata_as_list = search_directory_file_set_metadata_as_list;
}
