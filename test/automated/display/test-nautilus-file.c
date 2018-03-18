#include <gtk/gtk.h>
#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <unistd.h>

void *client1;

static void
changed (NautilusFile *file)
{
  g_print (" file changed\n");

    g_print ("file name: %s \n",
             nautilus_file_get_name (name));

    g_print ("file uri: %s \n",
             nautilus_file_get_uri (file));

    if (nautilus_file_can_get_owner (file))
        g_print ("owner name: %s\n",
                 nautilus_file_get_owner_name (file));

    if (nautilus_file_can_get_permissions (file))
        g_print ("file permissions: %d\n",
                 nautilus_file_get_permissions(file));
}

int
main (int    argc,
      char **argv)
{
    NautilusFile *file;
    NautilusFileAttributes attributes;
    const char *uri;

    client1 = g_new0 (int, 1);
    gtk_init (&argc, &argv);

    nautilus_ensure_extension_points ();

    if (argv[1] == NULL)
    {
        uri = "file:///tmp";
    }
    else
    {
        uri = argv[1];
    }
    g_print ("loading %s", uri);
    file = nautilus_file_get_by_uri (uri);

    g_signal_connect (file, "file-changed", G_CALLBACK (changed), NULL);

    attributes =
        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
        NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
        NAUTILUS_FILE_ATTRIBUTE_MOUNT |
        NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

    nautilus_file_monitor_add (file, client1,
                                         attributes);

    gtk_main ();
    return 0;
}
