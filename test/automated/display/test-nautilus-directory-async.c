#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <unistd.h>

static GMainLoop *main_loop;
void *client1, *client2;

static void
files_added (NautilusDirectory *directory,
             GList             *added_files)
{
#if 0
    GList *list;

    for (list = added_files; list != NULL; list = list->next)
    {
        NautilusFile *file = list->data;

        g_print (" - %s\n", nautilus_file_get_uri (file));
    }
#endif

    g_print ("files added: %d files\n",
             g_list_length (added_files));
}

static void
files_changed (NautilusDirectory *directory,
               GList             *changed_files)
{
#if 0
    GList *list;

    for (list = changed_files; list != NULL; list = list->next)
    {
        NautilusFile *file = list->data;

        g_print (" - %s\n", nautilus_file_get_uri (file));
    }
#endif
    g_print ("files changed: %d\n",
             g_list_length (changed_files));
}

static void
done_loading (NautilusDirectory *directory)
{
    g_print ("done loading\n");
    g_main_loop_quit (main_loop);
}

int
main (int    argc,
      char **argv)
{
    NautilusDirectory *directory;
    NautilusFileAttributes attributes;
    const char *uri;

    client1 = g_new0 (int, 1);
    client2 = g_new0 (int, 1);

    main_loop = g_main_loop_new (NULL, FALSE);

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
    directory = nautilus_directory_get_by_uri (uri);

    g_signal_connect (directory, "files-added", G_CALLBACK (files_added), NULL);
    g_signal_connect (directory, "files-changed", G_CALLBACK (files_changed), NULL);
    g_signal_connect (directory, "done-loading", G_CALLBACK (done_loading), NULL);

    attributes =
        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
        NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_MOUNT |
        NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

    nautilus_directory_file_monitor_add (directory, client1, TRUE,
                                         attributes,
                                         NULL, NULL);


    g_main_loop_run (main_loop);

    g_main_loop_unref (main_loop);

    return 0;
}
