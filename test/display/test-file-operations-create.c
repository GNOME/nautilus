/*
 * Copyright © 2025 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests for file creation operations
 */

#include <nautilus-file-undo-manager.h>
#include <nautilus-file-operations.h>
#include <nautilus-tag-manager.h>

#include <gtk/gtk.h>

#include "test-utilities.h"

typedef struct
{
    GFile *file;
    gboolean success;
    GMainLoop *loop;
} CreateTestData;

static void
create_test_data_clear (CreateTestData *data)
{
    g_clear_object (&data->file);
    data->success = FALSE;
    g_clear_pointer (&data->loop, g_main_loop_unref);
}

static void
create_test_data_init (CreateTestData *data)
{
    data->file = NULL;
    data->success = FALSE;
    data->loop = g_main_loop_new (NULL, FALSE);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CreateTestData, create_test_data_clear)

static void
create_done_callback (GFile    *created_file,
                      gboolean  success,
                      gpointer  callback_data)
{
    CreateTestData *data = callback_data;

    data->file = g_object_ref (created_file);
    data->success = success;

    g_main_loop_quit (data->loop);
}

static void
assert_is_directory (GFile *location)
{
    g_autoptr (GFileInfo) info = g_file_query_info (location,
                                                    G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, NULL);

    g_assert_nonnull (info);
    g_assert_cmpint (g_file_info_get_file_type (info), ==, G_FILE_TYPE_DIRECTORY);
}

static void
test_create_folder (void)
{
    g_autofree gchar *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *foldername = "created_folder";
    g_autoptr (GFile) created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                foldername,
                                                                NULL);
    g_auto (CreateTestData) data = { 0 };

    create_test_data_init (&data);

    nautilus_file_operations_new_folder (NULL,
                                         NULL,
                                         parent_uri,
                                         foldername,
                                         create_done_callback,
                                         &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, created_file));
    assert_is_directory (created_file);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (created_file, NULL));

    test_operation_redo ();

    assert_is_directory (created_file);

    test_clear_tmp_dir ();
}

static void
test_create_folder_no_name (void)
{
    g_autofree gchar *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *default_foldername = "Untitled Folder";
    g_autoptr (GFile) created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                default_foldername,
                                                                NULL);
    g_auto (CreateTestData) data = { 0 };

    create_test_data_init (&data);

    nautilus_file_operations_new_folder (NULL,
                                         NULL,
                                         parent_uri,
                                         NULL,
                                         create_done_callback,
                                         &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, created_file));
    assert_is_directory (created_file);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (created_file, NULL));

    test_operation_redo ();

    assert_is_directory (created_file);

    test_clear_tmp_dir ();
}

static void
assert_file_content (GFile_autoptr  location,
                     const gchar   *expected_content,
                     const gsize    content_length,
                     const char    *mime_type)
{
    g_autoptr (GFileInputStream) stream = g_file_read (location, NULL, NULL);
    gchar buffer[512];
    gsize read_bytes = 0;

    g_assert_nonnull (stream);
    g_input_stream_read_all (G_INPUT_STREAM (stream),
                             buffer, sizeof (buffer), &read_bytes,
                             NULL, NULL);
    g_assert_cmpint (read_bytes, ==, content_length);
    g_assert_cmpmem (buffer, read_bytes, expected_content, content_length);

    g_autoptr (GFileInfo) info = g_file_query_info (location,
                                                    G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, NULL);
    g_assert_nonnull (info);
    g_assert_cmpint (g_file_info_get_size (info), ==, content_length);
    g_assert_cmpstr (g_file_info_get_content_type (info), ==, mime_type);
}

static void
test_create_file (void)
{
    g_autofree char *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *filename = "created_file.txt";
    const char *contents = "Hello, World!\n";
    const gsize content_length = strlen (contents);
    const char *mime_type = "text/plain";
    g_autoptr (GFile) created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                filename,
                                                                NULL);
    g_auto (CreateTestData) data = { 0 };

    create_test_data_init (&data);

    nautilus_file_operations_new_file (NULL,
                                       parent_uri,
                                       filename,
                                       contents,
                                       content_length,
                                       create_done_callback,
                                       &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, created_file));
    assert_file_content (created_file, contents, content_length, mime_type);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (created_file, NULL));

    test_operation_redo ();

    assert_file_content (created_file, contents, content_length, mime_type);

    test_clear_tmp_dir ();
}

static void
test_create_file_no_name (void)
{
    g_autofree char *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *default_filename = "Untitled Document";
    const char *contents = "Auto named\n";
    const gsize content_length = strlen (contents);
    const char *mime_type = "text/plain";
    g_autoptr (GFile) created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                default_filename,
                                                                NULL);
    g_auto (CreateTestData) data = { 0 };

    create_test_data_init (&data);

    nautilus_file_operations_new_file (NULL,
                                       parent_uri,
                                       NULL,
                                       contents,
                                       content_length,
                                       create_done_callback,
                                       &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, created_file));
    assert_file_content (created_file, contents, content_length, mime_type);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (created_file, NULL));

    test_operation_redo ();

    assert_file_content (created_file, contents, content_length, mime_type);

    test_clear_tmp_dir ();
}

static void
create_file_with_content (GFile_autoptr  location,
                          const gchar   *content,
                          const gsize    content_length)
{
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_TYPE_FILE_CREATE_FLAGS, NULL, NULL);

    g_assert_nonnull (stream);
    g_output_stream_write_all (G_OUTPUT_STREAM (stream),
                               content, content_length,
                               NULL,
                               NULL,
                               NULL);
}

static void
test_create_file_from_template (void)
{
    g_autofree char *template_uri = g_strconcat ("file://",
                                                 test_get_tmp_dir (),
                                                 "/template_file.txt",
                                                 NULL);
    g_autoptr (GFile) template_location = g_file_new_for_uri (template_uri);
    g_autofree char *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *filename = "created_file_from_template.txt";
    const char *contents = "Hello, World!\n";
    const gsize content_length = strlen (contents);
    g_autoptr (GFile) created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                filename,
                                                                NULL);
    const char *duplicate_filename = "created_file_from_template (2).txt";
    g_autoptr (GFile) duplicate_created_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                          duplicate_filename,
                                                                          NULL);
    g_auto (CreateTestData) data = { 0 };
    const char *mime_type = "text/plain";

    create_test_data_init (&data);

    create_file_with_content (template_location, contents, content_length);

    nautilus_file_operations_new_file_from_template (NULL,
                                                     parent_uri,
                                                     filename,
                                                     template_uri,
                                                     create_done_callback,
                                                     &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, created_file));
    assert_file_content (created_file, contents, content_length, mime_type);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (created_file, NULL));

    test_operation_redo ();

    assert_file_content (created_file, contents, content_length, mime_type);

    /* Try to create from the template with the same name to verify conflict
     * resolution. */
    create_test_data_clear (&data);
    create_test_data_init (&data);
    nautilus_file_operations_new_file_from_template (NULL,
                                                     parent_uri,
                                                     filename,
                                                     template_uri,
                                                     create_done_callback,
                                                     &data);

    g_main_loop_run (data.loop);

    g_assert_true (data.success);
    g_assert_true (g_file_equal (data.file, duplicate_created_file));
    assert_file_content (duplicate_created_file, contents, content_length, mime_type);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (duplicate_created_file, NULL));

    test_operation_redo ();

    assert_file_content (duplicate_created_file, contents, content_length, mime_type);

    test_clear_tmp_dir ();
}

typedef struct
{
    gboolean success;
    GMainLoop *loop;
} SaveTestData;

static void
save_callback_data_clear (SaveTestData *data)
{
    data->success = FALSE;
    g_clear_pointer (&data->loop, g_main_loop_unref);
}

static void
save_callback_data_init (SaveTestData *data)
{
    data->success = FALSE;
    data->loop = g_main_loop_new (NULL, FALSE);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (SaveTestData, save_callback_data_clear)

static void
save_done_callback (GHashTable *debuting_uris,
                    gboolean    success,
                    gpointer    callback_data)
{
    SaveTestData *data = callback_data;

    data->success = success;

    g_main_loop_quit (data->loop);
}

static GdkTexture *
make_test_texture (void)
{
    /* Create a simple 1x1 RGBA texture using gdk_pixbuf as an easy route */
    g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);

    gdk_pixbuf_fill (pixbuf, 0x00ff00ff);

    return gdk_texture_new_for_pixbuf (pixbuf);
}

static void
test_save_image_from_texture (void)
{
    g_autofree gchar *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    const char *image_basename = "Dropped image";
    g_autoptr (GdkTexture) texture = make_test_texture ();
    g_auto (SaveTestData) data = { 0 };

    save_callback_data_init (&data);

    nautilus_file_operations_save_image_from_texture (NULL,
                                                      NULL,
                                                      parent_uri,
                                                      image_basename,
                                                      texture,
                                                      save_done_callback,
                                                      &data);

    g_main_loop_run (data.loop);

    const char *image_name = "Dropped image.png";
    g_autoptr (GFile) saved_image = g_file_new_build_filename (test_get_tmp_dir (),
                                                               image_name,
                                                               NULL);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (saved_image, NULL));

    test_clear_tmp_dir ();
}

static void
test_save_image_from_clipboard (void)
{
    /* Dumb widget to use with the operation call */
    g_autoptr (GtkWidget) box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    GdkClipboard *clipboard = gtk_widget_get_clipboard (box);
    g_autoptr (GdkTexture) texture = make_test_texture ();
    g_autofree gchar *parent_uri = g_strconcat ("file://", test_get_tmp_dir (), NULL);
    g_auto (SaveTestData) data = { 0 };

    save_callback_data_init (&data);

    gdk_clipboard_set_texture (clipboard, texture);

    nautilus_file_operations_paste_image_from_clipboard (GTK_WIDGET (box),
                                                         NULL,
                                                         parent_uri,
                                                         save_done_callback,
                                                         &data);

    g_main_loop_run (data.loop);

    const char *image_name = "Pasted image.png";
    g_autoptr (GFile) saved_image = g_file_new_build_filename (test_get_tmp_dir (), image_name, NULL);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (saved_image, NULL));

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;
    g_autoptr (NautilusTagManager) tag_manager = NULL;

    gtk_test_init (&argc, &argv, NULL);

    g_setenv ("RUNNING_TESTS", "TRUE", TRUE);

    undo_manager = nautilus_file_undo_manager_new ();
    test_init_config_dir ();

    g_test_add_func ("/create/folder/basic",
                     test_create_folder);
    g_test_add_func ("/create/folder/no-name",
                     test_create_folder_no_name);
    g_test_add_func ("/create/file/direct-content",
                     test_create_file);
    g_test_add_func ("/create/file/no-name",
                     test_create_file_no_name);
    g_test_add_func ("/create/file/template",
                     test_create_file_from_template);
    g_test_add_func ("/create/image/texture",
                     test_save_image_from_texture);
    g_test_add_func ("/create/image/clipboard-texture",
                     test_save_image_from_clipboard);

    return g_test_run ();
}
