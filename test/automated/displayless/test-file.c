#include <glib.h>

#include <nautilus-directory-private.h>
#include <nautilus-file.h>
#include <nautilus-file-private.h>
#include <nautilus-file-utilities.h>
#include <test-utilities.h>


static void
test_file_refcount_single_file (void)
{
    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 0);

    NautilusFile *file = nautilus_file_get_by_uri ("file:///home/");

    g_assert_cmpint (G_OBJECT (file)->ref_count, ==, 1);
    g_assert_cmpint (G_OBJECT (file->details->directory)->ref_count, ==, 1);
    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 1);

    nautilus_file_unref (file);

    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 0);
}

static void
test_file_check_name_bland (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home");
    const char *name = nautilus_file_get_name (file);
    g_assert_cmpstr (name, ==, "home");
}

static void
test_file_check_name_trailing_slash (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home/");
    const char *name = nautilus_file_get_name (file);
    g_assert_cmpstr (name, ==, "home");
}

static void
test_file_duplicate_pointers (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home/");

    g_assert_true (nautilus_file_get_by_uri ("file:///home/") == file);
    nautilus_file_unref (file);

    g_assert_true (nautilus_file_get_by_uri ("file:///home") == file);
    nautilus_file_unref (file);
}

static void
test_file_sort_order (void)
{
    g_autoptr (NautilusFile) file_1 = nautilus_file_get_by_uri ("file:///etc");
    g_autoptr (NautilusFile) file_2 = nautilus_file_get_by_uri ("file:///usr");
    NautilusFileSortType sort_type = NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;

    g_assert_cmpint (G_OBJECT (file_1)->ref_count, ==, 1);
    g_assert_cmpint (G_OBJECT (file_2)->ref_count, ==, 1);

    int order = nautilus_file_compare_for_sort (file_1, file_2, sort_type, FALSE, FALSE);
    g_assert_cmpint (order, <, 0);

    int order_reversed = nautilus_file_compare_for_sort (file_1, file_2, sort_type, FALSE, TRUE);
    g_assert_cmpint (order_reversed, >, 0);
}

static void
test_file_sort_with_self (void)
{
    g_autoptr (NautilusFile) file_1 = nautilus_file_get_by_uri ("file:///etc");
    NautilusFileSortType sort_type = NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;
    int order;

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, TRUE, TRUE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, TRUE, FALSE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, FALSE, TRUE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, FALSE, FALSE);
    g_assert_cmpint (order, ==, 0);
}

typedef struct
{
    const gsize len;
    GStrv expected_names;
    GStrv expected_content;
    gboolean *success;
} NautilusFileBatchRenameTestData;

static void
batch_rename_callback (NautilusFile *file,
                       GFile        *result_location,
                       GError       *error,
                       gpointer      callback_data)
{
    NautilusFileBatchRenameTestData *data = callback_data;
    g_autoptr (GStrvBuilder) name_builder = g_strv_builder_new ();
    g_autoptr (GStrvBuilder) content_builder = g_strv_builder_new ();
    g_auto (GStrv) result_names = NULL, result_content = NULL;

    g_assert_no_error (error);
    g_assert_cmpint (data->len, ==, g_strv_length (data->expected_names));
    g_assert_cmpint (data->len, ==, g_strv_length (data->expected_content));

    for (guint i = 0; i < data->len; i++)
    {
        g_autofree gchar *path = g_strconcat (test_get_tmp_dir (),
                                              G_DIR_SEPARATOR_S,
                                              data->expected_names[i],
                                              NULL);
        g_autoptr (GFile) location = g_file_new_for_path (path);
        g_autoptr (GFileInfo) info = g_file_query_info (location, NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                                        G_FILE_QUERY_INFO_NONE, NULL, NULL);
        g_autoptr (GFileInputStream) stream = g_file_read (location, NULL, NULL);
        gchar content[1024];

        g_assert_nonnull (stream);
        g_assert_true (g_input_stream_read_all (G_INPUT_STREAM (stream), content, sizeof (content),
                                                NULL, NULL, NULL));
        g_assert_true (g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL));

        g_strv_builder_add (name_builder, g_file_info_get_display_name (info));
        g_strv_builder_add (content_builder, content);
    }

    result_names = g_strv_builder_end (name_builder);
    result_content = g_strv_builder_end (content_builder);

    for (guint i = 0; i < data->len; i++)
    {
        g_autofree gchar *expected_name = g_path_get_basename (data->expected_names[i]);
        g_autofree gchar *expected_content = g_path_get_basename (data->expected_content[i]);

        g_assert_cmpstr (result_names[i], ==, expected_name);
        g_assert_cmpstr (result_content[i], ==, expected_content);
    }

    *data->success = TRUE;
}

static void
batch_rename_test (const GStrv hierarchy,
                   const GStrv original_names,
                   const GStrv expected_names)
{
    g_autolist (NautilusFile) files = NULL;
    g_autolist (GString) new_names = NULL;
    const gsize len = g_strv_length (expected_names);
    gboolean success = FALSE;
    NautilusFileBatchRenameTestData data = { len, expected_names, original_names, &success };

    create_hierarchy_from_template (hierarchy, "");

    for (gint i = len - 1; i >= 0; i--)
    {
        g_autofree gchar *origial_path = g_strconcat (test_get_tmp_dir (),
                                                      G_DIR_SEPARATOR_S,
                                                      original_names[i],
                                                      NULL);
        g_autoptr (GFile) location = g_file_new_for_path (origial_path);
        NautilusFile *file = nautilus_file_get (location);
        GString *new_name = g_string_new_take (g_path_get_basename (expected_names[i]));

        files = g_list_prepend (files, file);
        new_names = g_list_prepend (new_names, new_name);
    }

    nautilus_file_batch_rename (files, new_names, batch_rename_callback, &data);

    g_assert_true (success);

    /* Test undo by changing the expected names */
    data.expected_names = original_names;
    success = FALSE;

    test_operation_undo ();

    batch_rename_callback (NULL, NULL, NULL, &data);

    g_assert_true (success);

    test_clear_tmp_dir ();
}

static void
test_file_batch_rename_cycles (void)
{
    char *test_cases[][2][10] =
    {
        /* Small cycle */
        {{"file_1", "file_2", NULL},
         {"file_2", "file_1", NULL}},
        /* Medium cycle */
        {{"file_1", "file_2", "file_3", "file_4", "file_5", "file_6", "file_7", "file_8", "file_9", NULL},
         {"file_9", "file_1", "file_2", "file_3", "file_4", "file_5", "file_6", "file_7", "file_8", NULL}},
        /* Multi-cycle */
        {{"file_1", "file_2", "file_3", "file_4", "file_5", "file_6", "file_7", "file_8", NULL},
         {"file_8", "file_3", "file_4", "file_5", "file_6", "file_7", "file_2", "file_1", NULL}},
    };

    g_test_bug ("https://gitlab.gnome.org/GNOME/nautilus/-/issues/1443");

    for (guint i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        batch_rename_test (test_cases[i][0], test_cases[i][0], test_cases[i][1]);
    }
}

static void
test_file_batch_rename_chains (void)
{
    char *test_cases[][2][10] =
    {
        /* Medium chain */
        {{"file_1", "file_2", "file_3", "file_4", "file_5", "file_6", "file_7", "file_8", "file_9", NULL},
         {"file_2", "file_3", "file_4", "file_5", "file_6", "file_7", "file_8", "file_9", "file_10", NULL}},
    };

    for (guint i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        batch_rename_test (test_cases[i][0], test_cases[i][0], test_cases[i][1]);
    }
}

static void
test_file_batch_rename_replace (void)
{
    char *test_cases[][2][10] =
    {
        /* File Extension replacement */
        {{
             "file_1.jpg", "file_2.jpeg", "file_3.gif", "file_4.png", "file_5.webm",
             "file_6.avif", "file_7.jxl", "file_8.jpeg", "file_9.bmp", NULL
         },
         {
             "file_1.jpg", "file_2.jpg", "file_3.gif", "file_4.png", "file_5.webm",
             "file_6.avif", "file_7.jxl", "file_8.jpg", "file_9.bmp", NULL
         }},
    };

    for (guint i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        batch_rename_test (test_cases[i][0], test_cases[i][0], test_cases[i][1]);
    }
}

static void
test_file_batch_rename_different_folders (void)
{
    char *hierarchy[] =
    {
        "folder1" G_DIR_SEPARATOR_S,
        "folder1" G_DIR_SEPARATOR_S "file_1",
        "folder2" G_DIR_SEPARATOR_S,
        "folder2" G_DIR_SEPARATOR_S "file_1",
        NULL
    };
    char *test_cases[][2][3] =
    {
        /* Rename to the same name but in different folders */
        {{
             "folder1" G_DIR_SEPARATOR_S "file_1",
             "folder2" G_DIR_SEPARATOR_S "file_1",
             NULL
         },
         {
             "folder1" G_DIR_SEPARATOR_S "file_2",
             "folder2" G_DIR_SEPARATOR_S "file_2",
             NULL
         }},
    };

    for (guint i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        batch_rename_test (hierarchy, test_cases[i][0], test_cases[i][1]);
    }
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = nautilus_file_undo_manager_new ();

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/file-refcount/single-file",
                     test_file_refcount_single_file);
    g_test_add_func ("/file-check-name/bland",
                     test_file_check_name_bland);
    g_test_add_func ("/file-check-name/trailing-slash",
                     test_file_check_name_trailing_slash);
    g_test_add_func ("/file-duplicate-pointers/1.0",
                     test_file_duplicate_pointers);
    g_test_add_func ("/file-sort/order",
                     test_file_sort_order);
    g_test_add_func ("/file-sort/with-self",
                     test_file_sort_with_self);
    g_test_add_func ("/file-batch-rename/cycles",
                     test_file_batch_rename_cycles);
    g_test_add_func ("/file-batch-rename/chains",
                     test_file_batch_rename_chains);
    g_test_add_func ("/file-batch-rename/replace",
                     test_file_batch_rename_replace);
    g_test_add_func ("/file-batch-rename/different-folders",
                     test_file_batch_rename_different_folders);

    return g_test_run ();
}
