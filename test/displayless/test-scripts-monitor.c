/*
 * SPDX-FileCopyrightText: 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "test-utilities.h"

#include <glib.h>

#include <nautilus-directory.h>
#include <nautilus-file-utilities.h>
#include <nautilus-file.h>
#include <nautilus-scripts-monitor.h>

#define SCRIPT_CONTENT "#!/bin/sh\nexit 0\n"

typedef struct
{
    GPtrArray *script_names;
} ScriptsMenuData;

static void
scripts_menu_data_free (ScriptsMenuData *data)
{
    g_clear_pointer (&data->script_names, g_ptr_array_unref);
    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ScriptsMenuData, scripts_menu_data_free)

static ScriptsMenuData *
scripts_menu_data_new (void)
{
    ScriptsMenuData *data = g_new0 (ScriptsMenuData, 1);

    data->script_names = g_ptr_array_new_null_terminated (0, g_free, TRUE);

    return data;
}

static gboolean
scripts_menu_data_equal (ScriptsMenuData *data,
                         const GStrv      script_names)
{
    return g_strv_equal ((const gchar * const *) data->script_names->pdata,
                         (const gchar * const *) script_names);
}

/* Mimics how the real closure adds scripts to the menu. */
static void
add_script_to_menu (NautilusFile *file,
                    GMenu        *menu,
                    GHashTable   *script_accels,
                    gpointer      user_data)
{
    ScriptsMenuData *data = user_data;
    const gchar *name = nautilus_file_get_display_name (file);

    g_ptr_array_add (data->script_names, g_strdup (name));
    g_menu_append (menu, name, "scripts.run");
}

static gchar *
get_menu_item_label (GMenuModel *model,
                     gint        index)
{
    g_autoptr (GVariant) label = g_menu_model_get_item_attribute_value (model, index,
                                                                        G_MENU_ATTRIBUTE_LABEL, NULL);

    if (label == NULL)
    {
        return NULL;
    }

    return g_variant_dup_string (label, NULL);
}

static void
assert_submenu (GMenuModel  *model,
                gint         index,
                const gchar *expected_label,
                const GStrv  expected_items)
{
    g_autofree gchar *label = get_menu_item_label (model, index);
    g_autoptr (GMenuModel) submenu = g_menu_model_get_item_link (model, index,
                                                                 G_MENU_LINK_SUBMENU);
    guint n_expected_items = g_strv_length (expected_items);

    g_assert_cmpstr (label, ==, expected_label);
    g_assert_cmpuint (submenu ? g_menu_model_get_n_items (submenu) : 0, ==, n_expected_items);

    for (guint i = 0; i < n_expected_items; i++)
    {
        g_autofree gchar *item = get_menu_item_label (submenu, i);

        g_assert_cmpstr (item, ==, expected_items[i]);
    }
}

static void
on_scripts_changed (NautilusScriptsMonitor *monitor,
                    gpointer                user_data)
{
    gboolean *changed = user_data;

    *changed = TRUE;
}

/* The scripts monitor is a singleton whose initialization happens
 * asynchronously. Wait for it before using the monitor. */
static NautilusScriptsMonitor *
get_initialized_monitor (void)
{
    NautilusScriptsMonitor *monitor = nautilus_scripts_monitor_get ();
    gboolean changed = FALSE;
    gulong monitor_signal_id = g_signal_connect (monitor, "scripts-changed",
                                                 G_CALLBACK (on_scripts_changed), &changed);

    ITER_CONTEXT_WHILE (!changed);
    g_clear_signal_handler (&monitor_signal_id, monitor);

    return monitor;
}

static GFile *
get_scripts_relative_location (const gchar *relative_path)
{
    g_autofree gchar *scripts_dir_path = nautilus_get_scripts_directory_path ();

    return g_file_new_build_filename (scripts_dir_path, relative_path, NULL);
}

static NautilusDirectory *
get_scripts_directory (void)
{
    g_autoptr (GFile) location = get_scripts_relative_location (NULL);

    return nautilus_directory_get (location);
}

static NautilusDirectory *
get_scripts_subdirectory (const gchar *relative_path)
{
    g_autoptr (GFile) location = get_scripts_relative_location (relative_path);

    return nautilus_directory_get (location);
}

static void
create_script_file (const gchar *relative_path,
                    guint32      permissions)
{
    g_autoptr (GFile) location = get_scripts_relative_location (relative_path);
    g_autoptr (GFile) parent = g_file_get_parent (location);
    g_autoptr (GFileOutputStream) stream = NULL;
    g_autoptr (GError) error = NULL;

    if (!g_file_make_directory_with_parents (parent, NULL, &error) &&
        !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_assert_no_error (error);
    }
    g_clear_error (&error);

    stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_write_all (G_OUTPUT_STREAM (stream),
                                              SCRIPT_CONTENT, sizeof (SCRIPT_CONTENT) - 1,
                                              NULL, NULL, &error));
    g_assert_no_error (error);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error));
    g_assert_no_error (error);

    g_assert_true (g_file_set_attribute_uint32 (location,
                                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                                permissions,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, &error));
    g_assert_no_error (error);
}

static gboolean
directory_contains (NautilusDirectory *directory,
                    const GStrv        names)
{
    g_autolist (NautilusFile) files = nautilus_directory_get_file_list (directory);

    if (g_strv_length (names) != g_list_length (files))
    {
        return FALSE;
    }

    for (GList *node = files; node != NULL; node = node->next)
    {
        const char *name = nautilus_file_get_display_name (node->data);

        if (!g_strv_contains ((const gchar * const *) names, name))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/* Remove all files and directories from the scripts directory. */
static void
clear_scripts_directory (void)
{
    g_autoptr (GFile) location = get_scripts_relative_location (NULL);

    empty_directory_by_prefix (location, "");
}

/* Wait until the directory monitor notices the given entries. */
static void
wait_for_directory_entries (NautilusDirectory *directory,
                            const GStrv        names)
{
    ITER_CONTEXT_WHILE (!directory_contains (directory, names));
}

static void
test_empty (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (ScriptsMenuData) data = scripts_menu_data_new ();
    g_autoptr (NautilusScriptsMonitor) monitor = NULL;
    g_autoptr (GMenu) menu = NULL;

    clear_scripts_directory ();
    monitor = get_initialized_monitor ();
    directory = get_scripts_directory ();
    directory_load_attributes (directory,
                               NAUTILUS_ATTRIBUTE_FILE_LIST |
                               NAUTILUS_ATTRIBUTE_INFO);

    menu = nautilus_scripts_monitor_get_menu (monitor, add_script_to_menu, data);

    g_assert_null (menu);
    g_assert_cmpuint (data->script_names->len, ==, 0);
}

static void
test_non_exec_script (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (ScriptsMenuData) data = scripts_menu_data_new ();
    g_autoptr (NautilusScriptsMonitor) monitor = NULL;
    g_autoptr (GMenu) menu = NULL;
    const GStrv expected_entries = (char *[]){
        "not_executable.sh",
        NULL
    };

    clear_scripts_directory ();

    create_script_file ("not_executable.sh", 0644);

    monitor = get_initialized_monitor ();
    directory = get_scripts_directory ();
    wait_for_directory_entries (directory, expected_entries);
    directory_load_attributes (directory,
                               NAUTILUS_ATTRIBUTE_FILE_LIST |
                               NAUTILUS_ATTRIBUTE_INFO);

    menu = nautilus_scripts_monitor_get_menu (monitor, add_script_to_menu, data);

    /* The file is known to the directory but it is not executable, so the
     * scripts menu must be empty. */
    g_assert_null (menu);
    g_assert_cmpuint (data->script_names->len, ==, 0);
}

static void
test_single_script (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (ScriptsMenuData) data = scripts_menu_data_new ();
    g_autoptr (NautilusScriptsMonitor) monitor = NULL;
    g_autoptr (GMenu) menu = NULL;
    g_autofree gchar *label = NULL;
    const GStrv expected_entries = (char *[]){
        "executable.sh",
        NULL
    };

    clear_scripts_directory ();

    create_script_file ("executable.sh", 0755);

    monitor = get_initialized_monitor ();
    directory = get_scripts_directory ();
    wait_for_directory_entries (directory, expected_entries);
    directory_load_attributes (directory,
                               NAUTILUS_ATTRIBUTE_FILE_LIST |
                               NAUTILUS_ATTRIBUTE_INFO);

    menu = nautilus_scripts_monitor_get_menu (monitor, add_script_to_menu, data);

    g_assert_nonnull (menu);
    g_assert_cmpuint (g_menu_model_get_n_items (G_MENU_MODEL (menu)), ==, 1);

    label = get_menu_item_label (G_MENU_MODEL (menu), 0);
    g_assert_cmpstr (label, ==, "executable.sh");

    g_assert_true (scripts_menu_data_equal (data, expected_entries));
}

static void
test_hierarchy (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (NautilusDirectory) first_folder = NULL;
    g_autoptr (NautilusDirectory) second_folder = NULL;
    g_autoptr (ScriptsMenuData) data = scripts_menu_data_new ();
    g_autoptr (NautilusScriptsMonitor) monitor = NULL;
    NautilusAttributes attributes = NAUTILUS_ATTRIBUTE_FILE_LIST |
                                    NAUTILUS_ATTRIBUTE_INFO;
    g_autoptr (GMenu) menu = NULL;
    g_autofree gchar *label = NULL;
    const GStrv top_entries = (char *[]){
        "first_folder",
        "second_folder",
        "top_script.sh",
        NULL
    };
    const GStrv first_folder_entries = (char *[]){
        "script_a.sh",
        "script_b.sh",
        NULL
    };
    const GStrv second_folder_entries = (char *[]){
        "script_c.sh",
        "script_d.sh",
        NULL
    };
    const GStrv expected_scripts = (char *[]){
        "script_a.sh",
        "script_b.sh",
        "script_c.sh",
        "script_d.sh",
        "top_script.sh",
        NULL
    };

    clear_scripts_directory ();

    create_script_file ("first_folder/script_a.sh", 0755);
    create_script_file ("first_folder/script_b.sh", 0755);
    create_script_file ("second_folder/script_c.sh", 0755);
    create_script_file ("second_folder/script_d.sh", 0755);
    create_script_file ("top_script.sh", 0755);

    monitor = get_initialized_monitor ();
    directory = get_scripts_directory ();
    wait_for_directory_entries (directory, top_entries);
    directory_load_attributes (directory, attributes);

    first_folder = get_scripts_subdirectory ("first_folder");
    second_folder = get_scripts_subdirectory ("second_folder");

    /* Building the menu starts monitoring the subfolders, whose contents
     * may not be loaded yet at this point. Wait for them, then rebuild
     * the menu, like the real menu does when notified of script changes. */
    menu = nautilus_scripts_monitor_get_menu (monitor, add_script_to_menu, data);
    g_clear_object (&menu);
    g_ptr_array_set_size (data->script_names, 0);

    wait_for_directory_entries (first_folder, first_folder_entries);
    directory_load_attributes (first_folder, attributes);
    wait_for_directory_entries (second_folder, second_folder_entries);
    directory_load_attributes (second_folder, attributes);

    menu = nautilus_scripts_monitor_get_menu (monitor, add_script_to_menu, data);

    g_assert_nonnull (menu);
    g_assert_cmpuint (g_menu_model_get_n_items (G_MENU_MODEL (menu)), ==, 3);

    assert_submenu (G_MENU_MODEL (menu), 0, "first_folder", first_folder_entries);
    assert_submenu (G_MENU_MODEL (menu), 1, "second_folder", second_folder_entries);

    label = get_menu_item_label (G_MENU_MODEL (menu), 2);
    g_assert_cmpstr (label, ==, "top_script.sh");

    g_assert_true (scripts_menu_data_equal (data, expected_scripts));
}

int
main (int   argc,
      char *argv[])
{
    int result;

    /* The scripts monitor is a process-wide singleton reading the scripts
     * directory from the XDG user data directory. Redirect it to the test
     * temporary directory before it gets a chance to read the real one. */
    g_setenv ("XDG_DATA_HOME", test_get_tmp_dir (), TRUE);
    g_setenv ("XDG_CONFIG_HOME", test_get_tmp_dir (), TRUE);

    g_test_init (&argc, &argv, NULL);
    nautilus_ensure_extension_points ();

    g_test_add_func ("/empty",
                     test_empty);
    g_test_add_func ("/files/single-non-exec",
                     test_non_exec_script);
    g_test_add_func ("/files/single",
                     test_single_script);
    g_test_add_func ("/hierarchy/basic",
                     test_hierarchy);

    result = g_test_run ();

    test_clear_tmp_dir ();

    return result;
}
