/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#define G_LOG_DOMAIN "test-properties"

#include <nautilus-file.h>
#include <nautilus-file-utilities.h>
#include <nautilus-global-preferences.h>
#include <nautilus-properties.h>
#include <nautilus-resources.h>
#include <nautilus-tag-manager.h>

#include <test-utilities.h>

#include <glib.h>
#include <gtk/gtk.h>

static gboolean
all_files_are_ready (GList *files)
{
    for (GList *l = files; l != NULL; l = l->next)
    {
        NautilusFile *file = l->data;

        if (!nautilus_file_check_if_ready (file, NAUTILUS_ATTRIBUTE_INFO))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
test_directory_empty (void)
{
    g_autoptr (GFile) test_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (NautilusFile) test_file = nautilus_file_get (test_location);
    g_autoptr (GtkWindow) properties_window =
        nautilus_properties_present_window (&(GList) { .data = test_file }, NULL);

    ITER_CONTEXT_WHILE (!nautilus_file_check_if_ready (test_file, NAUTILUS_ATTRIBUTE_DEEP_COUNT));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    test_clear_tmp_dir ();
}

static void
test_directory_root (void)
{
    g_autoptr (GFile) test_location = g_file_new_for_path ("/");
    g_autoptr (NautilusFile) test_file = nautilus_file_get (test_location);
    g_autoptr (GtkWindow) properties_window =
        nautilus_properties_present_window (&(GList) { .data = test_file }, NULL);

    ITER_CONTEXT_WHILE (!nautilus_file_check_if_ready (test_file, NAUTILUS_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    test_clear_tmp_dir ();
}

static void
test_directory_unreadable (void)
{
    g_autoptr (GFile) unreadable_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                       "unaccessible",
                                                                       NULL);
    g_autoptr (NautilusFile) unreadable_file = nautilus_file_get (unreadable_location);
    g_autoptr (GtkWindow) properties_window = NULL;

    g_assert_true (g_file_make_directory (unreadable_location, NULL, NULL));
    g_assert_true (g_file_set_attribute_uint32 (unreadable_location,
                                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                                0000,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL));
    properties_window =
        nautilus_properties_present_window (&(GList) { .data = unreadable_file }, NULL);

    ITER_CONTEXT_WHILE (!nautilus_file_check_if_ready (unreadable_file, NAUTILUS_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    test_clear_tmp_dir ();
}

static void
test_image (void)
{
    g_autoptr (GFile) image1_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                   "image1.png",
                                                                   NULL);
    g_autoptr (GFile) image2_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                   "image2.png",
                                                                   NULL);
    g_autoptr (NautilusFile) image1_file = nautilus_file_get (image1_location);
    g_autoptr (NautilusFile) image2_file = nautilus_file_get (image2_location);
    g_autoptr (GtkWindow) properties_window = NULL;

    /* First image is thumbnailed */
    make_image_file_with_mtime (image1_location, 1, TRUE);
    properties_window = nautilus_properties_present_window (&(GList) { .data = image1_file }, NULL);

    ITER_CONTEXT_WHILE (!nautilus_file_check_if_ready (image1_file, NAUTILUS_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    /* Second image is not thumbnailed */
    make_image_file_with_mtime (image2_location, 1, FALSE);
    properties_window = nautilus_properties_present_window (&(GList) { .data = image2_file }, NULL);

    ITER_CONTEXT_WHILE (!nautilus_file_check_if_ready (image2_file, NAUTILUS_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));
    ITER_CONTEXT_WHILE (g_main_context_pending (NULL));

    test_clear_tmp_dir ();
}

static void
test_images (void)
{
    GStrv hier = (char *[])
    {
        "image1.png",
        "image2.png",
        "image3.png",
        "image4.png",
        NULL
    };
    g_autolist (GFile) image_locations = file_hierarchy_get_files_list (hier, "", FALSE);
    g_autolist (NautilusFile) image_files = NULL;
    g_autoptr (GtkWindow) properties_window = NULL;

    /* Many images that are not thumbnailed */
    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *location = l->data;

        make_image_file_with_mtime (location, 1, FALSE);
        image_files = g_list_prepend (image_files, nautilus_file_get (location));
    }
    properties_window = nautilus_properties_present_window (image_files, NULL);

    ITER_CONTEXT_WHILE (!all_files_are_ready (image_files));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    /* Many images that are thumbnailed */
    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *location = l->data;

        make_image_file_with_mtime (location, 2, TRUE);
    }
    properties_window = nautilus_properties_present_window (image_files, NULL);

    ITER_CONTEXT_WHILE (!all_files_are_ready (image_files));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (g_steal_pointer (&properties_window));

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusTagManager) tag_manager = NULL;

    gtk_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

    nautilus_register_resource ();
    nautilus_ensure_extension_points ();
    nautilus_global_preferences_init ();
    tag_manager = nautilus_tag_manager_new_dummy ();

    g_test_add_func ("/properties/directory/empty",
                     test_directory_empty);
    g_test_add_func ("/properties/directory/root",
                     test_directory_root);
    g_test_add_func ("/properties/directory/unreadable",
                     test_directory_unreadable);
    g_test_add_func ("/properties/image/one",
                     test_image);
    g_test_add_func ("/properties/image/many",
                     test_images);

    return g_test_run ();
}
