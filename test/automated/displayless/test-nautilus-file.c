#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <unistd.h>
#include "eel/eel-string.h"

#include <src/nautilus-directory.h>
#include <src/nautilus-file.h>
#include <src/nautilus-file.c>


static void
test_check_for_wrong_filename (void)
{
    char *new_name;
    new_name = "ab";

    g_assert (new_name != NULL);

    g_assert (new_name[0] != '\0');

    /*check incoming file names for path separators*/
    g_assert ((strstr (new_name, "/") == NULL));
}


static void
test_check_file_is_not_deleted(void)
{
/*if we want to rename an already deleted file, it should fail*/
    NautilusFile *file;
    GFile *gfile;
    GFileIOStream **giostream = NULL;
    const char *uri;

    gfile = g_file_new_tmp ("abc", giostream, NULL);
    file = nautilus_file_get (gfile);
    g_assert (NAUTILUS_IS_FILE (file));

    nautilus_file_rename (file, "rahul", rename_callback, NULL);
    g_file_delete (gfile, NULL, NULL);

    g_assert (nautilus_file_can_rename (file) == FALSE);
}


static void
setup_test_suite (void)
{
    g_test_add_func ("/renaming-operation/1.0",
                     test_check_for_wrong_filename);
    g_test_add_func ("/renaming-operation/1.1",
                     test_check_file_is_not_deleted);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();

    setup_test_suite ();

    return g_test_run ();
}
