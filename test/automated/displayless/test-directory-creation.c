#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <unistd.h>
#include "eel/eel-string.h"
#include "eel/eel-vfs-extensions.h"

#include <src/nautilus-directory.h>
#include <src/nautilus-directory.c>
#include <src/nautilus-file.h>


static void
test_check_for_creation_of_directory (void)
{
    GFile *gfile;
    NautilusDirectory *direc;
    char *uri;

    uri = "file:///tmp";
    gfile = g_file_new_for_uri (uri);
    direc = nautilus_directory_new (gfile);

    g_assert (direc != NULL);
    /* nautilus_directory_is used by trash monitor and only works if directory is monitored*/;
    g_assert (nautilus_directory_is_not_empty (direc));
}


static void
test_check_directory_for_search_uri(void)
{
    GFile *gfile;
    NautilusDirectory *direc;
    char *uri;

    uri = EEL_SEARCH_URI"://0/";
    gfile = g_file_new_for_uri (uri);
    direc = nautilus_directory_new (gfile);

    g_assert (direc != NULL);
    g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (direc));
}



static void
setup_test_suite (void)
{
    g_test_add_func ("/creation-of-new-directory/1.0",
                     test_check_for_creation_of_directory);
    g_test_add_func ("/creation-of-new-directory/1.0",
                     test_check_directory_for_search_uri);

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
