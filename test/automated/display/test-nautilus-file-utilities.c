#include <config.h>
#include <gtk/gtk.h>
#include <src/nautilus-file-utilities.h>
#include <stdio.h>

int
main (int   argc,
      char *argv[])
{
    const char *desktop_path;

    gtk_init (&argc, &argv);
    desktop_path = g_strdup (get_desktop_path ());
    g_print ("desktop_path");

    gtk_main ();
    return 0;
}
