/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#define G_LOG_DOMAIN "test-image"

#include <nautilus-application.h>
#include <nautilus-global-preferences.h>
#include <nautilus-image.h>
#include <nautilus-resources.h>
#include <nautilus-thumbnails.h>
#include <test-utilities.h>

#include <gtk/gtk.h>

#define DEFAULT_SIZE NAUTILUS_GRID_ICON_SIZE_SMALL

static void
increment (guint *value)
{
    (*value)++;
}

static NautilusImage *
build_window_with_image (GtkWindow **window_out)
{
    NautilusImage *image = nautilus_image_new ();
    GtkWindow *window = GTK_WINDOW (gtk_window_new ());

    gtk_widget_set_halign (GTK_WIDGET (image), GTK_ALIGN_START);
    gtk_widget_set_valign (GTK_WIDGET (image), GTK_ALIGN_START);
    gtk_window_set_default_size (window, 400, 400);
    gtk_window_set_child (window, GTK_WIDGET (image));
    gtk_window_present (window);

    *window_out = window;

    return image;
}

static void
test_image_basic (void)
{
    GtkWindow *window;
    NautilusImage *image = build_window_with_image (&window);

    g_assert_nonnull (image);
    g_assert_true (GTK_IS_WIDGET (image));
    g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (image)));

    g_assert_null (nautilus_image_get_source (image));
    g_assert_cmpint (nautilus_image_get_size (image), ==, DEFAULT_SIZE);
    g_assert_null (nautilus_image_get_fallback (image));

    gtk_window_close (window);
}

static void
test_image_size_empty (void)
{
    g_autoptr (NautilusImage) image = g_object_ref_sink (nautilus_image_new ());
    int size = NAUTILUS_GRID_ICON_SIZE_LARGE;
    int minimum, natural;

    nautilus_image_set_size (image, size);
    g_assert_cmpint (nautilus_image_get_size (image), ==, size);

    gtk_widget_measure (GTK_WIDGET (image),
                        GTK_ORIENTATION_HORIZONTAL, -1,
                        &minimum, &natural, NULL, NULL);
    g_assert_cmpint (minimum, ==, size);
    g_assert_cmpint (natural, ==, size);
    gtk_widget_measure (GTK_WIDGET (image),
                        GTK_ORIENTATION_VERTICAL, -1,
                        &minimum, &natural, NULL, NULL);
    g_assert_cmpint (minimum, ==, size);
    g_assert_cmpint (natural, ==, size);
}

static void
test_image_source_dir (void)
{
    GtkWindow *window;
    NautilusImage *image = build_window_with_image (&window);
    g_autoptr (GFile) source = g_file_new_for_path (test_get_tmp_dir ());
    guint changed_counter = 0;

    g_signal_connect_swapped (image, "notify::source", G_CALLBACK (increment), &changed_counter);

    nautilus_image_set_source (image, source);
    g_assert_cmpint (changed_counter, ==, 1);
    nautilus_image_set_source (image, source);
    g_assert_cmpint (changed_counter, ==, 1);
    g_assert_true (g_file_equal (source, nautilus_image_get_source (image)));

    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    ITER_CONTEXT_WHILE (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_FALLBACK);
    g_assert_cmpint (changed_counter, ==, 1);

    /* Wait for widget snapshot */
    gtk_test_widget_wait_for_draw (GTK_WIDGET (image));
    g_assert_cmpint (gtk_widget_get_width (GTK_WIDGET (image)), ==, DEFAULT_SIZE);
    g_assert_cmpint (gtk_widget_get_height (GTK_WIDGET (image)), ==, DEFAULT_SIZE);

    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    g_assert_null (nautilus_image_get_source (image));
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_FALLBACK);

    gtk_window_close (window);
    test_clear_tmp_dir ();
}

static void
thumbnailing_done_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      data)
{
    GMainLoop *loop = data;

    g_main_loop_quit (loop);
}

static void
test_image_source_image_thumbnailed (void)
{
    GtkWindow *window;
    NautilusImage *image = build_window_with_image (&window);
    g_autoptr (GFile) image_source = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "Image.png",
                                                                NULL);
    guint64 mtime = 1;
    guint8 color[4] = {255, 255, 0, 0};
    guint size = DEFAULT_SIZE;
    g_autofree char *uri = g_file_get_uri (image_source);
    g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
    guint changed_counter = 0;

    /* Create a thumbnail beforehand */
    make_image_file_full (image_source, color, size, size, mtime);
    nautilus_create_thumbnail_async (uri, "image/png",
                                     0,
                                     NULL,
                                     thumbnailing_done_cb,
                                     loop);
    g_main_loop_run (loop);

    nautilus_image_set_size (image, size);
    g_signal_connect_swapped (image, "notify::source", G_CALLBACK (increment), &changed_counter);

    nautilus_image_set_source (image, image_source);
    g_assert_cmpint (changed_counter, ==, 1);
    nautilus_image_set_source (image, image_source);
    g_assert_cmpint (changed_counter, ==, 1);
    g_assert_true (g_file_equal (image_source, nautilus_image_get_source (image)));

    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    ITER_CONTEXT_WHILE (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_THUMBNAIL);
    ITER_CONTEXT_WHILE (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_THUMBNAIL);
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_THUMBNAIL);
    g_assert_cmpint (changed_counter, ==, 1);

    /* Wait for widget snapshot */
    gtk_test_widget_wait_for_draw (GTK_WIDGET (image));
    g_assert_cmpint (gtk_widget_get_width (GTK_WIDGET (image)), ==, size);
    g_assert_cmpint (gtk_widget_get_height (GTK_WIDGET (image)), ==, size);

    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    g_assert_null (nautilus_image_get_source (image));
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_FALLBACK);

    gtk_window_close (window);
    test_clear_tmp_dir ();
}

static void
test_image_source_image_thumbnail (void)
{
    GtkWindow *window;
    NautilusImage *image = build_window_with_image (&window);
    g_autoptr (GFile) image_source = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "Image.png",
                                                                NULL);
    guint64 mtime = 1;
    guint8 color[4] = {255, 255, 0, 0};
    guint size = DEFAULT_SIZE;
    guint changed_counter = 0;

    make_image_file_full (image_source, color, size, size, mtime);
    nautilus_image_set_size (image, size);
    g_signal_connect_swapped (image, "notify::source", G_CALLBACK (increment), &changed_counter);

    nautilus_image_set_source (image, image_source);
    g_assert_cmpint (changed_counter, ==, 1);
    nautilus_image_set_source (image, image_source);
    g_assert_cmpint (changed_counter, ==, 1);
    g_assert_true (g_file_equal (image_source, nautilus_image_get_source (image)));

    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    ITER_CONTEXT_WHILE (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES);
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_THUMBNAIL);
    ITER_CONTEXT_WHILE (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_LOADING_THUMBNAIL);
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_THUMBNAIL);
    g_assert_cmpint (changed_counter, ==, 1);

    /* Wait for widget snapshot */
    gtk_test_widget_wait_for_draw (GTK_WIDGET (image));
    g_assert_cmpint (gtk_widget_get_width (GTK_WIDGET (image)), ==, size);
    g_assert_cmpint (gtk_widget_get_height (GTK_WIDGET (image)), ==, size);

    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    nautilus_image_set_source (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    g_assert_null (nautilus_image_get_source (image));
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_FALLBACK);

    gtk_window_close (window);
    test_clear_tmp_dir ();
}

static void
test_image_fallback (void)
{
    GtkWindow *window;
    NautilusImage *image = build_window_with_image (&window);
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    g_autoptr (GIcon) icon = g_themed_icon_new ("image-missing");
    g_autoptr (GtkIconPaintable) paintable =
        gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                        DEFAULT_SIZE, 1,
                                        GTK_TEXT_DIR_NONE,
                                        GTK_ICON_LOOKUP_PRELOAD);
    guint changed_counter = 0;
    GdkPaintable *fallback;

    g_signal_connect_swapped (image, "notify::fallback", G_CALLBACK (increment), &changed_counter);

    nautilus_image_set_fallback (image, GDK_PAINTABLE (paintable));
    g_assert_cmpint (changed_counter, ==, 1);
    nautilus_image_set_fallback (image, GDK_PAINTABLE (paintable));
    g_assert_cmpint (changed_counter, ==, 1);
    fallback = nautilus_image_get_fallback (image);
    g_assert_true (fallback == GDK_PAINTABLE (paintable));
    g_assert_true (nautilus_image_get_status (image) == NAUTILUS_IMAGE_STATUS_FALLBACK);

    /* Wait for widget snapshot */
    gtk_test_widget_wait_for_draw (GTK_WIDGET (image));
    g_assert_cmpint (gdk_paintable_get_intrinsic_height (fallback), ==, DEFAULT_SIZE);
    g_assert_cmpint (gdk_paintable_get_intrinsic_width (fallback), ==, DEFAULT_SIZE);

    nautilus_image_set_fallback (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    g_assert_null (nautilus_image_get_fallback (image));
    nautilus_image_set_fallback (image, NULL);
    g_assert_cmpint (changed_counter, ==, 2);
    g_assert_null (nautilus_image_get_fallback (image));

    gtk_window_close (window);
}

int
main (int   argc,
      char *argv[])
{
    gtk_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

    nautilus_register_resource ();
    nautilus_global_preferences_init ();

    if (nautilus_application_is_sandboxed ())
    {
        g_message ("Cannot run thumbnails tests inside a sandbox.");

        return 77;
    }

    if (!nautilus_can_thumbnail ("file:///tmp/nautilus_tests/image.png", "image/png", 1))
    {
        g_message ("System has no thumbnailer for PNGs, but tests are meant to test "
                   "thumbnailing images.");

        return 77;
    }

    /* HACK: Pretend to be a snap to disable sandboxing in libgnome-desktop */
    g_setenv ("SNAP_NAME", "1", TRUE);

    g_test_add_func ("/image/basic",
                     test_image_basic);
    g_test_add_func ("/image/size/empty",
                     test_image_size_empty);
    g_test_add_func ("/image/source/dir",
                     test_image_source_dir);
    g_test_add_func ("/image/source/image/thumbnailed",
                     test_image_source_image_thumbnailed);
    g_test_add_func ("/image/source/image/thumbnail",
                     test_image_source_image_thumbnail);
    g_test_add_func ("/image/fallback",
                     test_image_fallback);

    return g_test_run ();
}
