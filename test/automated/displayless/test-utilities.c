#include "test-utilities.h"

void
create_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    GFileOutputStream *out;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *file_name = NULL;

    location = g_file_new_for_path (g_get_tmp_dir ());

    file_name = g_strdup_printf ("engine_%s", search_engine);
    file = g_file_get_child (location, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }


    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
}

void
delete_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *file_name = NULL;

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

void
create_one_file (gchar *prefix)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    GError *error;
    gchar *file_name;

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    file_name = g_strdup_printf (file_name, "%s_first_dir", prefix);
    first_dir = g_file_get_child (root, file_name);
    g_clear_object (file_name);
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, &error);


    file_name = g_strdup_printf (file_name, "%s_first_dir_child", prefix);
    file = g_file_get_child (first_dir, file_name);
    g_clear_object (file_name);
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    file_name = g_strdup_printf (file_name, "%s_first_dir_child", prefix);
    second_dir = g_file_get_child (root, file_name);
    g_clear_object (file_name);
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

}