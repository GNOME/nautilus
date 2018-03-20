#include <gtk/gtk.h>
#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <unistd.h>

void *client1;

static void
file_renamed (NautilusFile *file)
{
  g_print (" file is being renamed\n");

    g_print ("file old name: %s \n",
             nautilus_file_get_name (name));

    g_print ("file uri: %s \n",
             nautilus_file_get_uri (file));
/*
    if (nautilus_file_can_get_owner (file))
        g_print ("owner name: %s\n",
                 nautilus_file_get_owner_name (file));

    if (nautilus_file_can_get_permissions (file))
        g_print ("file permissions: %d\n",
                 nautilus_file_get_permissions(file));
*/
}

static void
file_deleted (NautilusFile *file)
{
  g_print (" file is being deleted\n");

    g_print ("file name: %s \n",
             nautilus_file_get_name (name));

    g_print ("file uri: %s \n",
             nautilus_file_get_uri (file));
/*
    if (nautilus_file_can_get_owner (file))
        g_print ("owner name: %s\n",
                 nautilus_file_get_owner_name (file));

    if (nautilus_file_can_get_permissions (file))
        g_print ("file permissions: %d\n",
                 nautilus_file_get_permissions(file));
*/
}


int
main (int    argc,
      char **argv)
{
    NautilusFile *file;
    NautilusFileAttributes attributes;
    const char *uri;
    const char *new_name;
    const char *path;

    GFile *location;

    client1 = g_new0 (int, 1);
    gtk_init (&argc, &argv);

    nautilus_ensure_extension_points ();

    if (argv[1] == NULL)
    {
        path = "/home/rahul/xyz";
    }
    else
    {
        path = argv[1];
    }
    g_print ("loading %s", uri);

    location = g_file_new_for_path (path);
    file = nautilus_file_get (location);

    g_signal_connect (file, "changed", G_CALLBACK (file_renamed), NULL);
    g_signal_connect (file, "changed", G_CALLBACK (file_deleted), NULL);

    new_name = "rahultestfile";
    nautilus_file_rename (file, new_name, rename_callback, NULL);
    /*get the newname of file using its uri*/
    if ( g_strcmp0 (nautilus_file_get_name (file), new_name) == 0)
        {
            g_printf ("Succesfully renamed");
        }
    else
        {
            g_printf ("File renaming unsuccesfull");
        }

    directory = file->details->directory;
    if (nautilus_file_can_delete (file))
        {
            nautilus_directory_remove_file (directory,file);
            //g_printf ("File deleted");
        }

    if (nautilus_file_is_gone (file))
        g_printf ("File is succesfully deleted");


    gtk_main ();
    return 0;
}
