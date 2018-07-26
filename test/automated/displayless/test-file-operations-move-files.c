#include <glib.h>
#include <glib/gprintf.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include "src/nautilus-file-undo-manager.h"
#include <unistd.h>
#include "eel/eel-string.h"

/* This callback function quits the mainloop inside which the
 * asynchronous undo/redo operation happens.
 */
static void
quit_loop_callback (NautilusFileUndoManager *undo_manager,
                    GMainLoop               *loop)
{
    g_main_loop_quit (loop);
}

/* This undoes the last move operation blocking the current main thread. */
static void
test_move_operation_undo (void)
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
static void
test_move_operation_undo_redo (void)
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (GMainContext) context = NULL;
    gulong handler_id;    

    test_move_operation_undo ();

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

static void
test_move_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir_child");

    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_file_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir_child");

    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_file_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir_child");

    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_empty_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir_child");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_empty_directory_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_empty_directory_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir_child");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_directories_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_file_delete (file, NULL, NULL);
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_small_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_file_delete (file, NULL, NULL);
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_medium_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_directories_large_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_file_delete (file, NULL, NULL);
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_small_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_file_delete (file, NULL, NULL);
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_medium_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 50; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_file_delete (file, NULL, NULL);
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_large_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 500; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    for (int i = 0; i < 500; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're moving first_dir to second_dir.
 */
static void
test_move_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (first_dir, NULL));
}

static void
test_move_first_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_first_hierarchy_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (first_dir, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * /tmp/second_dir
 * We're moving first_dir to second_dir.
 */
static void
test_move_second_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));    

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_second_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);

    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));    
    g_file_delete (file, NULL, NULL);

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_second_hierarchy_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));    

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child/second_child
 * /tmp/second_dir
 * We're moving first_dir to second_dir.
 */
static void
test_move_third_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_third_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "first_child");
    file = g_file_get_child (file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);

    g_file_delete (second_dir, NULL, NULL);
}

static void
test_move_third_hierarchy_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_dir1/dir1_child
 * /tmp/first_dir/first_dir_dir2/dir2_child
 * /tmp/second_dir
 * We're moving first_dir to second_dir.
 */
static void
test_move_fourth_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "first_dir");

    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_false (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_fourth_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (second_dir, "first_dir");

    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);
    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_file_delete (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);
    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_file_delete (file, NULL, NULL);

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_fourth_hierarchy_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (second_dir, "first_dir");

    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_false (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir/second_dir_child
 * /tmp/third_dir
 * We're moving first_dir and second_dir to third_dir.
 */
static void
test_move_fifth_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    third_dir = g_file_get_child (root, "third_dir");
    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (third_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    result_file = g_file_get_child (third_dir, "second_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (second_dir, NULL));

    g_assert_true (g_file_delete (third_dir, NULL, NULL));
}

static void
test_move_fifth_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    third_dir = g_file_get_child (root, "third_dir");
    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo ();

    result_file = g_file_get_child (third_dir, "first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    result_file = g_file_get_child (third_dir, "second_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);
    g_assert_true (g_file_query_exists (second_dir, NULL));
    g_file_delete (second_dir, NULL, NULL);
}

static void
test_move_fifth_hierarchy_undo_redo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    third_dir = g_file_get_child (root, "third_dir");
    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_move_operation_undo_redo ();

    result_file = g_file_get_child (third_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    result_file = g_file_get_child (third_dir, "second_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (second_dir, "second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (second_dir, NULL));

    g_assert_true (g_file_delete (third_dir, NULL, NULL));
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/test-move-one-file/1.0",
                     test_move_one_file);
    g_test_add_func ("/test-move-one-file-undo/1.0",
                     test_move_one_file_undo);
    g_test_add_func ("/test-move-one-file-undo-redo/1.0",
                     test_move_one_file_undo_redo);
    g_test_add_func ("/test-move-one-empty-directory/1.0",
                     test_move_one_empty_directory);
    g_test_add_func ("/test-move-one-empty-directory-undo/1.0",
                     test_move_one_empty_directory_undo);
    g_test_add_func ("/test-move-one-empty-directory-undo-redo/1.0",
                     test_move_one_empty_directory_undo_redo);
    g_test_add_func ("/test-move-files/1.0",
                     test_move_files_small);
    g_test_add_func ("/test-move-files-undo/1.0",
                     test_move_files_small_undo);
    g_test_add_func ("/test-move-files-undo-redo/1.0",
                     test_move_files_small_undo_redo);
    g_test_add_func ("/test-move-files/1.1",
                     test_move_files_medium);
    g_test_add_func ("/test-move-files-undo/1.1",
                     test_move_files_medium_undo);
    g_test_add_func ("/test-move-files-undo-redo/1.1",
                     test_move_files_medium_undo_redo);
    g_test_add_func ("/test-move-files/1.2",
                     test_move_files_large);
    g_test_add_func ("/test-move-files-undo/1.2",
                     test_move_files_large_undo);
    g_test_add_func ("/test-move-files-undo-redo/1.2",
                     test_move_files_large_undo_redo);
    g_test_add_func ("/test-move-directories/1.0",
                     test_move_directories_small);
    g_test_add_func ("/test-move-directories-undo/1.0",
                     test_move_directories_small_undo);
    g_test_add_func ("/test-move-directories-undo-redo/1.0",
                     test_move_directories_small_undo_redo);
    g_test_add_func ("/test-move-directories/1.1",
                     test_move_directories_medium);
    g_test_add_func ("/test-move-directories-undo/1.1",
                     test_move_directories_medium_undo);
    g_test_add_func ("/test-move-directories-undo-redo/1.1",
                     test_move_directories_medium_undo_redo);
    g_test_add_func ("/test-move-directories/1.2",
                     test_move_directories_large);
    g_test_add_func ("/test-move-directories-undo/1.2",
                     test_move_directories_large_undo);
    g_test_add_func ("/test-move-directories-undo-redo/1.2",
                     test_move_directories_large_undo_redo);
    g_test_add_func ("/test-move-hierarchy/1.0",
                     test_move_first_hierarchy);
    g_test_add_func ("/test-move-hierarchy-undo/1.0",
                     test_move_first_hierarchy_undo);
    g_test_add_func ("/test-move-hierarchy-undo-redo/1.0",
                     test_move_first_hierarchy_undo_redo);
    g_test_add_func ("/test-move-hierarchy/1.1",
                     test_move_second_hierarchy);
    g_test_add_func ("/test-move-hierarchy-undo/1.1",
                     test_move_second_hierarchy_undo);
    g_test_add_func ("/test-move-hierarchy-undo-redo/1.1",
                     test_move_second_hierarchy_undo_redo);
    g_test_add_func ("/test-move-hierarchy/1.2",
                     test_move_third_hierarchy);
    g_test_add_func ("/test-move-hierarchy-undo/1.2",
                     test_move_third_hierarchy_undo);
    g_test_add_func ("/test-move-hierarchy-undo-redo/1.2",
                     test_move_third_hierarchy_undo_redo);
    g_test_add_func ("/test-move-hierarchy/1.3",
                     test_move_fourth_hierarchy);
    g_test_add_func ("/test-move-hierarchy-undo/1.3",
                     test_move_fourth_hierarchy_undo);
    g_test_add_func ("/test-move-hierarchy-undo-redo/1.3",
                     test_move_fourth_hierarchy_undo_redo);
    g_test_add_func ("/test-move-hierarchy/1.4",
                     test_move_fifth_hierarchy);
    g_test_add_func ("/test-move-hierarchy-undo/1.4",
                     test_move_fifth_hierarchy_undo);
    g_test_add_func ("/test-move-hierarchy-undo-redo/1.4",
                     test_move_fifth_hierarchy_undo_redo);
}

int
main (int argc, char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    undo_manager = nautilus_file_undo_manager_new ();
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();

    setup_test_suite ();

    return g_test_run ();
}