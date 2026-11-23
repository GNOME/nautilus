#include <glib.h>

#include <nautilus-directory-private.h>
#include <nautilus-file.h>
#include <nautilus-file-private.h>
#include <nautilus-file-undo-manager.h>
#include <nautilus-file-utilities.h>
#include <test-utilities.h>

static NautilusFile *
get_test_file (const gchar *relative_path)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            relative_path,
                                                            NULL);

    return nautilus_file_get (location);
}

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
assert_cmpfile_order (NautilusFile         *file_a,
                      int                   expected_order,
                      NautilusFile         *file_b,
                      gboolean              directories_first,
                      NautilusFileSortType  sort_attribute)
{
    int order = nautilus_file_compare_for_sort (file_a, file_b,
                                                sort_attribute,
                                                directories_first, FALSE);
    int reversed_order = nautilus_file_compare_for_sort (file_a, file_b,
                                                         sort_attribute,
                                                         directories_first, TRUE);
    int swapped_order = nautilus_file_compare_for_sort (file_b, file_a,
                                                        sort_attribute,
                                                        directories_first, FALSE);
    int swapped_reversed_order = nautilus_file_compare_for_sort (file_b, file_a,
                                                                 sort_attribute,
                                                                 directories_first, TRUE);
    gboolean reverse_direction = !(directories_first &&
                                   nautilus_file_is_directory (file_a) !=
                                   nautilus_file_is_directory (file_b));

    g_assert_true (order != 0);
    g_assert_true ((order > 0) == (expected_order > 0));
    g_assert_true (reversed_order != 0);
    g_assert_true ((reversed_order > 0) ==
                   (reverse_direction ? expected_order < 0 : expected_order > 0));
    g_assert_true (swapped_order != 0);
    g_assert_true ((swapped_order > 0) == (expected_order < 0));
    g_assert_true (swapped_reversed_order != 0);
    g_assert_true ((swapped_reversed_order > 0) ==
                   (reverse_direction ? expected_order > 0 : expected_order < 0));
}

#define assert_cmpfile(file_a, comparison, file_b, directories_first) \
        assert_cmpfile_order (file_a, \
                              (1 comparison 0) ? 1 : -1, \
                              file_b, \
                              directories_first, \
                              SORT_ATTRIBUTE)

#define assert_cmpfile_with_attribute(file_a, comparison, file_b, directories_first, attribute) \
        assert_cmpfile_order (file_a, \
                              (1 comparison 0) ? 1 : -1, \
                              file_b, \
                              directories_first, \
                              attribute)

static void
set_file_time_for_sort (NautilusFile *file,
                        const gchar  *attribute,
                        guint64       value)
{
    g_autofree gchar *uri = nautilus_file_get_uri (file);
    g_autoptr (GFile) location = g_file_new_for_uri (uri);
    g_autoptr (GError) error = NULL;

    g_assert_true (g_file_set_attribute_uint64 (location, attribute, value,
                                                G_FILE_QUERY_INFO_NONE, NULL, &error));
    g_assert_no_error (error);
}

static void
test_file_compare_for_sort_same_file (void)
{
    const NautilusFileSortType sort_types[] =
    {
        NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
        NAUTILUS_FILE_SORT_BY_SIZE,
        NAUTILUS_FILE_SORT_BY_TYPE,
        NAUTILUS_FILE_SORT_BY_MTIME,
        NAUTILUS_FILE_SORT_BY_ATIME,
        NAUTILUS_FILE_SORT_BY_BTIME,
        NAUTILUS_FILE_SORT_BY_RECENCY,
        NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE,
    };
    const GStrv hierarchy = (char *[])
    {
        "sort_same.txt",
        NULL
    };

    file_hierarchy_create (hierarchy, "");

    g_autoptr (NautilusFile) file = get_test_file ("sort_same.txt");

    for (guint i = 0; i < G_N_ELEMENTS (sort_types); i++)
    {
        for (guint j = 0; j < 4; j++)
        {
            gboolean directories_first = (j & 1) != 0;
            gboolean reversed = (j & 2) != 0;

            g_assert_cmpint (nautilus_file_compare_for_sort (file, file,
                                                             sort_types[i],
                                                             directories_first,
                                                             reversed), ==, 0);
        }
    }

    test_clear_tmp_dir ();
}

static void
test_file_compare_for_sort_display_name (void)
{
#define SORT_ATTRIBUTE NAUTILUS_FILE_SORT_BY_DISPLAY_NAME
    const GStrv hierarchy = (char *[])
    {
        "sort_name_a.txt",
        "sort_name_b.txt",
        ".sort_name_hidden",
        "#sort_name_hash",
        "sort_name_dir_a/",
        "sort_name_dir_a/same.txt",
        "sort_name_dir_b/",
        "sort_name_dir_b/same.txt",
        "sort_name_tie_a.txt",
        "sort_name_tie_z.txt",
        NULL
    };
    g_autoptr (NautilusFile) file_a = get_test_file ("sort_name_a.txt");
    g_autoptr (NautilusFile) file_b = get_test_file ("sort_name_b.txt");
    g_autoptr (NautilusFile) hidden = get_test_file (".sort_name_hidden");
    g_autoptr (NautilusFile) hash = get_test_file ("#sort_name_hash");
    g_autoptr (NautilusFile) file_in_a = get_test_file ("sort_name_dir_a/same.txt");
    g_autoptr (NautilusFile) file_in_b = get_test_file ("sort_name_dir_b/same.txt");
    g_autoptr (NautilusFile) tie_a = get_test_file ("sort_name_tie_a.txt");
    g_autoptr (NautilusFile) tie_z = get_test_file ("sort_name_tie_z.txt");

    file_hierarchy_create (hierarchy, "");

    assert_cmpfile (file_a, <, file_b, FALSE);

    /* Names starting with '.' or '#' sort last. */
    assert_cmpfile (file_a, <, hidden, FALSE);
    assert_cmpfile (file_a, <, hash, FALSE);

    /* Equal display names are tie-broken by directory name. */
    assert_cmpfile (file_in_a, <, file_in_b, FALSE);

    /* Equal display names in the same directory are tie-broken by name. */
    nautilus_file_set_display_name (tie_a, "same", NULL, TRUE);
    nautilus_file_set_display_name (tie_z, "same", NULL, TRUE);

    assert_cmpfile (tie_z, >, tie_a, FALSE);

    test_clear_tmp_dir ();
#undef SORT_ATTRIBUTE
}

static void
test_file_compare_for_sort_size (void)
{
#define SORT_ATTRIBUTE NAUTILUS_FILE_SORT_BY_SIZE
    const GStrv hierarchy = (char *[])
    {
        "sort_size_dir_small/",
        "sort_size_dir_small/child",
        "sort_size_dir_big/",
        "sort_size_dir_big/child1",
        "sort_size_dir_big/child2",
        "sort_size_dir_z_equal/",
        "sort_size_dir_z_equal/child",
        "sort_size_dir_unknown_a/",
        "sort_size_dir_unknown_b/",
        "sort_size_unknown_a",
        "sort_size_unknown_b",
        "sort_size_cross_a/",
        "sort_size_cross_a/equal",
        "sort_size_cross_b/",
        "sort_size_cross_b/equal",
        NULL
    };
    g_autoptr (GFile) small_location = g_file_new_build_filename (test_get_tmp_dir (), "sort_size_small", NULL);
    g_autoptr (GFile) large_location = g_file_new_build_filename (test_get_tmp_dir (), "sort_size_large", NULL);
    g_autoptr (GFile) equal_a_location = g_file_new_build_filename (test_get_tmp_dir (), "sort_size_equal_a", NULL);
    g_autoptr (GFile) equal_b_location = g_file_new_build_filename (test_get_tmp_dir (), "sort_size_equal_b", NULL);
    g_autoptr (NautilusFile) small = nautilus_file_get (small_location);
    g_autoptr (NautilusFile) large = nautilus_file_get (large_location);
    g_autoptr (NautilusFile) equal_a = nautilus_file_get (equal_a_location);
    g_autoptr (NautilusFile) equal_b = nautilus_file_get (equal_b_location);
    g_autoptr (NautilusFile) directory_small = get_test_file ("sort_size_dir_small");
    g_autoptr (NautilusFile) directory_big = get_test_file ("sort_size_dir_big");
    g_autoptr (NautilusFile) directory_z_equal = get_test_file ("sort_size_dir_z_equal");
    g_autoptr (NautilusFile) directory_unknown_a = get_test_file ("sort_size_dir_unknown_a");
    g_autoptr (NautilusFile) directory_unknown_b = get_test_file ("sort_size_dir_unknown_b");
    g_autoptr (NautilusFile) unknown_a = get_test_file ("sort_size_unknown_a");
    g_autoptr (NautilusFile) unknown_b = get_test_file ("sort_size_unknown_b");
    g_autoptr (NautilusFile) cross_a = get_test_file ("sort_size_cross_a/equal");
    g_autoptr (NautilusFile) cross_b = get_test_file ("sort_size_cross_b/equal");

    file_hierarchy_create (hierarchy, "");
    create_random_file (small_location, 1);
    create_random_file (large_location, 4096);
    create_random_file (equal_a_location, 16);
    create_random_file (equal_b_location, 16);

    file_load_attributes (small, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (large, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (equal_a, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (equal_b, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (cross_a, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (cross_b, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (directory_small,
                          NAUTILUS_ATTRIBUTE_INFO | NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
    file_load_attributes (directory_big,
                          NAUTILUS_ATTRIBUTE_INFO | NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
    file_load_attributes (directory_z_equal,
                          NAUTILUS_ATTRIBUTE_INFO | NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
    /* These only load the file type, leaving the item counts unknown. */
    file_load_attributes (directory_unknown_a, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (directory_unknown_b, NAUTILUS_ATTRIBUTE_INFO);

    assert_cmpfile (small, <, large, FALSE);

    /* Equal sizes are tie-broken by full path. */
    assert_cmpfile (equal_a, <, equal_b, FALSE);

    /* Equal sizes in different directories are tie-broken by directory name. */
    assert_cmpfile (cross_a, <, cross_b, FALSE);

    /* Equal display names in the same directory are tie-broken by name. */
    nautilus_file_set_display_name (equal_a, "same", NULL, TRUE);
    nautilus_file_set_display_name (equal_b, "same", NULL, TRUE);

    assert_cmpfile (equal_a, <, equal_b, FALSE);

    /* Files with unknown sizes are ordered relative to known sizes. */
    assert_cmpfile (unknown_a, <, small, FALSE);
    assert_cmpfile (unknown_a, <, unknown_b, FALSE);

    /* Directories are sorted by item count before files. */
    assert_cmpfile (directory_small, <, directory_big, FALSE);

    /* Directories with equal item counts are tie-broken by full path. */
    assert_cmpfile (directory_small, <, directory_z_equal, FALSE);

    /* Directories with unknown item counts are ordered relative to known ones. */
    assert_cmpfile (directory_unknown_a, <, directory_small, FALSE);
    assert_cmpfile (directory_unknown_a, <, directory_unknown_b, FALSE);

    assert_cmpfile (directory_small, <, small, FALSE);

    test_clear_tmp_dir ();
#undef SORT_ATTRIBUTE
}

static void
test_file_compare_for_sort_type (void)
{
#define SORT_ATTRIBUTE NAUTILUS_FILE_SORT_BY_TYPE
    const GStrv hierarchy = (char *[])
    {
        "sort_type_dir_a/",
        "sort_type_dir_b/",
        "sort_type_image.png",
        "sort_type_script.py",
        "sort_type_text_a.txt",
        "sort_type_text_b.txt",
        "sort_type_unknown_a",
        "sort_type_unknown_b",
        NULL
    };
    g_autoptr (NautilusFile) directory_a = get_test_file ("sort_type_dir_a");
    g_autoptr (NautilusFile) directory_b = get_test_file ("sort_type_dir_b");
    g_autoptr (NautilusFile) image = get_test_file ("sort_type_image.png");
    g_autoptr (NautilusFile) script = get_test_file ("sort_type_script.py");
    g_autoptr (NautilusFile) text_a = get_test_file ("sort_type_text_a.txt");
    g_autoptr (NautilusFile) text_b = get_test_file ("sort_type_text_b.txt");
    g_autoptr (NautilusFile) unknown_a = get_test_file ("sort_type_unknown_a");
    g_autoptr (NautilusFile) unknown_b = get_test_file ("sort_type_unknown_b");

    file_hierarchy_create (hierarchy, "");
    file_load_attributes (directory_a, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (directory_b, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (image, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (script, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (text_a, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (text_b, NAUTILUS_ATTRIBUTE_INFO);

    /* Directories are sorted before files. */
    assert_cmpfile (directory_a, <, text_a, FALSE);

    /* Files of the same type are tie-broken by full path. */
    assert_cmpfile (directory_a, <, directory_b, FALSE);
    assert_cmpfile (text_a, <, text_b, FALSE);

    /* Same generic type, so the MIME type breaks the tie. */
    assert_cmpfile (text_a, <, script, FALSE);

    /* Different types are compared by their type string. */
    int image_vs_text = nautilus_file_compare_for_sort (image, text_a,
                                                        SORT_ATTRIBUTE,
                                                        FALSE, FALSE);
    int text_vs_image = nautilus_file_compare_for_sort (text_a, image,
                                                        SORT_ATTRIBUTE,
                                                        FALSE, FALSE);

    /* Collation results need not be symmetric in magnitude, only in sign. */
    g_assert_cmpint (image_vs_text, !=, 0);
    g_assert_cmpint (image_vs_text * text_vs_image, <, 0);
    g_assert_cmpint (nautilus_file_compare_for_sort (image, text_a,
                                                     SORT_ATTRIBUTE,
                                                     FALSE, TRUE), ==, -image_vs_text);
    g_assert_cmpint (nautilus_file_compare_for_sort (text_a, image,
                                                     SORT_ATTRIBUTE,
                                                     FALSE, TRUE), ==, -text_vs_image);

    /* Files whose type is not known are ordered relative to known types. */
    assert_cmpfile (unknown_a, >, text_a, FALSE);
    assert_cmpfile (unknown_a, <, unknown_b, FALSE);

    test_clear_tmp_dir ();
#undef SORT_ATTRIBUTE
}

static void
test_file_compare_for_sort_directories_first (void)
{
#define SORT_ATTRIBUTE NAUTILUS_FILE_SORT_BY_DISPLAY_NAME
    const GStrv hierarchy = (char *[])
    {
        "sort_dirs_z_directory/",
        "sort_dirs_a_file",
        NULL
    };
    g_autoptr (NautilusFile) directory = get_test_file ("sort_dirs_z_directory");
    g_autoptr (NautilusFile) file = get_test_file ("sort_dirs_a_file");

    file_hierarchy_create (hierarchy, "");
    file_load_attributes (directory, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (file, NAUTILUS_ATTRIBUTE_INFO);

    /* Directories come first regardless of the sort criterion and reversed. */
    assert_cmpfile (directory, <, file, TRUE);

    /* Without directories_first the sort criterion decides. */
    assert_cmpfile (file, <, directory, FALSE);

    test_clear_tmp_dir ();
#undef SORT_ATTRIBUTE
}

static void
test_file_compare_for_sort_dates (void)
{
    const GStrv hierarchy = (char *[])
    {
        "sort_date_z_older.txt",
        "sort_date_a_newer.txt",
        "sort_date_unknown_a",
        "sort_date_unknown_b",
        NULL
    };
    g_autoptr (NautilusFile) older = get_test_file ("sort_date_z_older.txt");
    g_autoptr (NautilusFile) newer = get_test_file ("sort_date_a_newer.txt");
    g_autoptr (NautilusFile) unknown_a = get_test_file ("sort_date_unknown_a");
    g_autoptr (NautilusFile) unknown_b = get_test_file ("sort_date_unknown_b");

    file_hierarchy_create (hierarchy, "");

    /* The names order the opposite way, so these assert the times are used. */
    set_file_time_for_sort (older, G_FILE_ATTRIBUTE_TIME_MODIFIED, 1000000000);
    set_file_time_for_sort (newer, G_FILE_ATTRIBUTE_TIME_MODIFIED, 2000000000);
    set_file_time_for_sort (older, G_FILE_ATTRIBUTE_TIME_ACCESS, 1000000000);
    set_file_time_for_sort (newer, G_FILE_ATTRIBUTE_TIME_ACCESS, 2000000000);

    file_load_attributes (older, NAUTILUS_ATTRIBUTE_INFO);
    file_load_attributes (newer, NAUTILUS_ATTRIBUTE_INFO);

    assert_cmpfile_with_attribute (older, <, newer, FALSE, NAUTILUS_FILE_SORT_BY_MTIME);
    assert_cmpfile_with_attribute (older, <, newer, FALSE, NAUTILUS_FILE_SORT_BY_ATIME);

    /* Files with unknown times are ordered relative to known times. */
    assert_cmpfile_with_attribute (unknown_a, <, older, FALSE, NAUTILUS_FILE_SORT_BY_MTIME);
    assert_cmpfile_with_attribute (unknown_a, <, unknown_b, FALSE, NAUTILUS_FILE_SORT_BY_MTIME);

    /* Files outside the trash have no trashed time, so they fall back to the
     * full path. */
    assert_cmpfile_with_attribute (older, >, newer, FALSE, NAUTILUS_FILE_SORT_BY_TRASHED_TIME);

    /* Creation time may not be supported by the file system, and recency is
     * irrelevant outside recent directories; only assert they are consistent. */
    const NautilusFileSortType uncertain_sorts[] =
    {
        NAUTILUS_FILE_SORT_BY_BTIME,
        NAUTILUS_FILE_SORT_BY_RECENCY,
    };

    for (guint i = 0; i < G_N_ELEMENTS (uncertain_sorts); i++)
    {
        int order = nautilus_file_compare_for_sort (older, newer,
                                                    uncertain_sorts[i],
                                                    FALSE, FALSE);

        g_assert_cmpint (nautilus_file_compare_for_sort (newer, older,
                                                         uncertain_sorts[i],
                                                         FALSE, FALSE), ==, -order);
        g_assert_cmpint (nautilus_file_compare_for_sort (older, newer,
                                                         uncertain_sorts[i],
                                                         FALSE, TRUE), ==, -order);
    }

    test_clear_tmp_dir ();
}

static void
test_file_compare_for_sort_search_relevance (void)
{
#define SORT_ATTRIBUTE NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE
    const GStrv hierarchy = (char *[])
    {
        "sort_relevance_a.txt",
        "sort_relevance_b.txt",
        NULL
    };
    g_autoptr (NautilusFile) file_a = get_test_file ("sort_relevance_a.txt");
    g_autoptr (NautilusFile) file_b = get_test_file ("sort_relevance_b.txt");

    file_hierarchy_create (hierarchy, "");
    nautilus_file_set_search_relevance (file_a, 1.0);
    nautilus_file_set_search_relevance (file_b, 0.5);

    /* Higher relevance compares as greater. */
    assert_cmpfile (file_a, >, file_b, FALSE);

    /* Equal relevance is tie-broken by full path, but the result is inverted. */
    nautilus_file_set_search_relevance (file_b, 1.0);

    assert_cmpfile (file_a, >, file_b, FALSE);

    test_clear_tmp_dir ();
#undef SORT_ATTRIBUTE
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
        g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                data->expected_names[i],
                                                                NULL);
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

    file_hierarchy_create (hierarchy, "");

    for (gint i = len - 1; i >= 0; i--)
    {
        g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                original_names[i],
                                                                NULL);
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

static void
test_file_permissions_can_set (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);
    gint mode = 0600;

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));
    g_assert_true (g_file_set_attribute_uint32 (location,
                                                G_FILE_ATTRIBUTE_UNIX_MODE, mode,
                                                G_FILE_QUERY_INFO_NONE, NULL, NULL));

    g_autoptr (NautilusFile) file = nautilus_file_get (location);

    file_load_attributes (file, NAUTILUS_ATTRIBUTE_INFO);
    g_assert_true (nautilus_file_can_set_permissions (file));

    test_clear_tmp_dir ();
}

typedef struct
{
    NautilusFile *file;
    gboolean done;
} PermissionTestData;

static void
file_changed_set_cb (NautilusFile *file,
                     gboolean     *done)
{
    *done = TRUE;
}

static void
permissions_success_cb (NautilusFile *file,
                        GFile        *result_location,
                        GError       *error,
                        gpointer      user_data)
{
    PermissionTestData *perm_data = user_data;

    g_assert_no_error (error);
    g_assert_true (file == perm_data->file);

    perm_data->done = TRUE;
}

static void
test_file_permissions_set_basic (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));

    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    PermissionTestData perm_data = { file, FALSE };
    gboolean file_changed = FALSE;
    guint32 perms = 0600;

    file_load_attributes (file, NAUTILUS_ATTRIBUTE_INFO);
    g_signal_connect (file, "changed", G_CALLBACK (file_changed_set_cb), &file_changed);

    nautilus_file_set_permissions (file, perms, permissions_success_cb, &perm_data);
    ITER_CONTEXT_WHILE (!perm_data.done);

    g_autoptr (GFileInfo) info = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_assert_nonnull (info);
    g_assert_true (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE));

    guint32 mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

    g_assert_cmphex (mode & 0777, ==, perms);
    g_assert_true (file_changed);

    test_clear_tmp_dir ();
}

static void
test_file_permissions_set_same (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));

    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    gboolean file_changed = FALSE;
    g_autoptr (GFileInfo) info_before = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                           G_FILE_QUERY_INFO_NONE, NULL, NULL);
    guint32 mode_before = g_file_info_get_attribute_uint32 (info_before, G_FILE_ATTRIBUTE_UNIX_MODE);
    PermissionTestData perm_data = { file, FALSE };

    file_load_attributes (file, NAUTILUS_ATTRIBUTE_INFO);
    g_signal_connect (file, "changed", G_CALLBACK (file_changed_set_cb), &file_changed);

    nautilus_file_set_permissions (file, mode_before, permissions_success_cb, &perm_data);
    ITER_CONTEXT_WHILE (!perm_data.done);

    g_autoptr (GFileInfo) info_after = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_assert_nonnull (info_after);
    g_assert_true (g_file_info_has_attribute (info_after, G_FILE_ATTRIBUTE_UNIX_MODE));

    guint32 mode_after = g_file_info_get_attribute_uint32 (info_after, G_FILE_ATTRIBUTE_UNIX_MODE);

    g_assert_cmphex (mode_after, ==, mode_before);
    g_assert_false (file_changed);

    test_clear_tmp_dir ();
}

static void
test_directory_counts (void)
{
    const GStrv deep_hierarchy = (char *[])
    {
        "deepcount/",
        "deepcount/file1",
        "deepcount/dir1/",
        "deepcount/dir1/file2",
        "deepcount/dir1/dir1a/",
        "deepcount/dir1/dir1a/file3",
        "deepcount/dir2/",
        "deepcount/dir2/file4",
        NULL
    };
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (), "deepcount", NULL);
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    guint dir_count = 0, file_count = 0, unreadable = 0, item_count = 0;
    NautilusRequestStatus status;
    gboolean count_unreadable = TRUE;

    file_hierarchy_create (deep_hierarchy, "");
    file_load_attributes (file,
                          NAUTILUS_ATTRIBUTE_INFO |
                          NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
                          NAUTILUS_ATTRIBUTE_DEEP_COUNT);

    g_assert_true (nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable));
    g_assert_cmpint (item_count, ==, 3);
    g_assert_false (count_unreadable);

    status = nautilus_file_get_deep_counts (file, &dir_count, &file_count, &unreadable, NULL, TRUE);
    g_assert_cmpint (status, ==, NAUTILUS_REQUEST_DONE);
    g_assert_cmpint (dir_count, ==, 3);
    g_assert_cmpint (file_count, ==, 4);
    g_assert_cmpint (unreadable, ==, 0);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    undo_manager = nautilus_file_undo_manager_new ();

    g_test_add_func ("/file-refcount/single-file",
                     test_file_refcount_single_file);
    g_test_add_func ("/file-check-name/bland",
                     test_file_check_name_bland);
    g_test_add_func ("/file-check-name/trailing-slash",
                     test_file_check_name_trailing_slash);
    g_test_add_func ("/file-duplicate-pointers/1.0",
                     test_file_duplicate_pointers);
    g_test_add_func ("/file-sort/same-file",
                     test_file_compare_for_sort_same_file);
    g_test_add_func ("/file-sort/display-name",
                     test_file_compare_for_sort_display_name);
    g_test_add_func ("/file-sort/size",
                     test_file_compare_for_sort_size);
    g_test_add_func ("/file-sort/type",
                     test_file_compare_for_sort_type);
    g_test_add_func ("/file-sort/directories-first",
                     test_file_compare_for_sort_directories_first);
    g_test_add_func ("/file-sort/dates",
                     test_file_compare_for_sort_dates);
    g_test_add_func ("/file-sort/search-relevance",
                     test_file_compare_for_sort_search_relevance);
    g_test_add_func ("/file-batch-rename/cycles",
                     test_file_batch_rename_cycles);
    g_test_add_func ("/file-batch-rename/chains",
                     test_file_batch_rename_chains);
    g_test_add_func ("/file-batch-rename/replace",
                     test_file_batch_rename_replace);
    g_test_add_func ("/file-batch-rename/different-folders",
                     test_file_batch_rename_different_folders);
    g_test_add_func ("/file/permissions/can_set",
                     test_file_permissions_can_set);
    g_test_add_func ("/file/permissions/basic",
                     test_file_permissions_set_basic);
    g_test_add_func ("/file/permissions/same",
                     test_file_permissions_set_same);
    g_test_add_func ("/file/deep-counts/basic",
                     test_directory_counts);

    return g_test_run ();
}
