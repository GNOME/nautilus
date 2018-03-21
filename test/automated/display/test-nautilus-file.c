#include <gtk/gtk.h>
#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <unistd.h>

void *client1;

/*
static void
file_renamed (NautilusFile *file)
{
  g_print (" file is being renamed\n");

    g_print ("file old name: %s \n",
             nautilus_file_get_name (name));

    g_print ("file uri: %s \n",
             nautilus_file_get_uri (file));
}
*/

int
main (int    argc,
      char **argv)
{
    NautilusFile *file;
    NautilusFileAttributes attributes;
    const char *uri;
    const char *new_name, *initial_name;
    const char *path;
    GFile *location;

    client1 = g_new0 (int, 1);
    gtk_init (&argc, &argv);

    nautilus_ensure_extension_points ();
    initial_name = "abc";
    if (argv[1] == NULL)
    {
        uri = "file:///tmp";
    }
    else
    {
        uri = argv[1];
    }
    g_print ("loading %s", uri);
    directory = nautilus_directory_get_by_uri (uri);
    file = nautilus_file_new_from_filename (directory, initial_name, FALSE);

    /*location = g_file_new_for_path (path);
    file = nautilus_file_get (location);*/

    new_name = "xyz";

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (new_name != NULL);
    g_assert (new_name[0] != '\0');

//    g_signal_connect (file, "changed", G_CALLBACK (file_renamed), NULL);

    /*check incoming file names for path separators*/
    g_assert ((strstr (new_name, "/") != NULL);
    /*cant rename a file that is already gone*/
    g_assert (!nautilus_file_is_gone (file));

    /*nautilus_file_rename calls nautilus_file_can_rename inside it*/
    nautilus_file_rename (file, new_name, rename_callback, NULL);

    while (nautilus_file_rename_in_progress (file))
        {
            gtk_main_iteration();
        }

    /*get the newname of file using its uri and check whether it is same as the new name we gave it to*/
    g_assert (g_strcmp0 (nautilus_file_get_name (file), new_name) == 0);

/*
     attributes =
      NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
      NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
      NAUTILUS_FILE_ATTRIBUTE_INFO |
      NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
      NAUTILUS_FILE_ATTRIBUTE_MOUNT |
      NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

     nautilus_file_monitor_add (file, client1,
                                         attributes);
*/
    gtk_main ();
    return 0;
}
