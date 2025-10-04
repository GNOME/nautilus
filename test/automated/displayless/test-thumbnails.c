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

static void
make_image_file (GFile  *file,
                 GList **image_files)
{
    g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
                                                   ICON_SIZE, ICON_SIZE);
    const gchar *image_path = g_file_peek_path (file);
    g_autoptr (GError) error = NULL;

    gdk_pixbuf_fill (pixbuf, 0xff0000);
    g_assert_true (gdk_pixbuf_save (pixbuf, image_path, "png", &error, NULL));
    g_assert_no_error (error);

    if (image_files != NULL)
    {
        *image_files = g_list_append (*image_files, g_object_ref (file));
    }
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
    file_hierarchy_foreach (files_hier, "", (HierarchyCallback) make_image_file, &image_locations);

    /* Add to thumbnailing queue */
    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *image_location = l->data;
        g_autofree gchar *uri = g_file_get_uri (image_location);
        g_autoptr (GFileInfo) info = g_file_query_info (image_location,
                                                        "standard::*,"
                                                        "time::*",
                                                        G_FILE_QUERY_INFO_NONE,
                                                        NULL, NULL);
        const gchar *mime_type = g_file_info_get_content_type (info);
        guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

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
        g_autoptr (GFileInfo) info = g_file_query_info (image_location,
                                                        "standard::*,"
                                                        "time::*",
                                                        G_FILE_QUERY_INFO_NONE,
                                                        NULL, NULL);
        const gchar *mime_type = g_file_info_get_content_type (info);
        guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

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

    make_image_file (image_location, NULL);

    g_autoptr (GFileInfo) info = g_file_query_info (image_location,
                                                    "standard::*,time::*",
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, NULL);
    const gchar *mime_type = g_file_info_get_content_type (info);
    guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

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
    g_autoptr (GFileInfo) info = g_file_query_info (text_location,
                                                    "standard::*,time::*",
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, NULL);
    const gchar *mime_type = g_file_info_get_content_type (info);
    guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

    g_assert_false (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (nautilus_can_thumbnail (uri, mime_type, mtime))
    {
        g_test_skip ("System has a thumbnailer for text files, but this test is meant to test"
                     "a file that don't have one.");
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
    g_test_add_func ("/thumbnail/queue/deprioritize",
                     test_thumbnail_test_queue);

    return g_test_run ();
}
