/*
 * Copyright © 2025 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "test-utilities.h"

#include <nautilus-application.h>
#include <nautilus-file.h>
#include <nautilus-file-utilities.h>
#include <nautilus-thumbnails.h>

#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

static void
file_ready_cb (NautilusFile *file,
               gpointer      callback_data)
{
    gboolean *file_is_ready = callback_data;

    *file_is_ready = TRUE;
}

static void
files_ready_cb (GList    *file_list,
                gpointer  callback_data)
{
    gboolean *files_are_ready = callback_data;

    *files_are_ready = TRUE;
}

static void
make_image_file (GFile  *file,
                 GList **image_files)
{
    g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 256, 256);
    g_autofree gchar *image_path = g_file_get_path (file);
    g_autoptr (GError) error = NULL;

    gdk_pixbuf_fill (pixbuf, 0xff0000);
    g_assert_true (gdk_pixbuf_save (pixbuf, image_path, "png", &error, NULL));
    g_assert_no_error (error);

    if (image_files != NULL)
    {
        *image_files = g_list_append (*image_files, nautilus_file_get (file));
    }
}

static void
test_thumbnail_test_queue (void)
{
    GStrvBuilder *strv_builder = g_strv_builder_new ();
    g_auto (GStrv) files_hier = NULL;
    gboolean files_are_ready = FALSE;
    g_autolist (NautilusFile) image_files = NULL;
    guint n_images = MAX (g_get_num_processors (), 4);

    for (uint i = 0; i < 2 * n_images; i++)
    {
        g_strv_builder_take (strv_builder, g_strdup_printf ("image_%u", i + 1));
    }
    files_hier = g_strv_builder_unref_to_strv (strv_builder);
    file_hierarchy_foreach (files_hier, "", (HierarchyCallback) make_image_file, &image_files);

    nautilus_file_list_call_when_ready (image_files,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO |
                                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                        NULL,
                                        files_ready_cb, &files_are_ready);

    ITER_CONTEXT_WHILE (!files_are_ready);

    /* Add to thumbnailing queue */
    for (GList *l = image_files; l != NULL; l = l->next)
    {
        NautilusFile *image_file = NAUTILUS_FILE (l->data);

        if (!nautilus_can_thumbnail (image_file))
        {
            g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                         "thumbnailing an image.");
            test_clear_tmp_dir ();

            return;
        }

        nautilus_create_thumbnail (image_file);
    }

    /* Cancel all then enqeue again */
    for (GList *l = image_files; l != NULL; l = l->next)
    {
        NautilusFile *image_file = NAUTILUS_FILE (l->data);
        g_autofree gchar *image_uri = nautilus_file_get_uri (image_file);

        nautilus_thumbnail_remove_from_queue (image_uri);
    }
    for (GList *l = image_files; l != NULL; l = l->next)
    {
        NautilusFile *image_file = NAUTILUS_FILE (l->data);

        g_assert_false (nautilus_file_is_thumbnailing (image_file));
        nautilus_create_thumbnail (image_file);
        g_assert_true (nautilus_file_is_thumbnailing (image_file));
    }

    for (GList *l = image_files; l != NULL; l = l->next)
    {
        NautilusFile *image_file = NAUTILUS_FILE (l->data);
        g_autofree gchar *image_uri = nautilus_file_get_uri (image_file);
        g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (image_uri);
        g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);

        ITER_CONTEXT_WHILE (nautilus_file_is_thumbnailing (image_file));

        g_assert_false (nautilus_file_is_thumbnailing (image_file));
        g_assert_true (g_file_query_exists (thumbnail_location, NULL));
        g_assert_true (nautilus_file_has_thumbnail (image_file));
        g_assert_cmpstr (nautilus_file_get_thumbnail_path (image_file), ==, thumbnail_path);

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
    g_autoptr (NautilusFile) image_file = nautilus_file_get (image_location);
    gboolean file_is_ready = FALSE;
    const gchar *mime_type;

    make_image_file (image_location, NULL);
    nautilus_file_call_when_ready (image_file,
                                   NAUTILUS_FILE_ATTRIBUTE_INFO | NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                   file_ready_cb, &file_is_ready);

    ITER_CONTEXT_WHILE (!file_is_ready);

    mime_type = nautilus_file_get_mime_type (image_file);
    g_assert_true (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (!nautilus_can_thumbnail (image_file))
    {
        g_test_skip ("System has no thumbnailer for images, but this test is meant to test "
                     "thumbnailing an image.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *uri = g_file_get_uri (image_location);
    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);

    nautilus_create_thumbnail (image_file);

    ITER_CONTEXT_WHILE (nautilus_file_is_thumbnailing (image_file));

    g_assert_false (nautilus_file_is_thumbnailing (image_file));
    g_assert_true (g_file_query_exists (thumbnail_location, NULL));
    g_assert_true (nautilus_file_has_thumbnail (image_file));
    g_assert_cmpstr (nautilus_file_get_thumbnail_path (image_file), ==, thumbnail_path);

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
    g_autoptr (NautilusFile) text_file = nautilus_file_get (text_location);
    gboolean file_is_ready = FALSE;
    const gchar *mime_type;

    nautilus_file_call_when_ready (text_file,
                                   NAUTILUS_FILE_ATTRIBUTE_INFO | NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                   file_ready_cb, &file_is_ready);

    ITER_CONTEXT_WHILE (!file_is_ready);

    mime_type = nautilus_file_get_mime_type (text_file);

    g_assert_false (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type));

    if (nautilus_can_thumbnail (text_file))
    {
        g_test_skip ("System has a thumbnailer for text files, but this test is meant to test"
                     "a file that don't have one.");
        test_clear_tmp_dir ();

        return;
    }

    g_autofree gchar *uri = g_file_get_uri (text_location);
    g_autofree gchar *thumbnail_path = nautilus_thumbnail_get_path_for_uri (uri);
    g_autoptr (GFile) thumbnail_location = g_file_new_for_path (thumbnail_path);

    nautilus_create_thumbnail (text_file);

    ITER_CONTEXT_WHILE (nautilus_file_is_thumbnailing (text_file));

    g_assert_false (nautilus_file_is_thumbnailing (text_file));
    g_assert_false (g_file_query_exists (thumbnail_location, NULL));
    g_assert_false (nautilus_file_has_thumbnail (text_file));
    g_assert_null (nautilus_file_get_thumbnail_path (text_file));

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
