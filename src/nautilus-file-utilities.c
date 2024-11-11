/* nautilus-file-utilities.c - implementation of file manipulation routines.
 *
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "nautilus-file-utilities.h"

#include "nautilus-application.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-filename-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-names.h"
#include "nautilus-metadata.h"
#include "nautilus-network-directory.h"
#include "nautilus-scheme.h"
#include "nautilus-search-directory.h"
#include "nautilus-starred-directory.h"
#include "nautilus-ui-utilities.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include <stdlib.h>

#define NAUTILUS_USER_DIRECTORY_NAME "nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

char *
nautilus_compute_title_for_location (GFile *location)
{
    NautilusFile *file;
    g_autoptr (GMount) mount = NULL;
    char *title;

    /* TODO-gio: This doesn't really work all that great if the
     *  info about the file isn't known atm... */

    if (nautilus_is_home_directory (location))
    {
        return g_strdup (_("Home"));
    }

    mount = nautilus_get_mounted_mount_for_root (location);
    if (mount != NULL)
    {
        return g_mount_get_name (mount);
    }

    title = NULL;
    if (location)
    {
        file = nautilus_file_get (location);

        title = g_strdup (nautilus_file_get_display_name (file));

        nautilus_file_unref (file);
    }

    if (title == NULL)
    {
        title = g_file_get_basename (location);
    }

    return title;
}


/**
 * nautilus_get_user_directory:
 *
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_directory (void)
{
    char *user_directory = NULL;

    user_directory = g_build_filename (g_get_user_config_dir (),
                                       NAUTILUS_USER_DIRECTORY_NAME,
                                       NULL);

    if (!g_file_test (user_directory, G_FILE_TEST_EXISTS))
    {
        g_mkdir_with_parents (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
        /* FIXME bugzilla.gnome.org 41286:
         * How should we handle the case where this mkdir fails?
         * Note that nautilus_application_startup will refuse to launch if this
         * directory doesn't get created, so that case is OK. But the directory
         * could be deleted after Nautilus was launched, and perhaps
         * there is some bad side-effect of not handling that case.
         */
    }

    return user_directory;
}

/**
 * nautilus_get_scripts_directory_path:
 *
 * Get the path for the directory containing nautilus scripts.
 *
 * Return value: the directory path containing nautilus scripts
 **/
char *
nautilus_get_scripts_directory_path (void)
{
    if (nautilus_application_is_sandboxed ())
    {
        /* g_get_user_data_dir() leads to a different path then expected under
         * the flatpak sandbox */
        return g_build_filename (g_get_home_dir (), ".local", "share", "nautilus", "scripts", NULL);
    }
    else
    {
        return g_build_filename (g_get_user_data_dir (), "nautilus", "scripts", NULL);
    }
}

char *
nautilus_get_home_directory_uri (void)
{
    return g_filename_to_uri (g_get_home_dir (), NULL, NULL);
}


gboolean
nautilus_should_use_templates_directory (void)
{
    const char *dir;
    gboolean res;

    dir = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
    res = dir && (g_strcmp0 (dir, g_get_home_dir ()) != 0);
    return res;
}

char *
nautilus_get_templates_directory (void)
{
    return g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));
}

char *
nautilus_get_templates_directory_uri (void)
{
    char *directory, *uri;

    directory = nautilus_get_templates_directory ();
    uri = g_filename_to_uri (directory, NULL, NULL);
    g_free (directory);
    return uri;
}

gboolean
nautilus_is_home_directory_file (GFile      *dir,
                                 const char *filename)
{
    char *dirname;
    static GFile *home_dir_dir = NULL;
    static char *home_dir_filename = NULL;

    if (home_dir_dir == NULL)
    {
        dirname = g_path_get_dirname (g_get_home_dir ());
        home_dir_dir = g_file_new_for_path (dirname);
        g_free (dirname);
        home_dir_filename = g_path_get_basename (g_get_home_dir ());
    }

    return (g_file_equal (dir, home_dir_dir) &&
            strcmp (filename, home_dir_filename) == 0);
}

gboolean
nautilus_is_home_directory (GFile *dir)
{
    static GFile *home_dir = NULL;

    if (home_dir == NULL)
    {
        home_dir = g_file_new_for_path (g_get_home_dir ());
    }

    return g_file_equal (dir, home_dir);
}

gboolean
nautilus_is_root_directory (GFile *dir)
{
    static GFile *root_dir = NULL;

    if (root_dir == NULL)
    {
        root_dir = g_file_new_for_path ("/");
    }

    return g_file_equal (dir, root_dir);
}

gboolean
nautilus_is_root_for_scheme (GFile      *dir,
                             const char *scheme)
{
    return (g_file_has_uri_scheme (dir, scheme) &&
            !g_file_has_parent (dir, NULL));
}

GMount *
nautilus_get_mounted_mount_for_root (GFile *location)
{
    g_autoptr (GVolumeMonitor) volume_monitor = NULL;
    GList *mounts;
    GList *l;
    GMount *mount;
    GMount *result = NULL;

    volume_monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (volume_monitor);

    for (l = mounts; l != NULL; l = l->next)
    {
        g_autoptr (GFile) root = NULL;

        mount = l->data;

        if (g_mount_is_shadowed (mount))
        {
            continue;
        }

        root = g_mount_get_root (mount);
        if (g_file_equal (location, root))
        {
            result = g_object_ref (mount);
            break;
        }
    }

    g_list_free_full (mounts, g_object_unref);

    return result;
}

GFile *
nautilus_generate_unique_file_in_directory (GFile      *directory,
                                            const char *basename,
                                            gboolean    ignore_extension)
{
    g_return_val_if_fail (directory != NULL, NULL);
    g_return_val_if_fail (basename != NULL, NULL);
    g_return_val_if_fail (g_file_query_exists (directory, NULL), NULL);

    GFile *child = g_file_get_child (directory, basename);

    for (size_t counter = 1; g_file_query_exists (child, NULL); counter += 1)
    {
        g_autofree char *filename = nautilus_filename_for_conflict (basename,
                                                                    counter,
                                                                    -1,
                                                                    ignore_extension);

        g_object_unref (child);
        child = g_file_get_child (directory, filename);
    }

    return child;
}

GFile *
nautilus_find_existing_uri_in_hierarchy (GFile *location)
{
    GFile *tmp;

    g_assert (location != NULL);

    location = g_object_ref (location);
    while (location != NULL)
    {
        g_autoptr (GFileInfo) info = NULL;

        info = g_file_query_info (location,
                                  G_FILE_ATTRIBUTE_STANDARD_NAME,
                                  0, NULL, NULL);
        if (info != NULL)
        {
            return location;
        }
        tmp = location;
        location = g_file_get_parent (location);
        g_object_unref (tmp);
    }

    return location;
}

static gboolean
have_program_in_path (const char *name)
{
    gchar *path;
    gboolean result;

    path = g_find_program_in_path (name);
    result = (path != NULL);
    g_free (path);
    return result;
}

static GIcon *
special_directory_get_icon (GUserDirectory directory,
                            gboolean       symbolic)
{
#define ICON_CASE(x)                                                     \
            case G_USER_DIRECTORY_ ## x:                                     \
                return (symbolic) ? g_themed_icon_new (NAUTILUS_ICON_FOLDER_ ## x) : g_themed_icon_new (NAUTILUS_ICON_FULLCOLOR_FOLDER_ ## x);

    switch (directory)
    {
    ICON_CASE (DOCUMENTS);
    ICON_CASE (DOWNLOAD);
    ICON_CASE (MUSIC);
    ICON_CASE (PICTURES);
    ICON_CASE (PUBLIC_SHARE);
    ICON_CASE (TEMPLATES);
    ICON_CASE (VIDEOS);

        default:
        {
            return (symbolic) ? g_themed_icon_new (NAUTILUS_ICON_FOLDER) : g_themed_icon_new (NAUTILUS_ICON_FULLCOLOR_FOLDER);
        }
    }

#undef ICON_CASE
}

GIcon *
nautilus_special_directory_get_symbolic_icon (GUserDirectory directory)
{
    return special_directory_get_icon (directory, TRUE);
}

GIcon *
nautilus_special_directory_get_icon (GUserDirectory directory)
{
    return special_directory_get_icon (directory, FALSE);
}

gboolean
nautilus_is_file_roller_installed (void)
{
    static int installed = -1;

    if (installed < 0)
    {
        if (have_program_in_path ("file-roller"))
        {
            installed = 1;
        }
        else
        {
            installed = 0;
        }
    }

    return installed > 0 ? TRUE : FALSE;
}

GHashTable *
nautilus_trashed_files_get_original_directories (GList  *files,
                                                 GList **unhandled_files)
{
    GHashTable *directories;
    NautilusFile *file, *original_file, *original_dir;
    GList *l, *m;

    directories = NULL;

    if (unhandled_files != NULL)
    {
        *unhandled_files = NULL;
    }

    for (l = files; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);
        original_file = nautilus_file_get_trash_original_file (file);

        original_dir = NULL;
        if (original_file != NULL)
        {
            original_dir = nautilus_file_get_parent (original_file);
        }

        if (original_dir != NULL)
        {
            if (directories == NULL)
            {
                directories = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                     (GDestroyNotify) nautilus_file_unref,
                                                     (GDestroyNotify) nautilus_file_list_free);
            }
            nautilus_file_ref (original_dir);
            m = g_hash_table_lookup (directories, original_dir);
            if (m != NULL)
            {
                g_hash_table_steal (directories, original_dir);
                nautilus_file_unref (original_dir);
            }
            m = g_list_append (m, nautilus_file_ref (file));
            g_hash_table_insert (directories, original_dir, m);
        }
        else if (unhandled_files != NULL)
        {
            *unhandled_files = g_list_append (*unhandled_files, nautilus_file_ref (file));
        }

        nautilus_file_unref (original_file);
        nautilus_file_unref (original_dir);
    }

    return directories;
}

GList *
nautilus_file_list_from_uri_list (GList *uris)
{
    GList *l;
    GList *result = NULL;

    for (l = uris; l != NULL; l = l->next)
    {
        g_autoptr (GFile) location = NULL;

        location = g_file_new_for_uri (l->data);
        result = g_list_prepend (result, nautilus_file_get (location));
    }

    return g_list_reverse (result);
}

static GList *
locations_from_file_list (GList *file_list)
{
    NautilusFile *file;
    GList *l, *ret;

    ret = NULL;

    for (l = file_list; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);
        ret = g_list_prepend (ret, nautilus_file_get_location (file));
    }

    return g_list_reverse (ret);
}

typedef struct
{
    GHashTable *original_dirs_hash;
    GtkWindow *parent_window;
} RestoreFilesData;

static void
ensure_dirs_task_ready_cb (GObject      *_source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
    NautilusFile *original_dir;
    GFile *original_dir_location;
    GList *original_dirs, *files, *locations, *l;
    RestoreFilesData *data = user_data;

    original_dirs = g_hash_table_get_keys (data->original_dirs_hash);
    for (l = original_dirs; l != NULL; l = l->next)
    {
        original_dir = NAUTILUS_FILE (l->data);
        original_dir_location = nautilus_file_get_location (original_dir);

        files = g_hash_table_lookup (data->original_dirs_hash, original_dir);
        locations = locations_from_file_list (files);

        nautilus_file_operations_move_async (locations,
                                             original_dir_location,
                                             data->parent_window,
                                             NULL, NULL, NULL);

        g_list_free_full (locations, g_object_unref);
        g_object_unref (original_dir_location);
    }

    g_list_free (original_dirs);

    g_hash_table_unref (data->original_dirs_hash);
    g_slice_free (RestoreFilesData, data);
}

static void
ensure_dirs_task_thread_func (GTask        *task,
                              gpointer      source,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
    RestoreFilesData *data = task_data;
    NautilusFile *original_dir;
    GFile *original_dir_location;
    g_autoptr (GList) original_dirs = g_hash_table_get_keys (data->original_dirs_hash);

    for (GList *l = original_dirs; l != NULL; l = l->next)
    {
        original_dir = NAUTILUS_FILE (l->data);
        original_dir_location = nautilus_file_get_location (original_dir);

        g_file_make_directory_with_parents (original_dir_location, cancellable, NULL);
        g_object_unref (original_dir_location);
    }

    g_task_return_pointer (task, NULL, NULL);
}

static void
restore_files_ensure_parent_directories (GHashTable *original_dirs_hash,
                                         GtkWindow  *parent_window)
{
    RestoreFilesData *data;
    GTask *ensure_dirs_task;

    data = g_slice_new0 (RestoreFilesData);
    data->parent_window = parent_window;
    data->original_dirs_hash = g_hash_table_ref (original_dirs_hash);

    ensure_dirs_task = g_task_new (NULL, NULL, ensure_dirs_task_ready_cb, data);
    g_task_set_task_data (ensure_dirs_task, data, NULL);
    g_task_run_in_thread (ensure_dirs_task, ensure_dirs_task_thread_func);
    g_object_unref (ensure_dirs_task);
}

void
nautilus_restore_files_from_trash (GList     *files,
                                   GtkWindow *parent_window)
{
    GHashTable *original_dirs_hash;
    g_autolist (NautilusFile) unhandled_files = NULL;

    original_dirs_hash = nautilus_trashed_files_get_original_directories (files, &unhandled_files);

    for (GList *l = unhandled_files; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        g_autofree char *message = g_strdup_printf (_("Could not determine original location of “%s” "),
                                                    nautilus_file_get_display_name (file));

        show_dialog (message,
                     _("The item cannot be restored from trash"),
                     parent_window,
                     GTK_MESSAGE_WARNING);
    }

    if (original_dirs_hash != NULL)
    {
        restore_files_ensure_parent_directories (original_dirs_hash, parent_window);
        g_hash_table_unref (original_dirs_hash);
    }
}

typedef struct
{
    NautilusMountGetContent callback;
    gpointer user_data;
} GetContentTypesData;

static void
get_types_cb (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
    GetContentTypesData *data;
    char **types;

    data = user_data;
    types = g_mount_guess_content_type_finish (G_MOUNT (source_object), res, NULL);

    g_object_set_data_full (source_object,
                            "nautilus-content-type-cache",
                            g_strdupv (types),
                            (GDestroyNotify) g_strfreev);

    if (data->callback)
    {
        data->callback ((const char **) types, data->user_data);
    }
    g_strfreev (types);
    g_slice_free (GetContentTypesData, data);
}

void
nautilus_get_x_content_types_for_mount_async (GMount                  *mount,
                                              NautilusMountGetContent  callback,
                                              GCancellable            *cancellable,
                                              gpointer                 user_data)
{
    char **cached;
    GetContentTypesData *data;

    if (mount == NULL)
    {
        if (callback)
        {
            callback (NULL, user_data);
        }
        return;
    }

    cached = g_object_get_data (G_OBJECT (mount), "nautilus-content-type-cache");
    if (cached != NULL)
    {
        if (callback)
        {
            callback ((const char **) cached, user_data);
        }
        return;
    }

    data = g_slice_new0 (GetContentTypesData);
    data->callback = callback;
    data->user_data = user_data;

    g_mount_guess_content_type (mount,
                                FALSE,
                                cancellable,
                                get_types_cb,
                                data);
}

char **
nautilus_get_cached_x_content_types_for_mount (GMount *mount)
{
    char **cached;

    if (mount == NULL)
    {
        return NULL;
    }

    cached = g_object_get_data (G_OBJECT (mount), "nautilus-content-type-cache");
    if (cached != NULL)
    {
        return g_strdupv (cached);
    }

    return NULL;
}

char *
get_message_for_content_type (const char *content_type)
{
    char *message;
    const GStrv known_types = (char *[])
    {
        "x-content/audio-cdda",
        "x-content/audio-dvd",
        "x-content/video-vcd",
        "x-content/video-dvd",
        "x-content/video-svcd",
        "x-content/image-picturecd",
        NULL,
    };

    /* Customize greeting for well-known content types */
    if (g_strv_contains ((const gchar **) known_types, content_type))
    {
        message = g_content_type_get_description (content_type);
    }
    else if (strcmp (content_type, "x-content/image-photocd") == 0)
    {
        /* translators: these describe the contents of removable media */
        message = g_strdup (_("Photo CD"));
    }
    else if (strcmp (content_type, "x-content/image-dcf") == 0)
    {
        message = g_strdup (_("Contains digital photos"));
    }
    else if (strcmp (content_type, "x-content/audio-player") == 0)
    {
        message = g_strdup (_("Contains music"));
    }
    else if (strcmp (content_type, "x-content/unix-software") == 0)
    {
        message = g_strdup (_("Contains software to run"));
    }
    else if (strcmp (content_type, "x-content/ostree-repository") == 0)
    {
        message = g_strdup (_("Contains software to install"));
    }
    else
    {
        /* fallback to generic greeting */
        g_autofree char *description = g_content_type_get_description (content_type);
        message = g_strdup_printf (_("Detected as “%s”"), description);
    }

    return message;
}

char *
get_message_for_two_content_types (const char * const *content_types)
{
    char *message;

    g_assert (content_types[0] != NULL);
    g_assert (content_types[1] != NULL);

    /* few combinations make sense */
    if (strcmp (content_types[0], "x-content/image-dcf") == 0
        || strcmp (content_types[1], "x-content/image-dcf") == 0)
    {
        if (strcmp (content_types[0], "x-content/audio-player") == 0)
        {
            /* translators: these describe the contents of removable media */
            message = g_strdup (_("Contains music and photos"));
        }
        else if (strcmp (content_types[1], "x-content/audio-player") == 0)
        {
            message = g_strdup (_("Contains photos and music"));
        }
        else
        {
            message = g_strdup (_("Contains digital photos"));
        }
    }
    else if ((strcmp (content_types[0], "x-content/video-vcd") == 0
              || strcmp (content_types[1], "x-content/video-vcd") == 0)
             && (strcmp (content_types[0], "x-content/video-dvd") == 0
                 || strcmp (content_types[1], "x-content/video-dvd") == 0))
    {
        message = g_strdup_printf ("%s/%s",
                                   get_message_for_content_type (content_types[0]),
                                   get_message_for_content_type (content_types[1]));
    }
    else
    {
        message = get_message_for_content_type (content_types[0]);
    }

    return message;
}

gboolean
should_handle_content_type (const char *content_type)
{
    g_autoptr (GAppInfo) default_app = NULL;

    default_app = g_app_info_get_default_for_type (content_type, FALSE);

    return !g_str_has_prefix (content_type, "x-content/blank-") &&
           !g_content_type_is_a (content_type, "x-content/win32-software") &&
           default_app != NULL;
}

gboolean
should_handle_content_types (const char * const *content_types)
{
    int i;

    for (i = 0; content_types[i] != NULL; i++)
    {
        if (should_handle_content_type (content_types[i]))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static char *
trim_whitespace (const gchar *string)
{
    glong space_count;
    glong length;
    gchar *offset;

    space_count = 0;
    length = g_utf8_strlen (string, -1);
    offset = g_utf8_offset_to_pointer (string, length);

    while (space_count <= length)
    {
        gunichar character;

        offset = g_utf8_prev_char (offset);
        character = g_utf8_get_char (offset);

        if (!g_unichar_isspace (character))
        {
            break;
        }

        space_count++;
    }

    if (space_count == 0)
    {
        return g_strdup (string);
    }

    return g_utf8_substring (string, 0, length - space_count);
}

char *
nautilus_get_common_filename_prefix (GList *file_list,
                                     int    min_required_len)
{
    g_autoptr (GPtrArray) file_names = NULL;
    g_autoptr (GPtrArray) directory_names = NULL;
    g_autofree char *result_files = NULL;
    g_autofree char *result = NULL;
    g_autofree char *result_trimmed = NULL;

    if (file_list == NULL)
    {
        return NULL;
    }

    file_names = g_ptr_array_new_null_terminated (0, g_free, TRUE);
    directory_names = g_ptr_array_new_null_terminated (0, g_free, TRUE);

    for (GList *l = file_list; l != NULL; l = l->next)
    {
        char *name;

        g_return_val_if_fail (NAUTILUS_IS_FILE (l->data), NULL);

        name = g_strdup (nautilus_file_get_display_name (l->data));

        /* Since the concept of file extensions does not apply to directories,
         * we filter those out.
         */
        if (nautilus_file_is_directory (l->data))
        {
            g_ptr_array_add (directory_names, name);
        }
        else
        {
            g_ptr_array_add (file_names, name);
        }
    }

    result_files = nautilus_get_common_filename_prefix_from_filenames ((const char * const *) file_names->pdata,
                                                                       min_required_len);

    if (directory_names->len == 0)
    {
        return g_steal_pointer (&result_files);
    }

    if (result_files != NULL)
    {
        g_ptr_array_add (directory_names, g_steal_pointer (&result_files));
    }

    result = nautilus_filename_get_common_prefix ((const char * const *) directory_names->pdata,
                                                  min_required_len);

    if (result == NULL)
    {
        return NULL;
    }

    result_trimmed = trim_whitespace (result);

    if (g_utf8_strlen (result_trimmed, -1) < min_required_len)
    {
        return NULL;
    }

    return g_steal_pointer (&result_trimmed);
}

char *
nautilus_get_common_filename_prefix_from_filenames (const char * const *filenames,
                                                    int                 min_required_len)
{
    g_autoptr (GPtrArray) stripped_filenames = NULL;
    g_autofree char *common_prefix = NULL;
    g_autofree char *truncated = NULL;
    int common_prefix_len;

    if (filenames == NULL || filenames[0] == NULL)
    {
        return NULL;
    }

    stripped_filenames = g_ptr_array_new_from_null_terminated_array ((gpointer *) filenames,
                                                                     (GCopyFunc) nautilus_filename_strip_extension,
                                                                     NULL,
                                                                     g_free);

    common_prefix = nautilus_filename_get_common_prefix ((const char * const *) stripped_filenames->pdata,
                                                         min_required_len);
    if (common_prefix == NULL)
    {
        return NULL;
    }

    truncated = trim_whitespace (common_prefix);

    common_prefix_len = g_utf8_strlen (truncated, -1);
    if (common_prefix_len < min_required_len)
    {
        return NULL;
    }

    return g_steal_pointer (&truncated);
}

glong
nautilus_get_max_child_name_length_for_location (GFile *location)
{
    g_autofree gchar *path = NULL;
    glong name_max;
    glong path_max;
    glong max_child_name_length;

    g_return_val_if_fail (G_IS_FILE (location), -1);

    if (!g_file_has_uri_scheme (location, "file"))
    {
        /* FIXME: Can we query length limits for non-"file://" locations? */
        return -1;
    }

    path = g_file_get_path (location);

    g_return_val_if_fail (path != NULL, -1);

    name_max = pathconf (path, _PC_NAME_MAX);
    path_max = pathconf (path, _PC_PATH_MAX);
    max_child_name_length = -1;

    if (name_max == -1)
    {
        if (path_max != -1)
        {
            /* We don't know the name max, but we know the name can't make the
             * path longer than this.
             * Subtracting 1 because PATH_MAX includes the terminating null
             * character, as per limits.h(0P). */
            max_child_name_length = MAX ((path_max - 1) - strlen (path), 0);
        }
    }
    else
    {
        /* No need to subtract 1, because NAME_MAX excludes the terminating null
         * character, as per limits.h(0P) */
        max_child_name_length = name_max;
        if (path_max != -1)
        {
            max_child_name_length = CLAMP ((path_max - 1) - (glong) strlen (path),
                                           0,
                                           max_child_name_length);
        }
    }

    return max_child_name_length;
}

void
nautilus_ensure_extension_builtins (void)
{
    /* Please add new extension types here, even if you can guarantee
     * that they will be registered by the time the extension point
     * is iterating over its extensions.
     */
    g_type_ensure (NAUTILUS_TYPE_NETWORK_DIRECTORY);
    g_type_ensure (NAUTILUS_TYPE_SEARCH_DIRECTORY);
    g_type_ensure (NAUTILUS_TYPE_STARRED_DIRECTORY);
}

void
nautilus_ensure_extension_points (void)
{
    static gsize once_init_value = 0;

    if (g_once_init_enter (&once_init_value))
    {
        GIOExtensionPoint *extension_point;

        extension_point = g_io_extension_point_register (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME);
        g_io_extension_point_set_required_type (extension_point, NAUTILUS_TYPE_DIRECTORY);

        g_once_init_leave (&once_init_value, 1);
    }
}

gboolean
nautilus_file_can_rename_files (GList *files)
{
    GList *l;
    NautilusFile *file;

    for (l = files; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_rename (file))
        {
            return FALSE;
        }
    }

    return TRUE;
}

NautilusQueryRecursive
location_settings_search_get_recursive (void)
{
    switch (g_settings_get_enum (nautilus_preferences, "recursive-search"))
    {
        case NAUTILUS_SPEED_TRADEOFF_ALWAYS:
        {
            return NAUTILUS_QUERY_RECURSIVE_ALWAYS;
        }
        break;

        case NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY:
        {
            return NAUTILUS_QUERY_RECURSIVE_LOCAL_ONLY;
        }
        break;

        case NAUTILUS_SPEED_TRADEOFF_NEVER:
        {
            return NAUTILUS_QUERY_RECURSIVE_NEVER;
        }
        break;
    }

    return NAUTILUS_QUERY_RECURSIVE_ALWAYS;
}

NautilusQueryRecursive
location_settings_search_get_recursive_for_location (GFile *location)
{
    NautilusQueryRecursive recursive = location_settings_search_get_recursive ();

    g_return_val_if_fail (location, recursive);

    if (recursive == NAUTILUS_QUERY_RECURSIVE_LOCAL_ONLY)
    {
        g_autoptr (NautilusFile) file = nautilus_file_get_existing (location);

        g_return_val_if_fail (file != NULL, recursive);

        if (nautilus_file_is_remote (file))
        {
            recursive = NAUTILUS_QUERY_RECURSIVE_NEVER;
        }
    }

    return recursive;
}

/* check_schema_available() was copied from GNOME Settings */
gboolean
check_schema_available (const gchar *schema_id)
{
    GSettingsSchemaSource *source;
    g_autoptr (GSettingsSchema) schema = NULL;

    if (nautilus_application_is_sandboxed ())
    {
        return TRUE;
    }

    source = g_settings_schema_source_get_default ();
    if (!source)
    {
        return FALSE;
    }

    schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
    if (!schema)
    {
        return FALSE;
    }

    return TRUE;
}

gboolean
is_external_volume (GVolume *volume)
{
    gboolean is_external;
    GDrive *drive;
    char *id;

    drive = g_volume_get_drive (volume);
    id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

    is_external = g_volume_can_eject (volume);

    /* NULL volume identifier only happens on removable devices */
    is_external |= !id;

    if (drive)
    {
        is_external |= g_drive_is_removable (drive);
    }

    g_clear_object (&drive);
    g_free (id);

    return is_external;
}
