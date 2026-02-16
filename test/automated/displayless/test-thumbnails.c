/*
 * Copyright © 2025 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "test-utilities.h"

#include <nautilus-application.h>
#include <nautilus-file-utilities.h>
#include <nautilus-thumbnails.h>

#include <glib.h>
#include <gio/gio.h>
#include <glycin.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define ICON_SIZE 256

typedef struct
{
    GdkPixbuf *pixbuf;
    gboolean done;
    GError *error;
} ThumbnailCallbackData;

static void
thumbnail_data_free (ThumbnailCallbackData *data)
{
    g_clear_error (&data->error);
    g_clear_object (&data->pixbuf);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (ThumbnailCallbackData, thumbnail_data_free)

static void
thumbnailing_done_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      data)
{
    g_autoptr (GError) error = NULL;
    ThumbnailCallbackData *cb_data = data;

    cb_data->pixbuf = nautilus_create_thumbnail_finish (res, &error);

    cb_data->error = g_steal_pointer (&error);
    cb_data->done = TRUE;
}

static gboolean
file_has_valid_thumbnail_path (GFile       *file,
                               const gchar *thumbnail_path)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInfo) info = g_file_query_info (file,
                                                    G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                                    G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);

    if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID) &&
        g_strcmp0 (thumbnail_path, g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH)) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

static void *
tile_memory (void  *data,
             gsize  size,
             uint   n)
{
    char *buffer = g_malloc (size * n);
    char *ptr = buffer;

    for (uint i = 0; i < n; i++, ptr += size)
    {
        memcpy (ptr, data, size);
    }

    return buffer;
}

static guint64
get_file_mime_type_and_mtime (GFile  *file,
                              gchar **mime_type)
{
    g_autoptr (GFileInfo) info = g_file_query_info (file,
                                                    "standard::*,time::*",
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, NULL);

    if (mime_type != NULL)
    {
        g_clear_pointer (mime_type, g_free);
        *mime_type = g_strdup (g_file_info_get_content_type (info));
    }

    return g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
}

static void
make_image_file (GFile *file)
{
    guint8 data[4] = {255, 255, 0, 0};
    gsize length = sizeof (data) * ICON_SIZE * ICON_SIZE;
    g_autoptr (GBytes) image_bytes = g_bytes_new_take (tile_memory (data,
                                                                    sizeof (data),
                                                                    ICON_SIZE * ICON_SIZE),
                                                       length);
    g_autoptr (GlyCreator) creator = gly_creator_new ("image/png", NULL);

    g_assert_nonnull (creator);
    gly_creator_set_sandbox_selector (creator, GLY_SANDBOX_SELECTOR_NOT_SANDBOXED);

    g_autoptr (GlyNewFrame) frame = gly_creator_add_frame (creator,
                                                           ICON_SIZE, ICON_SIZE,
                                                           GLY_MEMORY_A8R8G8B8,
                                                           image_bytes,
                                                           NULL);
    g_autoptr (GlyEncodedImage) encoded_image = gly_creator_create (creator, NULL);

    g_assert_nonnull (encoded_image);

    g_autoptr (GBytes) binary_data = gly_encoded_image_get_data (encoded_image);
    g_autoptr (GError) error = NULL;

    g_file_replace_contents (file,
                             g_bytes_get_data (binary_data, NULL),
                             g_bytes_get_size (binary_data),
                             NULL,
                             FALSE,
                             G_FILE_CREATE_NONE,
                             NULL,
                             NULL,
                             &error);

    g_assert_no_error (error);
}

static void
test_thumbnail_test_queue (void)
{
    GStrvBuilder *strv_builder = g_strv_builder_new ();
    g_auto (GStrv) files_hier = NULL;
    g_autolist (GFile) image_locations = NULL;
    guint n_images = MAX (g_get_num_processors (), 4);
    g_autoptr (GPtrArray) cancellables_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

    for (uint i = 0; i < 2 * n_images; i++)
    {
        g_strv_builder_take (strv_builder, g_strdup_printf ("image_%u.png", i + 1));
    }
    files_hier = g_strv_builder_unref_to_strv (strv_builder);
    file_hierarchy_foreach (files_hier, "", (HierarchyCallback) make_image_file, NULL);
    image_locations = file_hierarchy_get_files_list (files_hier, "", FALSE);

    /* Add to thumbnailing queue */
    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *image_location = l->data;
        g_autofree gchar *uri = g_file_get_uri (image_location);
        g_autofree gchar *mime_type = NULL;
        guint64 mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

        if (!nautilus_can_thumbnail (uri, mime_type, mtime))
        {
            g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                         "thumbnailing an image.");
            test_clear_tmp_dir ();

            return;
        }

        GCancellable *cancellable = g_cancellable_new ();

        nautilus_create_thumbnail_async (uri, mime_type, mtime,
                                         cancellable, NULL, NULL);

        g_ptr_array_add (cancellables_array, cancellable);
    }

    /* Cancel all then enqeue again */
    g_ptr_array_foreach (cancellables_array, (GFunc) g_cancellable_cancel, NULL);

    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *image_location = l->data;
        g_autofree gchar *uri = g_file_get_uri (image_location);
        g_autofree gchar *mime_type = NULL;
        guint64 mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

        nautilus_create_thumbnail_async (uri,
                                         mime_type,
                                         mtime,
                                         NULL, NULL, NULL);
    }

    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *image_location = l->data;
        g_autofree gchar *image_uri = g_file_get_uri (image_location);
        g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (image_uri);
        g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);

        ITER_CONTEXT_WHILE (!file_has_valid_thumbnail_path (image_location, thumbnail_path));

        g_assert_true (file_has_valid_thumbnail_path (image_location, thumbnail_path));
        g_assert_true (g_file_query_exists (thumbnail_location, NULL));

        g_file_delete (thumbnail_location, NULL, NULL);
    }

    test_clear_tmp_dir ();
}

static void
test_thumbnail_image (void)
{
    g_autoptr (GFile) image_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                  "Image.png",
                                                                  NULL);
    g_autofree gchar *uri = g_file_get_uri (image_location);

    make_image_file (image_location);

    g_autofree gchar *mime_type = NULL;
    guint64 mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

    g_assert_true (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (!nautilus_can_thumbnail (uri, mime_type, mtime))
    {
        g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                     "thumbnailing an image.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    g_auto (ThumbnailCallbackData) thumbnailing_data = { NULL, FALSE, NULL };

    nautilus_create_thumbnail_async (uri,
                                     mime_type,
                                     mtime,
                                     NULL,
                                     thumbnailing_done_cb,
                                     &thumbnailing_data);

    ITER_CONTEXT_WHILE (!thumbnailing_data.done);

    g_assert_true (g_file_query_exists (thumbnail_location, NULL));
    g_assert_nonnull (thumbnailing_data.pixbuf);
    g_assert_no_error (thumbnailing_data.error);
    g_assert_true (file_has_valid_thumbnail_path (image_location, thumbnail_path));

    g_assert_cmpint (gdk_pixbuf_get_height (thumbnailing_data.pixbuf), ==, ICON_SIZE);
    g_assert_cmpint (gdk_pixbuf_get_width (thumbnailing_data.pixbuf), ==, ICON_SIZE);

    g_assert_true (g_file_delete (thumbnail_location, NULL, NULL));

    test_clear_tmp_dir ();
}

static void
test_thumbnail_cancel (void)
{
    g_autoptr (GFile) image_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                  "Image.png",
                                                                  NULL);
    g_autofree gchar *uri = g_file_get_uri (image_location);

    make_image_file (image_location);

    g_autofree gchar *mime_type = NULL;
    guint64 mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

    g_assert_true (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (!nautilus_can_thumbnail (uri, mime_type, mtime))
    {
        g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                     "thumbnailing an image.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);
    g_autoptr (GCancellable) cancellable = g_cancellable_new ();
    g_auto (ThumbnailCallbackData) thumbnailing_data = { NULL, FALSE, NULL };

    nautilus_create_thumbnail_async (uri,
                                     mime_type,
                                     mtime,
                                     cancellable,
                                     thumbnailing_done_cb,
                                     &thumbnailing_data);

    g_usleep (50 * 1000);
    g_cancellable_cancel (cancellable);

    ITER_CONTEXT_WHILE (!thumbnailing_data.done);

    g_assert_nonnull (thumbnailing_data.error);
    g_assert_error (thumbnailing_data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_null (thumbnailing_data.pixbuf);

    test_clear_tmp_dir ();
}

static void
test_thumbnail_overwrite (void)
{
    g_autoptr (GFile) image_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                  "Image.png",
                                                                  NULL);
    g_autofree gchar *uri = g_file_get_uri (image_location);

    make_image_file (image_location);

    g_autofree gchar *mime_type = NULL;
    guint64 mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

    g_assert_true (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (!nautilus_can_thumbnail (uri, mime_type, mtime))
    {
        g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                     "thumbnailing an image.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);
    g_auto (ThumbnailCallbackData) thumbnailing_data1 = { NULL, FALSE, NULL };
    g_auto (ThumbnailCallbackData) thumbnailing_data2 = { NULL, FALSE, NULL };

    nautilus_create_thumbnail_async (uri,
                                     mime_type,
                                     mtime,
                                     NULL,
                                     thumbnailing_done_cb,
                                     &thumbnailing_data1);

    /* Overwrite the image file while thumbnailing is in progress */
    g_usleep (50 * 1000);
    make_image_file (image_location);
    mtime = get_file_mime_type_and_mtime (image_location, &mime_type);

    nautilus_create_thumbnail_async (uri,
                                     mime_type,
                                     mtime,
                                     NULL,
                                     thumbnailing_done_cb,
                                     &thumbnailing_data2);

    ITER_CONTEXT_WHILE (!thumbnailing_data1.done);
    ITER_CONTEXT_WHILE (!thumbnailing_data2.done);

    g_assert_true (g_file_query_exists (thumbnail_location, NULL));
    g_assert_true (file_has_valid_thumbnail_path (image_location, thumbnail_path));
    g_assert_nonnull (thumbnailing_data1.pixbuf);
    g_assert_no_error (thumbnailing_data1.error);
    g_assert_nonnull (thumbnailing_data2.pixbuf);
    g_assert_no_error (thumbnailing_data2.error);

    g_assert_cmpint (gdk_pixbuf_get_height (thumbnailing_data1.pixbuf), ==, ICON_SIZE);
    g_assert_cmpint (gdk_pixbuf_get_width (thumbnailing_data1.pixbuf), ==, ICON_SIZE);
    g_assert_cmpint (gdk_pixbuf_get_height (thumbnailing_data2.pixbuf), ==, ICON_SIZE);
    g_assert_cmpint (gdk_pixbuf_get_width (thumbnailing_data2.pixbuf), ==, ICON_SIZE);

    g_assert_true (g_file_delete (thumbnail_location, NULL, NULL));

    test_clear_tmp_dir ();
}

static GFile *
make_text_file (void)
{
    const gchar *text = "Hello, world!";
    g_autoptr (GFile) text_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                 "Document.txt",
                                                                 NULL);
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileOutputStream) stream = g_file_create (text_location,
                                                          G_FILE_CREATE_NONE,
                                                          NULL,
                                                          &error);

    g_assert_no_error (error);

    g_output_stream_write_all (G_OUTPUT_STREAM (stream), text, strlen (text), NULL, NULL, &error);
    g_assert_no_error (error);

    return g_steal_pointer (&text_location);
}

static void
test_thumbnail_text (void)
{
    g_autoptr (GFile) text_location = make_text_file ();
    g_autofree gchar *uri = g_file_get_uri (text_location);
    g_autofree gchar *mime_type = NULL;
    guint64 mtime = get_file_mime_type_and_mtime (text_location, &mime_type);

    g_assert_false (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (nautilus_can_thumbnail (uri, mime_type, mtime))
    {
        g_test_skip ("System has a thumbnailer for text files, but this test is meant to test"
                     "a file that doesn't have one.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);
    g_auto (ThumbnailCallbackData) thumbnailing_data = { NULL, FALSE, NULL };

    nautilus_create_thumbnail_async (uri,
                                     mime_type,
                                     mtime,
                                     NULL,
                                     thumbnailing_done_cb,
                                     &thumbnailing_data);

    ITER_CONTEXT_WHILE (!thumbnailing_data.done);

    g_assert_false (g_file_query_exists (thumbnail_location, NULL));
    g_assert_null (thumbnailing_data.pixbuf);
    g_assert_nonnull (thumbnailing_data.error);
    g_assert_false (file_has_valid_thumbnail_path (text_location, thumbnail_path));

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    nautilus_ensure_extension_points ();

    gtk_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);
    g_test_set_nonfatal_assertions ();

    if (nautilus_application_is_sandboxed () || !can_run_bwrap ())
    {
        /* Can't thumbnail in flatpak-builder sandbox. */
        return 77;
    }

    g_test_add_func ("/thumbnail/invalid/text",
                     test_thumbnail_text);
    g_test_add_func ("/thumbnail/single/image",
                     test_thumbnail_image);
    g_test_add_func ("/thumbnail/single/cancel",
                     test_thumbnail_cancel);
    g_test_add_func ("/thumbnail/single/overwrite",
                     test_thumbnail_overwrite);
    g_test_add_func ("/thumbnail/queue/deprioritize",
                     test_thumbnail_test_queue);

    return g_test_run ();
}
