#include <glib.h>
#include <glib/gprintf.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include <unistd.h>
#include "eel/eel-string.h"

#define ROOT_DIR "file:///tmp"

/* Tests the function for a simple file */
static void
test_simple_file (void)
{
    g_autofree const gchar *root_uri = NULL;
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFileOutputStream) out = NULL;

    root_uri = g_get_tmp_dir ();
    root = g_file_new_for_uri (root_uri);
    file = g_file_get_child (root, "simple_file");
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    // file = g_file_new_build_filename (root, "simple_file", NULL);
    g_assert_false (dir_has_files (file));
    // g_output_stream_close (G_OUTPUT_STREAM (out), NULL, NULL);
}

/* Tests the function for an empty directory */
static void
test_empty_directory (void)
{
    g_autofree const gchar *root_uri = NULL;
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) child = NULL;

    root_uri = g_get_tmp_dir ();
    root = g_file_new_for_uri (root_uri);
    child = g_file_get_child (root, "empty_dir");

    g_assert_true (child != NULL);

    g_file_make_directory (child, NULL, NULL);

    g_assert_false (dir_has_files (child));
}

/* Tests the function for a directory containing one directory */
static void
test_directory_one_file (void)
{
    g_autofree const gchar *root_uri = NULL;
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) parent_dir = NULL;
    g_autoptr (GFile) child_file = NULL;

    root_uri = g_get_tmp_dir ();
    root = g_file_new_for_uri (root_uri);
    parent_dir = g_file_get_child (root, "parent_dir");
    g_assert_true (parent_dir != NULL);
    g_file_make_directory (parent_dir, NULL, NULL);

    child_file = g_file_get_child (parent_dir, "child_file");
    g_assert_true (child_file != NULL);
    g_file_make_directory (child_file, NULL, NULL);

    g_assert_true (dir_has_files (parent_dir));
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/dir-has-files-simple-file/1.0",
                     test_simple_file);
    g_test_add_func ("/dir-has-files-empty-dir/1.0",
                     test_empty_directory);
    g_test_add_func ("/dir-has-files-directroy-one-file/1.0",
                     test_directory_one_file);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();

    setup_test_suite ();

    return g_test_run ();
}