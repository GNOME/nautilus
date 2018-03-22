#include <glib.h>
#include <glib/gprintf.h>
#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <src/nautlius-file.c>
#include <unistd.h>
#include "eel/eel-string.h"


static void
test_check_for_wrong_filename (void)
{
    char *new_name;
    new_name = "ab/";

    g_assert (new_name != NULL);

    g_assert (new_name[0] != '\0');

    /*check incoming file names for path separators*/
    g_assert ((strstr (new_name, "/") != NULL));
}

static void
test_check_file_is_renamed_correctly (void)
{

    NautilusFile *file;
    NautilusDirectory *directory;
    const char *uri;
    const char *new_name, *initial_name;

    uri = "file:///tmp";
    initial_name = "abc";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    file = nautilus_file_new_from_filename (directory, initial_name, FALSE);
    g_assert (NAUTILUS_IS_FILE (file));

    new_name = "xyz";

    /*cant rename a file that is already gone*/
    g_assert (!nautilus_file_is_gone (file));

    /*nautilus_file_rename calls nautilus_file_can_rename inside it*/
    nautilus_file_rename (file, new_name, rename_callback, NULL);

    while (nautilus_file_rename_in_progress (file))
          {
              /*keep looping till file hasn't been renamed*/
          }

    /*get the newname of file using its uri and check whether it is same as the new name we gave it to*/
    g_assert (g_strcmp0 (nautilus_file_get_name (file), new_name) == 0);

}

static void
setup_test_suite (void)
{
    g_test_add_func ("/renaming-operation/1.0",
                     test_check_for_wrong_filename);
    g_test_add_func ("/renaming-operation/1.1",
                     test_check_file_is_renamed_correctly);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();

    setup_test_suite ();

    return g_test_run ();
}
