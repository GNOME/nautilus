/*
 * Copyright © 2025 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <test-utilities.h>

#undef G_LOG_DOMAIN
#include <nautilus-files-view.c>
#undef G_LOG_DOMAIN
#include <nautilus-resources.h>

#define G_LOG_DOMAIN "test-files-view"

static gboolean
ptr_arrays_equal_unordered (GPtrArray *a,
                            GPtrArray *b)
{
    if (a->len != b->len)
    {
        return FALSE;
    }

    for (guint i = 0; i < a->len; i++)
    {
        if (!g_ptr_array_find (b, a->pdata[i], NULL))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
set_true (gboolean *data)
{
    *data = TRUE;
}

static void
collect_renamed_files (NautilusFile *file,
                       GFile        *result_location,
                       GError       *error,
                       gpointer      callback_data)
{
    GPtrArray *renamed_files_arr = callback_data;

    g_ptr_array_add (renamed_files_arr, nautilus_file_ref (file));
}

static void
collect_changed_files (NautilusFilesView *view,
                       NautilusFile      *file,
                       NautilusDirectory *directory,
                       GPtrArray         *data_files)
{
    /* Ignore duplicate change emissions originating from nautilus-directory */
    if (!g_ptr_array_find (data_files, file, NULL))
    {
        g_ptr_array_add (data_files, nautilus_file_ref (file));
    }
}

const GStrv hidden_files_hierarchy = (char *[])
{
    "my_file",
    "my.file",
    "my_file.",
    "~my_file",
    "hidden_temp_file~",
    ".my_hidden_file",
    ".very_hidden_file~",
    "my_indirectly_hidden_file",
    "my_indirectly_hidden_tmp_file~",
    ".hidden",
    NULL
};

static void
create_hidden_files (void)
{
    g_autoptr (GFile) hidden_list_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                    ".hidden",
                                                                    NULL);
    const gchar *content = "my_indirectly_hidden_file\n"
                           "my_indirectly_hidden_tmp_file~\n";
    g_autoptr (GError) error = NULL;

    file_hierarchy_create (hidden_files_hierarchy, "");

    g_file_replace_contents (hidden_list_file,
                             content,
                             strlen (content),
                             NULL,
                             FALSE,
                             G_FILE_CREATE_NONE,
                             NULL,
                             NULL,
                             &error);
    g_assert_no_error (error);
}

static void
test_hidden_files_change (void)
{
    /* Save setting before test */
    gboolean saved_hidden_setting = g_settings_get_boolean (gtk_filechooser_preferences,
                                                            NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
    g_settings_set_boolean (gtk_filechooser_preferences,
                            NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                            FALSE);

    g_autoptr (NautilusWindowSlot) slot = g_object_ref_sink (nautilus_window_slot_new (NAUTILUS_MODE_BROWSE));
    g_autoptr (NautilusFilesView) files_view = nautilus_files_view_new (NAUTILUS_VIEW_GRID_ID, slot);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (files_view);
    g_autoptr (GFile) tmp_location = g_file_new_for_path (test_get_tmp_dir ());

    create_hidden_files ();

    /* Test visibility when hidden files are not shown. */
    nautilus_files_view_set_location (NAUTILUS_FILES_VIEW (files_view), tmp_location);
    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    for (gchar **filename = hidden_files_hierarchy; *filename != NULL; filename++)
    {
        g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                *filename,
                                                                NULL);
        g_autoptr (NautilusFile) file = nautilus_file_get (location);
        NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

        if (strstr (*filename, "hidden"))
        {
            g_assert_true (nautilus_file_is_hidden_file (file));
            g_assert_null (item);
        }
        else
        {
            g_assert_false (nautilus_file_is_hidden_file (file));
            g_assert_nonnull (item);
        }
    }

    /* Test visibility when hidden files are shown. */
    nautilus_files_view_set_show_hidden_files (files_view, TRUE);
    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    for (gchar **filename = hidden_files_hierarchy; *filename != NULL; filename++)
    {
        g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                *filename,
                                                                NULL);
        g_autoptr (NautilusFile) file = nautilus_file_get (location);
        NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

        g_assert_nonnull (item);

        if (strstr (*filename, "hidden"))
        {
            g_assert_true (nautilus_file_is_hidden_file (file));
        }
        else
        {
            g_assert_false (nautilus_file_is_hidden_file (file));
        }
    }

    /* Restore setting */
    g_settings_set_boolean (gtk_filechooser_preferences,
                            NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                            saved_hidden_setting);
    test_clear_tmp_dir ();
}

static void
test_rename_files (void)
{
    g_autoptr (NautilusWindowSlot) slot = g_object_ref_sink (nautilus_window_slot_new (NAUTILUS_MODE_BROWSE));
    g_autoptr (NautilusFilesView) files_view = nautilus_files_view_new (NAUTILUS_VIEW_GRID_ID, slot);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (files_view);
    g_autoptr (GFile) tmp_location = g_file_new_for_path (test_get_tmp_dir ());
    const guint file_count = 10, renamed_file_count = 5;
    g_autoptr (GPtrArray) file_arr = g_ptr_array_new_full (file_count,
                                                           (GDestroyNotify) nautilus_file_unref);
    g_autoptr (GPtrArray) renamed_files_arr = g_ptr_array_new_full (renamed_file_count,
                                                                    (GDestroyNotify) nautilus_file_unref);
    g_autoptr (GPtrArray) callback_arr = g_ptr_array_new_full (renamed_file_count,
                                                               (GDestroyNotify) nautilus_file_unref);

    /* Create the files before loading the view and keep them in an array. */
    for (guint i = 0; i < file_count; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("test_file_%i", i);
        g_autoptr (GFile) file = g_file_get_child (tmp_location, file_name);
        g_autoptr (GFileOutputStream) out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);

        g_assert_nonnull (out);
        g_ptr_array_add (file_arr, nautilus_file_get (file));
    }

    nautilus_files_view_set_location (files_view, tmp_location);
    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, file_count);

    /* Rename only some of the files and verify that changes are emitted*/
    g_signal_connect (files_view, "file-changed",
                      G_CALLBACK (collect_changed_files), callback_arr);

    for (guint i = 0; i < renamed_file_count; i++)
    {
        NautilusFile *file = file_arr->pdata[i];
        g_autoptr (GFile) location = nautilus_file_get_location (file);
        g_autofree gchar *file_name = g_strdup_printf ("test_file_%i.txt", i);
        nautilus_file_rename (file, file_name, collect_renamed_files, renamed_files_arr);
    }

    ITER_CONTEXT_WHILE (callback_arr->len != renamed_file_count ||
                        renamed_files_arr->len != renamed_file_count ||
                        !ptr_arrays_equal_unordered (callback_arr, renamed_files_arr));

    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, file_count);

    /* Both renamed and non-renamed nautilus file pointers from before renaming
     * should still point to the same files in the view. Renaming should not
     * make the old pointers invalid since we're using nautilus_file_rename(). */
    for (guint i = 0; i < file_arr->len; i++)
    {
        NautilusFile *file = file_arr->pdata[i];
        NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

        g_assert_nonnull (item);
    }
    for (guint i = 0; i < renamed_files_arr->len; i++)
    {
        NautilusFile *file = renamed_files_arr->pdata[i];
        NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

        g_assert_nonnull (item);
    }

    test_clear_tmp_dir ();
}

static void
collect_removed_files_cb (NautilusFilesView *view,
                          GList             *removed_files,
                          NautilusDirectory *directory,
                          GPtrArray         *data_files)
{
    for (GList *link = removed_files; link != NULL; link = link->next)
    {
        g_ptr_array_add (data_files, nautilus_file_ref (NAUTILUS_FILE (link->data)));
    }
}

static void
test_remove_files (void)
{
    g_autoptr (NautilusWindowSlot) slot = g_object_ref_sink (nautilus_window_slot_new (NAUTILUS_MODE_BROWSE));
    g_autoptr (NautilusFilesView) files_view = nautilus_files_view_new (NAUTILUS_VIEW_GRID_ID, slot);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (files_view);
    g_autoptr (GFile) tmp_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autofree gchar *uri = g_file_get_uri (tmp_location);
    const guint file_count = 10, deleted_file_count = 5;
    g_autoptr (GPtrArray) file_arr = g_ptr_array_new_full (file_count,
                                                           (GDestroyNotify) nautilus_file_unref);
    g_autoptr (GPtrArray) deleted_files_arr = g_ptr_array_new ();
    g_autoptr (GPtrArray) callback_arr = g_ptr_array_new_full (file_count,
                                                               (GDestroyNotify) nautilus_file_unref);

    /* Create the files before loading the view and keep them in an array. */
    for (guint i = 0; i < file_count; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("test_file_%i", i);
        g_autoptr (GFile) file = g_file_get_child (tmp_location, file_name);
        g_autoptr (GFileOutputStream) out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);

        g_assert_nonnull (out);
        g_ptr_array_add (file_arr, nautilus_file_get (file));
    }

    nautilus_files_view_set_location (files_view, tmp_location);

    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    g_assert_true (g_file_equal (tmp_location,
                                 nautilus_files_view_get_location (files_view)));
    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, file_count);


    /* Delete only some of the files and verify that they are emitted */
    g_signal_connect (files_view, "remove-file",
                      G_CALLBACK (collect_removed_files_cb), callback_arr);

    for (guint i = 0; i < deleted_file_count; i++)
    {
        g_autoptr (GFile) location = nautilus_file_get_location (file_arr->pdata[i]);
        g_autoptr (GError) error = NULL;
        g_file_delete (location, NULL, &error);

        g_assert_null (error);
        g_ptr_array_add (deleted_files_arr, file_arr->pdata[i]);
    }

    ITER_CONTEXT_WHILE (!ptr_arrays_equal_unordered (deleted_files_arr, callback_arr));

    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)),
                     ==,
                     file_count - deleted_file_count);

    for (guint i = 0; i < file_arr->len; i++)
    {
        NautilusFile *file = file_arr->pdata[i];
        NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

        if (g_ptr_array_find (deleted_files_arr, file, NULL))
        {
            g_assert_null (item);
        }
        else
        {
            g_assert_nonnull (item);
        }
    }

    test_clear_tmp_dir ();
}

static void
collect_added_files_cb (NautilusFilesView *view,
                        GList             *added_files,
                        GPtrArray         *data_files)
{
    for (GList *link = added_files; link != NULL; link = link->next)
    {
        g_ptr_array_add (data_files, nautilus_file_ref (NAUTILUS_FILE (link->data)));
    }
}

static void
test_add_files (void)
{
    g_autoptr (NautilusWindowSlot) slot = g_object_ref_sink (nautilus_window_slot_new (NAUTILUS_MODE_BROWSE));
    g_autoptr (NautilusFilesView) files_view = nautilus_files_view_new (NAUTILUS_VIEW_GRID_ID, slot);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (files_view);
    g_autoptr (GFile) tmp_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autofree gchar *uri = g_file_get_uri (tmp_location);
    const guint file_count = 10;
    g_autoptr (GPtrArray) file_arr = g_ptr_array_new_full (file_count,
                                                           (GDestroyNotify) nautilus_file_unref);
    g_autoptr (GPtrArray) callback_arr = g_ptr_array_new_full (file_count,
                                                               (GDestroyNotify) nautilus_file_unref);

    nautilus_files_view_set_location (files_view, tmp_location);

    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    g_assert_true (g_file_equal (tmp_location,
                                 nautilus_files_view_get_location (files_view)));
    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, 0);

    /* Keep a list of NautilusFiles */
    for (guint i = 0; i < file_count; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("test_file_%i", i);
        g_autoptr (GFile) location = g_file_get_child (tmp_location, file_name);

        g_ptr_array_add (file_arr, nautilus_file_get (location));
    }

    /* Create files and verify that the view added the correct files */
    g_signal_connect (files_view, "add-files", G_CALLBACK (collect_added_files_cb), callback_arr);

    for (guint i = 0; i < file_arr->len; i++)
    {
        g_autoptr (GFile) location = nautilus_file_get_location (file_arr->pdata[i]);
        g_autoptr (GFileOutputStream) out = g_file_create (location,
                                                           G_FILE_CREATE_NONE, NULL, NULL);

        g_assert_nonnull (out);
    }

    ITER_CONTEXT_WHILE (!ptr_arrays_equal_unordered (file_arr, callback_arr));

    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, file_count);

    for (guint i = 0; i < file_arr->len; i++)
    {
        NautilusFile *file = file_arr->pdata[i];

        g_assert_nonnull (nautilus_view_model_get_item_for_file (priv->model, file));
    }

    test_clear_tmp_dir ();
}

static void
test_load_dir (void)
{
    g_autoptr (NautilusWindowSlot) slot = g_object_ref_sink (nautilus_window_slot_new (NAUTILUS_MODE_BROWSE));
    g_autoptr (NautilusFilesView) files_view = nautilus_files_view_new (NAUTILUS_VIEW_GRID_ID, slot);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (files_view);
    g_autoptr (GFile) tmp_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autofree gchar *uri = g_file_get_uri (tmp_location);
    gboolean loading_started = FALSE, loading_ended = FALSE;
    const guint file_count = 10;

    create_multiple_files ("view_test", file_count);

    /* Verify loading signals and location */
    nautilus_files_view_set_location (files_view, tmp_location);
    g_signal_connect_swapped (files_view, "begin-loading", G_CALLBACK (set_true), &loading_started);
    g_signal_connect_swapped (files_view, "end-loading", G_CALLBACK (set_true), &loading_ended);

    g_assert_true (nautilus_files_view_get_loading (files_view));

    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (files_view));

    g_autofree gchar *view_uri = nautilus_files_view_get_uri (files_view);

    g_assert_true (g_file_equal (tmp_location,
                                 nautilus_files_view_get_location (files_view)));
    g_assert_cmpstr (view_uri, ==, uri);
    g_assert_true (loading_started);
    g_assert_true (loading_ended);

    /* Verify that files exist exist in the model */
    g_autoptr (GFileEnumerator) enumerator = NULL;
    g_autoptr (GError) error = NULL;
    GFile *child;
    guint counter = 0;

    enumerator = g_file_enumerate_children (tmp_location,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_TYPE_FILE_QUERY_INFO_FLAGS,
                                            NULL,
                                            &error);
    g_assert_no_error (error);

    /* Account for the extra folder */
    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (priv->model)), ==, file_count + 1);
    while (g_file_enumerator_iterate (enumerator, NULL, &child, NULL, NULL) && child != NULL)
    {
        g_autoptr (NautilusFile) file = nautilus_file_get (child);
        g_assert_nonnull (nautilus_view_model_get_item_for_file (priv->model, file));
        counter++;
    }
    g_assert_cmpint (counter, ==, file_count + 1);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusTagManager) tag_manager = NULL;

    gtk_test_init (&argc, &argv, NULL);

    nautilus_register_resource ();
    nautilus_ensure_extension_points ();
    nautilus_global_preferences_init ();
    tag_manager = nautilus_tag_manager_new_dummy ();
    test_init_config_dir ();

    g_autoptr (NautilusApplication) app = nautilus_application_new ();

    g_test_add_func ("/view/load_dir",
                     test_load_dir);
    g_test_add_func ("/view/add_files",
                     test_add_files);
    g_test_add_func ("/view/remove_files",
                     test_remove_files);
    g_test_add_func ("/view/change_files/rename",
                     test_rename_files);
    g_test_add_func ("/view/hidden_files/change",
                     test_hidden_files_change);

    return g_test_run ();
}
