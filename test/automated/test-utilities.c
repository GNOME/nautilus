#include "test-utilities.h"

#define ASYNC_FILE_LIMIT 100

static gchar *nautilus_tmp_dir = NULL;

const gchar *
test_get_tmp_dir (void)
{
    if (nautilus_tmp_dir == NULL)
    {
        nautilus_tmp_dir = g_dir_make_tmp ("nautilus.XXXXXX", NULL);
    }

    return nautilus_tmp_dir;
}

void
test_clear_tmp_dir (void)
{
    if (nautilus_tmp_dir != NULL)
    {
        rmdir (nautilus_tmp_dir);
        g_clear_pointer (&nautilus_tmp_dir, g_free);
    }
}

static gboolean config_dir_initialized = FALSE;

void
test_init_config_dir (void)
{
    if (config_dir_initialized == FALSE)
    {
        /* Initialize bookmarks */
        g_autofree gchar *gtk3_dir = g_build_filename (g_get_user_config_dir (),
                                                       "gtk-3.0",
                                                       NULL);
        g_autofree gchar *bookmarks_path = g_build_filename (gtk3_dir,
                                                             "bookmarks",
                                                             NULL);
        g_autoptr (GFile) bookmarks_file = g_file_new_for_path (bookmarks_path);
        g_autoptr (GError) error = NULL;

        if (g_mkdir_with_parents (gtk3_dir, 0700) == -1)
        {
            int saved_errno = errno;

            g_error ("Failed to create bookmarks folder %s: %s",
                     gtk3_dir, g_strerror (saved_errno));
            return;
        }

        g_autoptr (GFileOutputStream) stream = g_file_replace (bookmarks_file,
                                                               NULL,
                                                               FALSE,
                                                               G_FILE_CREATE_REPLACE_DESTINATION,
                                                               NULL, &error);
        g_assert_no_error (error);
        g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error);
        g_assert_no_error (error);

        g_debug ("Initialized config folder %s", g_get_user_config_dir ());
        config_dir_initialized = TRUE;
    }
}

void
empty_directory_by_prefix (GFile *parent,
                           gchar *prefix)
{
    g_autoptr (GFileEnumerator) enumerator = NULL;
    g_autoptr (GFile) child = NULL;

    enumerator = g_file_enumerate_children (parent,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL,
                                            NULL);

    g_file_enumerator_iterate (enumerator, NULL, &child, NULL, NULL);
    while (child != NULL)
    {
        g_autofree gchar *basename = g_file_get_basename (child);
        gboolean res;

        if (g_str_has_prefix (basename, prefix))
        {
            res = g_file_delete (child, NULL, NULL);
            /* The directory is not empty */
            if (!res)
            {
                empty_directory_by_prefix (child, prefix);
                g_file_delete (child, NULL, NULL);
            }
        }

        g_file_enumerator_iterate (enumerator, NULL, &child, NULL, NULL);
    }
}

void
create_hierarchy_from_template (const GStrv  hier,
                                const gchar *substitution)
{
    const gchar *root_path = test_get_tmp_dir ();

    for (guint i = 0; hier[i] != NULL; i++)
    {
        g_autoptr (GFile) file = NULL;
        g_autoptr (GString) file_path = g_string_new (hier[i]);
        gboolean is_directory = g_str_has_suffix (file_path->str, G_DIR_SEPARATOR_S);

        g_string_replace (file_path, "%s", substitution, 0);
        g_string_prepend (file_path, G_DIR_SEPARATOR_S);
        g_string_prepend (file_path, root_path);
        file = g_file_new_for_path (file_path->str);

        if (is_directory)
        {
            g_file_make_directory (file, NULL, NULL);
        }
        else
        {
            g_autoptr (GFileOutputStream) stream = g_file_create (file, G_FILE_CREATE_NONE,
                                                                  NULL, NULL);
        }
    }
}

static void
delete_hierarchy_from_template (const GStrv  hier,
                                const gchar *substitution)
{
    const gchar *root_path = test_get_tmp_dir ();
    guint len = g_strv_length (hier);

    for (guint i = 1; i <= len; i++)
    {
        g_autoptr (GFile) file = NULL;
        g_autoptr (GString) file_path = g_string_new (hier[len - i]);

        g_string_replace (file_path, "%s", substitution, 0);
        g_string_prepend (file_path, G_DIR_SEPARATOR_S);
        g_string_prepend (file_path, root_path);
        file = g_file_new_for_path (file_path->str);

        g_file_delete (file, NULL, NULL);
    }
}

const GStrv search_hierarchy = (char *[])
{
    "engine_%s",

    "engine_%s_directory/",
    "engine_%s_directory/%s_child",

    "engine_%s_second_directory/",
    "engine_%s_second_directory/engine_%s_child",

    "%s_directory/",
    "%s_directory/engine_%s_child",
    NULL
};

void
create_search_file_hierarchy (gchar *search_engine)
{
    create_hierarchy_from_template (search_hierarchy, search_engine);
}

void
delete_search_file_hierarchy (gchar *search_engine)
{
    delete_hierarchy_from_template (search_hierarchy, search_engine);
}

/* This undoes the last operation blocking the current main thread. */
void
test_operation_undo (void)
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (GMainContext) context = NULL;
    gulong handler_id;

    context = g_main_context_new ();
    g_main_context_push_thread_default (context);
    loop = g_main_loop_new (context, FALSE);

    handler_id = g_signal_connect_swapped (nautilus_file_undo_manager_get (),
                                           "undo-changed",
                                           G_CALLBACK (g_main_loop_quit),
                                           loop);

    nautilus_file_undo_manager_undo (NULL, NULL);

    g_main_loop_run (loop);

    g_main_context_pop_thread_default (context);

    g_signal_handler_disconnect (nautilus_file_undo_manager_get (),
                                 handler_id);
}

/* This undoes and redoes the last move operation blocking the current main thread. */
void
test_operation_undo_redo (void)
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (GMainContext) context = NULL;
    gulong handler_id;

    test_operation_undo ();

    context = g_main_context_new ();
    g_main_context_push_thread_default (context);
    loop = g_main_loop_new (context, FALSE);

    handler_id = g_signal_connect_swapped (nautilus_file_undo_manager_get (),
                                           "undo-changed",
                                           G_CALLBACK (g_main_loop_quit),
                                           loop);

    nautilus_file_undo_manager_redo (NULL, NULL);

    g_main_loop_run (loop);

    g_main_context_pop_thread_default (context);

    g_signal_handler_disconnect (nautilus_file_undo_manager_get (),
                                 handler_id);
}

/* Creates the following hierarchy:
 * /tmp/`prefix`_first_dir/`prefix`_first_dir_child
 * /tmp/`prefix`_second_dir/
 */
void
create_one_file (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    GFileOutputStream *out;
    gchar *file_name;

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);
    g_free (file_name);

    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (second_dir, NULL, NULL);
}

/* Creates the same hierarchy as above, but all files being directories. */
void
create_one_empty_directory (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    gchar *file_name;

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);
    g_free (file_name);

    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (second_dir, NULL, NULL);
}

static void
create_file_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      data)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileOutputStream) out = g_file_create_finish (G_FILE (source_object), res, &error);
    guint *count = data;

    g_assert_no_error (error);

    (*count)++;
}

void
create_multiple_files (gchar *prefix,
                       guint  number_of_files)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;
    gchar *file_name;
    guint count = 0;

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    for (guint i = 0; i < number_of_files; i++)
    {
        g_autoptr (GFile) file = NULL;

        file_name = g_strdup_printf ("%s_file_%i", prefix, i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);

        g_file_create_async (file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT,
                             NULL, create_file_cb, &count);

        if ((i + 1) % ASYNC_FILE_LIMIT == 0)
        {
            /* Need to rate limit the number of open files */
            ITER_CONTEXT_WHILE (count < i);
        }
    }

    ITER_CONTEXT_WHILE (count < number_of_files);

    file_name = g_strdup_printf ("%s_dir", prefix);
    dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (dir, NULL, NULL);
}

static void
create_dir_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      data)
{
    g_autoptr (GError) error = NULL;
    g_file_make_directory_finish (G_FILE (source_object), res, &error);
    guint *count = data;

    g_assert_no_error (error);

    (*count)++;
}

void
create_multiple_directories (gchar *prefix,
                             guint  number_of_directories)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;
    gchar *file_name;
    guint count = 0;

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    for (guint i = 0; i < number_of_directories; i++)
    {
        g_autoptr (GFile) file = NULL;

        file_name = g_strdup_printf ("%s_dir_%i", prefix, i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);

        g_file_make_directory_async (file, G_PRIORITY_DEFAULT,
                                     NULL, create_dir_cb, &count);

        if ((i + 1) % ASYNC_FILE_LIMIT == 0)
        {
            /* Need to rate limit the number of open files */
            ITER_CONTEXT_WHILE (count < i);
        }
    }

    ITER_CONTEXT_WHILE (count < number_of_directories);

    file_name = g_strdup_printf ("%s_destination_dir", prefix);
    dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_file_make_directory (dir, NULL, NULL);
}

void
create_first_hierarchy (gchar *prefix)
{
    const GStrv first_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_child/",
        "%s_first_dir/%s_second_child/",

        "%s_second_dir/",
        NULL
    };

    create_hierarchy_from_template (first_hierarchy, prefix);
}

void
create_second_hierarchy (gchar *prefix)
{
    const GStrv second_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_child/",
        "%s_first_dir/%s_first_child/%s_second_child/",

        "%s_second_dir/",
        NULL
    };

    create_hierarchy_from_template (second_hierarchy, prefix);
}

void
create_third_hierarchy (gchar *prefix)
{
    const GStrv third_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_dir_dir1/",
        "%s_first_dir/%s_first_dir_dir1/%s_dir1_child/",

        "%s_first_dir/%s_first_dir_dir2/",
        "%s_first_dir/%s_first_dir_dir2/%s_dir2_child",

        "%s_second_dir/",
        NULL
    };

    create_hierarchy_from_template (third_hierarchy, prefix);
}

void
create_fourth_hierarchy (gchar *prefix)
{
    const GStrv fourth_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_dir_child/",

        "%s_second_dir/",
        "%s_second_dir/%s_second_dir_child/",

        "%s_third_dir/",
        NULL
    };

    create_hierarchy_from_template (fourth_hierarchy, prefix);
}

void
create_multiple_full_directories (gchar *prefix,
                                  guint  number_of_directories)
{
    g_autoptr (GFile) root = NULL;

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    for (guint i = 0; i < number_of_directories; i++)
    {
        g_autoptr (GFile) directory = NULL;
        g_autoptr (GFile) file = NULL;
        gchar *file_name;

        file_name = g_strdup_printf ("%s_directory_%i", prefix, i);

        directory = g_file_get_child (root, file_name);
        g_free (file_name);

        file_name = g_strdup_printf ("%s_file_%i", prefix, i);
        file = g_file_get_child (directory, file_name);
        g_free (file_name);

        g_file_make_directory_with_parents (file, NULL, NULL);
    }
}
