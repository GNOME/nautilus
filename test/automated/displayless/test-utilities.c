#include "test-utilities.h"

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
        gboolean res;

        if (g_str_has_prefix (g_file_get_basename (child), prefix))
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
create_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    GFileOutputStream *out;
    gchar *file_name;

    location = g_file_new_for_path (g_get_tmp_dir ());

    file_name = g_strdup_printf ("engine_%s", search_engine);
    file = g_file_get_child (location, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);

    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);

    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);

    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);
}

void
delete_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    gchar *file_name;

    location = g_file_new_for_path (g_get_tmp_dir ()); 

    file_name = g_strdup_printf ("engine_%s", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);
}

/* This callback function quits the mainloop inside which the
 * asynchronous undo/redo operation happens.
 */

void
quit_loop_callback (NautilusFileUndoManager *undo_manager,
                    GMainLoop               *loop)
{
    g_main_loop_quit (loop);
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

    handler_id = g_signal_connect (nautilus_file_undo_manager_get (),
                                   "undo-changed",
                                   G_CALLBACK (quit_loop_callback),
                                   loop);

    nautilus_file_undo_manager_undo (NULL);

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

    handler_id = g_signal_connect (nautilus_file_undo_manager_get (),
                                   "undo-changed",
                                   G_CALLBACK (quit_loop_callback),
                                   loop);

    nautilus_file_undo_manager_redo (NULL);

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

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);
    g_free (file_name);

    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref (out);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);
    g_free (file_name);

    g_assert_true (second_dir != NULL);
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

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);

    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);
}

void
create_multiple_files (gchar *prefix, gint number_of_files)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < number_of_files; i++)
    {
        GFileOutputStream *out;

        file_name = g_strdup_printf ("%s_file_%i", prefix, i);
        file = g_file_get_child (root, file_name);

        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
        g_object_unref (out);
    }

    file_name = g_strdup_printf ("%s_dir", prefix);
    dir = g_file_get_child (root, file_name);

    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);
}

void
create_multiple_directories (gchar *prefix, gint number_of_directories)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < number_of_directories; i++)
    {
        file_name = g_strdup_printf ("%s_file_%i", prefix, i);
        file = g_file_get_child (root, file_name);

        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
    }

    file_name = g_strdup_printf ("%s_dir", prefix);
    dir = g_file_get_child (root, file_name);

    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);
}

void
create_first_hierarchy (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_child", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file_name = g_strdup_printf ("%s_second_child", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);

    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);
}

void
create_second_hierarchy (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_child", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file_name = g_strdup_printf ("%s_second_child", prefix);
    file = g_file_get_child (file, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);

    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);
}

void
create_third_hierarchy (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_dir1", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_dir1_child", prefix);
    file = g_file_get_child (file, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_dir2", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_dir2_child", prefix);
    file = g_file_get_child (file, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);

    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);
}

void
create_fourth_hierarchy (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf ("%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);

    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir", prefix);
    second_dir = g_file_get_child (root, file_name);

    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file_name = g_strdup_printf ("%s_second_dir_child", prefix);
    file = g_file_get_child (second_dir, file_name);

    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_third_dir", prefix);
    third_dir = g_file_get_child (root, file_name);

    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);
}

void
create_multiple_full_directories (gchar *prefix, gint number_of_directories)
{
    g_autoptr (GFile) root = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < number_of_directories; i++)
    {
        g_autoptr (GFile) directory = NULL;
        g_autoptr (GFile) file = NULL;
        gchar *file_name;

        file_name = g_strdup_printf ("%s_directory_%i", prefix, i);

        directory = g_file_get_child (root, file_name);

        g_file_make_directory (directory, NULL, NULL);

        file_name = g_strdup_printf ("%s_file_%i", prefix, i);
        file = g_file_get_child (directory, file_name);

        g_file_make_directory (file, NULL, NULL);
    }
}